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

struct ertm_lib_version {
	char *lib_version;
	char *git_commit;
	char *git_user;
	char *git_url;
};

/* error codes */
#define ERTM_OK		0
#define ERTM_BAD_CONNECTOR	(-1)
#define ERTM_CH_OUT_OF_RANGE	(-2)
#define ERTM_NOT_IMPLEMENTED	(-3)
#define ERTM_UART_LINK_SEND_ERR (-4)
#define ERTM_UART_LINK_RECV_ERR (-5)
#define ERTM_BAD_HANDLE		(-6)
#define ERTM_BAD_OPCODE		(-7)
#define	ERTM_UART_PROTO_ERR	(-8)
#define	ERTM_BAD_CLKAB_FREQ	(-9)
#define	ERTM_BAD_SYNC_SOURCE	(-10)

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
#define ERTM_CLKAB_MAX_CH	ERTM14_CLKAB_OUT_MAX_ID
#define ERTM_LOREF_MIN_CH	ERTM14_RF_OUT_MIN_ID
#define ERTM_LOREF_MAX_CH	ERTM14_RF_OUT_MAX_ID

/* sync states of DDS and CLKA/B channels */

#define	ERTM_SYNC_STATE_RESTART      ERTM14_CLK_SYNC_STATE_RESTART
#define	ERTM_SYNC_STATE_WAIT_TIMING  ERTM14_CLK_SYNC_STATE_WAIT_TIMING
#define	ERTM_SYNC_STATE_CONFIGURE    ERTM14_CLK_SYNC_STATE_CONFIGURE
#define	ERTM_SYNC_STATE_WAIT_TRIGGER ERTM14_CLK_SYNC_STATE_WAIT_TRIGGER
#define	ERTM_SYNC_STATE_READY        ERTM14_CLK_SYNC_STATE_READY

struct ertm_sync_states {
	int	sync_state;
	char	*label;
	char	*description;
};
extern struct ertm_sync_states ertm_sync_states[];

/* WR enable/disable modes */
#define	ERTM_WR_MASTER		WRC_MODE_MASTER
#define	ERTM_WR_SLAVE		WRC_MODE_SLAVE
#define	ERTM_WR_FREE_RUNNING	WRC_MODE_UNKNOWN	/* disable = 0 */
#define	ERTM_WR_OCXO		ERTM_WR_FREE_RUNNING

/* library operation modes:
 *
 *  ERTM_DEFERRED	operations deferred (cached) until ertm_commit() is called
 *  ERTM_IMMEDIATE	config operations executed synchronously
 *  ERTM_OPTIMIZED	same as ERTM_IMMEDIATE, but UART comm sped up (not implemented)
 *  ERTM_SIMULATED	no interaction with actual hardware
 */
#define	ERTM_DEFERRED	1
#define	ERTM_IMMEDIATE	2
#define	ERTM_OPTIMIZED	3
#define	ERTM_SIMULATED	4

/* firmware metadata according to The Convention (see
 * https://www.ohwr.org/project/fpga-dev-id/blob/master/device-structure.rst
 * probably, only version and source_id are useful here
 */
struct ertm_device_metadata {
	union {
	    char	fpga_build_info[256];
	    struct {
		uint32_t	vendor_id;
		uint32_t	device_id;
		uint32_t	version;
		uint32_t	byte_order_mark;
		unsigned char	source_id[16];
		uint32_t	capability_mask;
		unsigned char	vendor_uuid[16];
		char		fpga_buildinfo_text[200];
	    };
	};
};

struct ertm_board_info {
	/* module serials and MACs */
	char		ertm14_serial[32];
	char		ertm15_serial[32];
	union {
	    uint8_t	ertm14_mac1_bytes[8];
	    uint64_t	ertm14_mac1;
	};
	union {
	    uint8_t	ertm14_mac2_bytes[8];
	    uint64_t	ertm14_mac2;
	};
	/* wrpc-sw version lore */
        char		wrpc_sw_commit_id[32];
        char		wrpc_sw_build_date[16];
        char		wrpc_sw_build_time[16];
        char		wrpc_sw_build_by[32];
	/* MMC firmware versions */
	char		ertm14_firmware_version[32];
	char		ertm15_firmware_version[32];
	uint32_t        calibration_date;

	/* unused */
	struct ertm_device_metadata
			firmware_metadata;
};

/* a bad sensor value */
#define ERTM_MINUS_INFINITY (-1.0e9)

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
	double	ocxo_curr;	/* this is a bizarre place for mA */
	double	p12v_ertm14;
	double	p3v3_ertm14;
	double	unused[16];
};

struct ertm_nco_reset {
	int		enabled;
	union {
		int	subscribed;
		int	sync_source;
	};
	uint32_t	current_stream_id;
	uint32_t	rx_count;
	uint32_t	rx_timeouts;
	uint32_t	reset_count;
	uint32_t	connector;
	uint32_t	unused[7];
};

struct ertm_nco_status {
	union {
	    struct ertm_nco_reset nco_status[2];
	    struct {
		struct ertm_nco_reset lo;
		struct ertm_nco_reset ref;
	    };
	};
};

struct ertm_wr_status {
	/* FIXME: copied, not #include'd, from wrc_diags_regs.h */
	/* this is an alias of struct WRC_DIAGS_WB */
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

/* as a general rule, all methods in libertm return an integer exit
 * code 0 in case of success and < 0 in case of error, the type of error
 * mapped to an errno value
 */

/* FIXME: address shall define uniquely a ttyUSB -> UART, an IP address
 * in the WR network or further unique address of eRTM/host
 * The handle is an opaque pointer to keep status of the connection
 */
struct ertm_status;
struct ertm_status *ertm_init(const char *address);
void ertm_exit(struct ertm_status *handle);		/* end connection, destroy handle */
struct ertm_lib_version *ertm_lib_version(void);

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
int ertm_nco_reset_get_status(struct ertm_status *handle, struct ertm_nco_reset status[]);

/* WR enable/diagnostics */
struct ertm_wr_status;						/* to be defined with rabbits */
int ertm_wr_enable(struct ertm_status *handle, int enable);	/* free-running OCXO if disabled */
int ertm_wr_status(struct ertm_status *handle, int *link_up, int *is_locked);
int ertm_wr_diags(struct ertm_status *handle, struct ertm_wr_status *status);

/* streamer latency and timeout settings */
int ertm_set_streamers_latency(struct ertm_status *handle, uint32_t cycles16n);
int ertm_set_streamers_timeout(struct ertm_status *handle, uint32_t cycles16n);
int ertm_get_streamers_latency_timeout(struct ertm_status *handle,
	    uint32_t *latency_cycles16n, uint32_t *timeout_cycles16n);
					/* all in 16ns-cycle units */
#ifdef __cplusplus
}
#endif

#endif /* _LIBERTM_H_ */
