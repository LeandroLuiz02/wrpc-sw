/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012-2020 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __I2C_EEPROM_H
#define __I2C_EEPROM_H

#include "board.h"

/*
 * Some EEPROMs use the I2C single byte address
 * scheme, but provide more memory than addressable
 * by this single byte address. To make the
 * additional memory space available virtual I2C
 * devices are mapped onto the memory space similar
 * to paging techniques.
 *
 * The EEPROM device m24c08 on the ZCU102 platform
 * is one example. It maps the full address space
 * spanning 10 bits to 4 virtual devices (2 address
 * bits) with an address space of 8 bits each.
 * To support this addressing scheme the meaning of
 * the dev.offset_bytes field is modified:
 * dev.offset_bytes = 2: payload data is place at
 * Byte 2 ff in the I2C data stream and 2 address
 * bytes are used.
 * dev.offset_bytes = !2: payload data is placed
 * at Byte 1 ff in the I2C data stream and single
 * byte address is used
 * dev.offset_bytes = -<bw>: a negative value
 * indicates single byte addressing and the
 * abs(dev.offset_bytes) denotes the number of Bits
 * used of the address exceeding the single byte
 * address, being mapped onto the device address to
 * select the virtual devices.
 *
 * This adjustment needs to be done for every byte
 * access to the device as one multi-byte access can
 * span multiple virtual devices.
 */
struct i2c_eeprom_device {
    struct i2c_bus *bus;
    uint8_t addr;
    int offset_bytes;
};


int i2c_eeprom_create( struct i2c_eeprom_device *dev, struct i2c_bus *bus, uint8_t i2c_addr, int offset_bytes );
int i2c_eeprom_read(struct i2c_eeprom_device *dev, int offset, void *buf, int count);
int i2c_eeprom_write(struct i2c_eeprom_device *dev, int offset, void *buf, int count);
int i2c_eeprom_erase(struct i2c_eeprom_device *dev, int offset, int count);

#endif
