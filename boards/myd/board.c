#include "board.h"
#include "dev/syscon.h"
#include "dev/endpoint.h"
#include "storage.h"

/*
 * wrc_board_early_init - chamado logo no início do boot, antes do PLL e rede.
 * Use para inicializar I2C, EEPROM, osciladores externos, etc.
 * TODO: adicionar inicializações específicas do hardware da sua placa.
 */
int wrc_board_early_init(void)
{
	return 0;
}

/*
 * wrc_board_init - chamado após PLL e rede estarem prontos.
 * Deve configurar o endereço MAC do endpoint.
 * TODO: ler o MAC da EEPROM ou usar um endereço fixo para testes.
 */
int wrc_board_init(void)
{
	uint8_t mac_addr[6] = { 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };

	ep_set_mac_addr(&wrc_endpoint_dev, mac_addr);
	ep_pfilter_init_default(&wrc_endpoint_dev);

	return 0;
}

/*
 * wrc_board_create_tasks - registra tarefas periódicas específicas da placa.
 * TODO: adicionar tarefas se necessário (ex: leitura de temperatura).
 */
int wrc_board_create_tasks(void)
{
	return 0;
}
