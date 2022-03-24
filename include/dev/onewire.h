/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __ONEWIRE_H
#define __ONEWIRE_H

#define ONEWIRE_PORT 0

#define MAX_DEV1WIRE 8

#define FOUND_DS18B20 0x01

void own_scanbus(uint8_t portnum);
int16_t own_readtemp(uint8_t portnum, int16_t * temp, int16_t * t_frac);
/* 0 = success, -1 = error */

#endif /* __ONEWIRE_H */
