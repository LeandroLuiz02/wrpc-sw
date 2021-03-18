/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __BOARD_STATE_ERTM14_H
#define __BOARD_STATE_ERTM14_H

#include <stdint.h>

/* beware this header: it is only required for WRC_DIAG_WB,
 * which is not here (yet), and it unconditionally defines
 * the wretched PACKED macro
 * Dependencies, Makefiles and header order suffer accordingly.
 * Also, the file is only included when building for WRPC-SW as the target
 * (the board state structures are shared with the MMC MCUs)
 */
#if defined(CONFIG_TARGET_ERTM14)
#include "hw/wrc_diags_regs.h"
#include "ertm15_rf_distr.h"
#endif

/* OK, I'm committing an evil thing below, but wbgen and its way of defining packed structures is to blame.
   On non-wrpc platforms (such as the MMCs), I can't include the 'wrc_diags_regs.h' file (it's platform-dependent),
   so I had to redefine the wretched macro - Tom. */
#ifndef PACKED
    #if defined( __GNUC__)
    #define PACKED __attribute__ ((packed))
    #else
    #error "Unsupported compiler?"
    #endif
#endif

#define ERTM14_RF_OUT_MIN_ID 4
#define ERTM14_RF_OUT_MAX_ID 12

#define ERTM14_CLKAB_OUT_MIN_ID 4
#define ERTM14_CLKAB_OUT_MAX_ID 15

#define ERTM14_MAX_SENSORS_COUNT 21

#define ERTM14_SYNC_SOURCE_NONE 0
#define ERTM14_SYNC_SOURCE_PPS 1
#define ERTM14_SYNC_SOURCE_RF_TRIGGER 2

#define ERTM14_MAX_UART_LINK_PAYLOAD 512

// UART Protocol packet types
#define ERTM14_UART_PTYPE_PING 1
#define ERTM14_UART_PTYPE_SNMP_REQ 2
#define ERTM14_UART_PTYPE_SNMP_RESP 3
#define ERTM14_UART_PTYPE_MMC_STATUS_REQ 4
#define ERTM14_UART_PTYPE_MMC_STATUS_RESP 5

#define ERTM14_SENSOR_VOLTAGE_MV    (1<<0)
#define ERTM14_SENSOR_CURRENT_MA    (1<<1)
#define ERTM14_SENSOR_TEMP_CELSIUS  (1<<2)
#define ERTM14_SENSOR_VALID         (1<<7)

#define ERTM14_VOLTAGE_P3V3 0
#define ERTM14_VOLTAGE_P12V 1
#define ERTM14_TEMP_FPGA 2
#define ERTM14_TEMP_DCDC 3

#define ERTM15_TEMP_PSU 4
#define ERTM15_TEMP_LO_RF 5
#define ERTM15_TEMP_REF_RF 6
#define ERTM15_TEMP_LO_DDS 7
#define ERTM15_TEMP_REF_DDS 8
#define ERTM15_TEMP_LTC6150 9
#define ERTM15_TEMP_OCXO1 10
#define ERTM15_TEMP_OCXO2 11
#define ERTM15_TEMP_CLKA_FANOUT 12
#define ERTM15_TEMP_CLKB_FANOUT 13

#define ERTM15_VOLTAGE_P3V3 14
#define ERTM15_VOLTAGE_P12V 15
#define ERTM15_VOLTAGE_P9V0_LO 16
#define ERTM15_VOLTAGE_P9V0_REF 17
#define ERTM15_VOLTAGE_POCXO 18
#define ERTM15_CURRENT_OCXO 19

struct ertm14_dds_state
{
    uint32_t ftw;
    uint8_t out_state[ERTM14_RF_OUT_MAX_ID + 1];
    int out_power[ERTM14_RF_OUT_MAX_ID + 1];
    int amp_power;
    int ampl_factor;
    int sync_source;	/* one of ERTM14_SYNC_SOURCE_NONE/PPS/TRIGGER */
    int sync_count;	/* number of sync events */
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
    char board_serial_number[32];
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

struct proto_wrc_sensor
{
	uint32_t name;	/* this is a char* in wrpc sw */
	uint8_t flags;
	uint8_t id;
	int16_t value;
};

struct ertm14_nco_reset {
	int		enabled;
	union {
		int	subscribed;
		int	sync_source;
	};
	uint32_t	current_stream_id;
	uint32_t	rx_count;
	uint32_t	reset_count;
	uint32_t	connector;
	uint32_t	unused[7];
};

/* FIXME: this is lifted from ertm_board_info; structs *must* match */
struct ertm14_device_metadata {
	uint32_t	vendor_id;
	uint32_t	device_id;
	uint32_t	version;
	uint32_t	byte_order_mark;
	unsigned char	source_id[16];
	uint32_t	capability_mask;
	unsigned char	vendor_uuid[16];
};

/* FIXME: this is lifted from ertm_board_info; structs *must* match */
struct ertm14_version_info {
	/* module serials and MACs */
	char		ertm14_serial[32];
	char		ertm15_serial[32];
	union {
	    uint8_t	ertm14_mac1_bytes[8];
	    uint64_t	ertm14_mac1;
	};
	union {
	    uint8_t	ertm14_mac2_bytes[8];
	    uint64_t	ertm14_mac2;
	};
	/* wrpc-sw version lore */
        char		wrpc_sw_commit_id[32];
        char		wrpc_sw_build_date[16];
        char		wrpc_sw_build_time[16];
        char		wrpc_sw_build_by[32];
	/* MMC firmware versions */
	char		ertm14_firmware_version[32];
	char		ertm15_firmware_version[32];
	/* unused */
	struct ertm14_device_metadata
			firmware_metadata;
};

/* FIXME: this is not the best place for this declaration */
int wrc_diags_dump(struct WRC_DIAGS_WB *buf);

#endif /*  __BOARD_STATE_ERTM14_H */
