/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_PKTDIR_H_
#define _ARK_PKTDIR_H_

#include <stdint.h>

#define ARK_PKT_DIR_INIT_VAL 0x0110

typedef void *ark_pkt_dir_t;


/* The packet director is an internal Arkville hardware module for
 * directing packet data in non-typical flows, such as testing.
 * This module is *not* intended for end-user manipulation, hence
 * there is minimal documentation.
 */

/*
 * This is an overlay structures to a memory mapped FPGA device.  These
 * structs will never be instantiated in ram memory
 */
struct ark_pkt_dir_regs {
	uint32_t ctrl;
	uint32_t status;
	uint32_t stall_cnt;
} __rte_packed;

struct ark_pkt_dir_inst {
	volatile struct ark_pkt_dir_regs *regs;
};

ark_pkt_dir_t ark_pktdir_init(void *base);
void ark_pktdir_uninit(ark_pkt_dir_t handle);
void ark_pktdir_setup(ark_pkt_dir_t handle, uint32_t v);
uint32_t ark_pktdir_stall_cnt(ark_pkt_dir_t handle);
uint32_t ark_pktdir_status(ark_pkt_dir_t handle);

#endif
