/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <string.h>
#include <errno.h>

#include "pp-printf.h"
#include "shell.h"
#include "dev/syscon.h"
#include "storage.h"
#include <dev/flash.h>
#include "util.h"
#include "wrc.h"

/*
 * args[1] - where to write sdbfs image (0 - Flash, 1 - I2C EEPROM,
 *		2 - 1Wire EEPROM)
 * args[2] - base address for sdbfs image in Flash/EEPROM
 * args[3] - i2c address of EEPROM or blocksize of Flash
 */

static const char * const sdb_cmds[] =
{
	 [0] = "format",
	 [1] = "fs",
	 [2] = "fse",
	 [3] = "ls",
};

static int cmd_sdb(const char *args[])
{
	int icmd;

	icmd = sub_cmd(sdb_cmds, ARRAY_SIZE(sdb_cmds), args);

	switch(icmd) {
	case 0:
	case 1:
	{
		pp_printf("Formatting using ");

		if (!args[1]) {
			pp_printf("default location\n");
			storage_sdbfs_format( &wrc_storage_dev, 0, 0 );
		} else {
			uint32_t base = atoi(args[1]);
			pp_printf("location 0x%X\n", (unsigned int) base);
			storage_sdbfs_format( &wrc_storage_dev, base, 1 );
		}
		return 0;
	}
	case 2:
	{
		pp_printf("Erasing using ");
		if (!args[1]) {
			pp_printf("default location\n");
			storage_sdbfs_erase( &wrc_storage_dev, 0, 0 );
		} else {
			uint32_t base = atoi(args[1]);
			pp_printf("location 0x%X\n", (unsigned int) base);
			storage_sdbfs_erase( &wrc_storage_dev, base, 1 );
		}
		return 0;
	}
	case 3:
		storage_sdbfs_list();
		return 0;
	default:
		return icmd;
	}
}

DEFINE_WRC_COMMAND(sdb) = {
	.name = "sdb",
	.exec = cmd_sdb,
};
