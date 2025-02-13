/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2024-2025 Missing Link Electronics (www.missinglinkelectronics.com)
 * Author: Frederik Pfautsch <frederik.pfautsch@missinglinkelectronics.com>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <stdint.h>
#include <stdio.h>
#include <sys/errno.h>

#include "dev/si5324.h"
#include "dev/syscon.h"

#include <wrc-debug.h>

#define SI5324_REG0_BYPASS_REG (1u << 1)
#define SI5324_REG0_CKOUT_ALWAYS_ON (1u << 5)
#define SI5324_REG0_FREE_RUN (1u << 6)

#define SI5324_REG1_CK_PRIOR1_MASK  (3u << 0)
#define SI5324_REG1_CK_PRIOR1_CKIN1 (0u << 0)
#define SI5324_REG1_CK_PRIOR1_CKIN2 (1u << 0)
#define SI5324_REG1_CK_PRIOR2_MASK  (3u << 2)
#define SI5324_REG1_CK_PRIOR2_CKIN1 (0u << 2)
#define SI5324_REG1_CK_PRIOR2_CKIN2 (1u << 2)

#define SI5324_REG2_BWSEL_REG (0xf << 4)

#define SI5324_REG3_SQ_ICAL (1u << 4)
#define SI5324_REG3_DHOLD (1u << 5)
#define SI5324_REG3_CLKSEL_REG_MASK (3u << 6)
#define SI5324_REG3_CLKSEL_REG_CKIN1 (0u << 6)
#define SI5324_REG3_CLKSEL_REG_CKIN2 (1u << 6)

#define SI5324_REG4_AUTOSEL_REG_MASK (3u << 6)
#define SI5324_REG4_AUTOSEL_MANUAL (0u << 6)
#define SI5324_REG4_AUTOSEL_AUTO_NON_REV (1u << 6)
#define SI5324_REG4_AUTOSEL_AUTO_REV (2u << 6)

#define SI5324_REG10_DSBL2_REG (1u << 3)
#define SI5324_REG10_DSBL1_REG (1u << 2)

#define SI5324_REG11_PD_CKIN2 (1u << 1)
#define SI5324_REG11_PD_CKIN1 (1u << 0)

#define SI5324_REG21_CKSEL_PIN (1u << 0)
#define SI5324_REG21_CK1_ACTV_PIN (1u << 1)

#define SI5324_REG25_N1HS_MASK (7u << 5)

#define SI5324_REG3x_NCxLS_MASK (15u << 0)

#define SI5324_REG40_N2HS_MASK (7u << 5)
#define SI5324_REG40_N2LS_MASK (15u << 0)

#define SI5324_REG4x_N3x_MASK (7u << 0)

#define SI5324_REG136_RST_REG (1u << 7)
#define SI5324_REG136_ICAL (1u << 6)

static void si5324_set(uint8_t *reg, uint8_t mask)
{
	*reg |= mask;
}

static void si5324_unset(uint8_t *reg, uint8_t mask)
{
	*reg &= ~mask;
}

static void si5324_setreg(struct wr_si5324_interface_device *dev, uint8_t reg_index, uint8_t mask_set, uint8_t mask_unset)
{
	uint8_t reg;

	si5324_read(dev, reg_index, &reg, 1);
	si5324_unset(&reg, mask_unset);
	si5324_set(&reg, mask_set);
	si5324_write(dev, reg_index, &reg, 1);
}

void si5324_read( struct wr_si5324_interface_device *dev, uint8_t addr, uint8_t *data, int count )
{
	int i;

	bb_i2c_start( dev->master );
	bb_i2c_put_byte( dev->master, dev->i2c_addr << 1 );
	bb_i2c_put_byte( dev->master, addr );
	bb_i2c_repeat_start( dev->master );
	bb_i2c_put_byte( dev->master, (dev->i2c_addr << 1) | 1 );

	for(i = 0; i < count; i ++)
		bb_i2c_get_byte( dev->master, &data[i], i == (count - 1) ? 1 : 0 );

	bb_i2c_stop( dev->master );
}


void si5324_write( struct wr_si5324_interface_device *dev, uint8_t addr, uint8_t *data, int count )
{
	int i;

	bb_i2c_start( dev->master );
	bb_i2c_put_byte( dev->master, dev->i2c_addr << 1 );
	bb_i2c_put_byte( dev->master, addr );

	for(i = 0; i < count; i ++)
	{
		bb_i2c_put_byte( dev->master, data[i] );
	}

	bb_i2c_stop( dev->master );
}

void si5324_reset(struct wr_si5324_interface_device *dev )
{
	si5324_setreg(dev, 136, SI5324_REG136_RST_REG, 0);
	timer_delay_ms(100);
}

int si5324_set_bypass( struct wr_si5324_interface_device *dev )
{
	board_dbg("Si5324: Setting bypass mode\n" );

	si5324_setreg(dev, 0, SI5324_REG0_BYPASS_REG | SI5324_REG0_CKOUT_ALWAYS_ON, SI5324_REG0_FREE_RUN);
	si5324_setreg(dev, 10, SI5324_REG10_DSBL2_REG, SI5324_REG10_DSBL1_REG);
	si5324_setreg(dev, 11, SI5324_REG11_PD_CKIN2, SI5324_REG11_PD_CKIN1);

	si5324_setreg(dev, 136, SI5324_REG136_ICAL, 0);
	timer_delay_ms(100);

	board_dbg("Wait 1 seconds for clock to settle ...\n");
	timer_delay_ms(1000);

	return 0;
}

int si5324_set_125m( struct wr_si5324_interface_device *dev )
{
	board_dbg("Si5324: Setting freerun 125m fout\n");

	// Values taken from DSPLLsim
	si5324_setreg(dev, 0, SI5324_REG0_FREE_RUN, SI5324_REG0_BYPASS_REG | SI5324_REG0_CKOUT_ALWAYS_ON);
	si5324_setreg(dev, 2, 0x1 << 4, SI5324_REG2_BWSEL_REG); // 9Hz bandwith
	si5324_setreg(dev, 1, SI5324_REG1_CK_PRIOR1_CKIN1 | SI5324_REG1_CK_PRIOR2_CKIN2, SI5324_REG1_CK_PRIOR1_MASK | SI5324_REG1_CK_PRIOR2_MASK);
	si5324_setreg(dev, 3, SI5324_REG3_CLKSEL_REG_CKIN1, SI5324_REG3_CLKSEL_REG_MASK);
	si5324_setreg(dev, 4, SI5324_REG4_AUTOSEL_AUTO_REV, SI5324_REG4_AUTOSEL_REG_MASK);
	si5324_setreg(dev, 10, SI5324_REG10_DSBL2_REG, SI5324_REG10_DSBL1_REG);
	si5324_setreg(dev, 11, SI5324_REG11_PD_CKIN1, SI5324_REG11_PD_CKIN2);

	si5324_setreg(dev, 25, (7-4) << 5, SI5324_REG25_N1HS_MASK); // N1_HS = 7
	si5324_setreg(dev, 31, (6-1) >> 16, SI5324_REG3x_NCxLS_MASK); // NC1_LS = 6, must be even or 1
	si5324_setreg(dev, 32, (6-1) >> 8, 0xff); // NC1_LS = 6, must be even or 1
	si5324_setreg(dev, 33, (6-1) >> 0, 0xff); // NC1_LS = 6, must be even or 1
	si5324_setreg(dev, 34, (6-1) >> 16, SI5324_REG3x_NCxLS_MASK); // NC2_LS = 6, must be even or 1
	si5324_setreg(dev, 35, (6-1) >> 8, 0xff); // NC2_LS = 6, must be even or 1
	si5324_setreg(dev, 36, (6-1) >> 0, 0xff); // NC2_LS = 6, must be even or 1

	si5324_setreg(dev, 40, (10-4) << 5, SI5324_REG40_N2HS_MASK);
	si5324_setreg(dev, 40, (140000-1) >> 16, SI5324_REG40_N2LS_MASK); // N2_LS = 140000, must be even
	si5324_setreg(dev, 41, ((140000-1) >> 8) & 0xff, 0xff); // N2_LS = 140000, must be even
	si5324_setreg(dev, 42, ((140000-1) >> 0) & 0xff, 0xff); // N2_LS = 140000, must be even

	si5324_setreg(dev, 43, (30476-1) >> 16, SI5324_REG4x_N3x_MASK); // N31 = 30476
	si5324_setreg(dev, 44, ((30476-1) >> 8) & 0xff, 0xff); // N31 = 30476
	si5324_setreg(dev, 45, ((30476-1) >> 0) & 0xff, 0xff); // N31 = 30476
	si5324_setreg(dev, 46, (30476-1) >> 16, SI5324_REG4x_N3x_MASK); // N32 = 30476
	si5324_setreg(dev, 47, ((30476-1) >> 8) & 0xff, 0xff); // N32 = 30476
	si5324_setreg(dev, 48, ((30476-1) >> 0) & 0xff, 0xff); // N32 = 30476

	si5324_setreg(dev, 136, SI5324_REG136_ICAL, 0);
	timer_delay_ms(100);

	board_dbg("Wait 1 seconds for clock to settle ...\n");
	timer_delay_ms(1000);

	return 0;
}

void wr_si5324_interface_init( struct wr_si5324_interface_device *dev, struct i2c_bus *master, uint8_t i2c_addr )
{
	dev->i2c_addr = i2c_addr;
	dev->master = master;
}
