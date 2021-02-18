#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>

#ifdef CONFIG_TARGET_ERTM14
#include "dev/console.h"
#include "dev/simple_uart.h"
#endif

#include "ertm14-uart-link.h"

#ifndef DEBUG
#define ulink_dbg(...)
#else
#ifdef __linux__
#define ulink_dbg(...) fprintf(stderr,__VA_ARGS__)
#else
#define ulink_dbg(...)
#endif
#endif

#define CON_ESCAPE_CODE 0x1b
#define CON_SWITCH_BINARY_CODE 'B'
#define CON_SWITCH_TEXT_CODE 'T'

#define CRC_POLY 0x8408

#define LINK_STATE_IDLE 0
#define LINK_STATE_SYNC 1
#define LINK_STATE_PTYPE 2
#define LINK_STATE_LEN0 3
#define LINK_STATE_LEN1 4
#define LINK_STATE_PAYLOAD 5
#define LINK_STATE_CRC0 6
#define LINK_STATE_CRC1 7

#define RX_FSM_TIMEOUT 1000 /* ms */



static uint16_t crc_xmodem_update(uint16_t crc, uint8_t data)
{
    int i;

    crc = crc ^ (((uint16_t)data) << 8);

    for (i = 0; i < 8; i++)
    {
        if (crc & 0x8000)
        {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}


#ifdef CONFIG_TARGET_ERTM14

static uint32_t wrpc_get_ms_tics( struct uart_link* link )
{
    return timer_get_tics();
}

static int wrpc_console_uart_send_byte( struct uart_link* link, uint8_t b )
{
    return console_binary_send_byte( &console_uart_dev, b );
}

static int wrpc_console_uart_recv_byte( struct uart_link* link )
{
    return console_binary_recv_byte( &console_uart_dev ) ;
}

int uart_link_create_wrpc_console( struct uart_link *link )
{
    link->priv = NULL;
    link->send_byte = wrpc_console_uart_send_byte;
    link->recv_byte = wrpc_console_uart_recv_byte;
    link->get_ms_tics = wrpc_get_ms_tics;
    link->state = LINK_STATE_IDLE;
    link->rx_last_tics = 0;
    return 0;
};


static int wrpc_suart_send_byte( struct uart_link* link, uint8_t b )
{
    struct simple_uart_device *suart = (struct simple_uart_device* ) link->priv;
    suart_write_byte( suart, b );
    return 1;
}

static int wrpc_suart_recv_byte( struct uart_link* link )
{
    struct simple_uart_device *suart = (struct simple_uart_device* ) link->priv;
    return suart_read_byte( suart );
}

int uart_link_create_wrpc_suart( struct uart_link *link, struct simple_uart_device *uart_dev )
{
    link->priv = uart_dev;
    link->send_byte = wrpc_suart_send_byte;
    link->recv_byte = wrpc_suart_recv_byte;
    link->get_ms_tics = wrpc_get_ms_tics;
    link->state = LINK_STATE_IDLE;
    link->rx_last_tics = 0;
    return 0;
};


#endif

static uint16_t crc16(unsigned char *buf, int len)
{
    int i;
    uint16_t cksum;
    cksum = 0;

    for (i = 0; i < len; i++) {
        cksum = crc_xmodem_update(cksum, buf[i]);
    }
    return cksum;
}

int uart_link_reset( struct uart_link *link )
{
    link->state = LINK_STATE_IDLE;
    link->rx_last_tics = 0;
    return 0;		/* FIXME: mustn't this be void? */
}

int uart_link_send( struct uart_link* link, struct uart_packet* pkt )
{
    uint8_t  buf[ ERTM14_MAX_UART_LINK_PAYLOAD + 16 ];
    uint16_t crc = 0, i;

    buf[0] = 0x55;
    buf[1] = 0xaa;
    buf[2] = pkt->ptype;
    buf[3] = (pkt->length >> 8) & 0xff;
    buf[4] = (pkt->length & 0xff);

    if(pkt->length > 0)
        memcpy(buf+5, pkt->payload, pkt->length);

    crc = crc16(buf, pkt->length+5);

    buf[pkt->length+5] = (crc >> 8);
    buf[pkt->length+6] = (crc & 0xff);

    for (i = 0; i < pkt->length+7; i++)
    {
        int res = link->send_byte( link, buf[i] );
        if ( res < 0 )
            return res;
    }

    return 0;
}

#define RX_FSM_PACKET_ERROR -2
#define RX_FSM_NO_DATA -1
#define RX_FSM_NEED_DATA 0
#define RX_FSM_GOT_PACKET 1

static int recv_fsm( struct uart_link* link, struct uart_packet **pkt )
{
    int rx_byte = link->recv_byte( link );

    uint32_t current_tics = link->get_ms_tics( link );

    if( current_tics - link->rx_last_tics > RX_FSM_TIMEOUT )

    if( rx_byte < 0 )
        return RX_FSM_NO_DATA;

    ulink_dbg( "Rx %x state %d\n", rx_byte, link->state );


    switch( link->state )
    {
        case LINK_STATE_IDLE:
            if( rx_byte == 0x55 )
            {
                link->state = LINK_STATE_SYNC;
                link->check_crc = crc_xmodem_update( 0, 0x55 );
            }
            break;

        case LINK_STATE_SYNC:
            if( rx_byte == 0xaa )
            {
                link->state = LINK_STATE_PTYPE;
                link->check_crc = crc_xmodem_update( link->check_crc, 0xaa );
            }
            else if (rx_byte == 0x55)
                link->state = LINK_STATE_SYNC;
            else
                link->state = LINK_STATE_IDLE;
            break;

        case LINK_STATE_PTYPE:
            link->rx_packet.ptype = rx_byte;
            link->check_crc = crc_xmodem_update( link->check_crc, rx_byte);
            link->state = LINK_STATE_LEN0;
            break;

        case LINK_STATE_LEN0:
            link->rx_packet.length = (rx_byte << 8);
            link->check_crc = crc_xmodem_update( link->check_crc, rx_byte);
            link->state = LINK_STATE_LEN1;
            break;

        case LINK_STATE_LEN1:
            link->rx_packet.length |= rx_byte;
            link->check_crc = crc_xmodem_update( link->check_crc, rx_byte);
            link->rx_count = 0;
            
            if( link->rx_packet.length == 0 )
                link->state = LINK_STATE_CRC0;
            else
                link->state = LINK_STATE_PAYLOAD;

            
            break;

        case LINK_STATE_PAYLOAD:
            if( link->rx_count == link->rx_packet.length - 1 )
            {
                link->state = LINK_STATE_CRC0;
            }

            link->check_crc = crc_xmodem_update( link->check_crc, rx_byte);

            if( link->rx_count < ERTM14_MAX_UART_LINK_PAYLOAD )
                link->rx_packet.payload[ link->rx_count++ ] = rx_byte;
            break;

        case LINK_STATE_CRC0:
            link->rx_crc = (rx_byte << 8);
            link->state = LINK_STATE_CRC1;
            break;

        case LINK_STATE_CRC1:
        {
            link->rx_crc |= rx_byte;
            link->state = LINK_STATE_IDLE;

            if (link->rx_count != link->rx_packet.length )
            {            
               // blink(1);
                return RX_FSM_PACKET_ERROR;
            }
            else if (link->rx_crc != link->check_crc )
            {
                //blink(2);
                return RX_FSM_PACKET_ERROR;
            }
            else
            {
                //blink(0);

                *pkt = &link->rx_packet;
                return RX_FSM_GOT_PACKET;
            }
        }
    }

    return RX_FSM_NEED_DATA;
}

int uart_link_recv( struct uart_link* link, struct uart_packet **pkt, int timeout_ms )
{
    uint32_t start_tics = link->get_ms_tics( link );

    for(;;)
    {
        int ret = recv_fsm( link, pkt );

        if ( timeout_ms == 0 )
            return ret;
        else if( ret == RX_FSM_PACKET_ERROR || ret == RX_FSM_GOT_PACKET )
            return ret;
        else { // check timeout
            if( link->get_ms_tics( link ) - start_tics >= timeout_ms )
            {
                ulink_dbg( "Rx timeout expired\n");
                uart_link_reset( link );
                return -ECANCELED;
            } else {
                    linux_usleep(1000);
            }
        }
    }

    return 0;
}
