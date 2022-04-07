/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2019 CERN (home.cern)
 *
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>

#include "libdevmap.h"
#include "vuart_lib.h"
#include "hw/wrc_cpu_csr.h"

#define TRTL_GDB_PACKET_SIZE_MAX 2048

static int verbose;
static int swapping;
static int flag_term;

/**
 * struct trtl_gdb_packet - GDB packet
 * @data: message exchanged with GDB
 * @size: length in bytes
 */
struct trtl_gdb_packet {
	char data[TRTL_GDB_PACKET_SIZE_MAX];
	size_t size;
};

/**
 * struct trtl_dbg_port - descriptor to handle connection
 * @addr: Mock Turtle virtual address
 * @cpu: CPU index
 * @fd: socket file descriptor
 */
struct trtl_dbg_port {
	/* For debug */
	void *addr;
	uint8_t cpu;
	int fd;
	/* For vuart */
	struct mapping_desc vuart_map;
};

typedef int (trtl_gdb_command_t)(struct trtl_dbg_port *dbg,
				 struct trtl_gdb_packet *out,
				 struct trtl_gdb_packet *in);

/**
 * Change byte order but only when user asks for it
 * @val: value
 *
 * Return: value in swapped order
 *
 * It uses a global variable that is set by the user
 */
static uint32_t __io_swap32(uint32_t val)
{
	if (!swapping)
		return val;

	return ((val >> 24) & 0x000000FF) |
	       ((val >>  8) & 0x0000FF00) |
	       ((val <<  8) & 0x00FF0000) |
	       ((val << 24) & 0xFF000000);
}

/**
 * Read value from the Debug Port
 */
static uint32_t trtl_dbg_readl(struct trtl_dbg_port *dbg, uint32_t reg)
{
	char *addr = dbg->addr;
	uint32_t res;

	res = __io_swap32(*((volatile uint32_t *)(addr + reg)));

	if (verbose > 2)
		printf("dbg_readl @%02x -> %08x\n", reg, res);

	return res;
}

/**
 * Write value to the Debug Port
 */
static void trtl_dbg_writel(struct trtl_dbg_port *dbg,
			    uint32_t reg, uint32_t val)
{
	char *addr = dbg->addr;

	if (verbose > 2)
		printf("dbg_writel @%02x <- %08x\n", reg, val);
	*((volatile uint32_t *)(addr + reg)) = __io_swap32(val);
}

/**
 * Read mail-box
 * @dbg: debug port
 *
 * Return value read
 */
static uint32_t trtl_dbg_read_mbx(struct trtl_dbg_port *dbg)
{
	uint32_t reg;

	reg = WRC_CPU_CSR_REG_DBG_CORE0_MBX;
	reg += sizeof(uint32_t) * dbg->cpu;

	return trtl_dbg_readl(dbg, reg);
}

/**
 * Write mail-box
 * @dbg: debug port
 * @val: value to write
 */
static void trtl_dbg_write_mbx(struct trtl_dbg_port *dbg, uint32_t val)
{
	uint32_t reg;

	reg = WRC_CPU_CSR_REG_DBG_CORE0_MBX;
	reg += sizeof(uint32_t) * dbg->cpu;

	trtl_dbg_writel(dbg, reg, val);
}

/**
 * Execute one instructions
 * @dbg: debug port
 * @insn: instruction to execute
 */
static void trtl_dbg_exec_insn(struct trtl_dbg_port *dbg, uint32_t insn)
{
	uint32_t reg;

	reg = WRC_CPU_CSR_REG_DBG_CORE0_INSN;
	reg += sizeof(uint32_t) * dbg->cpu;

	trtl_dbg_writel(dbg, reg, insn);
}

/**
 * Execute instruction to copy a register to the mail-box
 * @dbg: debug port
 * @reg: register index
 */
static void trtl_dbg_exec_reg_to_mbx(struct trtl_dbg_port *dbg, uint32_t reg)
{
	trtl_dbg_exec_insn(dbg, 0x7D001073 | (reg << 15));
}

/**
 * Execute instruction to the mail-box to a register
 * @dbg: debug port
 * @reg: register index
 */
static void trtl_dbg_exec_mbx_to_reg(struct trtl_dbg_port *dbg, uint32_t reg)
{
	trtl_dbg_exec_insn(dbg, 0x7D002073 | (reg << 7));
}

/**
 * Execute NOP instruction
 * @dbg: debug port
 */
static void trtl_dbg_exec_nop(struct trtl_dbg_port *dbg)
{
	trtl_dbg_exec_insn(dbg, 0x00000013);
}

/**
 * Check if MockTurtle CPU is in debug mode
 * @dbg: debug port
 *
 * Return true when it is in debug mode
 */
static bool trtl_dbg_in_debug_mode(struct trtl_dbg_port *dbg)
{
	uint32_t status;

	status = trtl_dbg_readl(dbg, WRC_CPU_CSR_REG_DBG_STATUS);

	return ((status >> dbg->cpu) & 1);
}

/**
 * Set MockTurtle CPU in debug mode
 * @dbg: debug port
 *
 * Return 0 on success, -1 on error and errno is appropriately set
 */
static int trtl_dbg_debug_mode_force_set(struct trtl_dbg_port *dbg)
{
	int retry;

	if (trtl_dbg_in_debug_mode(dbg))
		return 0;
	trtl_dbg_writel(dbg, WRC_CPU_CSR_REG_DBG_FORCE, (1 << dbg->cpu));
	/* wait to debug to be ready max ~5s */
	retry = 5000;
	while (retry >= 0) {
		struct timespec ts = {0, 1000000};

		nanosleep(&ts, NULL);
		if (trtl_dbg_in_debug_mode(dbg))
			break;
		retry--;
	}
	trtl_dbg_writel(dbg, WRC_CPU_CSR_REG_DBG_FORCE, 0);

	if (retry < 0) {
		errno = ETIME;
		return -1;
	}
	return 0;
}

/**
 * Read from a CPU register
 * @dbg: debug port
 * @reg: register number [0, 31]
 *
 * Return: the register content
 */
static uint32_t trtl_dbg_read_reg(struct trtl_dbg_port *dbg,
				  int reg)
{
	trtl_dbg_exec_reg_to_mbx(dbg, reg);
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);

	return trtl_dbg_read_mbx(dbg);
}

/**
 * Write in a CPU register
 * @dbg: debug port
 * @reg: register number [0, 31]
 * @val: value
 */
static void trtl_dbg_write_reg(struct trtl_dbg_port *dbg,
			       int reg, uint32_t val)
{
	trtl_dbg_write_mbx(dbg, val);
	trtl_dbg_exec_mbx_to_reg(dbg, reg);
}

/**
 * Copy PC to RA register
 * @dbg: debug port
 *
 * Return PC value
 */
static uint32_t trtl_dbg_pc_read_via_ra(struct trtl_dbg_port *dbg)
{
	trtl_dbg_exec_insn(dbg, 0x000000ef); /* ra = pc + 4 */
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_reg_to_mbx(dbg, 1);
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
	return (trtl_dbg_read_mbx(dbg) - 4) & 0xFFFFFFFF;
}

/**
 * Write PC using RA content register
 * @dbg: debug port
 */
static void trtl_dbg_pc_write_via_ra(struct trtl_dbg_port *dbg)
{
	trtl_dbg_exec_insn(dbg, 0x00008067); /* ret */
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
}

/**
 * Increase PC by 4
 * @dbg: debug port
 */
static void trtl_dbg_pc_advance_4(struct trtl_dbg_port *dbg)
{
	trtl_dbg_exec_insn(dbg, 0x00000263); /* beqz zero, +4 */
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
}

/**
 * Continue command
 */
static int trtl_gdb_handle_c(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	if (in->size > 1) {
		out->size = 0;
		return 0;
	}

	trtl_dbg_exec_insn(dbg, 0x00100073); /* ebreak */
	while (1) {
		int ret;
		struct pollfd p[2];

		/* Dump vuart. */
		if (flag_term) {
			while (1) {
				int rx = wr_vuart_rx(&dbg->vuart_map);
				if (rx < 0)
					break;
				putchar(rx);
			}
			fflush(stdout);
		}

		if (trtl_dbg_in_debug_mode(dbg)) {
			/*
			 * TODO not clear but check twice due to possible
			 * race if the ebreak is not yet executed
			 */
			if (trtl_dbg_in_debug_mode(dbg)) {
				out->size = snprintf(out->data,
						     TRTL_GDB_PACKET_SIZE_MAX,
						     "S05");
				break;
			}
		}

		p[0].fd = dbg->fd;
		p[0].events = POLLIN;
		p[0].revents = 0;

		if (flag_term) {
			p[1].fd = 0;
			p[1].events = POLLIN;
			p[1].revents = 0;
			ret = poll(p, 2, 100);
		}
		else
			ret = poll(p, 1, 1000);

		if (ret == 0)
			continue;
		if (ret < 0)
			break;

		if (flag_term
		    && (p[1].revents & POLLIN)) {
			char c;
			if (read(0, &c, 1) == 1)
				wr_vuart_tx(&dbg->vuart_map, c);
		}
		if (p[0].revents & POLLIN) {
			/* GDB wants something from us */
			ret = trtl_dbg_debug_mode_force_set(dbg);
			if (ret < 0)
				fprintf(stderr, "Failed to set debug mode\n");
			out->size = snprintf(out->data,
					     TRTL_GDB_PACKET_SIZE_MAX,
					     "S02");
			break;
		}
	}

	return 0;
}

/**
 * Detach
 */
static int trtl_gdb_handle_D(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	trtl_dbg_exec_insn(dbg, 0x00100073); /* ebreak */
	out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX, "OK");

	return 0;
}

/**
 * Read all registers
 */
static int trtl_gdb_handle_g(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	uint32_t regs[32], pc;
	int i;

	out->size = 0;
	for (i = 0; i < 32; ++i) {
		regs[i] = trtl_dbg_read_reg(dbg, i);
		out->size += snprintf(out->data + out->size,
				      TRTL_GDB_PACKET_SIZE_MAX,
				      "%08"PRIx32, htonl(regs[i]));
	}
	pc = trtl_dbg_pc_read_via_ra(dbg);
	out->size += snprintf(out->data + out->size,
			      TRTL_GDB_PACKET_SIZE_MAX,
			      "%08"PRIx32, htonl(pc));
	trtl_dbg_write_reg(dbg, 1, regs[1]);

	return 0;
}

/**
 * Write all register
 */
static int trtl_gdb_handle_G(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	uint32_t regs[33]; /* 32 register, 1 PC */
	int i;

	if (in->size != (1 + 33 * 8)) {
		out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
				     "E01");
		return 0;
	}

	for (i = 0; i < 33; ++i) {
		int ret = sscanf(in->data + 1 + i * 8, "%08"SCNx32, &regs[i]);

		if (ret != 1) {
			out->size = snprintf(out->data,
					     TRTL_GDB_PACKET_SIZE_MAX,
					     "E02");
			return 0;
		}
	}

	trtl_dbg_write_reg(dbg, 1, ntohl(regs[32]));
	trtl_dbg_pc_write_via_ra(dbg);
	for (i = 0; i < 32; ++i)
		trtl_dbg_write_reg(dbg, i, ntohl(regs[i]));
	out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
			     "OK");

	return 0;
}

/**
 * Set thread for subsequent operations
 *
 * Partially supported
 */
static int trtl_gdb_handle_H(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	/* we just want to keep GDB quiet */
	if (strncmp(in->data, "Hg0", 3) == 0)
		out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
				     "OK");
	else
		out->size = 0;

	return 0;
}

/**
 * Kill
 *
 * Not supported yet
 *
 * kill leave the CPU in its current state. In the next connection it
 * will restart exactly from that point and the core remains stopped.
 */
static int trtl_gdb_handle_k(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	return 0;
}

/**
 * Write data to memory
 */
static int trtl_gdb_handle_M(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	uint32_t addr, n;
	uint32_t a0, a1;
	char *indata;
	int ret;

	indata = strchr(in->data, ':');
	if (!indata) {
		out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
				     "E01");
		return 0;
	}
	indata++; /* skip ':' */
	ret = sscanf(in->data + 1, "%"SCNx32",%"SCNx32":", &addr, &n);
	if (ret != 2) {
		out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
				     "E02");
		return 0;
	}

	if (n * 2 != in->size - (indata - in->data)) {
		out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
				     "E03");
		return 0;
	}

	a0 = trtl_dbg_read_reg(dbg, 10);
	a1 = trtl_dbg_read_reg(dbg, 11);
	trtl_dbg_write_reg(dbg, 10, addr);
	if (addr % 4 == 0) {
		for (; n >= 4; n -= 4, indata += 8) {
			uint32_t w;

			ret = sscanf(indata, "%08"SCNx32, &w);
			if (ret != 1)
				break;

			trtl_dbg_write_reg(dbg, 11, ntohl(w));
			/* sw a1, 0(a0) */
			trtl_dbg_exec_insn(dbg, 0x00B52023);
			/* addi a0, a0, 4 */
			trtl_dbg_exec_insn(dbg, 0x00450513);
		}
	}
	for (; n > 0; --n, indata += 2) {
		uint32_t b;

		ret = sscanf(indata, "%02"SCNx32, &b);
		if (ret != 1)
			break;
		trtl_dbg_write_reg(dbg, 11, b);
		/* sb a1, 0(a0) */
		trtl_dbg_exec_insn(dbg, 0x00B50023);
		/* addi a0, a0, 4 */
		trtl_dbg_exec_insn(dbg, 0x00150513);
	}

	trtl_dbg_write_reg(dbg, 10, a0);
	trtl_dbg_write_reg(dbg, 11, a1);

	if (n > 0)
		out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
				     "E04");
	else
		out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
				     "OK");

	return 0;
}

/**
 * Read data from memory
 */
static int trtl_gdb_handle_m(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	uint32_t addr, n;
	uint32_t a0, a1;
	int ret;

	ret = sscanf(in->data + 1, "%x,%x", &addr, &n);
	if (ret != 2) {
		out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
				     "E01");
		return 0;
	}
	a0 = trtl_dbg_read_reg(dbg, 10);
	a1 = trtl_dbg_read_reg(dbg, 11);
	trtl_dbg_write_reg(dbg, 10, addr);
	out->size = 0;
	for (; n > 0; --n) {
		uint8_t b;

		trtl_dbg_exec_insn(dbg, 0x00054583); /* lbu a1, 0(a0) */
		trtl_dbg_exec_insn(dbg, 0x00150513); /* addi a0, a0, 1 */
		b = trtl_dbg_read_reg(dbg, 11);
		out->size += snprintf(out->data + out->size,
				      TRTL_GDB_PACKET_SIZE_MAX,
				      "%02"PRIx8, b);
	}

	trtl_dbg_write_reg(dbg, 10, a0);
	trtl_dbg_write_reg(dbg, 11, a1);

	return 0;
}

/**
 * Read a specific register
 */
static int trtl_gdb_handle_p(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	unsigned int val;
	int ret;

	ret = sscanf(in->data + 1, "%x", &val);
	if (ret != 1) {
		out->size = 0;
		return 0;
	}
	printf("0x%x\n", val);
	if (val == (0x301 + 65)) /* MISA CSR */
		out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
				     "%08x",
				     (1 << 30) | (1 << ('I' - 65)));
	else
		out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
				     "E01");

	return 0;
}

/**
 * Write a specific register
 *
 *
 * Not supported yet
 */
static int trtl_gdb_handle_P(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	out->size = 0;

	return 0;
}

/**
 * Answer to qSupported request
 */
static int trtl_gdb_handle_q_supported(struct trtl_dbg_port *dbg,
				       struct trtl_gdb_packet *out,
				       struct trtl_gdb_packet *in)
{
	out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX,
			     "PacketSize=%x", TRTL_GDB_PACKET_SIZE_MAX);

	return 0;
}

static int trtl_gdb_handle_qm(struct trtl_dbg_port *dbg,
			      struct trtl_gdb_packet *out,
			      struct trtl_gdb_packet *in)
{
	out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX, "S05");

	return 0;
}

static int trtl_gdb_handle_q(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	if (strncmp(in->data, "qSupported:", 11) == 0)
		return trtl_gdb_handle_q_supported(dbg, out, in);
	else if (strncmp(in->data, "qm", 2) == 0)
		return trtl_gdb_handle_qm(dbg, out, in);
	out->size = 0;

	return 0;
}

/**
 * Single step
 */
static int trtl_gdb_handle_s(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	uint32_t pc, npc, ra, insn;

	if (in->size > 1) {
		out->size = 0;
		return 0;
	}

	/* Get ra(x1) and pc  */
	ra = trtl_dbg_read_reg(dbg, 1);
	pc = trtl_dbg_pc_read_via_ra(dbg);

	/* Read the instruction to be executed (at pc) */
	trtl_dbg_write_reg(dbg, 1, pc);
	trtl_dbg_exec_insn(dbg, 0x0000A083) ;/* lw ra,0(ra) */
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
	insn = trtl_dbg_read_reg(dbg, 1);
	if (verbose)
		fprintf(stdout, "execute: %08"PRIx32" at pc=%08"PRIx32,
			insn, pc);

	/* Restore ra */
	trtl_dbg_write_reg(dbg, 1, ra);

	/* Execute the instruction */
	trtl_dbg_exec_insn(dbg, insn);
	trtl_dbg_exec_nop(dbg);
	trtl_dbg_exec_nop(dbg);
	switch (insn & 0x77) {
	case 0x67: /* jump */
		/* Nothing to do, PC is always updated */
		break;
	case 0x63: /* branch */
		/* Read new PC */
		ra = trtl_dbg_read_reg(dbg, 1);
		npc = trtl_dbg_pc_read_via_ra(dbg);
		trtl_dbg_write_reg(dbg, 1, ra);
		/* In case of no change, the branch has not been taken,
		   so the PC needs to be updated to the next instruction.
		   If the branch has been taken, the PC has been updated.
		   NOTE: it doesn't work in case of conditional jump to the
		   current instruction.  Maybe decode the instruction
		   further. */
		if (npc == pc)
			trtl_dbg_pc_advance_4(dbg);
		break;
	default:
		/* The instruction has been executed, the pc needs to
		   be updated */
		trtl_dbg_pc_advance_4(dbg);
		break;
	}

	out->size = snprintf(out->data, TRTL_GDB_PACKET_SIZE_MAX, "S05");

	return 0;
}

/**
 * vAttach command
 *
 * Not supported yet
 */
static int trtl_gdb_handle_v_attach(struct trtl_dbg_port *dbg,
				    struct trtl_gdb_packet *out,
				    struct trtl_gdb_packet *in)
{
	out->size = 0;
	return 0;
}

/**
 * vCont command
 *
 * Not supported yet
 */
static int trtl_gdb_handle_v_cont(struct trtl_dbg_port *dbg,
				  struct trtl_gdb_packet *out,
				  struct trtl_gdb_packet *in)
{
	out->size = 0;

	return 0;
}

/**
 * vCtrlC command
 *
 * Not supported yet
 */
static int trtl_gdb_handle_v_ctrlc(struct trtl_dbg_port *dbg,
				   struct trtl_gdb_packet *out,
				   struct trtl_gdb_packet *in)
{
	out->size = 0;

	return 0;
}

/**
 * v<name> commands
 */
static int trtl_gdb_handle_v(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	int ret = 0;

	if (strncmp(in->data, "vAttach", 7) == 0)
		ret = trtl_gdb_handle_v_attach(dbg, out, in);
	else if (strncmp(in->data, "vCont", 5) == 0)
		ret = trtl_gdb_handle_v_cont(dbg, out, in);
	else if (strncmp(in->data, "vCtrlC", 6) == 0)
		ret = trtl_gdb_handle_v_ctrlc(dbg, out, in);
	else if (strncmp(in->data, "vMustReplyEmpty:", 16) == 0)
		out->size = 0;
	else
		out->size = 0;

	return ret;
}

/**
 * Write binary data to memory
 *
 * Not supported yet
 */
static int trtl_gdb_handle_X(struct trtl_dbg_port *dbg,
			     struct trtl_gdb_packet *out,
			     struct trtl_gdb_packet *in)
{
	out->size = 0;

	return 0;
}

static trtl_gdb_command_t *gdb_packet_exec[] = {
	['c'] = trtl_gdb_handle_c,
	['D'] = trtl_gdb_handle_D,
	['g'] = trtl_gdb_handle_g,
	['G'] = trtl_gdb_handle_G,
	['H'] = trtl_gdb_handle_H,
	['k'] = trtl_gdb_handle_k,
	['M'] = trtl_gdb_handle_M,
	['m'] = trtl_gdb_handle_m,
	['p'] = trtl_gdb_handle_p,
	['P'] = trtl_gdb_handle_P,
	['q'] = trtl_gdb_handle_q,
	['s'] = trtl_gdb_handle_s,
	['v'] = trtl_gdb_handle_v,
	['v'] = trtl_gdb_handle_v,
	['X'] = trtl_gdb_handle_X,
	['?'] = trtl_gdb_handle_qm,
};

/**
 * Process incoming packet and generate the outcoming
 * @out: outgoing packet
 * @in: incoming packet
 *
 * Return: 0 on success, otherwise -1 and errno is appropriately set
 *
 * The function does not receive or send packets, it only processes them;
 * the caller will handle recv(2) and send(2)
 */
static int gdb_command(struct trtl_dbg_port *dbg,
		       struct trtl_gdb_packet *out,
		       struct trtl_gdb_packet *in)
{
	int cmd = in->data[0];
	trtl_gdb_command_t *exec;

	if (in->size == 0)
		return -1;

	exec = gdb_packet_exec[cmd];
	if (exec)
		return exec(dbg, out, in);
	out->size = 0;
	return 0;
}

static void trtl_debugger_print_packet(struct trtl_gdb_packet *pkt,
				       const char *dir)
{
	int i, start, end;

	switch (verbose) {
	case 0:
		return;
	case 1:
		start = 1;
		end = pkt->size - 3;
		break;
	default:
		start = 0;
		end = pkt->size;
		break;
	}

	fputs(dir, stdout);
	fputc(' ', stdout);
	for (i = start; i < end; ++i)
		fputc(pkt->data[i], stdout);
	fputc('\n', stdout);
	fflush(stdout);
}

/**
 * Calculate mod 256 checksum
 * @data: input data
 * @n: number of bytes
 *
 * Return: checksum value
 */
static uint8_t trtl_debugger_checksum(uint8_t *data, size_t n)
{
	uint8_t checksum = 0;
	int i;

	for (i = 0; i < n; ++i)
		checksum += data[i];

	return checksum & 0xFF;
}

/**
 * Receive a GDB packet
 * @fd: socket file descriptor
 * @pkt: packet
 *
 * Return: 0 on success, otherwise -1 and errno is appropriately set
 */
static int __trtl_debugger_recv(int fd, struct trtl_gdb_packet *pkt)
{
	pkt->size = 0;
	do {
		char c;
		int ret;
		struct pollfd p = {
				   .fd = fd,
				   .events = POLLIN,
				   .revents = 0,
		};

		ret = poll(&p, 1, 1000);
		if (ret < 0)
			return ret;
		if (ret == 0)
			continue;
		ret = recv(fd, &c, 1, 0);
		if (ret < 0)
			return -1;
		if (ret == 0) {
			errno = ENOTCONN;
			return -1;
		}
		/* FIXME should we control more ? */
		if (pkt->size == 0 && c != '$')
			continue; /* wait for the beginning */
		pkt->data[pkt->size] = c;
		pkt->size++;

		if (verbose > 2) {
			fprintf(stdout, "Building message: [%zu]: %s\n",
				pkt->size, pkt->data);
		}

		if (pkt->size > TRTL_GDB_PACKET_SIZE_MAX - 1) {
			/* -1 to leave space for the string terminator */
			errno = EINVAL;
			return -1;
		}

	} while (!(pkt->size > 3 && pkt->data[pkt->size - 3] == '#'));

	return 0;
}

/**
 * Receive a GDB packet
 * @fd: socket file descriptor
 * @pkt: packet
 *
 * Return: 0 on success, otherwise -1 and errno is appropriately set
 */
static int trtl_debugger_recv(int fd, struct trtl_gdb_packet *pkt)
{
	uint8_t checksum_l, checksum_r;
	char ack[1];
	int ret;

	ret = __trtl_debugger_recv(fd, pkt);
	if (ret < 0)
		return ret;
	trtl_debugger_print_packet(pkt, "->");

	checksum_l = trtl_debugger_checksum((uint8_t *)(pkt->data + 1),
					    pkt->size - 4);

	ret = sscanf(pkt->data + pkt->size - 2, "%02"SCNx8, &checksum_r);
	if (ret != 1) {
		fprintf(stderr, "Received invalid checksum\n");
		return -1;
	}

	/* Remove checksum and special characters $payload#checksum */
	pkt->size -= 4;
	memmove(pkt->data, pkt->data + 1, pkt->size);
	pkt->data[pkt->size] = 0;

	if (checksum_l == checksum_r) {
		ack[0] = '+';
	} else {
		ack[0] = '-';
		if (verbose)
			fprintf(stderr,
				"Invalid checksum ' (L) %x != (R) %x'\n",
				checksum_l, checksum_r);
	}

	ret = send(fd, ack, 1, 0);
	if (ret != 1) {
		fputs("Failed to send acknowledge\n", stderr);
		errno = EIO;
		return -1;
	}

	return 0;
}

/**
 * Send a GDB packet
 * @fd: socket file descriptor
 * @pkt: packet
 *
 * Return: 0 on success, otherwise -1 and errno is appropriately set
 */
static int trtl_debugger_send(int fd, struct trtl_gdb_packet *pkt)
{
	uint8_t checksum_l;
	int ret;

	checksum_l = trtl_debugger_checksum((uint8_t *)pkt->data, pkt->size);

	/* Add checksum and special characters $payload#checksum */
	memmove(pkt->data + 1, pkt->data, pkt->size);
	pkt->data[0] = '$';
	snprintf(pkt->data + 1 + pkt->size, TRTL_GDB_PACKET_SIZE_MAX,
		 "#%02x", checksum_l);
	pkt->size += 4; /* 1 $, 1 #, 2 checksum */

	trtl_debugger_print_packet(pkt, "<-");

	ret = send(fd, pkt->data, pkt->size, 0);
	if (ret < 0)
		return -1;
	if (ret != pkt->size) {
		errno = EIO;
		return -1;
	}

	return 0;
}

/**
 * Run GDB server
 * @addr: MMAP address
 *
 * Return: 0 on success, otherwise -1 and errno is appropriately set
 */
static int trtl_debugger_run(struct trtl_dbg_port *dbg)
{
	bool run = true;
	int ret;
	struct trtl_gdb_packet *pkt, *in, *out;

	pkt = calloc(2, sizeof(struct trtl_gdb_packet));
	if (!pkt) {
		fprintf(stderr, "Memory allocation failed: %s\n",
			strerror(errno));
		return -1;
	}
	in = &pkt[0];
	out = &pkt[1];
	ret = trtl_dbg_debug_mode_force_set(dbg);
	if (ret < 0) {
		fprintf(stderr, "Failed to set debug mode\n");
		return -1;
	}

	fputs("Start receiving messages from GDB\n", stdout);
	while (run) {
		memset(in, 0, sizeof(*in));
		memset(out, 0, sizeof(*out));

		ret = trtl_debugger_recv(dbg->fd, in);
		if (ret) {
			if (errno == ENOTCONN)
				run = false;
			else
				fprintf(stderr,
					"Failed to receive message: %s\n",
					strerror(errno));
			continue;
		}

		ret = gdb_command(dbg, out, in);
		if (ret < 0)
			continue;

		ret = trtl_debugger_send(dbg->fd, out);
		if (ret) {
			fprintf(stderr, "Failed to send message: %s\n",
				strerror(errno));
		}
	}

	free(pkt);

	return 0;
}

static void wrpc_gdbserver_help(char *prog)
{
	const char *mapping_help_str;

	mapping_help_str = dev_mapping_help();
	fprintf(stderr, "%s [options]\n", prog);
	fprintf(stderr, "%s\n", mapping_help_str);
	fprintf(stderr, "(offset should be 0, address is the wrpc registers base)\n");
	fprintf(stderr, "wrpc-gdbserver options:\n");
	fprintf(stderr, " -p PORT       listen on tcp port PORT\n");
	fprintf(stderr, " -v            verbose\n");
	fprintf(stderr, " -t            enable terminal\n");
}

#define MEMPATH_LEN 128
int main(int argc, char *argv[])
{
	int gdb_port = 7471;
	int c, ret, sfd, ret_exit = EXIT_SUCCESS, optval;
	struct trtl_dbg_port dbg;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	struct mapping_args *map_args;
	struct mapping_desc *mdesc = NULL;

	map_args = dev_parse_mapping_args(&argc, argv);
	if (!map_args) {
		wrpc_gdbserver_help(argv[0]);
		goto out;
	}

	memset(&dbg, 0, sizeof(dbg));
	while ((c = getopt(argc, argv, "hp:vst")) != -1) {
		switch (c) {
		case 'h':
		case '?':
			wrpc_gdbserver_help(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 'p':
			gdb_port = atoi(optarg);
			if (gdb_port == 0) {
				fprintf(stderr, "bad port value\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'v':
			verbose++;
			break;
		case 's':
			swapping = 1;
			break;
		case 't':
			flag_term = 1;
			break;
		}
	}

	mdesc = dev_map(map_args, getpagesize());
	if (!mdesc) {
		fprintf(stderr, "%s: failed to map: %s\n", argv[0],
			strerror(errno));
		goto out;
	}

	dbg.addr = (void *)mdesc->base + 0xb00;
	swapping = mdesc->is_be ^ swapping;
	dbg.vuart_map.base = (void *)mdesc->base + 0x500;
	dbg.vuart_map.is_be = swapping;

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd < 0) {
		fprintf(stderr, "Failed to open a socket: %s\n",
			strerror(errno));
		ret_exit = EXIT_FAILURE;
		goto out_sock;
	}

	optval = 1;
	ret = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
			 &optval, sizeof(optval));
	if (ret < 0) {
		fprintf(stderr, "Failed to set REUSEADDR option: %s\n",
			strerror(errno));
		ret_exit = EXIT_FAILURE;
		goto out_sock;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(gdb_port);
	ret = bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		fprintf(stderr, "Failed to bind to an *:%d: %s\n",
			gdb_port, strerror(errno));
		ret_exit = EXIT_FAILURE;
		goto out_sock;
	}

	printf ("Waiting for connection on port %d\n", gdb_port);

	ret = listen(sfd, 1);
	if (ret < 0) {
		fprintf(stderr, "Failed to listen: %s\n",
			strerror(errno));
		ret_exit = EXIT_FAILURE;
		goto out_bind;
	}

	dbg.fd = accept(sfd, (struct sockaddr *)&client_addr, &client_len);
	if (dbg.fd < 0) {
		fprintf(stderr, "Failed to accept: %s\n",
			strerror(errno));
		ret_exit = EXIT_FAILURE;
		goto out_bind;
	}
	fprintf(stdout, "Accepted connection from %s\n",
		inet_ntoa(client_addr.sin_addr));

	ret = trtl_debugger_run(&dbg);
	if (ret < 0) {
		ret_exit = EXIT_FAILURE;
	}

out_bind:
out_sock:
	close(sfd);
	dev_unmap(mdesc);
out:
	exit(ret_exit);
}
