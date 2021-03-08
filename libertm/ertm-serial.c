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
#include "display.h"
#include "ertm14-uart-link.h"
#include "ertm15_rf_distr.h"

#define BUG(expr)	\
	do {	\
		if (!(expr)) {	\
			fprintf(stderr, "assertion" #expr "failed!\n");	\
			exit(1);	\
		}	\
	} while (0)

/* FIXME: repeated, not needed here */
static uint8_t float_to_ampl_factor(double level)
{
	return (uint8_t)floor(level * 256);
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
	//state_to_board(mask, cfg);
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
	// state_to_board(config, cfg);
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

/* get rid of this when rid of get_board_config_sim */
extern void board_state_to_host_order(struct ertm14_board_state *board, struct ertm14_board_state *host);

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
	board_state_to_host_order(board, &state->board_state);

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

/* constants of nature for this design */
char *usb_serial = "/dev/ttyUSB2";

int main(int argc, char *argv[])
{
	struct ertm_status *h = ertm_init(usb_serial);
	struct ertm_state c, *config = &c;
	struct ertm_state m, *mask = &m;
	struct ertm14_board_state *bs;
	struct ertm14_board_state *bsmask;
	struct ertm_wr_status d, *diags = &d;

	if (0)
		stress_test_comm(&h->link);

	fprintf(stderr,"------------------------------\n");
	fprintf(stderr,"getting board config\n");
	ertm_get_board_config(h, &h->state->board_state);
	display_ertm_state(h->state);
	exit(1);
	fprintf(stderr,"got board config\n");
	memcpy(config, h->state, sizeof(*config));
	memset(mask, 0, sizeof(*mask));
	fprintf(stderr,"------------------------------\n");

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
#if 0
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
#endif
ello:
	/* get wr diags */
	fprintf(stderr,"------------------------------\n");
	fprintf(stderr,"getting diags from wrc\n");
	ertm_wr_diags(h, diags);
	display_wrc_diags(diags);
	display_hex((void*)diags, sizeof(*diags));
	fprintf(stderr,"------------------------------\n");
	ertm_exit(h);

	return 0;
}
