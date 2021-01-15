/*
 * eRTM14/15 library (simulation mode)
 */

#include <limits.h>
#include <stdint.h>
#include "libertm.h"

/* translate enum to kHz if needed */
static clkab_freq_table[] = {
	[ERTM_CLKAB_1000MHz] = 1000000000,
	[ERTM_CLKAB_500MHz]  =  500000000,
	[ERTM_CLKAB_250MHz]  =  250000000,
	[ERTM_CLKAB_125MHz]  =  125000000,
	[ERTM_CLKAB_62_5MHz] =   62500000,
};
const clkab_nfreqs = sizeof(clkab_freq_table)/sizeof(clkab_freq_table[0]);

struct ertm_clk {
	uint32_t		enabled_mask;
	enum ertm_clkab_freq	chfreq[15];	/* only 4..14 legal */
	uint32_t		reserved[16];
};

struct ertm_lo_ref {
	uint32_t	enabled_mask;
	uint32_t	chfreq[15];	/* only 4..14 legal */
	double		chpower[15];	/* only 4..14 legal */
	uint32_t	pll_output_power;
	double		level_adjust;	/* full-scale DDS = 1.0 */
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

static struct ertm_state sim_state;
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

#if 0
int ertm_get_board_info(struct ertm_board_info *info);

/* all methods below have an implicit first arg struct ertm_status *
 * argument, omitted for brevity's sake
 */

#define ERTM_LO_DEFAULT_FREQ	0x3341BFBD	/* 200.222 MHz */
#define ERTM_REF_DEFAULT_FREQ	0x39374BC6	/* 223.499999 MHz */

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

#endif
