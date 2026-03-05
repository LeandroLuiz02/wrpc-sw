#ifndef __BOARD_MYD_J7A100T__
#define __BOARD_MYD_J7A100T__

/*
 * Endereços dos periféricos WR definidos relativos a DEV_BASE
 * (DEV_BASE = 0x100000 para RISC-V, definido em include/board.h)
 * TODO: ajustar conforme o mapa de endereços do seu projeto HDL
 */

/* Board-specific parameters */
#define TICS_PER_SECOND         1000

/* WR Core system/CPU clock frequency in Hz (clk_sys) */
#define CPU_CLOCK               62500000ULL

/* WR Reference clock period (picoseconds) and frequency (Hz) */
/* GENERIC_PHY_16BIT - Artix-7 GTP transceiver */
#define NS_PER_CLOCK            16
#define REF_CLOCK_PERIOD_PS     16000
#define REF_CLOCK_FREQ_HZ       62500000

/* Maximum number of simultaneously created sockets */
#define NET_MAX_SOCKETS         12

/* Socket buffer size, determines the max. RX packet size */
#define NET_MAX_SKBUF_SIZE      512

/* SoftPLL parameters */
/* DMTD clock divide: 0 = not divided, 1 = divided by 2 */
#define BOARD_DIVIDE_DMTD_CLOCKS    0

/* Number of reference channels (RX clocks) */
#define BOARD_MAX_CHAN_REF       1
/* Number of external PLLs that can be disciplined */
#define BOARD_MAX_CHAN_AUX       2
/* Should be the same as reference channels */
#define BOARD_MAX_PTRACKERS     1

/* Events are not used on this platform */
#define BOARD_USE_EVENTS        0

/* Console UART */
#define BOARD_CONSOLE_DEVICES   1
#define CONSOLE_UART_BAUDRATE   115200

/* Maximum number of files in the SDB filesystem.
 * Need at least 4: ., sfp database, init script and calibration */
#define SDBFS_REC               5

/* I2C address of the storage EEPROM
 * TODO: ajustar conforme o hardware da sua placa */
#define FMC_EEPROM_ADR          0x50

#endif /* __BOARD_MYD_J7A100T__ */
