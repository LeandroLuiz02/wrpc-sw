/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2019 CERN (home.cern)
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>
#include <termios.h>

#ifdef SUPPORT_CERN_VMEBRIDGE
#include <libvmebus.h>
#endif

#include "hw/wrc_cpu_csr.h"
#include "hw/wrc_syscon_regs.h"
#include "hw/wb_uart.h"

#define OFFSET_SYSCON		0x400
#define OFFSET_UART		0x500
#define OFFSET_CPU_CSR		0xb00

#define VUART_EOL 13
#define VUART_CMD_USLEEP 1000000
#define VUART_CMD_PROMPT "wrc#"

static const char *progname;

struct tool_base {
	const char *name;
        const char *short_help;
	int (*run)(int argc, char *argv[]);
	void (*help)(void);
};

struct board {
	const char *name;
	int (*init)(struct board *board, int *argc, char *argv[]);
	int (*fini)(struct board *board);
	void (*help)(void);
	uint32_t (*readl)(struct board *board, unsigned off);
	void (*writel)(struct board *board, unsigned off, uint32_t v);
};

static struct board *board;

static const struct tool_base *tools[];

static int verbose;
static int flag_check;

static void remove_arg1(int *argc, char *argv[])
{
	for (unsigned i = 2; i < *argc; i++)
		argv[i - 1] = argv[i];
	(*argc)--;
}

/* Any board which can be mapped into memory.  */
struct board_mem {
	struct board parent;
	volatile void *base;
	int is_be;
	void *map_addr;
	unsigned map_length;
};

struct board_pci {
	struct board_mem parent;
	const char *resource_file;
	uint64_t offset;
};

struct pci_slot {
	unsigned domain;
	unsigned bus;
	unsigned slot;
	unsigned func;
};

static int parse_pci_slot(struct pci_slot *res, const char *s)
{
	char *e;

	res->domain = strtoul (s, &e, 16);
	if (*e != ':') {
		fprintf (stderr, "missing pci device id in '%s'\n", s);
		return -1;
	}
	res->bus = strtoul (e + 1, &e, 16);
	if (*e == ':')
		res->slot = strtoul (e + 1, &e, 16);
	else {
		res->slot = res->bus;
		res->bus = res->domain;
		res->domain = 0;
	}
	if (*e == '.')
		res->func = strtoul (e + 1, &e, 16);
	else
		res->func = 0;
	if (*e != 0) {
		fprintf (stderr, "incorrect pci slot format in '%s'\n", s);
		return -1;
	}
	return 0;
}

static int board_pci_common_open(struct board_pci *board)
{
	int fd;
	unsigned pg = getpagesize();
	unsigned map_len = pg;
	unsigned pa_offset;

	fd = open(board->resource_file, O_RDWR | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "cannot open resource file '%s': %s\n",
			board->resource_file, strerror(errno));
		return -1;
	}

	/* offset is page aligned */
	pa_offset = board->offset & ~(getpagesize() - 1);
	board->parent.map_addr = mmap(NULL, map_len,
			   PROT_READ | PROT_WRITE,
			   MAP_SHARED, fd, pa_offset);
	if (board->parent.map_addr == MAP_FAILED) {
		fprintf(stderr, "cannot map resource file '%s': %s\n",
			board->resource_file, strerror(errno));
		close(fd);
		return -1;
	}
	close(fd);

	board->parent.map_length = pg;
	board->parent.base =
		board->parent.map_addr + (board->offset - pa_offset);

	board->parent.is_be = 0; /* default set to little endian */

	return 0;
}

/* Parse PCI board identifier (either resource file or slot).  */

static int parse_pci_board(struct board_pci *board, int *argc, char *argv[])
{
	/* Args: -f file, -o offset */
	if (*argc > 2 && !strcmp (argv[1], "-f")) {
		remove_arg1(argc, argv);
		board->resource_file = argv[1];
		remove_arg1(argc, argv);
	}
	else if (*argc > 2 && !strcmp(argv[1], "-s")) {
		static char pci_file[64];
		struct pci_slot slot;
		remove_arg1(argc, argv);
		if (parse_pci_slot(&slot, argv[1]) < 0)
			return -1;
		remove_arg1(argc, argv);
		snprintf (pci_file, sizeof(pci_file),
			  "/sys/bus/pci/devices/%04x:%02x:%02x.%x/resource0",
			  slot.domain, slot.bus, slot.slot, slot.func);
		board->resource_file = pci_file;
	}
	else {
		fprintf(stderr, "missing '-f resource-file' or '-s [dom:]bus:slot[.fn]' for pci\n");
		return -1;
	}

        return 0;
}

static int board_pci_init(struct board *board_base,
			  int *argc, char *argv[])
{
	struct board_pci *board = (struct board_pci *)board_base;

        if (parse_pci_board(board, argc, argv) < 0)
                return -1;

	if (*argc > 2 && !strcmp (argv[1], "-o")) {
		char *e;
		remove_arg1(argc, argv);
		board->offset = strtoul(argv[1], &e, 0);
		if (*e != 0) {
			fprintf (stderr, "bad offset '%s'\n", argv[1]);
			return -1;
		}
		remove_arg1(argc, argv);
	}

	if (board_pci_common_open(board) < 0)
		return -1;
	return 0;
}

static int board_pci_fini(struct board *base_board)
{
	struct board_pci *board = (struct board_pci *)base_board;
	munmap(board->parent.map_addr, board->parent.map_length);

	return 0;
}

static void board_pci_help(void)
{
        printf("Generic PCI board\n");
        printf(" -f resource-file\n");
        printf(" -s [domain:]bus:slot[.func]\n");
        printf(" -o offset\n");
        printf("One of -f or -s is required to identify the board\n");
}

static uint32_t mem_readl(struct board *base_board, unsigned reg)
{
	struct board_mem *board = (struct board_mem *)base_board;

	uint32_t r = *(volatile uint32_t *)(board->base + reg);

	if (board->is_be)
		return ntohl(r);
	else
		return r;
}

static void mem_writel(struct board *base_board, unsigned reg, uint32_t value)
{
	struct board_mem *board = (struct board_mem *)base_board;

	if (board->is_be)
		value = htonl(value);

	*(volatile uint32_t *)(board->base + reg ) = value;
}

static struct board_pci board_pci =
{
	{
		{
			"pci",
			board_pci_init,
			board_pci_fini,
			board_pci_help,
			mem_readl,
			mem_writel
		},
		NULL,
		0,
		NULL,
		0
	},
	NULL,
	0
};

static int board_spec_init(struct board *board_base,
			  int *argc, char *argv[])
{
	struct board_pci *board = (struct board_pci *)board_base;

        if (parse_pci_board(board, argc, argv) < 0)
                return -1;

	if (board_pci_common_open(board) < 0)
		return -1;
	return 0;
}

static void board_spec_help(void)
{
        printf("SPEC board (using the convention)\n");
        printf(" -f resource-file \n");
        printf(" -s [domain:]bus:slot[.func]\n");
        printf("One of -f or -s is required to identify the board\n");
        printf("(wrpc is at offset 0x1000)\n");
}

static struct board_pci board_spec =
{
	{
		{
			"spec",
			board_spec_init,
			board_pci_fini,
			board_spec_help,
			mem_readl,
			mem_writel
		},
		NULL,
		0,
		NULL,
		0
	},
	NULL,
	0x1000
};

#ifdef SUPPORT_CERN_VMEBRIDGE
struct board_cernvme {
        struct board_mem parent;
	uint32_t data_width; /**< default register size in bytes */
	uint32_t am; /**< VME address modifier to use */
	uint64_t addr; /**< physical base address */
        uint32_t offset;
        struct vme_mapping map;
};

static int cernvme_map(struct board_cernvme *board,
                       unsigned am, unsigned dw,
                       unsigned vme_addr, unsigned offset)
{
        unsigned pg = getpagesize();

        memset(&board->map, 0, sizeof(struct vme_mapping));
        board->map.am = am;
        board->map.data_width = dw;
        board->map.sizel = pg;
        board->map.vme_addrl = vme_addr | (offset & ~(pg - 1));

        board->parent.map_addr = vme_map(&board->map, 1);
        if (!board->parent.map_addr) {
                fprintf(stderr, "cannot map vme: %s\n", strerror(errno));
                return -1;
        }
        board->parent.map_length = pg;
	board->parent.base = board->parent.map_addr + (offset & (pg - 1));

        board->parent.is_be = 1;

        return 0;
}

static int board_cernvme_init(struct board *board_base,
                              int *argc, char *argv[])
{
	struct board_cernvme *board = (struct board_cernvme *)board_base;

        unsigned vme_addr = ~0;
        unsigned data_width = 32;
        unsigned am = 0x39;
        unsigned offset = 0;

        while (*argc > 2) {
                if (argv[1][0] != '-')
                        break;

                if (!strcmp(argv[1], "-a") || !strcmp(argv[1], "--address")) {
                        char *e;
                        remove_arg1(argc, argv);
                        vme_addr = strtoul(argv[1], &e, 0);
                        if (*e != 0) {
                                fprintf(stderr, "invalid address '%s'\n", argv[1]);
                                return -1;
                        }
                        remove_arg1(argc, argv);
                }
                else if (!strcmp(argv[1], "-s") || !strcmp(argv[1], "--slot")) {
                        char *e;
                        unsigned slot;

                        remove_arg1(argc, argv);
                        slot = strtoul(argv[1], &e, 0);
                        if (*e != 0) {
                                fprintf(stderr, "invalid slot '%s'\n", argv[1]);
                                return -1;
                        }
                        vme_addr = slot << 19;
                        remove_arg1(argc, argv);
                }
                else if (!strcmp(argv[1], "-w")
                         || !strcmp(argv[1], "--data-width")) {
                        char *e;

                        remove_arg1(argc, argv);
                        data_width = strtoul(argv[1], &e, 0);
                        if (*e != 0) {
                                fprintf(stderr, "invalid data-width '%s'\n", argv[1]);
                                return -1;
                        }
                        if (!(data_width == 8
                              || data_width == 16
                              || data_width == 32)) {
                                fprintf(stderr, "invalid data-width %u\n",
                                        data_width);
                                return -1;
                        }
                        remove_arg1(argc, argv);
                }
                else if (!strcmp(argv[1], "-m")
                         || !strcmp(argv[1], "--am")) {
                        char *e;

                        remove_arg1(argc, argv);
                        am = strtoul(argv[1], &e, 0);
                        if (*e != 0) {
                                fprintf(stderr, "invalid address-modifier '%s'\n", argv[1]);
                                return -1;
                        }
                        remove_arg1(argc, argv);
                }
                else if (!strcmp (argv[1], "-o")
                         || !strcmp(argv[1], "--offset")) {
                        char *e;
                        remove_arg1(argc, argv);
                        offset = strtoul(argv[1], &e, 0);
                        if (*e != 0) {
                                fprintf (stderr, "bad offset '%s'\n", argv[1]);
                                return -1;
                        }
                        remove_arg1(argc, argv);
                }
                else
                        break;

        }

        if (vme_addr == ~0) {
                fprintf (stderr,
                         "vme address (-a) or vme slot (-s) required\n");
                return -1;
        }

        return cernvme_map (board, am, data_width, vme_addr, offset);
}

static int board_cernvme_fini(struct board *base_board)
{
	struct board_cernvme *board = (struct board_cernvme *)base_board;
        vme_unmap(&board->map, 1);

	return 0;
}

static void board_cernvme_help(void)
{
        printf("VME board (using CERN-vme bridge)\n");
        printf(" -a, --address ADDR   board address\n");
        printf(" -s, --slot ADDR      board slot (512KB steps)\n");
        printf(" -w, --data-width WD  data width\n");
        printf(" -m, --am AM          address modified\n");
        printf(" -o, --offset OFF     offset\n");
        printf("One of -a or -s is required\n");
}

static struct board_cernvme board_cernvme =
{
	{
		{
			"vme",
			board_cernvme_init,
			board_cernvme_fini,
			board_cernvme_help,
			mem_readl,
			mem_writel
		},
		NULL,
		0,
		NULL,
		0
	},
};

static uint32_t wr2rf_readl(struct board *base_board, unsigned reg)
{
	struct board_mem *board = (struct board_mem *)base_board;
        volatile uint16_t *addr = (volatile uint16_t *)(board->base + reg);
        uint32_t l, h, res;

        /* A 16b VME bus with special circuitery to get an atomic 32b value */
	l = addr[0];
	h = addr[1];
	res = (l << 16) | h;
	return ntohl(res);
}

static void wr2rf_writel(struct board *base_board, unsigned reg, uint32_t val)
{
	struct board_mem *board = (struct board_mem *)base_board;
        volatile uint16_t *addr = (volatile uint16_t *)(board->base + reg);

        val = htonl(val);

        addr[1] = val & 0xffff;
	addr[0] = val >> 16;
}

static int board_wr2rf_init(struct board *board_base,
                            int *argc, char *argv[])
{
	struct board_cernvme *board = (struct board_cernvme *)board_base;

        unsigned vme_addr;

        if (*argc > 2
            && (!strcmp(argv[1], "-s") || !strcmp(argv[1], "--slot"))) {
                char *e;
                unsigned slot;

                remove_arg1(argc, argv);
                slot = strtoul(argv[1], &e, 0);
                if (*e != 0) {
                        fprintf(stderr, "invalid slot '%s'\n", argv[1]);
                        return -1;
                }
                vme_addr = slot << 19;
                remove_arg1(argc, argv);
        }
        else {
                fprintf(stderr, "missing slot number for wr2rf\n");
                return -1;
        }

        return cernvme_map(board, 0x39, 32, vme_addr, 0x2000);
}

static void board_wr2rf_help(void)
{
        printf("wr2rf board (using CERN-vme bridge)\n");
        printf(" -s, --slot ADDR      board slot (512KB steps)\n");
}

static struct board_cernvme board_wr2rf =
{
	{
		{
			"wr2rf",
			board_wr2rf_init,
			board_cernvme_fini,
			board_wr2rf_help,
			wr2rf_readl,
			wr2rf_writel
		},
		NULL,
		0,
		NULL,
		0
	},
};
#endif

static struct board *boards[] = {
        &board_pci.parent.parent,
        &board_spec.parent.parent,
#ifdef SUPPORT_CERN_VMEBRIDGE
        &board_cernvme.parent.parent,
        &board_wr2rf.parent.parent,
#endif
        NULL
};

static struct board *find_board(const char *name)
{
        struct board *b;

        for (unsigned i = 0; (b = boards[i]); i++)
                if (!strcmp(b->name, name))
                        return b;
        fprintf(stderr,
                "board '%s' is unknown, try %s board\n",
                name, progname);
        return NULL;
}

static int board_open(int *argc, char *argv[])
{
	if (*argc > 2 && !strcmp(argv[1], "-b")) {
                struct board *b;
		/* Board selection */
		remove_arg1(argc, argv);
                b = find_board(argv[1]);
                if (b == NULL)
                        return -1;
                board = b;
		remove_arg1(argc, argv);
	}
	else
		board = &board_pci.parent.parent;

	return board->init(board, argc, argv);
}

static void wrc_cpu_reset(struct board *board, unsigned int rst)
{
	board->writel (board, OFFSET_CPU_CSR + WRC_CPU_CSR_REG_RESET, rst);
}

static void wrc_write_uaddr(struct board *board, unsigned int addr)
{
	board->writel(board, OFFSET_CPU_CSR + WRC_CPU_CSR_REG_UADDR, addr >> 2);
}

static void wrc_write_udata(struct board *board, uint32_t data)
{
	board->writel(board, OFFSET_CPU_CSR + WRC_CPU_CSR_REG_UDATA, data);
}

static uint32_t wrc_read_udata(struct board *board)
{
	return board->readl(board, OFFSET_CPU_CSR + WRC_CPU_CSR_REG_UDATA);
}

static int wrc_write_buf(struct board *board,
                         const unsigned char *buf,
                         unsigned len,
                         unsigned addr)
{
	if ((len & 0x03) != 0 || (addr & 0x03) != 0)
		abort();

	while (len > 0) {
		uint32_t v;

		wrc_write_uaddr(board, addr);

		/* Use BE.  */
		v = (buf[3] << 0)
			| (buf[2] << 8)
			| (buf[1] << 16)
			| (buf[0] << 24);
		wrc_write_udata(board, v);

		if (verbose)
			printf ("Write %08x at %08x\n", v, addr);

                if (flag_check) {
                        uint32_t r;
                        wrc_write_uaddr(board, addr);
                        r = wrc_read_udata(board);
                        if (r != v) {
                                printf ("Error at %08x: "
                                        "read %08x instead of %08x\n",
                                        addr, r, v);
                                return -1;
                        }
                }

		len -= 4;
		addr += 4;
		buf += 4;
	}

        return 0;
}

static int wrc_load_firmware(struct board *board, const char *filename)
{
	int fd;
	unsigned char hdr[4];
	unsigned char buf[1024];
	ssize_t res;
	unsigned addr;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "cannot open %s\n", filename);
		return -1;
	}

	res = read(fd, hdr, sizeof(hdr));
	if (res != sizeof(hdr)) {
		fprintf(stderr, "cannot read %s\n", filename);
		goto err_close;
	}

	if (hdr[0] == 0x7f
	    && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F') {
		fprintf(stderr, "ELF file %s not yet handled\n", filename);
		goto err_close;
	}

	addr = 0;
	if (wrc_write_buf(board, hdr, sizeof(hdr), addr) != 0)
                goto err_close;
	addr += sizeof (hdr);

	while (1) {
		res = read(fd, buf, sizeof(buf));
		if (res <= 0)
			break;
		if (wrc_write_buf(board, buf, res, addr) != 0)
                        goto err_close;
		addr += res;
	}
	printf ("%u KB written\n", addr / 1024);
	close(fd);
	return 0;

err_close:
	close(fd);
	return -1;
}

static int wrc_save_firmware(struct board *board, const char *filename)
{
	int fd;
	unsigned length = 0x20000;
	unsigned char buf[1024];
	ssize_t res;
	unsigned addr;

	fd = open(filename, O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "cannot open %s\n", filename);
		return -1;
	}

	for (addr = 0; addr < length;) {
		unsigned l = length - addr;
		unsigned off;
		if (l > sizeof (buf))
			l = sizeof (buf);

		for (off = 0; off < l; off += 4) {
			unsigned int v;
			wrc_write_uaddr(board, addr);
			v = wrc_read_udata(board);
			/* Use BE */
			buf[off + 0] = v >> 24;
			buf[off + 1] = v >> 16;
			buf[off + 2] = v >> 8;
			buf[off + 3] = v >> 0;

			addr += 4;
		}
		res = write(fd, buf, l);
		if (res != l) {
			fprintf(stderr, "write failure\n");
			close(fd);
			return -1;
		}
	}

	printf ("%u KB written to %s\n", length / 1024, filename);
	close(fd);
	return 0;
}

static void wrc_dump(struct board *board, unsigned addr, unsigned len)
{
	unsigned off;

	off = 0;
	for (off = 0; off < len; off += 4) {
		uint32_t v;

		if ((off & 0x0f) == 0)
			printf ("%08x:", addr + off);

		wrc_write_uaddr(board, addr + off);

		v = wrc_read_udata(board);
		printf (" %08x", v);
		if ((off & 0x0f) == 0xc)
			printf ("\n");
	}
	if ((off & 0x0f) != 0xc)
		printf ("\n");
}

/* Extract the basename of :param name: */
static const char *get_basename(const char *name)
{
	const char *res = name;

	for (res = name; *name; name++)
		if (*name == '/')
			res = name + 1;
	if (*res)
		return res;
	else
		return name;
}

static int do_help(int argc, char *argv[])
{
	printf ("usage: %s [command] [OPTIONS...]\n", progname);
	printf ("command is one of:\n");
	for (unsigned i = 0; tools[i]; i++)
		printf(" %-10s - %s\n", tools[i]->name, tools[i]->short_help);
	return 0;
}

static int do_version(int argc, char *argv[])
{
	printf ("version 1.0\n");
	return 0;
}

static void help_load(void)
{
        printf("usage: %s load BOARD-OPTIONS FILENAME\n", progname);
        printf("Load FILENAME into WR cpu and restart the code\n");
}

static int do_load(int argc, char *argv[])
{
	int c;
        int status;
	const char *filename;
	enum { CMD_LOAD, CMD_DUMP, CMD_SAVE } cmd;

        if (board_open(&argc, argv) < 0)
		return 1;

        status = 0;

	cmd = CMD_LOAD;
	while ((c = getopt(argc, argv, "vdsc")) != -1) {
		switch (c) {
		case 'd':
			cmd = CMD_DUMP;
			break;
		case 's':
			cmd = CMD_SAVE;
			break;
		case 'v':
			verbose++;
			break;
                case 'c':
                        flag_check++;
                        break;
		case '?':
                        printf("%s: unknown option, try -h\n", argv[0]);
                        exit(1);
		}
	}

	if (((cmd == CMD_LOAD || cmd == CMD_SAVE) && (optind != argc - 1))
	    || (cmd == CMD_DUMP && optind != argc)) {
		help_load();
		return 2;
	}
	filename = argv[optind];

	/* Reset */
	wrc_cpu_reset(board, 1 << 0);

	switch (cmd) {
	case CMD_LOAD:
		/* Load */
		if (wrc_load_firmware (board, filename) < 0)
                        status = 1;
		break;
	case CMD_SAVE:
		/* Save */
		wrc_save_firmware (board, filename);
		break;
	case CMD_DUMP:
		/* TODO: specify offset and length */
		wrc_dump(board, 0, 0x200);
		break;
	}

	/* Start */
	wrc_cpu_reset(board, 0);

        board->fini(board);

	return status;
}

static void help_vuart(void)
{
	fprintf(stderr, "%s BOARD-OPTIONS [-k]\n", progname);
	fprintf(stderr, " -k keep terminal\n");
}

#if 0
/**
 * It receives a single byte
 * @param[in] vuart token from dev_map()
 *
 *
 */

static uint32_t io_readl(volatile void *addr)
{
#ifdef WR2RF
	/* A 16b VME bus with special circuitery to get an atomic 32b value */
	uint32_t l, h, res;
	l = *(volatile uint16_t *)(addr + 0);
	h = *(volatile uint16_t *)(addr + 2);
	res = (l << 16) | h;
	return res;
#else
	return *(volatile uint32_t *)addr;
#endif
}

static void io_writel(volatile void *addr, uint32_t val)
{
#ifdef WR2RF
	*(volatile uint16_t *)(addr + 2) = val & 0xffff;
	*(volatile uint16_t *)(addr + 0) = val >> 16;
#else
	*(volatile uint32_t *)addr = val;
#endif
}
#endif

static uint32_t vuart_readl(struct board *board, int reg)
{
	return board->readl(board, reg | OFFSET_UART);
}


static void vuart_writel(struct board *board, uint32_t value, int reg)
{
	board->writel(board, reg | OFFSET_UART, value);
}

static int wr_vuart_rx(struct board *board)
{
	int rdr = vuart_readl(board, UART_REG_HOST_RDR );
	return (rdr & UART_HOST_RDR_RDY) ? UART_HOST_RDR_DATA_R(rdr) : -1;
}

/**
 * It transmits a single byte
 * @param[in] vuart token from dev_map()
 */
static void wr_vuart_tx(struct board *board, char data)
{
	int sr = vuart_readl(board, UART_REG_SR );

	while(sr & UART_SR_RX_RDY)
		 sr = vuart_readl(board, UART_REG_SR );

	vuart_writel(board, UART_HOST_TDR_DATA_W(data), UART_REG_HOST_TDR );
}

/**
 * It reads a number of bytes and it stores them in a given buffer
 * @param[in] vuart token from dev_map()
 * @param[out] buf destination for read bytes
 * @param[in] size numeber of bytes to read
 *
 * @return the number of read bytes
 */
static size_t wr_vuart_read(struct board *board, char *buf, size_t size)
{
	size_t s = size, n_rx = 0;
	int8_t c;

	while(s--) {
		c =  wr_vuart_rx(board);
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
static void wr_vuart_flush(struct board *board)
{
	char rx;

	while(wr_vuart_read(board,&rx,1) == 1) {}
}

/**
 * It writes a number of bytes from a given buffer
 * @param[in] vuart token from dev_map()
 * @param[in] buf buffer to write
 * @param[in] size numeber of bytes to write
 */
static void wr_vuart_write(struct board *board, char *buf, size_t size)
{
	while(size--)
		wr_vuart_tx(board, *buf++);
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


static void wrpc_vuart_term(struct board *board, int keep_term)
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

			wr_vuart_tx(board, tx);
			break;
		}

		/* Print all the incoming characters */
		while((rx = wr_vuart_rx(board)) > 0) {
			putchar(rx);
		}
		fflush(stdout);
	}

	if(!keep_term)
		wrpc_vuart_restore_tty(&oldkey);
}

static void wrpc_vuart_command(struct board *board, char *command)
{
	//above is place for old and new port settings for keyboard teletype
	int cmd_len = 0;
	char *prompt = VUART_CMD_PROMPT;
	int i_prompt = 0;
	int i;
	int rx;

	/* Flush Vuart before sending command */
	wr_vuart_flush(board);
	/* Send command */
	cmd_len = strlen(command);
	wr_vuart_write(board, command, cmd_len);
	/* Flush command echo */
	wr_vuart_flush(board);
	/* Send end character */
	wr_vuart_tx(board, VUART_EOL);
	/* Wait for a while before reading command results */
	usleep(VUART_CMD_USLEEP);
	/* Discard characters until end of line control one */
	while((rx = wr_vuart_rx(board)) > 0) {
		if(rx == VUART_EOL)
			break;
	}

	while(1) {
		/* Print all the incoming characters */
		rx = wr_vuart_rx(board);
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


static int do_vuart(int argc, char *argv[])
{
	char c;
	int keep_term = 0;
	char *cmd = NULL;

	if (board_open(&argc, argv) < 0)
		return 1;

	/* Parse specific args */
	while ((c = getopt (argc, argv, "c:k")) != -1) {
		switch (c) {
		case 'c':
			/* Enable command mode */
			cmd = optarg;
			break;
		case 'k':
			keep_term = 1;
			break;
		case '?':
			break;
		}
	}

	if (cmd)
		wrpc_vuart_command(board, cmd);
	else
		wrpc_vuart_term(board, keep_term);

	board->fini(board);

	return 0;
}

static void help_info(void)
{
	printf("usage: %s info\n", progname);
	printf("display board info\n"
	       "also useful to check mapping\n");
}

static int do_info(int argc, char *argv[])
{
	unsigned hwfr;
	unsigned hwir;

	if (board_open(&argc, argv) < 0)
		return 1;

	hwfr = board->readl(board, OFFSET_SYSCON + SYSC_REG_HWFR);
	printf ("hwfr=%08x:  "
		"memsize: %ukB,  storage: %u, storage sector size: %ukB\n",
		hwfr,
		SYSC_HWFR_MEMSIZE_R(hwfr) * 16,
		SYSC_HWFR_STORAGE_TYPE_R(hwfr),
		SYSC_HWFR_STORAGE_SEC_R(hwfr));

	hwir = board->readl(board, OFFSET_SYSCON + SYSC_REG_HWIR);
	printf ("hwir=%08x:  ", hwir);
        for (unsigned i = 0; i < 4; i++) {
                unsigned c = (hwir >> (24 - i * 8)) & 0xff;
                putchar (c >= 32 && c < 127 ? c : '.');
        }
        printf ("\n");

	board->fini(board);

	return 0;
}

static int do_board(int argc, char *argv[])
{
        struct board *b;

        if (argc > 1) {
                b = find_board(argv[1]);
                if (b == NULL)
                        return 1;
                b->help();
        }
        else {
                printf ("List of boards:\n");
                for (unsigned i = 0; (b = boards[i]); i++)
                        printf(" %s\n", b->name);
        }
        return 0;
}

static const struct tool_base tool_help = {
        "help",
        "display list of commands (this help), or help for a command",
        do_help,
        NULL
};

static const struct tool_base tool_board = {
        "board",
        "display list of supported boards, or help for a board",
        do_board,
        NULL
};

static const struct tool_base tool_version = {
        "version",
        "display tool version",
        do_version,
        NULL
};

static const struct tool_base tool_load = {
        "load",
        "load wrpc firmware and restart",
        do_load,
        help_load
};

static const struct tool_base tool_vuart = {
        "vuart",
        "virtual uart, connect to wrpc cli",
        do_vuart,
        help_vuart
};

static const struct tool_base tool_info = {
        "info",
        "display wrpc info and check board",
        do_info,
        help_info
};

static const struct tool_base *tools[] = {
	&tool_help,
	&tool_version,
        &tool_board,
	&tool_load,
	&tool_vuart,
	&tool_info,
	NULL
};

int main(int argc, char *argv[])
{
	const char *progbase;
	const char *toolname;
	const struct tool_base *tool;

	/* Extract tool from argv[0].  */
	progname = argv[0];
	progbase = get_basename(progname);
	if (argc > 1 && argv[1][0] != '-') {
		/* Extract command. */
		toolname = argv[1];

		remove_arg1(&argc, argv);
	}
	else if (argc > 1 && strcmp(argv[1], "--version") == 0) {
		do_version(0, NULL);
		return 0;
	}
	else if (memcmp(progbase, "wrpc-", 5) == 0) {
		toolname = progbase + 5;
	}
	else {
		fprintf (stderr, "no tool name, try %s help\n", progname);
		return 1;
	}

	/* Find tool.  */
	for (unsigned i = 0; (tool = tools[i]); i++) {
		if (strcmp (tool->name, toolname) == 0)
			break;
	}
	if (tool == NULL) {
		fprintf(stderr, "tool '%s' is unknown, try %s help\n",
			toolname, progname);
		return 1;
	}

	if (argc > 1)
		if (strcmp (argv[1], "-h") == 0
		    || strcmp (argv[1], "--help") == 0) {
			tool->help();
			return 0;
		}

	return tool->run(argc, argv);
}
