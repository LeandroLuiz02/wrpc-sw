/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __GENERIC_STORAGE_H
#define __GENERIC_STORAGE_H

/* Setup storage for an i2c eeprom on the fmc bus at address FMC_EEPROM_ADR
   Note: storage_mount() must be called after. */
void generic_board_i2c_storage(void);

/* Setup storage for a spi flash.  Sector size and sdbfd offset are read
   from syscon device (set by generics).
   Note: storage_mount() must be called after. */
void generic_board_spi_storage(void);

/* Try 1w, i2c or spi flash and then call storage_mount.
   Boards can either setup the storage by themselves, or call this function,
   or call one of the above function and call storage_mount(). */
int generic_board_storage_init(void);

#endif
