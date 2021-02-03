/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2018 CERN (www.cern.ch)
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
//#include <wrpc.h>
#include <string.h>

#include "ipv4.h"
#include "ptpd_netif.h"
#include "shell.h"
#include "netconsole.h"

static uint8_t __netconsole_queue[128];
static struct wrpc_socket __static_netconsole_socket = {
	.queue.buff = __netconsole_queue,
	.queue.size = sizeof(__netconsole_queue),
};

static struct wrpc_socket *netconsole_socket;
static unsigned char *cmd_rx_p = NULL;
/* net headers + cmd len */
static uint8_t tx_buf[UDP_END + SH_MAX_LINE_LEN + 1];
static uint8_t rx_buf[UDP_END + SH_MAX_LINE_LEN + 1];
struct wr_sockaddr netconsole_sock_addr;
static int netconsole_has_peer = 0;
struct wr_udp_addr netconsole_udp_addr;


void netconsole_init(void)
{
	netconsole_socket = ptpd_netif_create_socket(
					&__static_netconsole_socket, NULL,
					PTPD_SOCK_UDP, 55);
					/* TODO: find a better port number */
}

int netconsole_read_byte(void)
{
	if (cmd_rx_p && *cmd_rx_p != 0)
		return *(cmd_rx_p++);
	return -1;
}

int netconsole_write_string(const char *s)
{
	int len;
	if (!netconsole_has_peer)
		return 0;
	/* Prevent recursive calls when net verbose configured.
	 * NOTE: Even with the following if, NET_IS_VERBOSE does not work
	 * with netconsole */
	if (NET_IS_VERBOSE)
		netconsole_has_peer = 0;
	strncpy((char *) &tx_buf[UDP_END], s, SH_MAX_LINE_LEN);
	len = min(strlen(s), SH_MAX_LINE_LEN);
	len += UDP_END;
	fill_udp((uint8_t *)tx_buf, len, &netconsole_udp_addr);
	ptpd_netif_sendto(netconsole_socket,
			  &netconsole_sock_addr, tx_buf, len, 0);
	if (NET_IS_VERBOSE)
		netconsole_has_peer = 1;
	return 0;
}


int netconsole_poll(void)
{
	int len;

	if (ip_status == IP_TRAINING)
		return 0;	/* can't do netconsole w/o an address... */

	if ((len = ptpd_netif_recvfrom(netconsole_socket,
				       &netconsole_sock_addr, rx_buf,
				       sizeof(rx_buf), NULL)
	    ) > 0) {
		if (check_dest_ip(rx_buf)) {
			/* wrong destination IP */
			return 0;
		}

		rx_buf[len] = 0;

		/* copy source and destination address */
		memcpy(&netconsole_udp_addr.daddr, rx_buf + IP_SOURCE, 4);
		memcpy(&netconsole_udp_addr.saddr, rx_buf + IP_DEST, 4);
		memcpy(&netconsole_udp_addr.dport, rx_buf + UDP_SPORT, 2);
		memcpy(&netconsole_udp_addr.sport, rx_buf + UDP_DPORT, 2);

		cmd_rx_p = &rx_buf[UDP_END];

		netconsole_has_peer = 1;

		return 1;
	}
	return 0;
}
