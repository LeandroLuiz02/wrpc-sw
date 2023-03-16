/*
 * We build for both wr-switch and wr-node (core).
 *
 * Unfortunately, our submodules include <board.h> without using
 * our own Kconfig defines. Thus, assume wr-node unless building
 * specifically for wr-switch (which doesn't refer to submodules).
 * Same appplies to ./tools/, where we can avoid a Makefile
 * patch for add "-include ../include/generated/autoconf.h"
 *
*/

#ifndef __BOARD_H
#define __BOARD_H

#include <hw/rawmem.h>

#ifdef CONFIG_ARCH_RISCV
    #define DEV_BASE	0x100000
#elif defined CONFIG_ARCH_LM32
    #define DEV_BASE	0x40000
#else
    #error Wrong CPU architecture. Must define either LM32 or RISC-V.
#endif

#if defined(CONFIG_TARGET_GENERIC_PHY_8BIT) || defined(CONFIG_TARGET_GENERIC_PHY_16BIT)
#  include "boards/generic/board.h"
#elif defined(CONFIG_TARGET_WR_SWITCH)
#  include "boards/wr-switch/board.h"
#elif defined(CONFIG_TARGET_AFCZ_V1)
#  include "boards/afcz/board.h"
#elif defined(CONFIG_TARGET_AFCZ_V2)
#  include "boards/afcz/board.h"
#elif defined(CONFIG_TARGET_ERTM14)
#  include "boards/ertm14/board.h"
#elif defined(CONFIG_TARGET_SIS8300KU)
#  include "boards/sis8300ku/board.h"
#elif defined(CONFIG_TARGET_PXIE_FMC)
#  include "boards/pxie-fmc/board.h"
#elif defined(CONFIG_TARGET_WR2RF_VME)
#  include "boards/wr2rf-vme/board.h"
else
#error no board defined
#endif

extern struct wr_endpoint_device wrc_endpoint_dev;

int wrc_board_early_init(void);
int wrc_board_init(void);
int wrc_board_create_tasks(void);

#endif
