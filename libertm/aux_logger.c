/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright 2020-2021 CERN
 * Author: Juan David Gonzalez Cobas
 *
 * This program calls the library libertm to control an eRTM14/15 combo
 */

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "libertm.h"
#include "private.h"
#include "display.h"

/*
    char *ertm_perror(int error)
 */
static uint64_t get_tics(void)
{
	struct timezone tz = {0, 0};
	struct timeval tv;
	gettimeofday(&tv, &tz);

	return (uint64_t) tv.tv_sec * 1000000ULL + (uint64_t) tv.tv_usec;
}

int main(int argc, char *argv[])
{
	static char usb[] = "/dev/ttyUSB2";
	struct ertm_status *handle = ertm_init(NULL);
	int attempt;

	if (handle == NULL) {
		fprintf(stderr, "could not open %s\n", usb);
		exit(1);
	}

	for(;;)
	{
		ertm_wr_diags(handle, &handle->state->wr_status);

		uint32_t aux0_stat = handle->state->wr_status.WDIAG_AUX0_DETAIL_STAT;

		//printf("aux0: %08x\n", aux0_stat );

		if( aux0_stat & WRC_DIAGS_WDIAG_AUX0_DETAIL_STAT_LOCKED )
		{
			uint32_t phase = aux0_stat & 0xffffff;
			printf("[%-.20f,0,0,0]\n", (double)phase*1e-12);
		}

		fflush(stdout);

		sleep(1);
	}

	ertm_exit(handle);

	return 0;
}

