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
#include "libertm.h"
#include "private.h"
#include "display.h"

/*
    char *ertm_perror(int error)
 */

int main(int argc, char *argv[])
{
	static char usb[] = "/dev/ttyUSB2";
	struct ertm_status *handle = ertm_init(usb);

	if (handle == NULL) {
		fprintf(stderr, "could not open %s\n", usb);
		exit(1);
	}
	ertm_get_board_config(handle, &handle->state->board_state);
	display_ertm_state(handle->state);
    	ertm_wr_diags(handle, &handle->state->wr_status);
	display_wrc_diags(&handle->state->wr_status);

	ertm_exit(handle);

	return 0;
}

