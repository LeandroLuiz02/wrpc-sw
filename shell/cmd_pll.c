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
