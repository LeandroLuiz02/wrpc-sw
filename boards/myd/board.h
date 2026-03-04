#ifndef __BOARD_MYD_J7A100T__
#define __BOARD_MYD_J7A100T__

/* Endereços base dos periféricos Wishbone
 * TODO: ajustar de acordo com o mapa de endereços dos perifericos
 * os valores abaixo podem estar incorretos */
#define BASE_UART       0x00010000
#define BASE_MINIC      0x00020000
#define BASE_EP         0x00030000
#define BASE_SOFTPLL    0x00040000
#define BASE_PPS_GEN    0x00050000
#define BASE_SYSCON     0x00060000
#define BASE_TIMER      0x00070000
#define BASE_GPIO       0x00080000

/* Clock de referência da placa em Hz */
#define CLOCK_FREQ      125000000

#endif /* __BOARD_MYD_J7A100T__ */
