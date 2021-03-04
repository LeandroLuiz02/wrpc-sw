/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright 2020-2021 CERN
 * Author: Juan David Gonzalez Cobas
 *
 * The comm via USB serial is implemented here
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <math.h>


#include "psnmp-proto.h"
#include "board-state.h"
#include "private.h"
#include "libertm.h"
#include "ertm14-uart-link.h"
#include "ertm15_rf_distr.h"

#define BUG(expr)	\
	do {	\
		if (!(expr)) {	\
			fprintf(stderr, "assertion" #expr "failed!\n");	\
			exit(1);	\
		}	\
	} while (0)

void dds_board_to_host(struct ertm14_dds_state *dds, struct ertm14_dds_state *host)
{
	int i;

	host->ftw 		= ntohl(dds->ftw);
	host->amp_power 	= ntohl(dds->amp_power);
	host->ampl_factor 	= ntohl(dds->ampl_factor);
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++) {
		host->out_power[i] = ntohl(dds->out_power[i]);
		host->out_state[i] = dds->out_state[i];
	}
}

void host_to_dds_board(struct ertm14_dds_state *host, struct ertm14_dds_state *dds)
{
	int i;

	dds->ftw                 = htonl(host->ftw);
	dds->amp_power           = htonl(host->amp_power);
	dds->ampl_factor         = htonl(host->ampl_factor);
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++) {
		dds->out_power[i] = htonl(host->out_power[i]);
		dds->out_state[i] = host->out_state[i];
	}
}

static char *state_literal[] = {
	[ERTM_RF_OUT_ON] = "on",
	[ERTM_RF_OUT_OFF] = "off",
	[ERTM_RF_OUT_MONITOR] = "monitor",
};
/* FIXME: all these are repeated, same as above */
static double ampl_factor_to_float(uint8_t ampl_factor)
{
	return ampl_factor/256.0;
}

static uint8_t float_to_ampl_factor(double level)
{
	return (uint8_t)floor(level * 256);
}

void display_dds_state(struct ertm14_dds_state *dds)
{
	int i;

	printf("ftw: %08x\n", dds->ftw);
	printf("level adjust: %0.4f (%d/256)\n", ampl_factor_to_float(dds->ampl_factor), dds->ampl_factor);
	printf("pll_out_power: %3.1f dBm (%08x mdBm)\n",  dds->amp_power/1000.0, dds->amp_power);
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++)
		printf("ch: %02d pow: %08x mdBm (%8.3f dBm)  %d %-8s\n",
		    i, dds->out_power[i], dds->out_power[i]/1000.0,
		    dds->out_state[i], state_literal[dds->out_state[i]]);
}

void board_to_host(struct ertm14_board_state *board, struct ertm14_board_state *host)
{
	int i;

	dds_board_to_host(&board->ref, &host->ref);
	dds_board_to_host(&board->lo, &host->lo);
	host->clka_enable_mask = ntohl(board->clka_enable_mask);
	host->clkb_enable_mask = ntohl(board->clkb_enable_mask);
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		/* FIXME: not enum */
		host->clka_freq_hz[i] = ntohl(board->clka_freq_hz[i]);
		host->clkb_freq_hz[i] = ntohl(board->clkb_freq_hz[i]);
	}
}

void state_to_board(struct ertm_state *state, struct ertm14_board_state *board)
{
	int i;
	struct ertm14_board_state *bs = &state->board_state;

	host_to_dds_board(&bs->ref, &board->ref);
	host_to_dds_board(&bs->lo, &board->lo);
	board->clka_enable_mask = htonl(bs->clka_enable_mask);
	board->clkb_enable_mask = htonl(bs->clkb_enable_mask);
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		/* FIXME: not enum */
		board->clka_freq_hz[i] = htonl(bs->clka_freq_hz[i]);
		board->clkb_freq_hz[i] = htonl(bs->clkb_freq_hz[i]);
	}
}

void display_ertm_clkab(struct ertm14_board_state *bs)
{
	int i;
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		char *aonoff = (bs->clka_enable_mask & (1<<i)) ? "on " : "off";
		char *bonoff = (bs->clkb_enable_mask & (1<<i)) ? "on " : "off";
		printf("CLKA%02d: %3s  %10dHz\t\t", i, aonoff, bs->clka_freq_hz[i]);
		printf("CLKB%02d: %3s  %10dHz\n", i, bonoff, bs->clkb_freq_hz[i]);
	}
}

void display_ertm_state(struct ertm_state *st)
{
	struct ertm14_board_state *bs = &st->board_state;

	printf("CLKAB: --------------------------------------------------\n");
	display_ertm_clkab(bs);
	printf("LO: --------------------------------------------------\n");
	display_dds_state(&bs->lo);
	printf("REF: --------------------------------------------------\n");
	display_dds_state(&bs->ref);
}

void display_hex(uint8_t *buf, size_t len)
{
	int i;

	for (i = 0; i < len; i++)
		fprintf(stderr, "%02x%c", buf[i],
			((i+1) % 16 == 0) ? '\n' : ' ');
	if ((i+1) % 16 != 0)
		fprintf(stderr, "\n");
}

int commit_board_config(struct ertm_status *st, struct ertm_state *mask)
{
	int res, stat;

	struct uart_link *link = &st->link;
	struct uart_packet tx, *tx_pkt = &tx;
	struct uart_packet rx, *rx_pkt = &rx;

	struct ertm14_board_state *cfg;
	struct ertm14_protocol_op *op = get_proto_op(ertm14_commit_board_config);

	memset(tx_pkt, 0, sizeof(*tx_pkt));
	tx_pkt->ptype = ERTM14_UART_PTYPE_SNMP_REQ;
	tx_pkt->length = op->offset1 + op->length1;
	tx_pkt->payload[0] = op->opcode;
	BUG(op->length1 == sizeof(*cfg));
	BUG(op->opcode == ertm14_commit_board_config);
	cfg = (struct ertm14_board_state *)&tx_pkt->payload[op->offset1];
	state_to_board(mask, cfg);
	cfg->valid = 1;

	res = uart_link_send(link, tx_pkt);
	if (res < 0) {
		printf("error %d in uart_link_send\n", res);
		return res;
		return ERTM_UART_LINK_SEND_ERR;
	}
	memset(rx_pkt, 0, sizeof(*rx_pkt));
	stat = uart_link_recv(link, &rx_pkt, 2000);
	if (stat < 0) {
		fprintf(stderr, "error (stat %d) in uart_link_recv\n", stat);
		return ERTM_UART_LINK_RECV_ERR;
	}
	fprintf(stderr,"recvd %d bytes: \n", rx_pkt->length);

	return 0;
}

int set_board_config(struct ertm_status *st, struct ertm_state *config)
{
	int res, stat;

	struct uart_link *link = &st->link;
	struct uart_packet tx, *tx_pkt = &tx;
	struct uart_packet rx, *rx_pkt = &rx;

	struct ertm14_board_state *cfg;
	struct ertm14_protocol_op *op = get_proto_op(ertm14_set_board_config);

	memset(tx_pkt, 0, sizeof(*tx_pkt));
	tx_pkt->ptype = ERTM14_UART_PTYPE_SNMP_REQ;
	tx_pkt->length = op->offset1 + op->length1;
	tx_pkt->payload[0] = op->opcode;
	BUG(op->length1 == sizeof(*cfg));
	BUG(op->opcode == ertm14_set_board_config);
	cfg = (struct ertm14_board_state *)&tx_pkt->payload[op->offset1];
	state_to_board(config, cfg);
	cfg->valid = 1;

	res = uart_link_send(link, tx_pkt);
	if (res < 0) {
		printf("error %d in uart_link_send\n", res);
		return res;
		return ERTM_UART_LINK_SEND_ERR;
	}
	memset(rx_pkt, 0, sizeof(*rx_pkt));
	usleep(200000);
	stat = uart_link_recv(link, &rx_pkt, 2000);
	if (stat < 0) {
		fprintf(stderr, "error (stat %d) in uart_link_recv\n", stat);
		return ERTM_UART_LINK_RECV_ERR;
	}
	fprintf(stderr,"recvd %d bytes: \n", rx_pkt->length);

	return 0;
}

int get_board_config_sim(struct ertm_status *st, int sim)
{
	int res, stat;

	struct uart_link *link = &st->link;
	struct uart_packet pack1, *tx_pkt = &pack1;
	struct uart_packet pack2, *rx_pkt = &pack2;

	struct ertm14_board_state *board;
	struct ertm_state *state = st->state;

	tx_pkt->ptype = ERTM14_UART_PTYPE_SNMP_REQ;
	tx_pkt->length = 1;
	tx_pkt->payload[0] = (sim ? ertm14_get_sim_board_config : ertm14_get_board_config);

	res = uart_link_send(link, tx_pkt);
	if (res < 0) {
		printf("error %d in uart_link_send\n", res);
		return ERTM_UART_LINK_SEND_ERR;
	}
	memset(rx_pkt, 0, sizeof(*rx_pkt));
	stat = uart_link_recv(link, &rx_pkt, 1000);
	if (stat <= 0) {
		fprintf(stderr, "error (stat %d) in uart_link_recv\n", stat);
		return ERTM_UART_LINK_RECV_ERR;
	}
	fprintf(stderr,"recvd %d bytes: \n", rx_pkt->length);

	board = (struct ertm14_board_state *)&rx_pkt->payload[0];
	board_to_host(board, &state->board_state);

	return 0;
}

int get_board_config(struct ertm_status *st)
{
	return get_board_config_sim(st, 0);
}

int get_sim_board_config(struct ertm_status *st)
{
	return get_board_config_sim(st, 1);
}

int get_wr_diags(struct ertm_status *st, struct WRC_DIAGS_WB *diags)
{
	int res, stat;

	struct uart_link *link = &st->link;
	struct uart_packet pack1, *tx_pkt = &pack1;
	struct uart_packet pack2, *rx_pkt = &pack2;
	struct ertm14_protocol_op *op = get_proto_op(ertm14_get_wrc_diags);

	tx_pkt->ptype = ERTM14_UART_PTYPE_SNMP_REQ;
	tx_pkt->length = op->offset1 + op->length1;
	tx_pkt->payload[0] = op->opcode;
	res = uart_link_send(link, tx_pkt);
	if (res < 0) {
		printf("error %d in uart_link_send\n", res);
		return ERTM_UART_LINK_SEND_ERR;
	}
	memset(rx_pkt, 0, sizeof(*rx_pkt));
	stat = uart_link_recv(link, &rx_pkt, 1000);
	if (stat <= 0) {
		fprintf(stderr, "error (stat %d) in uart_link_recv\n", stat);
		return ERTM_UART_LINK_RECV_ERR;
	}
	fprintf(stderr,"recvd %d bytes: \n", rx_pkt->length);

	BUG(op->length2 == sizeof(*diags));
	memcpy(diags, &rx_pkt->payload[op->offset2], op->length2);

	return 0;
}


/* constants of nature for this design */
static char *usb_serial = "/dev/ttyUSB2";
static int serial_speed = 8*115200;

struct ertm_status *ertm_init(char *address)
{
	struct ertm_status *st = malloc(sizeof(*st));
	int err;

	if (st == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	st->state = malloc(sizeof(*st->state));
	if (st == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	err = uart_link_create_linux(&st->link, address, serial_speed);
	if (err != 0) {
		errno = ENODEV;
		return NULL;
	}

	return st;
}

void ertm_exit(struct ertm_status *handle)
{
	if (handle != NULL)
		free(handle->state);
	free(handle);
}

void display_wrc_diags(struct WRC_DIAGS_WB *diags)
{
	char fmt[] = "%-38s: 0x%08x\n";

	printf(fmt, "Version register", diags->VER);
	printf(fmt, "Ctrl", diags->CTRL);
	printf(fmt, "servo status", diags->WDIAG_SSTAT);
	printf(fmt, "Port status", diags->WDIAG_PSTAT);
	printf(fmt, "PTP state", diags->WDIAG_PTPSTAT);
	printf(fmt, "AUX state", diags->WDIAG_ASTAT);
	printf(fmt, "Tx PTP Frame cnts", diags->WDIAG_TXFCNT);
	printf(fmt, "Rx PTP Frame cnts", diags->WDIAG_RXFCNT);
	printf(fmt, "WRPC Diag:local time [msb of s]", diags->WDIAG_SEC_MSB);
	printf(fmt, "local time [lsb of s]", diags->WDIAG_SEC_LSB);
	printf(fmt, "local time [ns]", diags->WDIAG_NS);
	printf(fmt, "Round trip (mu) [msb of ps]", diags->WDIAG_MU_MSB);
	printf(fmt, "Round trip (mu) [lsb of ps]", diags->WDIAG_MU_LSB);
	printf(fmt, "Master-slave delay (dms) [msb of ps]", diags->WDIAG_DMS_MSB);
	printf(fmt, "Master-slave delay (dms) [lsb of ps]", diags->WDIAG_DMS_LSB);
	printf(fmt, "Total link asymmetry [ps]", diags->WDIAG_ASYM);
	printf(fmt, "Clock offset (cko) [ps]", diags->WDIAG_CKO);
	printf(fmt, "Phase setpoint (setp) [ps]", diags->WDIAG_SETP);
	printf(fmt, "Update counter (ucnt)", diags->WDIAG_UCNT);
	printf(fmt, "Board temperature [C degree]", diags->WDIAG_TEMP);
	
}
long long usecofday(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000LL + tv.tv_usec;
}

void test_comm(struct uart_link *link, size_t length)
{
	struct uart_packet snd, rcv, *rcvp;
	int res = 0;
	long long us;

	memset(snd.payload, 0x5a, sizeof(snd.payload));
	snd.length = length;
	snd.ptype = ERTM14_UART_PTYPE_SNMP_REQ;
	res = uart_link_send(link, &snd);
	if (res < 0) {
	    fprintf(stderr, "error %d in uart_link_send\n", res);
	} else {
	}

	us = usecofday();
	res = uart_link_recv(link, &rcvp, 2000);
	us = usecofday() - us;
	if (res < 0) {
	    fprintf(stderr, "error %d in uart_link_recv\n", res);
	    return;
	} else {
	}
	snd.length = rcvp->length;
	memcpy(rcv.payload, rcvp->payload, rcvp->length);

	return;
}

void stress_test_comm(struct uart_link *link)
{
	int size = 2;
	int i;

	/* stress-test uart traffic */
	/* this samples the 1..512 space of lengths
	 * by doing some power residue arithmetic
	 */
	for (i = 0; i < 200; i++) {
		if (size > 512)
		    test_comm(link, 523-size);
		else
		    test_comm(link, size);
		fprintf(stderr, ".");
		size *= 2;
		size %= 523;
	}
	fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
	struct ertm_status *h = ertm_init(usb_serial);
	struct ertm_state c, *config = &c;
	struct ertm_state m, *mask = &m;
	struct ertm14_board_state *bs;
	struct ertm14_board_state *bsmask;
	struct WRC_DIAGS_WB d, *diags = &d;

	stress_test_comm(&h->link);
	exit(1);

	fprintf(stderr,"------------------------------\n");
	fprintf(stderr,"getting board config\n");
	get_board_config(h);
	display_ertm_state(h->state);
	fprintf(stderr,"got board config\n");
	memcpy(config, h->state, sizeof(*config));
	memset(mask, 0, sizeof(*mask));
	fprintf(stderr,"------------------------------\n");
	exit(1);
	goto ello;

	/* set a visually recognizable value */
	fprintf(stderr,"------------------------------\n");
	fprintf(stderr,"setting funny board config\n");
	bs = &config->board_state;
	bsmask = &mask->board_state;
	bs->lo.ampl_factor = float_to_ampl_factor(0.577216);
	bsmask->lo.ampl_factor = 1;
	bs->ref.ampl_factor = float_to_ampl_factor(0.314159);
	bsmask->lo.ampl_factor = 1;
	set_board_config(h, config);
	commit_board_config(h, mask);
	get_sim_board_config(h);
	display_ertm_state(h->state);
	fprintf(stderr,"------------------------------\n");

	goto ello;

	/* switch those visually recognizable values */
	fprintf(stderr,"------------------------------\n");
	fprintf(stderr,"setting a different funny board config\n");
	bs->lo.ampl_factor = float_to_ampl_factor(0.314159);
	bsmask->lo.ampl_factor = 1;
	bs->ref.ampl_factor = float_to_ampl_factor(0.577216);
	bsmask->lo.ampl_factor = 1;
	set_board_config(h, config);
	commit_board_config(h, mask);
	display_ertm_state(h->state);
	fprintf(stderr,"------------------------------\n");
ello:
	/* get wr diags */
	fprintf(stderr,"------------------------------\n");
	fprintf(stderr,"getting diags from wrc\n");
	get_wr_diags(h, diags);
	display_wrc_diags(diags);
	display_hex((void*)diags, sizeof(*diags));
	fprintf(stderr,"------------------------------\n");
	ertm_exit(h);

	return 0;
}
