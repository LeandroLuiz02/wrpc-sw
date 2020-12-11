/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdint.h>
#include <stdio.h>
#include <ppsi/ppsi.h>

#include "dev/gpio.h"
#include "dev/bb_spi.h"
#include "dev/ad951x.h"
#include "dev/ltc695x.h"
#include "dev/ad9910.h"
#include "dev/clock_monitor.h"
#include "dev/24aa025.h"
#include "dev/ad7888.h"
#include "dev/spi_flash.h"
#include "dev/bb_i2c.h"
#include "dev/pps_gen.h"
#include "dev/console.h"
#include "dev/endpoint.h"
#include "dev/netif.h"


#include "storage.h"
#include "wrc_ptp.h"
#include <wrc-event.h>
#include "wrc-task.h"

int wrc_board_early_init()
{
    static int32_t flash_entry_points[64];
    int i;


//    bist_init( ertm_bist );

//    wrc_register_sensors( ertm_sensors );

    /* initialize SPI flash */
    bb_spi_create( &spi_wrc_flash,
		&pin_sysc_spi_ncs,
		&pin_sysc_spi_mosi,
		&pin_sysc_spi_miso,
		&pin_sysc_spi_sclk, 0 );

	spi_flash_create( &wrc_flash_dev, &spi_wrc_flash, 16384, 0x600000 );


#if 0
    uint32_t ts = timer_get_tics();

    for(i=0;i<1000000;i++)
    {
        //gen_gpio_out(&pin_sysc_spi_sclk, 0);
        //gen_gpio_out(&pin_sysc_spi_sclk, 1);

        sysc_gpio_set_out(&pin_sysc_spi_sclk, 0);
        sysc_gpio_set_out(&pin_sysc_spi_sclk, 1);

    }

    uint32_t te = timer_get_tics();
    pp_printf("meas: %d ms\n", te - ts);
    for(;;);
#endif



	uint32_t id = spi_flash_read_id( &wrc_flash_dev );

//    bist_checkpoint( ertm_bist, ERTM14_BIST_FLASH_PRESENCE, 0, id == ERTM14_EXPECTED_FLASH_ID );

	/* initialize I2C bus */
//	bb_i2c_init( &dev_i2c_fmc );

    for(i = 0; i < 32 + 8; i++)
        flash_entry_points[i] = 0x600000 + 0x40000 * i;

    flash_entry_points[i] = -1;

    /* init storage (we use the SPI flash on eRTM14) */
    storage_spiflash_create( &wrc_storage_dev, &wrc_flash_dev );
    wrc_storage_dev.entry_points = &flash_entry_points[0];

    int rv = storage_mount( &wrc_storage_dev );
//    bist_checkpoint( ertm_bist, ERTM14_BIST_FLASH_FS_MOUNT, 0, rv == 0 );

    /* reset the networking part of the WRCore and start the WR Endpoint */
   	net_rst();

    ep_init( &wrc_endpoint_dev, (void *) BASE_EP );

	netif_register_device( "wru0", "default", &wrc_endpoint_dev );

	/* Sleep for 1s to make sure WRS v4.2 always realizes that
	 * the link is down */
	timer_delay_ms(200);
	ep_enable( &wrc_endpoint_dev, 1, 1);
	timer_delay_ms(200);

        //  int ll = ertm14_low_level_init();

//    bist_summary( ertm_bist );

    return 0;
}

extern int phy_calibration_poll(void);
extern void phy_calibration_init(void);

int wrc_board_init()
{
    wrc_task_create( "phy-cal", phy_calibration_init, phy_calibration_poll );

    return 0;
}


int wrc_board_create_tasks()
{
    return 0;
}
