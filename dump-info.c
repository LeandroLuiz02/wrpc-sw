#include <sys/types.h>
#include <ppsi/ppsi.h>
#include <softpll_ng.h>
#include <wrc_global.h>

#include <wrpc.h>

#include "dump-info.h"

struct dump_info  dump_wrpc_info[] = {
#undef DUMP_STRUCT
#define DUMP_STRUCT struct wrc_global
	DUMP_HEADER("wrc_global"),
	DUMP_FIELD(uint32_t, magic),
	DUMP_FIELD(uint32_t, version),
	DUMP_FIELD_SIZE(char, wrc_hw_name, HW_NAME_LENGTH),
	DUMP_FIELD(pointer, link_status),
	DUMP_FIELD(int, task_list_max),
	DUMP_FIELD(pointer, task_list),
	DUMP_FIELD(pointer, config),
	DUMP_FIELD(pointer, softpll),
	DUMP_FIELD(pointer, pll_fifo),

#undef DUMP_STRUCT
#define DUMP_STRUCT struct wrc_global_link
	DUMP_HEADER("wrc_global_link"),
	DUMP_FIELD(uint32_t, version),
	DUMP_FIELD(link_up_status, link_up),
	DUMP_FIELD(int, vlan),

#undef DUMP_STRUCT
#define DUMP_STRUCT struct wrc_task
	/* Save the size of the structure, there is no other way to pass
	 * the size of wrc_task structure */
	DUMP_HEADER_SIZE("wrc_task", sizeof(struct wrc_task)),
	DUMP_FIELD(int, used),
	DUMP_FIELD_SIZE(char, name, 16),
	DUMP_FIELD(pointer, enabled), /* pointer to a function */
	DUMP_FIELD(pointer, init), /* pointer to a function */
	DUMP_FIELD(pointer, job), /* pointer to a function */
	DUMP_FIELD(unsigned_long, nrun),
	DUMP_FIELD(unsigned_long, seconds),
	DUMP_FIELD(unsigned_long, nanos),
	DUMP_FIELD(unsigned_long, max_run_ticks), /* in ticks */

#undef DUMP_STRUCT
#define DUMP_STRUCT struct softpll_state

	DUMP_HEADER("struct_softpll"),
	DUMP_FIELD(spll_mode, mode),
	DUMP_FIELD(int, seq_state),
	DUMP_FIELD(int, dac_timeout),
	DUMP_FIELD(int, delock_count),
	DUMP_FIELD(uint32_t, irq_count),
	DUMP_FIELD(int, mpll_shift_ps),
	DUMP_FIELD(int, helper.p_adder),
	DUMP_FIELD(int, helper.p_setpoint),
	DUMP_FIELD(int, helper.tag_d0),
	DUMP_FIELD(int, helper.ref_src),
	DUMP_FIELD(int, helper.sample_n),
	/* FIXME: missing helper.pi etc.. */
	DUMP_FIELD(yes_no, ext.enabled),
	DUMP_FIELD(int, ext.align_state),
	DUMP_FIELD(int, ext.align_timer),
	DUMP_FIELD(int, ext.align_target),
	DUMP_FIELD(int, ext.align_step),
	DUMP_FIELD(int, ext.align_shift),
	DUMP_FIELD(int, mpll.state),
	/* FIXME: mpll.pi etc */
	DUMP_FIELD(int, mpll.adder_ref),
	DUMP_FIELD(int, mpll.adder_out),
	DUMP_FIELD(int, mpll.tag_ref),
	DUMP_FIELD(int, mpll.tag_out),
	DUMP_FIELD(int, mpll.tag_ref_d),
	DUMP_FIELD(int, mpll.tag_out_d),
	DUMP_FIELD(int, mpll.phase_shift_target),
	DUMP_FIELD(int, mpll.phase_shift_current),
	DUMP_FIELD(int, mpll.id_ref),
	DUMP_FIELD(int, mpll.id_out),
	DUMP_FIELD(int, mpll.sample_n),
	DUMP_FIELD(int, mpll.dac_index),
	DUMP_FIELD(yes_no, mpll.enabled),

#undef DUMP_STRUCT
#define DUMP_STRUCT struct spll_fifo_log

	DUMP_HEADER_SIZE("struct_pll_fifo", sizeof(struct spll_fifo_log)),
	DUMP_FIELD(uint32_t, trr),
	DUMP_FIELD(uint32_t, tstamp),
	DUMP_FIELD(uint32_t, duration),
	DUMP_FIELD(uint16_t, irq_count),
	DUMP_FIELD(uint16_t, tag_count),
	/* FIXME: aux_state and ptracker_state -- variable-len arrays */

#undef DUMP_STRUCT
#define DUMP_STRUCT struct spll_stats

	DUMP_HEADER("stats"),
	DUMP_FIELD(uint32_t, magic),
	DUMP_FIELD(int, ver),
	DUMP_FIELD(int, sequence),
	DUMP_FIELD(spll_mode, mode),
	DUMP_FIELD(int, irq_cnt),
	DUMP_FIELD(int, seq_state),
	DUMP_FIELD(int, align_state),
	DUMP_FIELD(int, H_lock),
	DUMP_FIELD(int, M_lock),
	DUMP_FIELD(int, H_y),
	DUMP_FIELD(int, M_y),
	DUMP_FIELD(int, del_cnt),
	DUMP_FIELD(int, start_cnt),
	DUMP_FIELD_SIZE(char, commit_id, 32),
	DUMP_FIELD_SIZE(char, build_date, 16),
	DUMP_FIELD_SIZE(char, build_time, 16),
	DUMP_FIELD_SIZE(char, build_by, 32),

	DUMP_HEADER("end"),

};
