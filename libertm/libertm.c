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
#include <stdio.h>
#include <arpa/inet.h>
#include <math.h>

#include "libertm.h"
#include "private.h"
#include "psnmp-proto.h"

struct ertm_error_codes ertm_error_codes[] = {
	[-ERTM_OK]		= { ERTM_OK, "success" },
	[-ERTM_BAD_CONNECTOR]	= { ERTM_BAD_CONNECTOR, "bad connector parameter" },
	[-ERTM_CH_OUT_OF_RANGE]	= { ERTM_CH_OUT_OF_RANGE, "channel number out of range" },
	[-ERTM_NOT_IMPLEMENTED]	= { ERTM_NOT_IMPLEMENTED, "function not implemented" },
	[-ERTM_UART_LINK_SEND_ERR] ={ ERTM_UART_LINK_SEND_ERR, "USB serial link send failed" },
	[-ERTM_UART_LINK_RECV_ERR] ={ ERTM_UART_LINK_RECV_ERR, "USB serial link recv failed" },
};

char *ertm_perror(int error)
{
	return ertm_error_codes[-error].message;
}

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

static void clkab_defaults(struct ertm14_board_state *bs)
{
	int i;
	bs->clka_enable_mask = 0;
	bs->clkb_enable_mask = 0;
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		bs->clka_freq_hz[i] = ERTM_CLKAB_DEFAULT_FREQ;
		bs->clkb_freq_hz[i] = ERTM_CLKAB_DEFAULT_FREQ;
	}
}

/* any sensible value will do, simulation-only stuff */
#define	ERTM_LOREF_DEFAULT_CHPOWER	15.0	/* dBm, random dflt */;

static void dds_defaults(struct ertm14_dds_state *lo_ref, uint32_t default_ftw)
{
	int i;

	memset(lo_ref, 0, sizeof(*lo_ref));
	lo_ref->ftw = default_ftw;
	for (i = ERTM_LOREF_MIN_CH; i <= ERTM_LOREF_MAX_CH; i++) {
		lo_ref->out_state[i] = ERTM_RF_OUT_OFF;
		lo_ref->out_power[i] = ERTM_LOREF_DEFAULT_CHPOWER;
	}
	lo_ref->ampl_factor = 0x7f;
	lo_ref->amp_power = ERTM_LOREF_DEFAULT_CHPOWER;
}

static struct ertm_temperatures temperatures_defaults = {
	50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
	{0, 0, 0, 0},
};
	
static struct ertm_voltages voltages_defaults = {
	11.9, 3.2, 1.0, 8.3, 8.3, 5.0, 11.95, 3.1,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};
struct ertm_wr_status wr_status_default = {
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
	struct ertm14_board_state *bs = &st->board_state;

	memcpy(&st->board_info, &board_info_defaults,
		sizeof(st->board_info));
	clkab_defaults(bs);
	dds_defaults(&bs->lo, ERTM_LO_DEFAULT_FREQ);
	dds_defaults(&bs->ref, ERTM_REF_DEFAULT_FREQ);
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

struct ertm_status *ertm_init(const char *address)
{
	struct ertm_status *st = malloc(sizeof(*st));
	int err;

	if (st == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	st->state = malloc(sizeof(*st->state));
	if (st == NULL) {
		free(st);
		errno = ENOMEM;
		return NULL;
	}
	/* FIXME: this has to be parameterized */
	err = uart_link_create_linux(&st->link, usb_serial, serial_speed);
	if (err != 0) {
		errno = ENODEV;
		return NULL;
	}
	/* FIXME: useless now */
	ertm_status_init(st->state);

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

int ertm_proto_cycle(struct uart_link *link,
	int8_t opcode, void *payload, void *answer)
{
	int res = 0;
	struct uart_packet request, *tx_pkt = &request;
	struct uart_packet *r;
	struct ertm14_protocol_op *op = get_proto_op(opcode);

	if (op == NULL) {
		errno = EINVAL;
		return ERTM_BAD_OPCODE;
	}

	tx_pkt->ptype = ERTM14_UART_PTYPE_SNMP_REQ;
	tx_pkt->length = op->length1;
	tx_pkt->payload[0] = op->opcode;
	memcpy(&tx_pkt->payload[op->offset1], payload, op->length1);
	res = uart_link_send(link, tx_pkt);
	if (res < 0) {
		fprintf(stderr, "error %d in uart_link_send\n", res);
		return ERTM_UART_LINK_SEND_ERR;
	}
	res = uart_link_recv(link, &r, 1000);
	if (res <= 0) {
		fprintf(stderr, "error (res %d) in uart_link_recv\n", res);
		errno = ECOMM;
		return ERTM_UART_LINK_RECV_ERR;
	}
	if (r->ptype != ERTM14_UART_PTYPE_SNMP_RESP) {
		fprintf(stderr, "error (bad packet type != RESP) in uart_link_recv\n");
		errno = EINVAL;
		return ERTM_UART_PROTO_ERR;
	}
	fprintf(stderr,"recvd %d bytes: \n", r->length);
	memset(answer, 0x5a, op->length2);
	memcpy(answer, &r->payload[op->offset2], op->length2);

	return 0;
}

void dds_board_to_host(struct ertm14_dds_state *dds, struct ertm14_dds_state *host)
{
	int i;

	host->ftw 		= ntohl(dds->ftw);
	host->amp_power 	= ntohl(dds->amp_power);
	host->ampl_factor 	= ntohl(dds->ampl_factor);
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++) {
		host->out_power[i] = ntohl(dds->out_power[i]);
		host->out_state[i] = dds->out_state[i];
	}
}

void host_to_dds_board(struct ertm14_dds_state *host, struct ertm14_dds_state *dds)
{
	int i;

	dds->ftw                 = htonl(host->ftw);
	dds->amp_power           = htonl(host->amp_power);
	dds->ampl_factor         = htonl(host->ampl_factor);
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++) {
		dds->out_power[i] = htonl(host->out_power[i]);
		dds->out_state[i] = host->out_state[i];
	}
}

void state_to_board(struct ertm_state *state, struct ertm14_board_state *board)
{
	int i;
	struct ertm14_board_state *bs = &state->board_state;

	host_to_dds_board(&bs->ref, &board->ref);
	host_to_dds_board(&bs->lo, &board->lo);
	board->clka_enable_mask = htonl(bs->clka_enable_mask);
	board->clkb_enable_mask = htonl(bs->clkb_enable_mask);
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		/* FIXME: not enum */
		board->clka_freq_hz[i] = htonl(bs->clka_freq_hz[i]);
		board->clkb_freq_hz[i] = htonl(bs->clkb_freq_hz[i]);
	}
}

void board_to_host(struct ertm14_board_state *board, struct ertm14_board_state *host)
{
	int i;

	dds_board_to_host(&board->ref, &host->ref);
	dds_board_to_host(&board->lo, &host->lo);
	host->clka_enable_mask = ntohl(board->clka_enable_mask);
	host->clkb_enable_mask = ntohl(board->clkb_enable_mask);
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		/* FIXME: not enum */
		host->clka_freq_hz[i] = ntohl(board->clka_freq_hz[i]);
		host->clkb_freq_hz[i] = ntohl(board->clkb_freq_hz[i]);
	}
}

/* here, bs **can** (and should) be st->state->board_state */
int ertm_get_board_config(struct ertm_status *st, struct ertm14_board_state *bs)
{
	struct uart_link *link = &st->link;
	struct ertm14_board_state b, *board = &b;
	int res;

	res = ertm_proto_cycle(link, ertm14_get_board_config, NULL, board);
	if (res < 0)
		return res;

	board_to_host(board, bs);
	return 0;
}

static int ertm_get_set_freq(struct ertm_status *handle,
		enum ertm_connector connector,int channel, uint32_t *freq,
		int set)
{
	int err = 0;
	struct ertm14_board_state *bs;
	uint32_t *reg;

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

	bs = &handle->state->board_state;
	switch (connector) {
	case ERTM_CLKA:
		reg = &bs->clka_freq_hz[channel];
		// clkab_set_output_divider(ERTM14_OUT_CLKA, channel, freq);
		break;
	case ERTM_CLKB:
		reg = &bs->clkb_freq_hz[channel];
		break;
	case ERTM_LO:
		reg = &bs->lo.ftw;
		break;
	case ERTM_REF:
		reg = &bs->ref.ftw;
		break;
	default:
		errno = EINVAL;
		return ERTM_BAD_CONNECTOR;
	}
	get_set(freq, reg, set);
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

static void set_bit(uint32_t *word, unsigned bit, int value)
{
	value = ((!!value) << bit);
	*word &= ~(1<<bit);
	*word |= value;
}

int ertm_channel_enable(struct ertm_status *handle,
		enum ertm_connector connector, int channel, int enable)
{
	struct ertm14_board_state *bs;
	struct ertm14_dds_state *dds;
	uint32_t *mask;
	int err;

	if ((err = out_of_range(connector, channel)) != 0) {
		return err;
	}
	bs = &handle->state->board_state;
	switch (connector) {
	case ERTM_CLKA:
		mask = &bs->clka_enable_mask;
		set_bit(mask, channel, enable);
		break;
	case ERTM_CLKB:
		mask = &bs->clkb_enable_mask;
		set_bit(mask, channel, enable);
		//clkab_enable_output(ERTM14_OUT_CLKA, channel, enable);
		break;
	case ERTM_LO:
		dds = &bs->lo;
		dds->out_state[channel] = (enable ? ERTM_RF_OUT_ON : ERTM_RF_OUT_OFF);
		break;
	case ERTM_REF:
		dds = &bs->ref;
		dds->out_state[channel] = (enable ? ERTM_RF_OUT_ON : ERTM_RF_OUT_OFF);
		break;
	default:
		errno = EINVAL;
		return ERTM_BAD_CONNECTOR;
	}
	return 0;
}

static int get_dds(struct ertm_status *handle,
		enum ertm_connector connector, struct ertm14_dds_state **dds)
{
	switch (connector) {
	case ERTM_LO:
		*dds = &handle->state->board_state.lo;
		break;
	case ERTM_REF:
		*dds = &handle->state->board_state.ref;
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
	struct ertm14_dds_state *dds;
	int err;

	if ((err = get_dds(handle, connector, &dds)) != 0) {
		errno = EINVAL;
		return err;
	}

	*power = dds->amp_power;
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
	struct ertm14_dds_state *dds;
	int i, err;

	if ((err = get_dds(handle, connector, &dds)) != 0) {
		errno = EINVAL;
		return err;
	}
	for (i = ERTM_LOREF_MIN_CH; i <= ERTM_LOREF_MAX_CH; i++) {
		if (valid_mask & (1<<i))
		    power[i] = dds->out_power[i];
	}

	return 0;
}

static double ampl_factor_to_float(uint8_t ampl_factor)
{
	return ampl_factor/256.0;
}

static uint8_t float_to_ampl_factor(double level)
{
	return (uint8_t)floor(level * 256);
}

int ertm_dds_set_level_adjust(struct ertm_status *handle,
		enum ertm_connector connector, double level)
{
	struct ertm14_dds_state *dds;
	int err;

	if ((err = get_dds(handle, connector, &dds)) != 0)
		return err;

	dds->ampl_factor = float_to_ampl_factor(level);
	return 0;
}

int ertm_dds_get_level_adjust(struct ertm_status *handle,
		enum ertm_connector connector, double *level)
{
	struct ertm14_dds_state *dds;
	int err;

	if ((err = get_dds(handle, connector, &dds)) != 0)
		return err;

	*level = ampl_factor_to_float(dds->ampl_factor);
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
