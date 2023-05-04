/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012-2020 CERN (www.cern.ch)
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include "pp-printf.h"
#include "board.h"
#include "dev/simple_uart.h"
#include "dev/console.h"
#include "lib/syslog.h"
#include <netconsole.h>

static int puts_direct = 0;

#define CON_STATE_IDLE 0
#define CON_STATE_ESC_PENDING 1 // previous char was escape, waiting for control code
#define CON_STATE_ESC_FLUSH 2  // previous char was an unrecognized escape sequence, pass both to the user

struct console_uart_priv_data
{
    struct simple_uart_device uart_dev;
    uint8_t state;
    uint8_t prev_char;
    void (*mode_switch_hook)( int is_binary );
};

static struct console_uart_priv_data console_uart_priv;
#ifdef ERTM14_SECONDARY_DEBUG_UART
static struct console_uart_priv_data console_uart_priv_2nd;
#endif
struct console_device console_uart_dev, console_uart_2nd;
struct console_device* console_devs[BOARD_MAX_CONSOLE_DEVICES];

#define CON_ESCAPE_CODE 0x1b
#define CON_SWITCH_BINARY_CODE 'B'
#define CON_SWITCH_TEXT_CODE 'T'

static int con_rx_internal(struct console_device* dev)
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    if ( priv->state == CON_STATE_ESC_FLUSH )
    {
        priv->state = CON_STATE_IDLE;
        return priv->prev_char;
    }

    int rx_char = suart_read_byte( &priv->uart_dev );

    if( rx_char < 0 )
        return rx_char;

    if( priv->state == CON_STATE_IDLE )
    {
        if( rx_char == CON_ESCAPE_CODE )
        {
            priv->state = CON_STATE_ESC_PENDING;
            return -1;
        } else {
            return rx_char;
        }
    }
    else if( priv->state == CON_STATE_ESC_PENDING )
    {
        switch( rx_char )
        {
            case CON_ESCAPE_CODE: // double escape = actual esc
                return CON_ESCAPE_CODE;
            case CON_SWITCH_TEXT_CODE: // switch to tty mode
                dev->flags &= ~CONSOLE_FLAGS_MODE_BINARY;
                dev->flags |= CONSOLE_FLAGS_MODE_TTY;
                if( priv->mode_switch_hook )
                    priv->mode_switch_hook( 0 );
                return -1;
            case CON_SWITCH_BINARY_CODE: // switch to binary mode
                dev->flags &= ~CONSOLE_FLAGS_MODE_TTY;
                dev->flags |= CONSOLE_FLAGS_MODE_BINARY;
                if( priv->mode_switch_hook )
                    priv->mode_switch_hook( 1 );
                return -1;
            default:
                priv->state = CON_STATE_ESC_FLUSH;
                priv->prev_char = rx_char;
                return -1;
        }
        priv->state = CON_STATE_IDLE;
    }
    return rx_char;
}

static int con_uart_put_string(struct console_device* dev, const char *s)
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;
    char c;
    int count = 0;

    // binary mode uses different API
    if( dev->flags & CONSOLE_FLAGS_MODE_BINARY )
        return 0;

    while ( (c = *s++) != 0 )
    {
	    if( (dev->flags & CONSOLE_FLAGS_INSERT_CRLF) &&  c == '\n')
    		suart_write_byte(&priv->uart_dev, '\r');

        suart_write_byte(&priv->uart_dev, c);
        count++;
    }

    return count;
}

static int con_uart_getc(struct console_device* dev)
{
    if( dev->flags & CONSOLE_FLAGS_MODE_BINARY )
        return 0;

    return con_rx_internal( dev );
}

void console_uart_set_crlf_mode(int on)
{
    if(on)
        console_uart_dev.flags |= CONSOLE_FLAGS_INSERT_CRLF;
    else
        console_uart_dev.flags &= ~CONSOLE_FLAGS_INSERT_CRLF;
}

void console_register_device( struct console_device *dev )
{
    int i;
    for(i = 0; i < BOARD_MAX_CONSOLE_DEVICES; i++)
    {
        if ( console_devs[i] == NULL )
        {
            console_devs[i] = dev;
            return;
        }
    }
}

int puts(const char *s)
{
    if( puts_direct)
    {
        return con_uart_put_string( &console_uart_dev, s );
    }

    int i, rv = 0;

    for(i = 0; i < BOARD_MAX_CONSOLE_DEVICES; i++)
    {
	    struct console_device *con = console_devs[i];
        if(!con)
            continue;
        rv = con->put_string( con, s);
    }

    return rv;
}


int console_getc(void)
{
    int i;

    for(i = 0; i < BOARD_MAX_CONSOLE_DEVICES; i++)
    {
        struct console_device *con = console_devs[i];
        if(!con)
            continue;
        /* continue if no get_char function implemented */
        if (!con->get_char)
            continue;
        int b = con->get_char( con );

        if( b > 0 )
            return b;
    }

    return -1;
}

void console_set_mode_switch_hook( struct console_device *dev, void (*callback)(int) )
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;
    priv->mode_switch_hook = callback;
}

void console_init()
{
    int i;

    for(i = 0; i < BOARD_MAX_CONSOLE_DEVICES; i++)
        console_devs[i] = NULL;

    suart_init( &console_uart_priv.uart_dev, BASE_UART, CONSOLE_UART_BAUDRATE );

    console_uart_dev.flags = CONSOLE_FLAGS_MODE_TTY | CONSOLE_FLAGS_INSERT_CRLF;
    console_uart_dev.priv = &console_uart_priv;
    console_uart_dev.get_char = con_uart_getc;
    console_uart_dev.put_string = con_uart_put_string;
    console_uart_priv.prev_char = 0;
    console_uart_priv.state = CON_STATE_IDLE;
    console_uart_priv.mode_switch_hook = NULL;
    console_register_device( &console_uart_dev );

#ifdef CONFIG_IPMI_CONSOLE
    console_ipmi_init();
#endif

#ifdef CONFIG_NETCONSOLE
    console_netconsole_init();
#endif

#ifdef CONFIG_PUTS_SYSLOG
    console_syslog_init();
#endif

#ifdef ERTM14_SECONDARY_DEBUG_UART
    // hack: there's a second UART attached to the console available on the J11 pins 2 & 3.
    // This is meant to help debugging the UART link (which uses the primary front panel USB console uart...)
    suart_init( &console_uart_priv_2nd.uart_dev, BASE_ERTM14_DEBUG_UART, CONSOLE_UART_BAUDRATE );

    console_uart_2nd.flags = CONSOLE_FLAGS_MODE_TTY | CONSOLE_FLAGS_INSERT_CRLF;
    console_uart_2nd.priv = &console_uart_priv_2nd;
    console_uart_2nd.get_char = con_uart_getc;
    console_uart_2nd.put_string = con_uart_put_string;

    console_uart_priv_2nd.prev_char = 0;
    console_uart_priv_2nd.state = CON_STATE_IDLE;
    console_uart_priv_2nd.mode_switch_hook = NULL;
    console_register_device( &console_uart_2nd );

    pp_printf("Console UART FIFO:: %d\n", suart_is_fifo_supported( &console_uart_priv.uart_dev ) );
    pp_printf("Debug UART FIFO:: %d\n", suart_is_fifo_supported( &console_uart_priv_2nd.uart_dev ) );
#endif
}

void console_force_mode( struct console_device *dev, int mode )
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    dev->flags &= ~( CONSOLE_FLAGS_MODE_BINARY | CONSOLE_FLAGS_MODE_TTY );
    dev->flags |= mode;
    priv->state = CON_STATE_IDLE;
}

int console_get_mode( struct console_device *dev )
{
    return dev->flags & ( CONSOLE_FLAGS_MODE_BINARY | CONSOLE_FLAGS_MODE_TTY );
}

int console_binary_send_byte( struct console_device *dev, uint8_t b )
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    if( dev->flags & CONSOLE_FLAGS_MODE_TTY )
        return 0;

    int has_fifo = suart_is_fifo_supported( &priv->uart_dev );

    if( !has_fifo )
        return -ENODEV;

    suart_write_byte( &priv->uart_dev, b );

    return 0;
}

int console_binary_recv_byte( struct console_device *dev )
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    if( dev->flags & CONSOLE_FLAGS_MODE_TTY )
        return -1;

    int has_fifo = suart_is_fifo_supported( &priv->uart_dev );

    if( !has_fifo )
        return -ENODEV;

    int rx_byte = con_rx_internal( dev );

    return rx_byte;
}
