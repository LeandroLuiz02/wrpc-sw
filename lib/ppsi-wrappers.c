/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 - 2015 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdint.h>
#include <stddef.h>
#include <libwr/hal_shmem.h>
#include <libwr/shmem.h>
#include <wrc_ptp.h>
#include <dev/syscon.h>
#include <dev/endpoint.h>
#include <softpll_ng.h>
#include <ptpd_netif.h>

#include <sfp.h>

#include <board.h>

void *ppsi_head;

/* Following code from ptp-noposix/libposix/freestanding-wrapper.c */

static int read_phase_val(struct wrc_port_state *port)
{
	int32_t dmtd_phase;

	if (spll_read_ptracker(0, &dmtd_phase, NULL)) {
		port->phase_val = dmtd_phase;
		port->phase_val_valid = 1;
	} else {
		port->phase_val = 0;
		port->phase_val_valid = 0;
	}

	return 0;
}

extern uint32_t cal_phase_transition;

int wrpc_get_port_state(struct wrc_port_state *port, const char *port_name)
{
	/* fill deltas */
	port->calib.delta_tx_ps = sfp_info.sfp_params.dTx;
	port->calib.delta_rx_ps = sfp_info.sfp_params.dRx;
	/* fill alpha */
	port->calib.alpha = sfp_info.sfp_params.alpha;
	/* get the bitslide */
	port->calib.bitslide_ps = ep_get_bitslide(&wrc_endpoint_dev);
	read_phase_val(port);
	port->locked = spll_check_lock(0);
	port->clock_period  = REF_CLOCK_PERIOD_PS;
	port->t2_phase_transition = cal_phase_transition;
	port->t4_phase_transition = cal_phase_transition;
	ep_get_mac_addr(&wrc_endpoint_dev, port->hw_addr);

	return 0;
}

/* dummy function, no shmem locks (no even shmem) are implemented in wrpc */
void wrs_shm_write(void *headptr, int flags)
{
	return;
}
