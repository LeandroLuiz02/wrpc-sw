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

int main(int argc, char *argv[])
{
	static char usb[] = "/dev/ttyUSB2";
	struct ertm_status *handle = ertm_init(NULL);

	if (handle == NULL) {
		fprintf(stderr, "could not open %s\n", usb);
		exit(1);
	}

	int timeout = 120;
	if( argc >= 2 )
		timeout = atoi(argv[1]);

	int good_samples = 0;

	for(;;)
	{
		ertm_wr_diags(handle, &handle->state->wr_status);

		uint32_t aux0_stat = handle->state->wr_status.WDIAG_AUX0_DETAIL_STAT;
		struct ertm_wr_status *st = &handle->state->wr_status;


		//printf("aux0: %08x\n", aux0_stat );

		if( aux0_stat & WRC_DIAGS_WDIAG_AUX0_DETAIL_STAT_LOCKED )
		{
			uint32_t phase = aux0_stat & 0xffffff;
			if(good_samples == 3 )
			{
				printf("[%d,%lld,%lld,%d,%d,%d]\n", phase,
					(( uint64_t) st->WDIAG_MU_MSB << 32 ) | st->WDIAG_MU_LSB,
					(( uint64_t) st->WDIAG_DMS_MSB << 32 ) | st->WDIAG_DMS_LSB,
					st->WDIAG_ASYM,
					st->WDIAG_CKO,
					st->WDIAG_SETP );
				break;
			}

			good_samples++;
			timeout--;

			if(!timeout)
			{
				printf("Timeout!\n");
				fflush(stdout);
				ertm_exit(handle);
				return 0;
			}
		}

		fflush(stdout);

		sleep(1);
	}

	ertm_exit(handle);

	return 0;
}

