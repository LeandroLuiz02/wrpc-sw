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

struct storage_w1_priv
{
	struct w1_bus *dev;
};

/* The methods for W1 access */
static int sdb_w1_read(struct storage_device *dev, int offset, void *buf, int count)
{
	struct storage_w1_priv *priv = (struct storage_w1_priv* ) dev->priv;
	return w1_read_eeprom_bus(priv->dev, offset, buf, count);
}

static int sdb_w1_write(struct storage_device *dev, int offset, void *buf, int count)
{
	struct storage_w1_priv *priv = (struct storage_w1_priv* ) dev->priv;
	return w1_write_eeprom_bus(priv->dev, offset, buf, count);
}

static int sdb_w1_erase(struct storage_device *dev, int offset, int count)
{
	struct storage_w1_priv *priv = (struct storage_w1_priv* ) dev->priv;
	return w1_erase_eeprom_bus(priv->dev, offset, count);
}

const struct storage_rwops spi_w1_rwops = {
	sdb_w1_read,
	sdb_w1_write,
	sdb_w1_erase
};

