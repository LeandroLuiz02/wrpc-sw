/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __BOARD_STATE_ERTM14_H
#define __BOARD_STATE_ERTM14_H

#define ERTM14_RF_OUT_MIN_ID 4
#define ERTM14_RF_OUT_MAX_ID 12

#define ERTM14_CLKAB_OUT_MIN_ID 4
#define ERTM14_CLKAB_OUT_MAX_ID 15

struct ertm14_dds_state
{
    uint32_t ftw;
    uint8_t out_state[ERTM14_RF_OUT_MAX_ID + 1];
    int out_power[ERTM14_RF_OUT_MAX_ID + 1];
    int amp_power;
    int ampl_factor;
    int sync_source;
    int sync_count;
};

struct ertm14_board_state
{
    int valid;
    struct ertm14_dds_state ref;
    struct ertm14_dds_state lo;
    uint32_t clka_freq_hz[ERTM14_CLKAB_OUT_MAX_ID + 1];
    uint32_t clkb_freq_hz[ERTM14_CLKAB_OUT_MAX_ID + 1];
    uint32_t clka_enable_mask;
    uint32_t clkb_enable_mask;
};

#endif /*  __BOARD_STATE_ERTM14_H */
