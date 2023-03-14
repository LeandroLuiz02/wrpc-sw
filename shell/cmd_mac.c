/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>

#include "shell.h"
#include "storage.h"
#include "ptpd_netif.h"
#include "dev/endpoint.h"
#include "ppsi/lib.h"


static int cmd_mac(const char *args[])
{
	unsigned char mac[6];
	char buf[32];
	int port;

	if (!args[0] || !strcasecmp(args[0], "get")) {
		/* get current MAC */
		copy_eth_addr(mac, wrc_endpoint_dev.mac_addr);
	} else if (!strcasecmp(args[0], "getp")) {
		/* get persistent MAC */
		decode_port(args[1], &port);
		storage_get_persistent_mac(port, mac);
	} else if (!strcasecmp(args[0], "set") && args[1]) {
		decode_mac(args[1], mac);
		ep_set_mac_addr(&wrc_endpoint_dev, mac);
		ep_pfilter_init_default(&wrc_endpoint_dev);
	} else if (!strcasecmp(args[0], "setp") && args[1]) {
		decode_mac(args[1], mac);
		decode_port(args[2], &port);
		storage_set_persistent_mac(port, mac);
	} else {
		return -EINVAL;
	}

	pp_printf("MAC-address: %s\n", format_mac(buf, mac));
	return 0;
}

DEFINE_WRC_COMMAND(mac) = {
	.name = "mac",
	.exec = cmd_mac,
};
