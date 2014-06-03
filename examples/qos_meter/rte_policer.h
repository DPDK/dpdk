/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __INCLUDE_RTE_POLICER_H__
#define __INCLUDE_RTE_POLICER_H__

#include <stdint.h>
#include <rte_meter.h>

enum rte_phb_action {
	e_RTE_PHB_ACTION_GREEN = e_RTE_METER_GREEN,
	e_RTE_PHB_ACTION_YELLOW = e_RTE_METER_YELLOW,
	e_RTE_PHB_ACTION_RED = e_RTE_METER_RED,
	e_RTE_PHB_ACTION_DROP = 3,
};

struct rte_phb {
	enum rte_phb_action actions[e_RTE_METER_COLORS][e_RTE_METER_COLORS];
};

int
rte_phb_config(struct rte_phb *phb_table, uint32_t phb_table_index,
	enum rte_meter_color pre_meter, enum rte_meter_color post_meter, enum rte_phb_action action);

static inline enum rte_phb_action
policer_run(struct rte_phb *phb_table, uint32_t phb_table_index, enum rte_meter_color pre_meter, enum rte_meter_color post_meter)
{
	struct rte_phb *phb = &phb_table[phb_table_index];
	enum rte_phb_action action = phb->actions[pre_meter][post_meter];

	return action;
}

#endif
