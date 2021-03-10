/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright 2020-2021 CERN
 * Author: Juan David Gonzalez Cobas
 */

#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "libertm.h"
#include "board-state.h"
#include "common-uart-link.h"

struct ertm_state {
	struct ertm_board_info		board_info;
	struct ertm14_board_state	board_state;
	struct ertm14_board_state	next_state;
	struct ertm14_board_state	commit_mask;
	struct ertm_temperatures	temperatures;
	struct ertm_voltages		voltages;
	struct ertm_nco_reset		nco_reset;
	union {
		struct ertm_wr_status	wr_status;
		struct WRC_DIAGS_WB	diags_wb;
	};
	int				ptp_enabled;

	/* one of ERTM_DEFERRED, ERTM_IMMEDIATE, ERTM_OPTIMIZED */
	int				mode;
	uint32_t			reserved[64];
};

struct ertm_connection {
	char	*address;
	char	serial_connection[PATH_MAX];
};

struct ertm_status {
	struct ertm_connection connection;
	struct ertm_state *state;
	struct uart_link link;
	uint32_t reserved[64];
};

extern int ertm_get_board_config(struct ertm_status *st, struct ertm14_board_state *bs);
