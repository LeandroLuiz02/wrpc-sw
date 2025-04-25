/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2024-2025 Missing Link Electronics (www.missinglinkelectronics.com)
 * Author: Frederik Pfautsch <frederik.pfautsch@missinglinkelectronics.com>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef __SI5324_h
#define __SI5324_h

#include <stdint.h>
#include <stdio.h>

#include "dev/bb_i2c.h"

struct wr_si5324_interface_device
{
	uint8_t i2c_addr;
	struct i2c_bus *master;
};

void si5324_read( struct wr_si5324_interface_device *dev, uint8_t addr, uint8_t *data, int count );
void si5324_write( struct wr_si5324_interface_device *dev, uint8_t addr, uint8_t *data, int count );
void si5324_reset(struct wr_si5324_interface_device *dev );
int si5324_set_bypass( struct wr_si5324_interface_device *dev );
int si5324_set_125m_freerun( struct wr_si5324_interface_device *dev );
int si5324_set_125m_clean( struct wr_si5324_interface_device *dev );

void wr_si5324_interface_init( struct wr_si5324_interface_device *dev, struct i2c_bus *master, uint8_t i2c_addr );

#endif // __SI5324_h
