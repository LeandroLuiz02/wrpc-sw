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
#define ertm14_get_sim_board_config	0x17
#define	ertm14_comm_test		0x18

static struct ertm14_protocol_op {
	int8_t	opcode;
	size_t	offset1;
	size_t	length1;
	size_t	offset2;
	size_t	length2;
} protocol_ops[] = {
    {
	.opcode = ertm14_get_board_config,
	.offset1 = 1,
	.length1 = 0,
	.offset2 = 0,
	.length2 = sizeof(struct ertm14_board_state),
    },
    {
	.opcode = ertm14_set_board_config,
	.offset1 = 4,
	.length1 = sizeof(struct ertm14_board_state),
	.offset2 = 1,
	.length2 = 0,
    },
    {
	.opcode = ertm14_commit_board_config,
	.offset1 = 1,
	.length1 = sizeof(struct ertm14_board_state),
	.offset2 = 1,
	.length2 = 0,
    },
    {
	.opcode = ertm14_get_mmc_state,
	.offset1 = 1,
	.length1 = 0,
	.offset2 = 0,
	.length2 = sizeof(struct ertm14_mmc_state),
    },
    {
	.opcode = ertm14_get_wrc_diags,
	.offset1 = 1,
	.length1 = 0,
	.offset2 = 0,
	.length2 = sizeof(struct WRC_DIAGS_WB),
    },
    {
	.opcode = ertm14_get_wrc_nco,
	.offset1 = 1,
	.length1 = 0,
	.offset2 = 0,
	.length2 = /* FIXME: what comes here? */ -1,
    },
    {
	.opcode = ertm14_set_wrc_nco,
	.offset1 = 1,
	.length1 = /* FIXME: what comes here? */ -1,
	.offset2 = 1,
	.length2 = sizeof(struct ertm14_board_state),
    },
    {
	.opcode = -1,
    },
};

static const int protocol_nops = sizeof(protocol_ops)/sizeof(protocol_ops[0]);

static struct ertm14_protocol_op *get_proto_op(uint8_t opcode)
{
	int i;

	for (i = 0; i < protocol_nops; i++)
		if (protocol_ops[i].opcode == opcode)
			return &protocol_ops[i];
	return NULL;
}

/* auxiliary */
extern void board_to_host(struct ertm14_board_state *board, struct ertm14_board_state *host);

#endif /* __PSNMP_PROTO_H */
