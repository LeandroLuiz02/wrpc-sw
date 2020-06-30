/*
 *
 */

#include <stdint.h>

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

/* firmware metadata according to The Convention (see
 * https://www.ohwr.org/project/fpga-dev-id/blob/master/device-structure.rst
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

struct ertm_voltages {
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


/* FIXME: error handling via errno, specific lib codes, or some
   other schema?
   Here, all functions return an error code. I vote for < 0 plus
   errno, specific error codes, or a status in the handle, in this order
/* FIXME: address shall define uniquely a ttyUSB -> UART, an IP address
 * in the WR network or further unique address of eRTM/host
 * The handle is an opaque pointer to keep status of the connection
 */
/* FIXME: locking? */
struct ertm_status;
struct ertm_status *ertm_init(char *address);
void ertm_exit(struct ertm_status *handle);		/* end connection, destroy handle */

int ertm_get_board_info(struct ertm_board_info *info);

/* all methods below have an implicit first arg struct ertm_status *
 * argument, omitted for brevity's sake
 */

#define ERTM_LO_DEFAULT_FREQ	0x3341BFBD	/* 200.222 MHz */
#define ERTM_REF_DEFAULT_FREQ	0x39374BC6	/* 223.499999 MHz */

/* clock distribution properties */
int ertm_set_clka_freq(enum ertm_clkab_freq freq);		/* better with a narrow interface FIXME */
/* FIXME: add channel parameter, it's channelwise */
int ertm_get_clka_freq(enum ertm_clkab_freq *freq);
int ertm_set_clkb_freq(enum ertm_clkab_freq freq);
int ertm_get_clkb_freq(enum ertm_clkab_freq *freq);

/* RF distribution properties */
int ertm_set_lo_freq(uint32_t freq);				/* not per channel, all 9ch created equal */
int ertm_get_lo_freq(uint32_t *freq);				/* not per channel, all 9ch created equal */
int ertm_lo_channel_enable(int channel, int enable);		/* default disabled */
int ertm_lo_set_level_adjust(int channel, double level);	/* level in [0,1] FIXME per channel? */
int ertm_lo_get_power(double *power);				/* power level in dBm */
int ertm_lo_get_channel_power(int channel, double *power);	/* power per channel in dBm */
int ertm_lo_power_channel_select(int slot);			/* slot in 4..12 */
	/* FIXME: duplicated entry in table for the above function? */

int ertm_set_ref_freq(uint32_t freq);				/* not per channel, all 9ch created equal FIXME */
int ertm_get_ref_freq(uint32_t *freq);				/* not per channel, all 9ch created equal FIXME */
int ertm_ref_channel_enable(int channel, int enable);		/* default disabled */
int ertm_ref_get_power(double *power);				/* power level in dBm */
int ertm_ref_get_channel_power(int channel, double *power);	/* power per channel in dBm */

int ertm_dds_set_level_adjust(int channel, double level);	/* level in [0,1] per channel? */
/* RF distribution properties, alio modo */
int ertm_set_freq(enum ertm_connector conn, uint32_t freq);	/* conn = CLKA,CLKB,LO,REF summarize 12 functions FIXME */
int ertm_get_freq(enum ertm_connector conn, uint32_t *freq);
int ertm_channel_enable(enum ertm_connector, int channel, int enable);	
int ertm_set_level_adjust(int channel, double level);

/* monitoring */
int ertm_get_ocxo_current(double *current);			/* amperes */
int ertm_get_temperatures(struct ertm_temperatures *temps);	/* all celsius */
int ertm_get_voltages(struct ertm_voltages *volts);		/* all volt */

/* system-wide actions */
int ertm_rf_nco_reset_enable(int enable);		/* default disabled */
int ertm_rf_nco_reset(void);				/* do a reset */

/* suggested by Tom */
int ertm_rf_nco_reset_subscribe(enum ertm_connector, int enable, int channel, uint32_t stream_id); /* default disabled */
// get status (currently subscribed ID, rx count, reset count)
int ertm_rf_nco_reset_get_status(status);

/* WR enable/diagnostics */
struct ertm_wr_status;					/* to be defined with rabbits */
int ertm_wr_enable(int enable);				/* free-running OCXO if disabled */
// add auxiliaries for diagnostics of basics of wr link/lock
int ertm_wr_diags(struct ertm_wr_status);		/* to be defined with rabbits */


