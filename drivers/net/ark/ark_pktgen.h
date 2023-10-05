/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_PKTGEN_H_
#define _ARK_PKTGEN_H_

#include <stdint.h>
#include <inttypes.h>

#define ARK_PKTGEN_BASE_ADR  0x10000

typedef void *ark_pkt_gen_t;

/* The packet generator is an internal Arkville hardware module, which
 * generates known packets for use in integrity and line-rate testing.
 * This module is *not* intended for end-user manipulation, hence
 * there is minimal documentation.
 */

/*
 * This is an overlay structure to a memory mapped FPGA device.  These
 * structs will never be instantiated in ram memory
 */
struct ark_pkt_gen_regs {
	uint32_t r0;
	volatile uint32_t pkt_start_stop;
	volatile uint32_t pkt_ctrl;
	uint32_t pkt_payload;
	uint32_t pkt_spacing;
	uint32_t pkt_size_min;
	uint32_t pkt_size_max;
	uint32_t pkt_size_incr;
	volatile uint32_t num_pkts;
	volatile uint32_t pkts_sent;
	uint32_t src_mac_addr_l;
	uint32_t src_mac_addr_h;
	uint32_t dst_mac_addr_l;
	uint32_t dst_mac_addr_h;
	uint32_t eth_type;
	uint32_t hdr_dw[7];
	uint32_t start_offset;
	uint32_t bytes_per_cycle;
} __rte_packed;

struct ark_pkt_gen_inst {
	struct rte_eth_dev_info *dev_info;
	struct ark_pkt_gen_regs *regs;
	int l2_mode;
	int ordinal;
};

/*  packet generator functions */
ark_pkt_gen_t ark_pktgen_init(void *arg, int ord, int l2_mode);
void ark_pktgen_uninit(ark_pkt_gen_t handle);
void ark_pktgen_run(ark_pkt_gen_t handle);
void ark_pktgen_pause(ark_pkt_gen_t handle);
uint32_t ark_pktgen_paused(ark_pkt_gen_t handle);
uint32_t ark_pktgen_is_gen_forever(ark_pkt_gen_t handle);
uint32_t ark_pktgen_is_running(ark_pkt_gen_t handle);
uint32_t ark_pktgen_tx_done(ark_pkt_gen_t handle);
void ark_pktgen_reset(ark_pkt_gen_t handle);
void ark_pktgen_wait_done(ark_pkt_gen_t handle);
uint32_t ark_pktgen_get_pkts_sent(ark_pkt_gen_t handle);
void ark_pktgen_set_payload_byte(ark_pkt_gen_t handle, uint32_t b);
void ark_pktgen_set_pkt_spacing(ark_pkt_gen_t handle, uint32_t x);
void ark_pktgen_set_pkt_size_min(ark_pkt_gen_t handle, uint32_t x);
void ark_pktgen_set_pkt_size_max(ark_pkt_gen_t handle, uint32_t x);
void ark_pktgen_set_pkt_size_incr(ark_pkt_gen_t handle, uint32_t x);
void ark_pktgen_set_num_pkts(ark_pkt_gen_t handle, uint32_t x);
void ark_pktgen_set_src_mac_addr(ark_pkt_gen_t handle, uint64_t mac_addr);
void ark_pktgen_set_dst_mac_addr(ark_pkt_gen_t handle, uint64_t mac_addr);
void ark_pktgen_set_eth_type(ark_pkt_gen_t handle, uint32_t x);
void ark_pktgen_set_hdr_dW(ark_pkt_gen_t handle, uint32_t *hdr);
void ark_pktgen_set_start_offset(ark_pkt_gen_t handle, uint32_t x);
void ark_pktgen_parse(char *argv);
void ark_pktgen_setup(ark_pkt_gen_t handle);
uint32_t ark_pktgen_delay_start(void *arg);

#endif
