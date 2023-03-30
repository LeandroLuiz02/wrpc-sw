#ifndef __CHEBY__LPDC_MDIO__H__
#define __CHEBY__LPDC_MDIO__H__
#define LPDC_MDIO_SIZE 8192 /* 0x2000 = 8KB */

/* Low Phase Drift Calibration Control Register */
#define LPDC_MDIO_CTRL 0x0UL
#define LPDC_MDIO_CTRL_TX_SW_RESET 0x1UL
#define LPDC_MDIO_CTRL_TX_ENABLE 0x2UL
#define LPDC_MDIO_CTRL_RX_ENABLE 0x4UL
#define LPDC_MDIO_CTRL_RX_SW_RESET 0x8UL
#define LPDC_MDIO_CTRL_QPLL_SW_RESET 0x10UL
#define LPDC_MDIO_CTRL_TXUSRPLL_RESET 0x20UL
#define LPDC_MDIO_CTRL_COMMA_TARGET_POS_MASK 0x3fc0UL
#define LPDC_MDIO_CTRL_COMMA_TARGET_POS_SHIFT 6
#define LPDC_MDIO_CTRL_DMTD_CLK_SEL_MASK 0xc000UL
#define LPDC_MDIO_CTRL_DMTD_CLK_SEL_SHIFT 14

/* Low Phase Drift Calibration Status Register */
#define LPDC_MDIO_STAT 0x4UL
#define LPDC_MDIO_STAT_QPLL_LOCKED 0x1UL
#define LPDC_MDIO_STAT_LINK_UP 0x2UL
#define LPDC_MDIO_STAT_LINK_ALIGNED 0x4UL
#define LPDC_MDIO_STAT_TX_RST_DONE 0x8UL
#define LPDC_MDIO_STAT_TXUSRPLL_LOCKED 0x10UL
#define LPDC_MDIO_STAT_RX_RST_DONE 0x20UL
#define LPDC_MDIO_STAT_COMMA_CURRENT_POS_MASK 0x7f80UL
#define LPDC_MDIO_STAT_COMMA_CURRENT_POS_SHIFT 7

/* Xilinx DRP registers, specific to the transceiver */
#define LPDC_MDIO_DRP_REGS 0x1000UL
#define ADDR_MASK_LPDC_MDIO_DRP_REGS 0x1000UL
#define LPDC_MDIO_DRP_REGS_SIZE 4096 /* 0x1000 = 4KB */

struct lpdc_mdio {
  /* [0x0]: REG (rw) Low Phase Drift Calibration Control Register */
  uint32_t CTRL;

  /* [0x4]: REG (ro) Low Phase Drift Calibration Status Register */
  uint32_t STAT;

  /* padding to: 1024 words */
  uint32_t __padding_0[1022];

  /* [0x1000]: SUBMAP Xilinx DRP registers, specific to the transceiver */
  uint32_t drp_regs[1024];
};

#endif /* __CHEBY__LPDC_MDIO__H__ */
