/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>

#include "softpll_ng.h"
#include "shell.h"

/* The sub-commands.  */
static const char * const pll_menu[] =
{
	[0] = "init",
	[1] = "cl",
	[2] = "stat",
	[3] = "sps",
	[4] = "gps",
	[5] = "start",
	[6] = "stop",
	[7] = "sdac",
	[8] = "gdac",
	[9] = "checkvco"
};

/* Number of arguments for the sub-commands.  Mind the order!  */
static const unsigned char nargs[] =
{
	[0] = 3,
	[1] = 1,
	[2] = 0,
	[3] = 2,
	[4] = 1,
	[5] = 1,
	[6] = 1,
	[7] = 2,
	[8] = 1,
	[9] = 0
};


static int calc_apr(int meas_min, int meas_max, int f_center )
{
	// apr_min is in PPM

	int64_t delta_low =  meas_min - f_center;
	int64_t delta_hi = meas_max - f_center;
	uint64_t u_delta_low, u_delta_hi;
	int ppm_lo, ppm_hi;

	if(delta_low >= 0)
		return -1;
	if(delta_hi <= 0)
		return -1;

	/* __div64_32 divides 64 by 32; result is in the 64 argument. */
	u_delta_low = -delta_low * 1000000LL;
	__div64_32(&u_delta_low, f_center);
	ppm_lo = (int)u_delta_low;

	u_delta_hi = delta_hi * 1000000LL;
	__div64_32(&u_delta_hi, f_center);
	ppm_hi = (int)u_delta_hi;

	return ppm_lo < ppm_hi ? ppm_lo : ppm_hi;
}

static void check_vco_frequencies(void)
{
	//disable_irq();

	int f_min, f_max;
	pp_printf("SoftPLL VCO Frequency/APR test:\n");

	spll_set_dac(-1, 0);
	f_min = spll_measure_frequency(SPLL_OSC_DMTD);
	spll_set_dac(-1, 65535);
	f_max = spll_measure_frequency(SPLL_OSC_DMTD);
	pp_printf("DMTD VCO:  Low=%d Hz Hi=%d Hz, APR = %d ppm.\n", f_min, f_max, calc_apr(f_min, f_max, 62500000));

	spll_set_dac(0, 0);
	f_min = spll_measure_frequency(SPLL_OSC_REF);
	spll_set_dac(0, 65535);
	f_max = spll_measure_frequency(SPLL_OSC_REF);
	pp_printf("REF VCO:   Low=%d Hz Hi=%d Hz, APR = %d ppm.\n", f_min, f_max, calc_apr(f_min, f_max, REF_CLOCK_FREQ_HZ));

	f_min = spll_measure_frequency(SPLL_OSC_EXT);
	pp_printf("EXT clock: Freq=%d Hz\n", f_min);
}

static int cmd_pll(const char *args[])
{
	unsigned narg;
	int vals[8];
	int icmd;

	icmd = sub_cmd(pll_menu, ARRAY_SIZE(pll_menu), args);

	/* Decode arguments.  */
	for (narg = 1; args[narg]; narg++)
		vals[narg] = atoi(args[narg]);

	/* Args from 1 to NARG.  */
	narg--;

	if (icmd < 0 || nargs[icmd] != narg)
		return -EINVAL;

	switch (icmd) {
	case 0:
		spll_init(vals[1], vals[2], vals[3]);
		return 0;
	case 1:
		pp_printf("%d\n", spll_check_lock(vals[1]));
		return 0;
	case 2:
		spll_show_stats();
		return 0;
	case 3:
		spll_set_phase_shift(vals[1], vals[2]);
		return 0;
	case 4:
	{
		int32_t cur, tgt;
		spll_get_phase_shift(vals[1], &cur, &tgt);
		pp_printf("%d %d\n", (int) cur, (int) tgt);
		return 0;
	}
	case 5:
		spll_start_channel(vals[1]);
		return 0;
	case 6:
		spll_stop_channel(vals[1]);
		return 0;
	case 7:
		spll_set_dac(vals[1], vals[2]);
		return 0;
	case 8:
		pp_printf("%d\n", spll_get_dac(vals[1]));
		return 0;
	case 9:
		check_vco_frequencies();
		return 0;
	default:
		return 0;
	}
}

DEFINE_WRC_COMMAND(pll) = {
	.name = "pll",
	.exec = cmd_pll,
};
