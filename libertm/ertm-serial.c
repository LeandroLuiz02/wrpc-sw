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
	loref->level_adjust 			/= (1<<14);
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++) {
		loref->chpower[i] = ntohl(dds->out_power[i]);
		loref->chpower[i] /= 1000;
		loref->state[i] = dds->out_state[i];
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

int main(int argc, char *argv[])
{
    	struct uart_link ln, *link = &ln;
        struct uart_packet pack, *tx_pkt = &pack;
        struct uart_packet pack2, *rx_pkt = &pack2;
	struct ertm_lo_ref loref;
	int res, stat;

	uart_link_create_linux(link, usb_serial, serial_speed);

        fprintf(stderr,"sending command 'command'\n");
        tx_pkt->ptype = ERTM14_UART_PTYPE_CONFIG_REQ;
	// #define ERTM14_UART_PTYPE_CONFIG_RESP 7
        tx_pkt->length = strlen("command");
	memcpy(&tx_pkt->payload, "command", strlen("command") + 1);

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

		fprintf(stderr,"recvd %d bytes: \n", rx_pkt->length);
		for (i = 0; i < rx_pkt->length; i++)
			fprintf(stderr, "%02x%c", rx_pkt->payload[i],
				((i+1) % 16 == 0) ? '\n' : ' ');
		if ((i+1) % 16 != 0)
			fprintf(stderr, "\n");
		board = (struct ertm14_board_state *)&rx_pkt->payload[4];
		dds_state_to_lo_ref(&board->ref, &loref);
		display_ertm_lo_ref(&loref);
        }

	return 0;
}
