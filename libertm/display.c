#include <math.h>
#include <stdio.h>

#include "libertm.h"
#include "private.h"
#include <time.h>

static char *state_literal[] = {
	[ERTM_RF_OUT_ON] = "on",
	[ERTM_RF_OUT_OFF] = "off",
	[ERTM_RF_OUT_MONITOR] = "monitor",
};
/* FIXME: all these are repeated, same as above */
static double ampl_factor_to_float(uint8_t ampl_factor)
{
	return ampl_factor/256.0;
}

void display_dds_state(struct ertm14_dds_state *dds)
{
	int i;

	printf("ftw: %08x\n", dds->ftw);
	printf("level adjust: %0.4f (%d/256)\n", ampl_factor_to_float(dds->ampl_factor), dds->ampl_factor);
	printf("pll_out_power: %3.1f dBm (%08x mdBm)\n",  dds->amp_power/1000.0, dds->amp_power);
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++)
		printf("ch: %02d pow: %08x mdBm (%8.3f dBm)  %d %-8s\n",
		    i, dds->out_power[i], dds->out_power[i]/1000.0,
		    dds->out_state[i], state_literal[dds->out_state[i]]);
}

void display_ertm_clkab(struct ertm14_board_state *bs)
{
	int i;
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		char *aonoff = (bs->clka_enable_mask & (1<<i)) ? "on " : "off";
		char *bonoff = (bs->clkb_enable_mask & (1<<i)) ? "on " : "off";
		printf("CLKA%02d: %3s  %10dHz\t\t", i, aonoff, bs->clka_freq_hz[i]);
		printf("CLKB%02d: %3s  %10dHz\n", i, bonoff, bs->clkb_freq_hz[i]);
	}
}

void display_ertm_state(struct ertm_state *st)
{
	struct ertm14_board_state *bs = &st->board_state;

	printf("CLKAB: --------------------------------------------------\n");
	display_ertm_clkab(bs);
	printf("LO: --------------------------------------------------\n");
	display_dds_state(&bs->lo);
	printf("REF: --------------------------------------------------\n");
	display_dds_state(&bs->ref);
}

void display_wrc_diags(struct ertm_wr_status *diags)
{
	char fmt[] = "%-38s: 0x%08x\n";

	printf(fmt, "Version register", diags->VER);
	printf(fmt, "Ctrl", diags->CTRL);
	printf(fmt, "servo status", diags->WDIAG_SSTAT);
	printf(fmt, "Port status", diags->WDIAG_PSTAT);
	printf(fmt, "PTP state", diags->WDIAG_PTPSTAT);
	printf(fmt, "AUX state", diags->WDIAG_ASTAT);
	printf(fmt, "Tx PTP Frame cnts", diags->WDIAG_TXFCNT);
	printf(fmt, "Rx PTP Frame cnts", diags->WDIAG_RXFCNT);
	printf(fmt, "WRPC Diag:local time [msb of s]", diags->WDIAG_SEC_MSB);
	printf(fmt, "local time [lsb of s]", diags->WDIAG_SEC_LSB);
	printf(fmt, "local time [ns]", diags->WDIAG_NS);
	printf(fmt, "Round trip (mu) [msb of ps]", diags->WDIAG_MU_MSB);
	printf(fmt, "Round trip (mu) [lsb of ps]", diags->WDIAG_MU_LSB);
	printf(fmt, "Master-slave delay (dms) [msb of ps]", diags->WDIAG_DMS_MSB);
	printf(fmt, "Master-slave delay (dms) [lsb of ps]", diags->WDIAG_DMS_LSB);
	printf(fmt, "Total link asymmetry [ps]", diags->WDIAG_ASYM);
	printf(fmt, "Clock offset (cko) [ps]", diags->WDIAG_CKO);
	printf(fmt, "Phase setpoint (setp) [ps]", diags->WDIAG_SETP);
	printf(fmt, "Update counter (ucnt)", diags->WDIAG_UCNT);
	printf(fmt, "Board temperature [C degree]", diags->WDIAG_TEMP);
	
}

/* pulled from wrpc_diags.c */
static void print_servo_status(uint32_t val)
{
	static char *sstat_str[] = {
		"Not initialized",
		"Sync ns",
		"Sync TAI",
		"Sync phase",
		"Track phase",
		"Wait offset stable",
	};

	fprintf(stderr, "servo status:\t\t%s\n",
		sstat_str[val >> WRC_DIAGS_WDIAG_SSTAT_SERVOSTATE_SHIFT]);
}

static void print_port_status(uint32_t val)
{
	static int nbits = 2;
	static char *pstat_str[][2] = {
		//bit = 0     	 	bit = 1
		{"Link down", 		"Link up",},
		{"PLL not locked",	"PLL locked",},
	};
	int i, idx;

	fprintf(stderr, "Port status:\t\t");
	for (i = 0; i < nbits; ++i) {
		idx = (val & (1 << i)) ? 1 : 0;
		fprintf(stderr, "%s, ", pstat_str[i][idx]);
	}
	fprintf(stderr, "\n");
}

static void print_ptp_state(uint32_t val)
{
	static char *ptpstat_str[] = {
		"None",
		"PPS initializing",
		"PPS faulty",
		"disabled",
		"PPS listening",
		"PPS pre-master",
		"PPS master",
		"PPS passive",
		"PPS uncalibrated",
		"PPS slave",
	};

	fprintf(stderr, "PTP state:\t\t");
	if (val <= 9)
		fprintf(stderr, "%s", ptpstat_str[val]);
	else if (val >= 100 && val <= 116)
		fprintf(stderr, "WR STATES(see ppsi/ieee1588_types.h): %d", val);
	else
		fprintf(stderr, "Unknown");
	fprintf(stderr, "\n");
}

static void print_aux_state(uint32_t val)
{
	int nch = 8; //should be retrieved from a register
	int i;

	fprintf(stderr, "Aux state:\t\t");
	for (i = 0; i < nch; i++) {
		if (val & (1 << i))
			fprintf(stderr, "ch%d:enabled ", i);
	}
	fprintf(stderr, "\n");
}

static void print_tx_frame_count(uint32_t val)
{
	fprintf(stderr, "TX frame count:\t\t%d\n", val);
}

static void print_rx_frame_count(uint32_t val)
{
	fprintf(stderr, "RX frame count:\t\t%d\n", val);
}

static void print_local_time(uint32_t sec_msw, uint32_t sec_lsw, uint32_t ns)
{
	uint64_t sec = (uint64_t)(sec_msw) << 32 | sec_lsw;
//	fprintf(stderr, "TAI time:\t\t %" PRIu64 "sec %d nsec\n",
//		sec, ns);
	fprintf(stderr, "TAI time:\t\t%s", ctime((time_t *)&sec));
}

static void print_roundtrip_time(uint32_t msw, uint32_t lsw)
{
	uint64_t val = (uint64_t)(msw) << 32 | lsw;
	fprintf(stderr, "Round trip time:\t%" PRIu64 " ps\n", val);
}

static void print_master_slave_delay(uint32_t msw, uint32_t lsw)
{
	uint64_t val = (uint64_t)(msw) << 32 | lsw;
	fprintf(stderr, "Master slave delay:\t%" PRIu64 " ps\n", val);
}

static void print_link_asym(uint32_t val)
{
	fprintf(stderr, "Total Link asymmetry:\t%d ps\n", val);
}

static void print_clock_offset(uint32_t val)
{
	fprintf(stderr, "Clock offset:\t\t%d ps\n", val);
}

static void print_phase_setpoint(uint32_t val)
{
	fprintf(stderr, "Phase setpoint:\t\t%d ps\n", val);
}

static void print_update_counter(uint32_t val)
{
	fprintf(stderr, "Update counter:\t\t%d\n", val);
}

static void print_board_temp(uint32_t val)
{
	 fprintf(stderr, "temp:\t\t\t%d.%04d C\n", val >> 16,
	 	   (int)((val & 0xffff) * 10 * 1000 >> 16));
}

void display_wrc_diags_cooked(struct ertm_wr_status *diags)
{
	char fmt[] = "%-20s\t0x%08x\n";

	printf(fmt, "Version register", diags->VER);
	printf(fmt, "Ctrl", diags->CTRL);
	print_servo_status(diags->WDIAG_SSTAT);
	print_port_status(diags->WDIAG_PSTAT);
	print_ptp_state(diags->WDIAG_PTPSTAT);
	print_aux_state(diags->WDIAG_ASTAT);
	print_tx_frame_count(diags->WDIAG_TXFCNT);
	print_rx_frame_count(diags->WDIAG_RXFCNT);
	print_local_time(diags->WDIAG_SEC_MSB, diags->WDIAG_SEC_LSB, diags->WDIAG_NS);
	print_roundtrip_time(diags->WDIAG_MU_MSB, diags->WDIAG_MU_LSB);
	print_master_slave_delay(diags->WDIAG_DMS_MSB, diags->WDIAG_DMS_LSB);
	print_link_asym(diags->WDIAG_ASYM);
	print_clock_offset(diags->WDIAG_CKO);
	print_phase_setpoint(diags->WDIAG_SETP);                                     
	print_update_counter(diags->WDIAG_UCNT);
	print_board_temp(diags->WDIAG_TEMP);                                     
}

