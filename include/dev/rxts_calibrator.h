/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __RXTS_CALIBRATOR_H
#define __RXTS_CALIBRATOR_H

extern uint32_t cal_phase_transition;

void rxts_calibration_start(void);
int measure_t24p(void);
int calib_t24p(int mode, uint32_t *value);

#endif
