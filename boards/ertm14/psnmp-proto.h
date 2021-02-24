#ifndef __PSNMP_PROTO_H
#define __PSNMP_PROTO_H

#include <hw/wrc_diags_regs.h>
#include "board-state.h"

/* visually recognizable opcodes */
#define ertm14_get_board_config		0x10
#define ertm14_set_board_config		0x11
#define ertm14_commit_board_config	0x12
#define ertm14_get_mmc_state		0x13
#define ertm14_get_wrc_diags		0x14
#define ertm14_get_wrc_nco		0x15
#define ertm14_set_wrc_nco		0x16

struct ertm14_protocol_ops {
	int8_t	opcode;
	void	*arg1;
	void	*arg2;
	size_t	length1;
	size_t	length2;
} protocol_ops[] = {
    { ertm14_get_board_config, NULL, NULL, sizeof(struct ertm14_board_state), 0, },
    { ertm14_set_board_config, NULL, NULL, sizeof(struct ertm14_board_state), 0, },
    { ertm14_commit_board_config, NULL, NULL, sizeof(struct ertm14_board_state), 0, },
    { ertm14_get_mmc_state,    NULL, NULL, sizeof(struct ertm14_mmc_state), 0, },
    { ertm14_get_wrc_diags,    NULL, NULL, sizeof(struct WRC_DIAGS_WB), 0, },
    { ertm14_get_wrc_nco,      NULL, NULL, -1, 0, },
    { ertm14_set_wrc_nco,      NULL, NULL, -1, 0, },
    { -1, },
};

#endif /* __PSNMP_PROTO_H */
