/**
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <errno.h>

#include <hw/wb_uart.h>
#include <libdevmap.h>

#define VUART_EOL 13
#define VUART_CMD_USLEEP 1000000
#define VUART_CMD_PROMPT "wrc#"

static void wrpc_vuart_help(char *prog)
{
	const char *mapping_help_str;

	mapping_help_str = dev_mapping_help();
	fprintf(stderr, "%s [options]\n", prog);
	fprintf(stderr, "%s\n", mapping_help_str);
	fprintf(stderr, "for vuart, address offset should be 0x500\n");
	fprintf(stderr, "Vuart specific option: [-k(keep terminal)]\n");
}

/**
 * It receives a single byte
 * @param[in] vuart token from dev_map()
 *
 *
 */

static uint32_t vuart_readl(struct mapping_desc *vuart, int reg)
{
	uint32_t r = *(volatile uint32_t *)( vuart->base + reg );

	if(vuart->is_be)
		return ntohl(r);
	else
		return r;
}


static void vuart_writel(struct mapping_desc *vuart, uint32_t value, int reg)
{
	if(vuart->is_be)
		value = htonl(value);

	*(volatile uint32_t *)( vuart->base + reg ) = value;
}

static int wr_vuart_rx(struct mapping_desc *vuart)
{
	int rdr = vuart_readl( vuart, UART_REG_HOST_RDR );
	return (rdr & UART_HOST_RDR_RDY) ? UART_HOST_RDR_DATA_R(rdr) : -1;
}

/**
 * It transmits a single byte
 * @param[in] vuart token from dev_map()
 */
static void wr_vuart_tx(struct mapping_desc *vuart, char data)
{
	int sr = vuart_readl( vuart, UART_REG_SR );

	while(sr & UART_SR_RX_RDY)
		 sr = vuart_readl( vuart, UART_REG_SR );

	vuart_writel( vuart, UART_HOST_TDR_DATA_W(data), UART_REG_HOST_TDR );
}

/**
 * It reads a number of bytes and it stores them in a given buffer
 * @param[in] vuart token from dev_map()
 * @param[out] buf destination for read bytes
 * @param[in] size numeber of bytes to read
 *
 * @return the number of read bytes
 */
static size_t wr_vuart_read(struct mapping_desc *vuart, char *buf, size_t size)
{
	size_t s = size, n_rx = 0;
	int8_t c;

	while(s--) {
		c =  wr_vuart_rx(vuart);
		if(c < 0)
			return n_rx;
		*buf++ = c;
		n_rx ++;
	}
	return n_rx;
}

/**
 * It flush vuart buffer.
 *
 * @param[in] vuart token from dev_map()
 *
 */
static void wr_vuart_flush(struct mapping_desc *vuart)
{
	char rx;

	while(wr_vuart_read(vuart,&rx,1) == 1) {}
}

/**
 * It writes a number of bytes from a given buffer
 * @param[in] vuart token from dev_map()
 * @param[in] buf buffer to write
 * @param[in] size numeber of bytes to write
 */
static void wr_vuart_write(struct mapping_desc *vuart, char *buf, size_t size)
{
	while(size--)
		wr_vuart_tx(vuart, *buf++);
}

static void wrpc_vuart_set_tty_raw(struct termios *old_termios)
{
  	struct termios newkey;

	tcgetattr(STDIN_FILENO,old_termios);
	memcpy(&newkey, old_termios, sizeof(struct termios));
	newkey.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
	newkey.c_iflag = IGNPAR;
	newkey.c_oflag = 0;
	newkey.c_lflag = ISIG;  /* Keep C-c, C-z, ... */
	tcflush(STDIN_FILENO, TCIFLUSH);
	tcsetattr(STDIN_FILENO,TCSANOW,&newkey);
}

static void wrpc_vuart_restore_tty(struct termios *old_termios)
{
	tcsetattr(STDIN_FILENO, TCSANOW, old_termios);
}

static void wrpc_vuart_term(struct mapping_desc *vuart, int keep_term)
{
	struct termios oldkey;
	int need_exit = 0;
	fd_set fds;
	int ret;
	int rx, tx;

	fprintf(stderr, "[press C-a to exit]\n");

	if(!keep_term)
		wrpc_vuart_set_tty_raw(&oldkey);

	while(!need_exit) {
		struct timeval tv = {0, 10000};

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);

		/*
		 * Check if the STDIN has characters to read
		 * (what the user writes)
		 */
		ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
		switch (ret) {
		case -1:
			perror("select");
			break;
		case 0: /* timeout */
			break;
		default:
			if(!FD_ISSET(STDIN_FILENO, &fds))
				break;
			/* The user wrote something */
			do {
				ret = read(STDIN_FILENO, &tx, 1);
			} while (ret < 0 && errno == EINTR);
			if (ret != 1) {
				fprintf(stderr, "nothing to read. Port disconnected?\n");
				need_exit = 1; /* kill */
			}
			/* If the user character is C-a, then kill */
			if(tx == '\x01') {
				need_exit = 1;
				break;
			}

			wr_vuart_tx(vuart, tx);
			break;
		}

		/* Print all the incoming characters */
		while((rx = wr_vuart_rx(vuart)) > 0) {
			putchar(rx);
		}
		fflush(stdout);
	}

	if(!keep_term)
		wrpc_vuart_restore_tty(&oldkey);
}

static void wrpc_vuart_command(struct mapping_desc *vuart, char *command)
{
	//above is place for old and new port settings for keyboard teletype
	int cmd_len = 0;
	char *prompt = VUART_CMD_PROMPT;
	int i_prompt = 0;
	int i;
	int rx;

	/* Flush Vuart before sending command */
	wr_vuart_flush(vuart);
	/* Send command */
	cmd_len = strlen(command);
	wr_vuart_write(vuart, command, cmd_len);
	/* Flush command echo */
	wr_vuart_flush(vuart);
	/* Send end character */
	wr_vuart_tx(vuart, VUART_EOL);
	/* Wait for a while before reading command results */
	usleep(VUART_CMD_USLEEP);
	/* Discard characters until end of line control one */
	while((rx = wr_vuart_rx(vuart)) > 0) {
		if(rx == VUART_EOL)
			break;
	}

	while(1) {
		/* Print all the incoming characters */
		rx = wr_vuart_rx(vuart);
		if (rx < 0) {
			usleep(10);
			continue;
		}

		/* Prompt detection, skip characters */
		if (rx == prompt[i_prompt]) {
			i_prompt++;
			/* Prompt detected! */
			if(i_prompt == strlen(prompt))
				return;
		} else {
			/* Check if some previous characters have been skipped
			   by prompt detector code and print them */
			for(i = 0 ; i < i_prompt ; i++)
				putchar(prompt[i]);
			/* Reset prompt detector */
			i_prompt = 0;
			/* Print current character */
			putchar(rx);
			fflush(stdout);
		}
	}
}


int main(int argc, char *argv[])
{
	char c;
	int keep_term = 0;
	char *cmd = NULL;
	struct mapping_args *map_args;
	struct mapping_desc *vuart = NULL;

	map_args = dev_parse_mapping_args(&argc, argv);
	if (!map_args) {
		wrpc_vuart_help(argv[0]);
		return 1;
	}

	/* Parse specific args */
	while ((c = getopt (argc, argv, "c:kh")) != -1) {
		switch (c) {
		case 'c':
			/* Enable command mode */
			cmd = optarg;
			break;
		case 'k':
			keep_term = 1;
			break;
		case 'h':
			wrpc_vuart_help(argv[0]);
			return 0;
		case '?':
			break;
		}
	}

	vuart = dev_map(map_args, getpagesize() );
	if (!vuart) {
		fprintf(stderr, "%s: vuart_open() failed: %s\n", argv[0],
			strerror(errno));
		return 1;
	}

	if (cmd)
		wrpc_vuart_command(vuart, cmd);
	else
		wrpc_vuart_term(vuart, keep_term);

	dev_unmap(vuart);

	return 0;
}
