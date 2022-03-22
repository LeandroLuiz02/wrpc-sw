/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __BOARD_WR2RF_VME_H
#define __BOARD_WR2RF_VME_H

#include "dev/gpio.h"
#include "dev/bb_spi.h"
#include "dev/24aa025.h"
#include "dev/spi_flash.h"
#include "dev/bb_i2c.h"

#define BOARD_USE_CUSTOM_SDBFS 1
#define BOARD_HAS_CUSTOM_NETWORK_INIT 1
#define BOARD_MAX_CONSOLE_DEVICES 2

#define BOARD_USE_EVENTS 0

/* Board-specific parameters */
#define TICS_PER_SECOND 1000

/* WR Core system/CPU clock frequency in Hz */
#define CPU_CLOCK 62500000ULL

/* WR Reference clock period (picoseconds) and frequency (Hz) */
#define REF_CLOCK_PERIOD_PS 16000
#define REF_CLOCK_FREQ_HZ 62500000

/* Center DMTD frequency (Hz) */
#define DMTD_CLOCK_FREQ_HZ 62500000

/* Baud rate of the builtin UART (does not apply to the VUART) */
#define CONSOLE_UART_BAUDRATE 921600ULL

/* Maximum number of simultaneously created sockets */
#define NET_MAX_SOCKETS 12

/* Socket buffer size, determines the max. RX packet size */
#define NET_MAX_SKBUF_SIZE 512

/* Number of auxillary clock channels - usually equal to the number of FMCs */
#define NUM_AUX_CLOCKS 1

int board_init(void);
int board_update(void);

/* spll parameter that are board-specific */
#  define BOARD_DIVIDE_DMTD_CLOCKS	0
#  define NS_PER_CLOCK 16

#define BOARD_MAX_CHAN_REF		1
#define BOARD_MAX_CHAN_AUX		2
#define BOARD_MAX_PTRACKERS		1

#define ERTM14_MAX_CONFIGS 8

#define SDB_ADDRESS 0x50000

#define FMC_EEPROM_ADR 0x50

#define SDBFS_REC 5

#define BASE_AUXWB                  0x48000
#define BASE_SOFTPLL                0x40200
#define BASE_PPS_GEN                0x40300
#define BASE_UART                   0x40500
#define BASE_SYSCON                 0x40400
#define BASE_EP                     0x40100
#define BASE_MINIC                  0x40000
#define BASE_ONEWIRE                0x40600


#endif /* __BOARD_WRC_H */
