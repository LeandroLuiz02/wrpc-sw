/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright 2020-2021 CERN
 * Author: Juan David Gonzalez Cobas
 *
 * The comm via USB serial is implemented here
 */

#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <stdlib.h>
#include <stdio.h>

#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <math.h>


#include "board-state.h"
#include "private.h"
#include "libertm.h"
#include "ertm14-uart-link.h"
#include "ertm15_rf_distr.h"

/* constants of nature for this design */
static const char *usb_serial = "/dev/ttyUSB2";
static const int serial_speed = 8*115200;

void dds_state_to_lo_ref(struct ertm14_dds_state *dds, struct ertm_lo_ref *loref)
{
	int i;

	loref->freq 				= ntohl(dds->ftw);
	loref->pll_output_power 		= ntohl(dds->amp_power);
	loref->pll_output_power 		/= 1000;	/* to dBm */
	loref->level_adjust 			= ntohl(dds->ampl_factor);
	loref->level_adjust 			/= (1<<8);
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++) {
		loref->chpower[i] = ntohl(dds->out_power[i]);
		loref->chpower[i] /= 1000;
		loref->state[i] = dds->out_state[i];
	}
}

void lo_ref_to_dds_state(struct ertm_lo_ref *loref, struct ertm14_dds_state *dds)
{
	int i;

	dds->ftw                 = htonl(loref->freq);
	loref->pll_output_power *= 1000;	/*  to  mdBm  */
	dds->amp_power           = htonl(floor(loref->pll_output_power));
	loref->level_adjust 	/= (1<<8);
	dds->ampl_factor         = htonl(floor(loref->level_adjust));
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++) {
		loref->chpower[i] *= 1000;
		dds->out_power[i] = htonl(floor(loref->chpower[i]));
		dds->out_state[i] = loref->state[i];
	}
}

static char *state_literal[] = {
	[ERTM15_RF_OUT_ON] = "on",
	[ERTM15_RF_OUT_OFF] = "off",
	[ERTM15_RF_OUT_MONITOR] = "monitor",
};

void display_ertm_lo_ref(struct ertm_lo_ref *dds)
{
	int i;

	printf("ftw: %08x\n", dds->freq);
	printf("level adjust: %0.4f\n", dds->level_adjust);
	printf("pll_out_power: %08x\n", dds->pll_output_power);
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++)
		printf("ch: %02d pow: %8.3f %d %-8s\n",
		    i, dds->chpower[i], dds->state[i],
		    state_literal[dds->state[i]]);
}

void board_to_state(struct ertm14_board_state *board, struct ertm_state *state)
{
	int i;

	dds_state_to_lo_ref(&board->ref, &state->ref);
	dds_state_to_lo_ref(&board->lo, &state->lo);
	state->clka.enabled_mask = ntohl(board->clka_enable_mask);
	state->clkb.enabled_mask = ntohl(board->clkb_enable_mask);
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		/* FIXME: not enum */
		state->clka.chfreq[i] = ntohl(board->clka_freq_hz[i]);
		state->clkb.chfreq[i] = ntohl(board->clkb_freq_hz[i]);
	}
}

void state_to_board(struct ertm_state *state, struct ertm14_board_state *board)
{
	int i;

	lo_ref_to_dds_state(&state->ref, &board->ref);
	lo_ref_to_dds_state(&state->lo, &board->lo);
	board->clka_enable_mask = htonl(state->clka.enabled_mask);
	board->clkb_enable_mask = htonl(state->clkb.enabled_mask);
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		/* FIXME: not enum */
		board->clka_freq_hz[i] = htonl(state->clka.chfreq[i]);
		board->clkb_freq_hz[i] = htonl(state->clkb.chfreq[i]);
	}
}

void display_ertm_clk(struct ertm_clk *clk)
{
	int i;
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		char *onoff = (clk->enabled_mask & (1<<i)) ? "on " : "off";
		printf("ch %02d: %3s  %10dHz\n", i, onoff, clk->chfreq[i]);
	}
}

void display_ertm_state(struct ertm_state *st)
{
	printf("LO:\n");
	display_ertm_lo_ref(&st->lo);
	printf("REF:\n");
	display_ertm_lo_ref(&st->ref);
	printf("CLKA:\n");
	display_ertm_clk(&st->clka);
	printf("CLKB:\n");
	display_ertm_clk(&st->clkb);
}

int main(int argc, char *argv[])
{
    	struct uart_link ln, *link = &ln;
        struct uart_packet pack, *tx_pkt = &pack;
        struct uart_packet pack2, *rx_pkt = &pack2;
	int res, stat;

	uart_link_create_linux(link, usb_serial, serial_speed);

        fprintf(stderr,"sending command 'command'\n");
        tx_pkt->ptype = ERTM14_UART_PTYPE_SNMP_REQ;
        tx_pkt->length = 1;
	tx_pkt->payload[0] = 0x10;

        res = uart_link_send(link, tx_pkt);
	if (res < 0) {
		printf("error %d in uart_link_send\n", res);
		return res;
	}

	memset(rx_pkt, 0, sizeof(*rx_pkt));
        stat = uart_link_recv(link, &rx_pkt, sizeof(*rx_pkt) + 10);
        if (stat > 0) {
		int i;
		struct ertm14_board_state *board;
		struct ertm_state st, *state = &st;

		fprintf(stderr,"recvd %d bytes: \n", rx_pkt->length);
		for (i = 0; i < rx_pkt->length; i++)
			fprintf(stderr, "%02x%c", rx_pkt->payload[i],
				((i+1) % 16 == 0) ? '\n' : ' ');
		if ((i+1) % 16 != 0)
			fprintf(stderr, "\n");
		board = (struct ertm14_board_state *)&rx_pkt->payload[1];
		board_to_state(board, state);
		display_ertm_state(state);
        }

	return 0;
}
