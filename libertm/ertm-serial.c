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

/* #include "libertm.h" FIXME */
#include "ertm14-uart-link.h"

/* constants of nature for this design */
static const char *usb_serial = "/dev/ttyUSB2";
static const int serial_speed = 8*115200;

int main(int argc, char *argv[])
{
    	struct uart_link ln, *link = &ln;
        struct uart_packet pack, *tx_pkt = &pack;
        struct uart_packet pack2, *rx_pkt = &pack2;
	int res, stat;

	uart_link_create_linux(link, usb_serial, serial_speed);

        fprintf(stderr,"sending command 'command'\n");
        tx_pkt->ptype = ERTM14_UART_PTYPE_PING;
        tx_pkt->length = strlen("command");
	memcpy(&tx_pkt->payload, "command", strlen("command") + 1);

        res = uart_link_send(link, tx_pkt);
	if (res < 0) {
		printf("error %d in uart_link_send\n", res);
		return res;
	}

	memset(rx_pkt, 0, sizeof(*rx_pkt));
        stat = uart_link_recv(link, &rx_pkt, 1000);
        if (stat > 0) {
		int i;
		fprintf(stderr,"recvd %d bytes [", rx_pkt->length);
		for (i = 0; i < rx_pkt->length; i++)
			fprintf(stderr, "%02x ", rx_pkt->payload[i]);
		fprintf(stderr,"]\n");
		fprintf(stderr, "to wit: [%s]\n", rx_pkt->payload);
        }

	return 0;
}
