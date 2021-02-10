#ifndef __ERTM14_UART_LINK_H
#define __ERTM14_UART_LINK_H

#include <stdint.h>

#define ERTM14_MAX_UART_LINK_PAYLOAD 512


// UART Protocol packet types
#define ERTM14_UART_PTYPE_PING 1
#define ERTM14_UART_PTYPE_SNMP_REQ 2
#define ERTM14_UART_PTYPE_SNMP_RESP 3
#define ERTM14_UART_PTYPE_MMC_STATUS_REQ 4
#define ERTM14_UART_PTYPE_MMC_STATUS_RESP 5
#define ERTM14_UART_PTYPE_CONFIG_REQ 6
#define ERTM14_UART_PTYPE_CONFIG_RESP 7


#define ERTM14_SENSOR_VOLTAGE_MV   (1<<0)
#define ERTM14_SENSOR_CURRENT_MA   (1<<1)
#define ERTM14_SENSOR_TEMP_CELSIUS (1<<2)
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

#define ERTM14_MAX_SENSORS_COUNT 21

#ifndef PACKED
    #define PACKED __attribute__((packed))
#endif

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

struct simple_uart_device;

struct uart_packet
{
    uint8_t ptype;
    uint16_t length;
    uint8_t payload[ ERTM14_MAX_UART_LINK_PAYLOAD ];
};

struct uart_link
{
    int (*send_byte)(struct uart_link *link, uint8_t byte );
    int (*recv_byte)(struct uart_link *link );
    uint32_t (*get_ms_tics)( struct uart_link *link );
    void *priv;
    int state;
    int rx_count;
    uint16_t rx_crc, check_crc;
    uint32_t rx_last_tics;
    struct uart_packet rx_packet;
};


#ifdef __linux__
int uart_link_create_linux( struct uart_link *link, const char* dev_name, int speed );
int uart_link_close_linux( struct uart_link *link );
#endif

#ifdef CONFIG_TARGET_ERTM14
int uart_link_create_wrpc_console( struct uart_link *link );
int uart_link_create_wrpc_suart( struct uart_link *link, struct simple_uart_device *uart_dev );
#endif

#ifdef MODULE_ERTM14_FPGA_UART // openMMC

#endif

int uart_link_reset( struct uart_link *link );
int uart_link_send( struct uart_link* link, struct uart_packet* pkt );
int uart_link_recv( struct uart_link* link, struct uart_packet **pkt, int timeout_ms );

#endif
