/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __BOARD_STATE_ERTM14_H
#define __BOARD_STATE_ERTM14_H

/* beware this header: it is only required for WRC_DIAG_WB,
 * which is not here (yet), and it unconditionally defines
 * the wretched PACKED macro
 * Dependencies, Makefiles and header order suffer accordingly
 */
#include "hw/wrc_diags_regs.h"
#include "ertm15_rf_distr.h"

#define ERTM14_RF_OUT_MIN_ID 4
#define ERTM14_RF_OUT_MAX_ID 12

#define ERTM14_CLKAB_OUT_MIN_ID 4
#define ERTM14_CLKAB_OUT_MAX_ID 15

#define ERTM14_MAX_SENSORS_COUNT 21

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

PACKED struct ertm14_mmc_version_info
{
    char git_tag[32];
    char git_sha[32];
    uint32_t build_date;
};

PACKED struct ertm14_mmc_sensor_state
{
    uint8_t flags;
    uint8_t id;
    uint16_t value;
};

PACKED struct ertm14_mmc_state
{
    struct ertm14_mmc_version_info info;
    struct ertm14_mmc_sensor_state sensors[ERTM14_MAX_SENSORS_COUNT];
};

/* FIXME: this is not the best place for this declaration */
int wrc_diags_dump(struct WRC_DIAGS_WB *buf);

#endif /*  __BOARD_STATE_ERTM14_H */
