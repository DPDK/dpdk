/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_UDM_H_
#define _ARK_UDM_H_

#include <stdint.h>

#include <rte_memory.h>

/* The UDM or Upstream Data Mover is an internal Arkville hardware
 * module for moving packet from the RX packet streams to host memory.
 * This module is *not* intended for end-user manipulation, hence
 * there is minimal documentation.
 */

/* Meta data structure passed from FPGA, must match layout in FPGA
 * -- 32 bytes
 */
struct ark_rx_meta {
	uint32_t user_meta[5];	/* user defined based on fpga code */
	uint8_t  reserved[10];
	uint16_t pkt_len;
} __rte_packed;

/*
 * UDM hardware structures
 * These are overlay structures to a memory mapped FPGA device.  These
 * structs will never be instantiated in ram memory
 */

#define ARK_RX_WRITE_TIME_NS 2500
#define ARK_UDM_SETUP 0
#define ARK_UDM_CONST2 0xbACECACE
#define ARK_UDM_CONST3 0x334d4455
#define ARK_UDM_CONST ARK_UDM_CONST3
struct ark_udm_setup_t {
	uint32_t r0;
	uint32_t r4;
	volatile uint32_t cycle_count;
	uint32_t const0;
};

#define ARK_UDM_CFG 0x010
struct ark_udm_cfg_t {
	volatile uint32_t stop_flushed;	/* RO */
	volatile uint32_t command;
	uint32_t dataroom;
	uint32_t headroom;
};

typedef enum {
	ARK_UDM_START = 0x1,
	ARK_UDM_STOP = 0x2,
	ARK_UDM_RESET = 0x3
} ark_udm_commands;

#define ARK_UDM_STATS 0x020
struct ark_udm_stats_t {
	volatile uint64_t rx_byte_count;
	volatile uint64_t rx_packet_count;
	volatile uint64_t rx_mbuf_count;
	volatile uint64_t rx_sent_packets;
};

#define ARK_UDM_PQ 0x040
struct ark_udm_queue_stats_t {
	volatile uint64_t q_byte_count;
	volatile uint64_t q_packet_count;	/* includes drops */
	volatile uint64_t q_mbuf_count;
	volatile uint64_t q_ff_packet_count;
	volatile uint64_t q_pkt_drop;
	uint32_t q_enable;
};

#define ARK_UDM_TLP 0x0070
struct ark_udm_tlp_t {
	volatile uint64_t pkt_drop;	/* global */
	volatile uint32_t tlp_q1;
	volatile uint32_t tlp_q2;
	volatile uint32_t tlp_q3;
	volatile uint32_t tlp_q4;
	volatile uint32_t tlp_full;
};

#define ARK_UDM_PCIBP 0x00a0
struct ark_udm_pcibp_t {
	volatile uint32_t pci_clear;
	volatile uint32_t pci_empty;
	volatile uint32_t pci_q1;
	volatile uint32_t pci_q2;
	volatile uint32_t pci_q3;
	volatile uint32_t pci_q4;
	volatile uint32_t pci_full;
};

#define ARK_UDM_TLP_PS 0x00bc
struct ark_udm_tlp_ps_t {
	volatile uint32_t tlp_clear;
	volatile uint32_t tlp_ps_min;
	volatile uint32_t tlp_ps_max;
	volatile uint32_t tlp_full_ps_min;
	volatile uint32_t tlp_full_ps_max;
	volatile uint32_t tlp_dw_ps_min;
	volatile uint32_t tlp_dw_ps_max;
	volatile uint32_t tlp_pldw_ps_min;
	volatile uint32_t tlp_pldw_ps_max;
};

#define ARK_UDM_RT_CFG 0x00e0
struct ark_udm_rt_cfg_t {
	rte_iova_t hw_prod_addr;
	uint32_t write_interval;	/* 4ns cycles */
	volatile uint32_t prod_idx;	/* RO */
};

/*  Consolidated structure */
#define ARK_UDM_EXPECT_SIZE (0x00fc + 4)
#define ARK_UDM_QOFFSET ARK_UDM_EXPECT_SIZE
struct ark_udm_t {
	struct ark_udm_setup_t setup;
	struct ark_udm_cfg_t cfg;
	struct ark_udm_stats_t stats;
	struct ark_udm_queue_stats_t qstats;
	uint8_t reserved1[(ARK_UDM_TLP - ARK_UDM_PQ) -
			  sizeof(struct ark_udm_queue_stats_t)];
	struct ark_udm_tlp_t tlp;
	uint8_t reserved2[(ARK_UDM_PCIBP - ARK_UDM_TLP) -
			  sizeof(struct ark_udm_tlp_t)];
	struct ark_udm_pcibp_t pcibp;
	struct ark_udm_tlp_ps_t tlp_ps;
	struct ark_udm_rt_cfg_t rt_cfg;
	int8_t reserved3[(ARK_UDM_EXPECT_SIZE - ARK_UDM_RT_CFG) -
			 sizeof(struct ark_udm_rt_cfg_t)];
};


int ark_udm_verify(struct ark_udm_t *udm);
int ark_udm_stop(struct ark_udm_t *udm, int wait);
void ark_udm_start(struct ark_udm_t *udm);
int ark_udm_reset(struct ark_udm_t *udm);
void ark_udm_configure(struct ark_udm_t *udm,
		       uint32_t headroom,
		       uint32_t dataroom,
		       uint32_t write_interval_ns);
void ark_udm_write_addr(struct ark_udm_t *udm, rte_iova_t addr);
void ark_udm_stats_reset(struct ark_udm_t *udm);
void ark_udm_dump_stats(struct ark_udm_t *udm, const char *msg);
void ark_udm_dump_queue_stats(struct ark_udm_t *udm, const char *msg,
			      uint16_t qid);
void ark_udm_dump(struct ark_udm_t *udm, const char *msg);
void ark_udm_dump_perf(struct ark_udm_t *udm, const char *msg);
void ark_udm_dump_setup(struct ark_udm_t *udm, uint16_t q_id);
int ark_udm_is_flushed(struct ark_udm_t *udm);

/* Per queue data */
uint64_t ark_udm_dropped(struct ark_udm_t *udm);
uint64_t ark_udm_bytes(struct ark_udm_t *udm);
uint64_t ark_udm_packets(struct ark_udm_t *udm);

void ark_udm_queue_stats_reset(struct ark_udm_t *udm);
void ark_udm_queue_enable(struct ark_udm_t *udm, int enable);

#endif
