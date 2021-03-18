/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#include <stdint.h>
#include <inttypes.h>

#include "ark_pktdir.h"
#include "ark_global.h"
#include "ark_logs.h"


ark_pkt_dir_t
ark_pktdir_init(void *base)
{
	struct ark_pkt_dir_inst *inst =
		rte_malloc("ark_pkt_dir_inst",
			   sizeof(struct ark_pkt_dir_inst),
			   0);
	if (inst == NULL) {
		ARK_PMD_LOG(ERR, "Failed to malloc ark_pkt_dir_inst.\n");
		return inst;
	}
	inst->regs = (struct ark_pkt_dir_regs *)base;
	inst->regs->ctrl = ARK_PKT_DIR_INIT_VAL; /* POR state */
	return inst;
}

void
ark_pktdir_uninit(ark_pkt_dir_t handle)
{
	struct ark_pkt_dir_inst *inst = (struct ark_pkt_dir_inst *)handle;

	rte_free(inst);
}

void
ark_pktdir_setup(ark_pkt_dir_t handle, uint32_t v)
{
	struct ark_pkt_dir_inst *inst = (struct ark_pkt_dir_inst *)handle;
	inst->regs->ctrl = v;
}

uint32_t
ark_pktdir_status(ark_pkt_dir_t handle)
{
	struct ark_pkt_dir_inst *inst = (struct ark_pkt_dir_inst *)handle;
	return inst->regs->ctrl;
}

uint32_t
ark_pktdir_stall_cnt(ark_pkt_dir_t handle)
{
	struct ark_pkt_dir_inst *inst = (struct ark_pkt_dir_inst *)handle;
	return inst->regs->stall_cnt;
}
