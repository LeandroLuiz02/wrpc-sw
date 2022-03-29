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
#include <storage.h>

#include "types.h"
#include <dev/fram.h>


struct storage_fram_priv
{
	struct fram_device *dev;
};

/* Functions for FRAM access */
static int sdb_fram_read(struct storage_device *dev, int offset, void *buf, int count)
{
	struct storage_fram_priv *priv = (struct storage_fram_priv* ) dev->priv;
	return fram_read( priv->dev , offset, buf, count);
}

static int sdb_fram_write(struct storage_device *dev, int offset, void *buf, int count)
{
	struct storage_fram_priv *priv = (struct storage_fram_priv* ) dev->priv;
	return fram_write(priv->dev, offset, buf, count);
}

static int sdb_fram_erase(struct storage_device *dev, int offset, int count)
{
	struct storage_fram_priv *priv = (struct storage_fram_priv* ) dev->priv;
	return fram_erase(priv->dev, offset, count);
}

const struct storage_rwops spi_fram_rwops = {
	sdb_fram_read,
	sdb_fram_write,
	sdb_fram_erase
};

