/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012-2019 CERN (www.cern.ch)
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __CONSOLE_UART_H
#define __CONSOLE_UART_H

#include <stdint.h>

#include "dev/console.h"
#include "dev/simple_uart.h"

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

void console_uart_init(struct console_device *dev,
		       struct console_uart_priv_data *priv,
		       unsigned addr,
		       unsigned baudrate);

extern struct console_uart_priv_data console_uart_priv;
extern struct console_device console_uart_dev;
#endif

