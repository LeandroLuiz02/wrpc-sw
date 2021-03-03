/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright 2020-2021 CERN
 * Author: Juan David Gonzalez Cobas
 *
 * This program calls the library libertm to control an eRTM14/15 combo
 */

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "libertm.h"
/*
    char *ertm_perror(int error)
 */

int main(int argc, char *argv[])
{
	static char usb[] = "/dev/ttyUSB2";
	struct ertm_status *handle = ertm_init(usb);

	if (handle == NULL) {
		fprintf(stderr, "could not open %s\n", usb);
		exit(1);
	}

	ertm_exit(handle);

	return 0;
}

#if 0
struct ertm_status *ertm_init(char *address);
void ertm_exit(struct ertm_status *handle);		/* end connection, destroy handle */

static void clkab_defaults(struct ertm_clk *clk)
{
	int i;
	memset(clk, 0, sizeof(*clk));
	clk->enabled_mask = 0;
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++)
		clk->chfreq[i] = ERTM_CLKAB_DEFAULT_FREQ;
}

/* any sensible value will do, simulation-only stuff */
#define	ERTM_LOREF_DEFAULT_CHPOWER	15.0	/* dBm, random dflt */;

static void lo_ref_defaults(struct ertm_lo_ref *lo_ref, uint32_t default_freq)
{
	int i;

	memset(lo_ref, 0, sizeof(*lo_ref));
	lo_ref->enabled_mask= 0;
	lo_ref->freq = default_freq;
	for (i = ERTM_LOREF_MIN_CH; i <= ERTM_LOREF_MAX_CH; i++)
		lo_ref->chpower[i] = ERTM_LOREF_DEFAULT_CHPOWER;
	lo_ref->level_adjust = 1.0;
	lo_ref->pll_output_power = ERTM_LOREF_DEFAULT_CHPOWER;
}

static struct ertm_temperatures temperatures_defaults = {
	50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
	{0, 0, 0, 0},
};
	
static struct ertm_voltages voltages_defaults = {
	11.9, 3.2, 1.0, 8.3, 8.3, 5.0, 11.95, 3.1,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};
struct WRC_DIAGS_WB wr_status_default = {
	/* FIXME: copied, not #include'd, from wrc_diags_regs.h */
	/* eventually replace by struct WRC_DIAGS_WB */
	.VER		= 0xdeadbabe,	/* [0x0]: REG Version register */
	.CTRL		= 0,   		/* [0x4]: REG Ctrl */
	.WDIAG_SSTAT	= 0,  		/* [0x8]: REG WRPC Diag: servo status */
	.WDIAG_PSTAT	= 1,  		/* [0xc]: REG WRPC Diag: Port status */
	.WDIAG_PTPSTAT	= 3,		/* [0x10]: REG WRPC Diag: PTP state */
	.WDIAG_ASTAT	= 0xa5,		/* [0x14]: REG WRPC Diag: AUX state */
	.WDIAG_TXFCNT	= 0xa5,		/* [0x18]: REG WRPC Diag: Tx PTP Frame cnts */
	.WDIAG_RXFCNT	= 0xa5,		/* [0x1c]: REG WRPC Diag: Rx PTP Frame cnts */
	.WDIAG_SEC_MSB	= 0xa5,		/* [0x20]: REG WRPC Diag:local time [msb of s] */
	.WDIAG_SEC_LSB	= 0xa5,		/* [0x24]: REG WRPC Diag: local time [lsb of s] */
	.WDIAG_NS	= 0xa5,     	/* [0x28]: REG WRPC Diag: local time [ns] */
	.WDIAG_MU_MSB	= 0xa5,		/* [0x2c]: REG WRPC Diag: Round trip (mu) [msb of ps] */
	.WDIAG_MU_LSB	= 0xa5,		/* [0x30]: REG WRPC Diag: Round trip (mu) [lsb of ps] */
	.WDIAG_DMS_MSB	= 0xa5,		/* [0x34]: REG WRPC Diag: Master-slave delay (dms) [msb of ps] */
	.WDIAG_DMS_LSB	= 0xa5,		/* [0x38]: REG WRPC Diag: Master-slave delay (dms) [lsb of ps] */
	.WDIAG_ASYM	= 0xa5,		/* [0x3c]: REG WRPC Diag: Total link asymmetry [ps] */
	.WDIAG_CKO	= 0xa5,		/* [0x40]: REG WRPC Diag: Clock offset (cko) [ps] */
	.WDIAG_SETP	= 0xa5,		/* [0x44]: REG WRPC Diag: Phase setpoint (setp) [ps] */
	.WDIAG_UCNT	= 0xa5,		/* [0x48]: REG WRPC Diag: Update counter (ucnt) */
	.WDIAG_TEMP	= 0xa5,		/* [0x4c]: REG WRPC Diag: Board temperature [C degree] */
};

/* WARNING: mostly unused */
struct ertm_device_metadata device_metadata_defaults = {
	.vendor_id = 0x10dc,
	.device_id = 0xbabe,
	.version = 0xdeadbabe,
	.byte_order_mark = 0xFEFF,
	.source_id = "sim-ertm14/15",
	.capability_mask = 0,
	.vendor_uuid = { 0xad, 0x38, 0xb6, 0xb6, 0x86, 0x48,
			0x4a, 0x35, 0x98, 0x0e, 0xba, 0x93,
			0x75, 0xbd, 0x27, 0x61, },
};

/* All fake values to clearly spot simulation */
struct ertm_board_info board_info_defaults = {
	.ertm14_storage = 0xbabecafea5a5a514,
	.ertm14_mac1 = 0x00112233445566,
	.ertm14_mac2 = 0x00223344556677,
	.ertm15 = 0xbabecafea5a5a515,
	.firmware_version = "sim-0.0",		/* FIXME */
	.wrpc_sw_version = "wrpc_sw-sim-0.0",
        .wrpc_sw_commit_id =
		"8f087ad4e0aa8ede6736506bfdc1fbde",
        .wrpc_sw_build_date = "Mon Jan 25 2021",
        .wrpc_sw_build_time = "10:40:46 CET",
        .wrpc_sw_build_by = "dcobas@cern.ch",
	.firmware_metadata = {
		    .vendor_id = 0x10dc,
		    .device_id = 0xbabe,
		},
};

/* provide sensible initial values for all params */
static void ertm_status_init(struct ertm_state *st)
{
	memcpy(&st->board_info, &board_info_defaults,
		sizeof(st->board_info));
	clkab_defaults(&st->clka);
	clkab_defaults(&st->clkb);
	lo_ref_defaults(&st->lo, ERTM_LO_DEFAULT_FREQ);
	lo_ref_defaults(&st->ref, ERTM_REF_DEFAULT_FREQ);
	memcpy(&st->temperatures, &temperatures_defaults,
		sizeof(st->temperatures));
	memcpy(&st->voltages, &voltages_defaults,
		sizeof(st->voltages));
	memcpy(&st->wr_status, &wr_status_default, sizeof(st->wr_status));
	/* FIXME: st->nco_reset */
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

int ertm_get_board_info(struct ertm_status *handle, struct ertm_board_info *info)
{
	if (handle == NULL) {
		errno = EINVAL;
		return -1;
	}
	memcpy(info, &handle->state, sizeof(*info));
	return 0;
}

#define ERTM_WITHIN(ch, min, max)	\
	((min <= (ch)) && ((ch) <= max))

static struct ertm_ch_range {
	int	min;
	int	max;
} ranges[] = {
	[ERTM_CLKA] = { ERTM_CLKAB_MIN_CH, ERTM_CLKAB_MAX_CH },
	[ERTM_CLKB] = { ERTM_CLKAB_MIN_CH, ERTM_CLKAB_MAX_CH },
	[ERTM_REF]  = { ERTM_LOREF_MIN_CH, ERTM_LOREF_MAX_CH },
	[ERTM_LO]   = { ERTM_LOREF_MIN_CH, ERTM_LOREF_MAX_CH },
};

/* sanity check connector/channel combinations */
static int out_of_range(enum ertm_connector connector, int channel)
{
	int min, max;

	switch (connector) {
	case ERTM_CLKA:
	case ERTM_CLKB:
	case ERTM_REF:
	case ERTM_LO:
		min = ranges[connector].min;
		max = ranges[connector].max;
		if ((channel < min) || (channel > max)) {
			errno = EINVAL;
			return ERTM_CH_OUT_OF_RANGE;
		}
	default:
		return ERTM_BAD_CONNECTOR;
	}
	return 0;
}

static void get_set(uint32_t *attr, uint32_t *val, int set)
{
	if (!set)
		*attr = *val;
	else
		*val = *attr;
}

#if 0
static int clkab_set_output_divider(int clka_or_clkb, int output, int divider)
{
	struct ertm14_board_state state, *st = &state;
	struct ertm14_board_state mask, *msk = &mask;

}
#endif

static int ertm_get_set_freq(struct ertm_status *handle,
		enum ertm_connector connector,int channel, uint32_t *freq,
		int set)
{
	int err = 0;
	struct ertm_clk *clk;
	struct ertm_lo_ref *loref;

	if (handle == NULL) {
		errno = EINVAL;
		return -ERTM_BAD_HANDLE;
	}

	/* channel param is irrelevant for lo/ref */
	if (connector == ERTM_LO || connector == ERTM_REF) {
		channel = ERTM_LOREF_MIN_CH;
	}

	/* but it must be within range for CLKA/B */
	if ((err = out_of_range(connector, channel)) != 0)
		return err;

	switch (connector) {
	case ERTM_CLKA:
		clk = &handle->state->clka;
		get_set(freq, &clk->chfreq[channel], set);
		// clkab_set_output_divider(ERTM14_OUT_CLKA, channel, freq);
		break;
	case ERTM_CLKB:
		clk = &handle->state->clkb;
		get_set(freq, &clk->chfreq[channel], set);
		break;
	case ERTM_LO:
		loref = &handle->state->lo;
		get_set(freq, &loref->freq, set);
		break;
	case ERTM_REF:
		loref = &handle->state->ref;
		get_set(freq, &loref->freq, set);
		break;
	default:
		errno = EINVAL;
		return ERTM_BAD_CONNECTOR;
	}

	return 0;
}

int ertm_get_freq(struct ertm_status *handle,
		enum ertm_connector connector,int channel, uint32_t *freq)
{
	return ertm_get_set_freq(handle, connector, channel, freq, 0);
}

int ertm_set_freq(struct ertm_status *handle,
		enum ertm_connector connector,int channel, uint32_t freq)
{
	return ertm_get_set_freq(handle, connector, channel, &freq, 1);
}

int ertm_channel_enable(struct ertm_status *handle,
		enum ertm_connector connector, int channel, int enable)
{
	uint32_t *mask;
	int err;

	if ((err = out_of_range(connector, channel)) != 0) {
		return err;
	}
	switch (connector) {
	case ERTM_CLKA:
		mask = &handle->state->clka.enabled_mask;
		break;
	case ERTM_CLKB:
		mask = &handle->state->clkb.enabled_mask;
		//clkab_enable_output(ERTM14_OUT_CLKA, channel, enable);
		break;
	case ERTM_LO:
		mask = &handle->state->lo.enabled_mask;
		break;
	case ERTM_REF:
		mask = &handle->state->ref.enabled_mask;
		break;
	default:
		errno = EINVAL;
		return ERTM_BAD_CONNECTOR;
	}
	enable = (!!enable) << channel;
	*mask &= ~(1<<channel);
	*mask |= enable;

	return 0;
}

static int get_dds(struct ertm_status *handle,
		enum ertm_connector connector, struct ertm_lo_ref **dds)
{
	switch (connector) {
	case ERTM_LO:
		*dds = &handle->state->lo;
		break;
	case ERTM_REF:
		*dds = &handle->state->ref;
		break;
	default:
		return ERTM_BAD_CONNECTOR;
		break;
	}
	return 0;
}

int ertm_get_power(struct ertm_status *handle,
		enum ertm_connector connector, double *power)
{
	struct ertm_lo_ref *clk;
	int err;

	if ((err = get_dds(handle, connector, &clk)) != 0) {
		errno = EINVAL;
		return err;
	}

	*power = clk->pll_output_power;
	return 0;
}

int ertm_get_channel_power(struct ertm_status *handle,
		enum ertm_connector connector, int channel, double *power)
{
	uint32_t mask = (1<<channel);
	return ertm_get_channel_power_all(handle,
		connector, mask, power);
}

int ertm_get_channel_power_all(struct ertm_status *handle,
		enum ertm_connector connector,
		uint32_t valid_mask, double *power)
{
	struct ertm_lo_ref *clk;
	int i, err;

	if ((err = get_dds(handle, connector, &clk)) != 0) {
		errno = EINVAL;
		return err;
	}
	for (i = ERTM_LOREF_MIN_CH; i <= ERTM_LOREF_MAX_CH; i++) {
		if (valid_mask & (1<<i))
		    power[i] = clk->chpower[i];
	}

	return 0;
}

int ertm_dds_set_level_adjust(struct ertm_status *handle,
		enum ertm_connector connector, double level)
{
	struct ertm_lo_ref *clk;
	int err;

	if ((err = get_dds(handle, connector, &clk)) != 0)
		return err;

	clk->level_adjust = level;
	return 0;
}

int ertm_dds_get_level_adjust(struct ertm_status *handle,
		enum ertm_connector connector, double *level)
{
	struct ertm_lo_ref *clk;
	int err;

	if ((err = get_dds(handle, connector, &clk)) != 0)
		return err;

	*level = clk->level_adjust;
	return 0;
}

int ertm_get_temperatures(struct ertm_status *handle, struct ertm_temperatures *temps)
{
	memcpy(temps, &handle->state->temperatures, sizeof(*temps));
	return 0;
}

int ertm_get_voltages(struct ertm_status *handle, struct ertm_voltages *volts)
{
	memcpy(volts, &handle->state->voltages, sizeof(*volts));
	return 0;
}

int ertm_get_ocxo_current(struct ertm_status *handle, double *current)
{
	/* not implemented */
	return ERTM_NOT_IMPLEMENTED;
}
#if 0
/* system-wide NCO reset */
int ertm_rf_nco_reset_enable(struct ertm_status *handle, int enable);
int ertm_rf_nco_reset(struct ertm_status *handle);
int ertm_nco_reset_subscribe(struct ertm_status *handle,
		enum ertm_connector, int enable, int channel, uint32_t stream_id);
int ertm_nco_reset_get_status(struct ertm_status *handle, struct ertm_nco_reset *status);
#endif

int ertm_wr_diags(struct ertm_status *handle, struct ertm_wr_status *status)
{
	memcpy(status, &handle->state->wr_status, sizeof(*status));
	return 0;
}

int ertm_wr_status(struct ertm_status *handle, int *link_up, int *is_locked)
{
	struct ertm_wr_status status;
	int err;

	if ((err = ertm_wr_diags(handle, &status)) != 0)
		return err;
	*link_up = status.WDIAG_PSTAT & 1;
	*is_locked = status.WDIAG_PSTAT & 2;

	return 0;
}

int ertm_wr_enable(struct ertm_status *handle, int enable)
{
	/* do a call to ptp start/stop */
	handle->state->ptp_enabled = enable;
	return 0;
}
#endif
