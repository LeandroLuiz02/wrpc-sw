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

#include "types.h"
#include "dev/spi_flash.h"

static const int32_t spi_flash_default_entry_points[] =
{
				0x000000,	/* flash base */
				0x100,		/* second page in flash */
				0x200,		/* IPMI with MultiRecord */
				0x300,		/* IPMI with larger MultiRecord */
				0x170000,	/* after first FPGA bitstream */
				0x2e0000,	/* after MultiBoot bitstream */
				0x600000,	/* after SVEC AFPGA bitstream */
				-1 };

/* Functions for Flash access */
static int sdb_flash_read(struct storage_device *dev, int offset, void *buf, int count)
{
	struct spi_flash_device *priv = (struct spi_flash_device* ) dev->priv;
	return spi_flash_read( priv ,offset, buf, count);
}

static int sdb_flash_write(struct storage_device *dev, int offset, void *buf, int count)
{
	struct spi_flash_device *priv = (struct spi_flash_device* ) dev->priv;
	return spi_flash_write( priv, offset, buf, count);
}

static int sdb_flash_erase(struct storage_device *dev, int offset, int count)
{
	struct spi_flash_device *priv = (struct spi_flash_device* ) dev->priv;
	return spi_flash_erase( priv, offset, count);
}

const struct storage_rwops spi_flash_rwops = {
	sdb_flash_read,
	sdb_flash_write,
	sdb_flash_erase
};

void storage_spiflash_create(struct storage_device *dev, struct spi_flash_device *flash)
{
	static const char* spi_flash_str = "spi-flash";
	dev->name = (char *) spi_flash_str;
	dev->priv = flash;
	dev->rwops = (struct storage_rwops *) &spi_flash_rwops;
	dev->size = flash->size;
	dev->cfg_entry = flash->cfg_entry;
	dev->block_size = flash->sector_size;
	dev->entry_points = (int32_t *) spi_flash_default_entry_points;
	dev->flags = STORAGE_FLAG_DEVICE_OK;
}
