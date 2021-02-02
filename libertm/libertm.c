/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright 2020-2021 CERN
 * Author: Juan David Gonzalez Cobas
 *
 * This library interacts with a simulated eRTM14/15 combo
 */

#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "libertm.h"

#define ERTM_BAD_CONNECTOR	(-1)
#define ERTM_CH_OUT_OF_RANGE	(-2)

/* translate enum to kHz if needed */
static uint32_t clkab_freq_table[] = {
	[ERTM_CLKAB_1000MHz] = 1000000000UL,
	[ERTM_CLKAB_500MHz]  =  500000000UL,
	[ERTM_CLKAB_250MHz]  =  250000000UL,
	[ERTM_CLKAB_125MHz]  =  125000000UL,
	[ERTM_CLKAB_62_5MHz] =   62500000UL,
		/* FIXME: do we really need 62.5? */
};
const int clkab_nfreqs = sizeof(clkab_freq_table)/sizeof(clkab_freq_table[0]);

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
	unsigned int	state[ERTM_LOREF_MAX_CH+1];	/* one of
					ERTM15_RF_OUT_ON|OFF|MONITOR */
	uint32_t	pll_output_power;	/* aka amp_power */
	double		level_adjust;		/* full-scale DDS = 1.0 */
						/* aka ampl_factor */
	uint32_t	reserved[16];
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
	uint32_t		reserved[64];
};

struct ertm_connection {
	char	*address;
	char	serial_connection[PATH_MAX];
};

struct ertm_status {
	struct ertm_connection connection;
	struct ertm_state *state;
};
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

/* WARNING: mostly unused */
struct ertm_device_metadata device_metadata_defaults = {
	.vendor_id = 0x10dc,
	.device_id = 0xbabe,
	.version = 0xcafe,
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
	.firmware_version = "sim-0.0",
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
	/* FIXME: st->nco_reset */
}
struct ertm_status *ertm_init(char *address)
{
	struct ertm_status *status = malloc(sizeof(*status));
	status->state = malloc(sizeof(*status->state));
	ertm_status_init(status->state);

	return status;
}

void ertm_exit(struct ertm_status *handle)
{
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

static int ertm_get_set_freq(struct ertm_status *handle,
		enum ertm_connector connector,int channel, uint32_t *freq,
		int set)
{
	int err = 0;
	struct ertm_clk *clk;
	struct ertm_lo_ref *loref;

	if (handle == NULL) {
		errno = EINVAL;
		return -1;
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

#if 0
int ertm_set_freq(struct ertm_status *handle,
		enum ertm_connector connector, int channel, uint32_t freq);

/* the following refer only to REF/LO connectors */
int ertm_channel_enable(struct ertm_status *handle,
		enum ertm_connector connector, int channel, int enable);		/* default disabled */
int ertm_get_power(struct ertm_status *handle,
		enum ertm_connector connector, double *power);				/* power level in dBm */
int ertm_get_channel_power(struct ertm_status *handle,
		enum ertm_connector connector, int channel, double *power);		/* power per channel in dBm */
int ertm_get_channel_power_all(struct ertm_status *handle,
		enum ertm_connector connector, uint32_t valid_mask, double *power);	/* powers in dBm */
int ertm_dds_set_level_adjust(struct ertm_status *handle,
		enum ertm_connector connector, double level);			/* level in [0,1] */
int ertm_dds_get_level_adjust(struct ertm_status *handle,
		enum ertm_connector connector, double *level);			/* level in [0,1] */

/* monitoring */
int ertm_get_ocxo_current(struct ertm_status *handle, double *current);			/* amperes */
int ertm_get_temperatures(struct ertm_status *handle, struct ertm_temperatures *temps);	/* all celsius */
int ertm_get_voltages(struct ertm_status *handle, struct ertm_voltages *volts);		/* all volt */

/* system-wide NCO reset */
int ertm_rf_nco_reset_enable(struct ertm_status *handle, int enable);
int ertm_rf_nco_reset(struct ertm_status *handle);
int ertm_nco_reset_subscribe(struct ertm_status *handle,
		enum ertm_connector, int enable, int channel, uint32_t stream_id);
int ertm_nco_reset_get_status(struct ertm_status *handle, struct ertm_nco_reset *status);

/* WR enable/diagnostics */
struct ertm_wr_status;						/* to be defined with rabbits */
int ertm_wr_enable(struct ertm_status *handle, int enable);	/* free-running OCXO if disabled */
int ertm_wr_status(struct ertm_status *handle, int *link_up, int *is_locked);
int ertm_wr_diags(struct ertm_status *handle, struct ertm_wr_status *status);

#endif
