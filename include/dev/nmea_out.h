/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __NMEA_OUT_H
#define __NMEA_OUT_H

#include <stdint.h>
#include <hw/nmea_master.h>

void nmea_out_init(struct nmea_master *dev, uint32_t baudrate, uint32_t invert);
int nmea_out_set_baud(struct nmea_master *dev, uint32_t baudrate);
void nmea_out_set_invert(struct nmea_master *dev, int invert);
int nmea_out_get_invert(struct nmea_master *dev);
void nmea_out_get_status(struct nmea_master *dev, int *valid, int *tip);
void nmea_out_get_tod(struct nmea_master *dev, int *hour, int *min, int *sec);
void nmea_out_get_date(struct nmea_master *dev, int *day, int *month, int *year);

#endif
