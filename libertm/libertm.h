/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright 2020-2021 CERN
 * Author: Juan David Gonzalez Cobas
 *
 * This library interacts with a simulated eRTM14/15 combo
 */

#ifndef _LIBERTM_H_
#define _LIBERTM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* error codes */
#define ERTM_OK		0
#define ERTM_BAD_CONNECTOR	(-1)
#define ERTM_CH_OUT_OF_RANGE	(-2)
#define ERTM_NOT_IMPLEMENTED	(-3)
#define ERTM_UART_LINK_SEND_ERR (-4)
#define ERTM_UART_LINK_RECV_ERR (-5)
#define ERTM_BAD_HANDLE		(-6)

struct ertm_error_codes {
	int	code;
	char	*message;
};

extern struct ertm_error_codes ertm_error_codes[];

extern char *ertm_perror(int error);

/* board constant of nature definitions */
enum ertm_clkab_freq {
	ERTM_CLKAB_1000MHz,
	ERTM_CLKAB_500MHz,
	ERTM_CLKAB_250MHz,
	ERTM_CLKAB_125MHz,
	ERTM_CLKAB_62_5MHz,
};

enum ertm_connector {
	ERTM_CLKA,
	ERTM_CLKB,
	ERTM_LO,
	ERTM_REF,
};

/* MONITOR is OFF to all effects and purposes */
#define	ERTM_RF_OUT_ON		ERTM15_RF_OUT_ON
#define	ERTM_RF_OUT_OFF		ERTM15_RF_OUT_OFF
#define	ERTM_RF_OUT_MONITOR	ERTM15_RF_OUT_MONITOR

/* real available channels in connectors */
#define ERTM_CLKAB_MIN_CH	ERTM14_CLKAB_OUT_MIN_ID
#define ERTM_CLKAB_MAX_CH	14
			/* FIXME: ERTM14_CLKAB_OUT_MAX_ID is 15??*/
#define ERTM_LOREF_MIN_CH	ERTM14_RF_OUT_MIN_ID
#define ERTM_LOREF_MAX_CH	ERTM14_RF_OUT_MAX_ID

/* firmware metadata according to The Convention (see
 * https://www.ohwr.org/project/fpga-dev-id/blob/master/device-structure.rst
 * probably, only version and source_id are useful here
 */
struct ertm_device_metadata {
	uint32_t	vendor_id;
	uint32_t	device_id;
	uint32_t	version;
	uint32_t	byte_order_mark;
	unsigned char	source_id[16];
	uint32_t	capability_mask;
	unsigned char	vendor_uuid[16];
};

struct ertm_board_info {
	uint64_t	ertm14_storage;
	uint64_t	ertm14_mac1;
	uint64_t	ertm14_mac2;
	uint64_t	ertm15;
	char		firmware_version[32];
	char		wrpc_sw_version[32];
        char		wrpc_sw_commit_id[32];
        char		wrpc_sw_build_date[16];
        char		wrpc_sw_build_time[16];
        char		wrpc_sw_build_by[32];
	struct ertm_device_metadata
			firmware_metadata;
};

struct ertm_temperatures {	/* celsius SVP */
	double	fpga;			/* eRTM 14 I2C temp  0x49 */
	double	power_supplies14;	/* eRTM 14 I2C temp  0x48 */
	double	dds_lo;			/* eRTM 15 I2C temp1 0x4a */
	double	dds_ref;		/* eRTM 15 I2C temp1 0x4d */
	double	lo_amp;			/* eRTM 15 I2C temp1 0x49 */
	double	ltc6150;		/* eRTM 15 I2C temp1 0x4b */
	double	ocxo_near;		/* eRTM 15 I2C temp1 0x4e */
	double	ocxo_under;		/* eRTM 15 I2C temp1 0x4f */
	double	power_supplies15;	/* eRTM 15 I2C temp1 0x48 */
	double	ref_amp;		/* eRTM 15 I2C temp1 0x4c */
	double	clka;			/* eRTM 15 I2C temp2 0x4c */
	double	clkb;			/* eRTM 15 I2C temp2 0x49 */
	double  unused[4];		/* future extensions */
};

struct ertm_voltages {		/* volts SVP */
	double	p12v_ertm15;
	double	p3v3_ertm15;
	double	pocxo;
	double	p9v0_lo;
	double	p9v0_ref;
	double	ocxo_curr;
	double	p12v_ertm14;
	double	p3v3_ertm14;
	double	unused[16];
};

struct ertm_nco_reset {
	int		enabled;
	int		subscribed;
	uint32_t	current_stream_id;
	uint32_t	rx_count;
	uint32_t	reset_count;
	uint32_t	unused[8];
};

/* as a general rule, all methods in libertm return an integer exit
 * code 0 in case of success and < 0 in case of error, the type of error
 * mapped to an errno value
 */

/* FIXME: address shall define uniquely a ttyUSB -> UART, an IP address
 * in the WR network or further unique address of eRTM/host
 * The handle is an opaque pointer to keep status of the connection
 */
struct ertm_status;
struct ertm_status *ertm_init(char *address);
void ertm_exit(struct ertm_status *handle);		/* end connection, destroy handle */

int ertm_get_board_info(struct ertm_status *handle,
			struct ertm_board_info *info);

/* all methods below have an implicit first arg struct ertm_status *
 * argument, omitted for brevity's sake
 */

#define ERTM_LO_DEFAULT_FREQ	0x3341BFBD	/* 200.222 MHz */
#define ERTM_REF_DEFAULT_FREQ	0x39374BC6	/* 223.499999 MHz */
#define	ERTM_CLKAB_DEFAULT_FREQ	ERTM_CLKAB_125MHz	/* ditto */

/* connector can be any of ERTM_{CLKA,CLKB,REF,LO}. For REF and LO, the
 * channel parameter is ignored; for CLKA/CLKB, the freq parameter is
 * one of the ertm_clkab_freq values, while for REF/LO, it is an actual
 * uint32_t where 2**32 = 1GHz
 */
int ertm_get_freq(struct ertm_status *handle,
		enum ertm_connector connector, int channel, uint32_t *freq);
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

#ifdef __cplusplus
}
#endif

#endif /* _LIBERTM_H_ */
