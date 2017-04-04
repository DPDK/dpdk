/*-
 * BSD LICENSE
 *
 * Copyright (c) 2015-2017 Atomic Rules LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <inttypes.h>

#include "ark_pktdir.h"
#include "ark_global.h"


ark_pkt_dir_t
ark_pktdir_init(void *base)
{
	struct ark_pkt_dir_inst *inst =
		rte_malloc("ark_pkt_dir_inst",
			   sizeof(struct ark_pkt_dir_inst),
			   0);
	inst->regs = (struct ark_pkt_dir_regs *)base;
	inst->regs->ctrl = 0x00110110;	/* POR state */
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
