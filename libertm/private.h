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
#include "ertm14-uart-link.h"

struct ertm_clk {
	uint32_t		enabled_mask;
	enum ertm_clkab_freq	chfreq[ERTM_CLKAB_MAX_CH+1];
					/* only 4..14 legal */
	uint32_t		reserved[16];
};

struct ertm_lo_ref {
	uint32_t	enabled_mask;
	uint32_t	freq;			/* the ftw */
	double		chpower[ERTM_LOREF_MAX_CH+1];	/* only 4..12 legal */
	uint8_t		state[ERTM_LOREF_MAX_CH+1];	/* one of
					ERTM15_RF_OUT_ON|OFF|MONITOR */
	uint32_t	pll_output_power;	/* aka amp_power */
	double		level_adjust;		/* full-scale DDS = 1.0 */
						/* aka ampl_factor */
	uint32_t	reserved[16];
};

struct ertm_wr_status {
	/* FIXME: copied, not #include'd, from wrc_diags_regs.h */
	/* eventually replace by struct WRC_DIAGS_WB */
	uint32_t VER;		/* [0x0]: REG Version register */
	uint32_t CTRL;         	/* [0x4]: REG Ctrl */
	uint32_t WDIAG_SSTAT;  	/* [0x8]: REG WRPC Diag: servo status */
	uint32_t WDIAG_PSTAT;  	/* [0xc]: REG WRPC Diag: Port status */
	uint32_t WDIAG_PTPSTAT;	/* [0x10]: REG WRPC Diag: PTP state */
	uint32_t WDIAG_ASTAT;  	/* [0x14]: REG WRPC Diag: AUX state */
	uint32_t WDIAG_TXFCNT; 	/* [0x18]: REG WRPC Diag: Tx PTP Frame cnts */
	uint32_t WDIAG_RXFCNT; 	/* [0x1c]: REG WRPC Diag: Rx PTP Frame cnts */
	uint32_t WDIAG_SEC_MSB;	/* [0x20]: REG WRPC Diag:local time [msb of s] */
	uint32_t WDIAG_SEC_LSB;	/* [0x24]: REG WRPC Diag: local time [lsb of s] */
	uint32_t WDIAG_NS;     	/* [0x28]: REG WRPC Diag: local time [ns] */
	uint32_t WDIAG_MU_MSB; 	/* [0x2c]: REG WRPC Diag: Round trip (mu) [msb of ps] */
	uint32_t WDIAG_MU_LSB; 	/* [0x30]: REG WRPC Diag: Round trip (mu) [lsb of ps] */
	uint32_t WDIAG_DMS_MSB;	/* [0x34]: REG WRPC Diag: Master-slave delay (dms) [msb of ps] */
	uint32_t WDIAG_DMS_LSB;	/* [0x38]: REG WRPC Diag: Master-slave delay (dms) [lsb of ps] */
	uint32_t WDIAG_ASYM;   	/* [0x3c]: REG WRPC Diag: Total link asymmetry [ps] */
	uint32_t WDIAG_CKO;    	/* [0x40]: REG WRPC Diag: Clock offset (cko) [ps] */
	uint32_t WDIAG_SETP;   	/* [0x44]: REG WRPC Diag: Phase setpoint (setp) [ps] */
	uint32_t WDIAG_UCNT;   	/* [0x48]: REG WRPC Diag: Update counter (ucnt) */
	uint32_t WDIAG_TEMP;   	/* [0x4c]: REG WRPC Diag: Board temperature [C degree] */
};

struct ertm_state {
	struct ertm_board_info	board_info;
	struct ertm_clk		clka;
	struct ertm_clk		clkb;
	struct ertm_lo_ref	lo;
	struct ertm_lo_ref	ref;
	struct ertm_temperatures
				temperatures;
	struct ertm_voltages	voltages;
	struct ertm_nco_reset	nco_reset;
	struct ertm_wr_status	wr_status;
	int			ptp_enabled;
	uint32_t		reserved[64];
};

struct ertm_connection {
	char	*address;
	char	serial_connection[PATH_MAX];
};

struct ertm_status {
	struct ertm_connection connection;
	struct ertm_state *state;
	struct uart_link link;
};
