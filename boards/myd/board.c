#include <wrc.h>
#include <board.h>
#include <uart.h>
#include <syscon.h>
#include "board.h"

/* Ponteiros para os periféricos Wishbone */
struct SYSCON_WB  *syscon  = (void *) BASE_SYSCON;
struct UART_WB    *uart0   = (void *) BASE_UART;

/* Inicialização básica da placa */
void board_init(void)
{
    /* Inicializa UART para console */
    uart_init_hw();

    /* Inicializa syscon */
    // TODO: adicionar inicializações específicas do hardware
}

/* Retorna frequência do clock em Hz */
uint32_t board_get_ref_clock(void)
{
    return CLOCK_FREQ;
}
