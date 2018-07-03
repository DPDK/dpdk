/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_PKTCHKR_H_
#define _ARK_PKTCHKR_H_

#include <stdint.h>
#include <inttypes.h>

#define ARK_PKTCHKR_BASE_ADR  0x90000

typedef void *ark_pkt_chkr_t;

/* The packet checker is an internal Arkville hardware module, which
 * verifies packet streams generated from the corresponding packet
 * generator.  This module is used for Arkville testing.
 * This module is *not* intended for end-user manipulation, hence
 * there is minimal documentation.
 */

/*
 * This are overlay structures to a memory mapped FPGA device.  These
 * structs will never be instantiated in ram memory
 */
struct ark_pkt_chkr_stat_regs {
	uint32_t r0;
	uint32_t pkt_start_stop;
	uint32_t pkt_ctrl;
	uint32_t pkts_rcvd;
	uint64_t bytes_rcvd;
	uint32_t pkts_ok;
	uint32_t pkts_mismatch;
	uint32_t pkts_err;
	uint32_t first_mismatch;
	uint32_t resync_events;
	uint32_t pkts_missing;
	uint32_t min_latency;
	uint32_t max_latency;
} __attribute__ ((packed));

struct ark_pkt_chkr_ctl_regs {
	uint32_t pkt_ctrl;
	uint32_t pkt_payload;
	uint32_t pkt_size_min;
	uint32_t pkt_size_max;
	uint32_t pkt_size_incr;
	uint32_t num_pkts;
	uint32_t pkts_sent;
	uint32_t src_mac_addr_l;
	uint32_t src_mac_addr_h;
	uint32_t dst_mac_addr_l;
	uint32_t dst_mac_addr_h;
	uint32_t eth_type;
	uint32_t hdr_dw[7];
} __attribute__ ((packed));

struct ark_pkt_chkr_inst {
	struct rte_eth_dev_info *dev_info;
	volatile struct ark_pkt_chkr_stat_regs *sregs;
	volatile struct ark_pkt_chkr_ctl_regs *cregs;
	int l2_mode;
	int ordinal;
};

/*  packet checker functions */
ark_pkt_chkr_t ark_pktchkr_init(void *addr, int ord, int l2_mode);
void ark_pktchkr_uninit(ark_pkt_chkr_t handle);
void ark_pktchkr_run(ark_pkt_chkr_t handle);
int ark_pktchkr_stopped(ark_pkt_chkr_t handle);
void ark_pktchkr_stop(ark_pkt_chkr_t handle);
int ark_pktchkr_is_running(ark_pkt_chkr_t handle);
int ark_pktchkr_get_pkts_sent(ark_pkt_chkr_t handle);
void ark_pktchkr_set_payload_byte(ark_pkt_chkr_t handle, uint32_t b);
void ark_pktchkr_set_pkt_size_min(ark_pkt_chkr_t handle, uint32_t x);
void ark_pktchkr_set_pkt_size_max(ark_pkt_chkr_t handle, uint32_t x);
void ark_pktchkr_set_pkt_size_incr(ark_pkt_chkr_t handle, uint32_t x);
void ark_pktchkr_set_num_pkts(ark_pkt_chkr_t handle, uint32_t x);
void ark_pktchkr_set_src_mac_addr(ark_pkt_chkr_t handle, uint64_t mac_addr);
void ark_pktchkr_set_dst_mac_addr(ark_pkt_chkr_t handle, uint64_t mac_addr);
void ark_pktchkr_set_eth_type(ark_pkt_chkr_t handle, uint32_t x);
void ark_pktchkr_set_hdr_dW(ark_pkt_chkr_t handle, uint32_t *hdr);
void ark_pktchkr_parse(char *args);
void ark_pktchkr_setup(ark_pkt_chkr_t handle);
void ark_pktchkr_dump_stats(ark_pkt_chkr_t handle);
int ark_pktchkr_wait_done(ark_pkt_chkr_t handle);

#endif
