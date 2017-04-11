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

#ifndef _ARK_RQP_H_
#define _ARK_RQP_H_

#include <stdint.h>

#include <rte_memory.h>

/* The RQP or ReQuest Pacer is an internal Arkville hardware module
 * which limits the PCIE data flow to insure correct operation for the
 * particular hardware PCIE endpoint.
 * This module is *not* intended for end-user manipulation, hence
 * there is minimal documentation.
 */

/*
 * RQ Pacing core hardware structure
 * This is an overlay structures to a memory mapped FPGA device.  These
 * structs will never be instantiated in ram memory
 */
struct ark_rqpace_t {
	volatile uint32_t ctrl;
	volatile uint32_t stats_clear;
	volatile uint32_t cplh_max;
	volatile uint32_t cpld_max;
	volatile uint32_t err_cnt;
	volatile uint32_t stall_ps;
	volatile uint32_t stall_ps_min;
	volatile uint32_t stall_ps_max;
	volatile uint32_t req_ps;
	volatile uint32_t req_ps_min;
	volatile uint32_t req_ps_max;
	volatile uint32_t req_dw_ps;
	volatile uint32_t req_dw_ps_min;
	volatile uint32_t req_dw_ps_max;
	volatile uint32_t cpl_ps;
	volatile uint32_t cpl_ps_min;
	volatile uint32_t cpl_ps_max;
	volatile uint32_t cpl_dw_ps;
	volatile uint32_t cpl_dw_ps_min;
	volatile uint32_t cpl_dw_ps_max;
	volatile uint32_t cplh_pending;
	volatile uint32_t cpld_pending;
	volatile uint32_t cplh_pending_max;
	volatile uint32_t cpld_pending_max;
	volatile uint32_t err_count_other;
	char eval[4];
	volatile int lasped;
};

void ark_rqp_dump(struct ark_rqpace_t *rqp);
void ark_rqp_stats_reset(struct ark_rqpace_t *rqp);
int ark_rqp_lasped(struct ark_rqpace_t *rqp);
#endif
