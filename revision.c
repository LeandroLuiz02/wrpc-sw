/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 - 2015 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "softpll_ng.h"
#include "revision.h"
const char *const build_revision = stats.commit_id;

const char *const build_date = stats.build_date;
const char *const build_time = stats.build_time;
const char *const build_by = stats.build_by;

struct spll_stats stats = {
	.magic = 0x5b1157a7,
	.ver = SPLL_STATS_VER,
#ifdef CONFIG_DETERMINISTIC_BINARY
	.build_date = "",
	.build_time = "",
	.build_by = "",
#else
	.build_date = __DATE__,
	.build_date[sizeof(stats.build_date) - 1] = 0,
	.build_time = __TIME__,
	.build_time[sizeof(stats.build_time) - 1] = 0,
	.build_by = __GIT_USR__,
	.build_by[sizeof(stats.build_by) - 1] = 0,
#endif
	.commit_id = __GIT_VER__,
	.commit_id[sizeof(stats.commit_id) - 1] = 0,
};
