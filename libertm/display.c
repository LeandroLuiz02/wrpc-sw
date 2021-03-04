#include <math.h>
#include <stdio.h>

#include "libertm.h"
#include "private.h"

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
