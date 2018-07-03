/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
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
