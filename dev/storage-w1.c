/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012, 2013 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <errno.h>
#include <wrc.h>
#include <dev/w1.h>
#include <storage.h>

/* The methods for W1 access */

const struct storage_rwops spi_w1_rwops = {
	(void *)w1_read_eeprom_bus,
	(void *)w1_write_eeprom_bus,
	(void *)w1_erase_eeprom_bus
};
