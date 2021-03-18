/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_DDM_H_
#define _ARK_DDM_H_

#include <stdint.h>

#include <rte_memory.h>


/* The DDM or Downstream Data Mover is an internal Arkville hardware
 * module for moving packet from host memory to the TX packet streams.
 * This module is *not* intended for end-user manipulation, hence
 * there is minimal documentation.
 */

/* struct defining Tx meta data --  fixed in FPGA -- 8 bytes */
union ark_tx_meta {
	uint64_t physaddr;
	struct {
		uint32_t usermeta0;
		uint32_t usermeta1;
	};
	struct {
		uint16_t data_len;	/* of this MBUF */
#define   ARK_DDM_EOP   0x01
#define   ARK_DDM_SOP   0x02
		uint8_t  flags;
		uint8_t  meta_cnt;
		uint32_t user1;
	};
} __rte_packed;

/*
 * DDM core hardware structures
 * These are overlay structures to a memory mapped FPGA device.  These
 * structs will never be instantiated in ram memory
 */
#define ARK_DDM_CFG 0x0000
/* Set unique HW ID for hardware version */
#define ARK_DDM_CONST3 (0x334d4444)
#define ARK_DDM_CONST2 (0x324d4444)
#define ARK_DDM_CONST1 (0xfacecafe)

struct ark_ddm_cfg_t {
	uint32_t r0;
	volatile uint32_t tlp_stats_clear;
	uint32_t const0;
	volatile uint32_t tag_max;
	volatile uint32_t command;
	volatile uint32_t stop_flushed;
};

#define ARK_DDM_STATS 0x0020
struct ark_ddm_stats_t {
	volatile uint64_t tx_byte_count;
	volatile uint64_t tx_pkt_count;
	volatile uint64_t tx_mbuf_count;
};

#define ARK_DDM_MRDQ 0x0040
struct ark_ddm_mrdq_t {
	volatile uint32_t mrd_q1;
	volatile uint32_t mrd_q2;
	volatile uint32_t mrd_q3;
	volatile uint32_t mrd_q4;
	volatile uint32_t mrd_full;
};

#define ARK_DDM_CPLDQ 0x0068
struct ark_ddm_cpldq_t {
	volatile uint32_t cpld_q1;
	volatile uint32_t cpld_q2;
	volatile uint32_t cpld_q3;
	volatile uint32_t cpld_q4;
	volatile uint32_t cpld_full;
};

#define ARK_DDM_MRD_PS 0x0090
struct ark_ddm_mrd_ps_t {
	volatile uint32_t mrd_ps_min;
	volatile uint32_t mrd_ps_max;
	volatile uint32_t mrd_full_ps_min;
	volatile uint32_t mrd_full_ps_max;
	volatile uint32_t mrd_dw_ps_min;
	volatile uint32_t mrd_dw_ps_max;
};

#define ARK_DDM_QUEUE_STATS 0x00a8
struct ark_ddm_qstats_t {
	volatile uint64_t byte_count;
	volatile uint64_t pkt_count;
	volatile uint64_t mbuf_count;
};

#define ARK_DDM_CPLD_PS 0x00c0
struct ark_ddm_cpld_ps_t {
	volatile uint32_t cpld_ps_min;
	volatile uint32_t cpld_ps_max;
	volatile uint32_t cpld_full_ps_min;
	volatile uint32_t cpld_full_ps_max;
	volatile uint32_t cpld_dw_ps_min;
	volatile uint32_t cpld_dw_ps_max;
};

#define ARK_DDM_SETUP  0x00e0
struct ark_ddm_setup_t {
	rte_iova_t cons_write_index_addr;
	uint32_t write_index_interval;	/* 4ns each */
	volatile uint32_t cons_index;
};

#define ARK_DDM_EXPECTED_SIZE 256
#define ARK_DDM_QOFFSET ARK_DDM_EXPECTED_SIZE
/*  Consolidated structure */
struct ark_ddm_t {
	struct ark_ddm_cfg_t cfg;
	uint8_t reserved0[(ARK_DDM_STATS - ARK_DDM_CFG) -
			  sizeof(struct ark_ddm_cfg_t)];
	struct ark_ddm_stats_t stats;
	uint8_t reserved1[(ARK_DDM_MRDQ - ARK_DDM_STATS) -
			  sizeof(struct ark_ddm_stats_t)];
	struct ark_ddm_mrdq_t mrdq;
	uint8_t reserved2[(ARK_DDM_CPLDQ - ARK_DDM_MRDQ) -
			  sizeof(struct ark_ddm_mrdq_t)];
	struct ark_ddm_cpldq_t cpldq;
	uint8_t reserved3[(ARK_DDM_MRD_PS - ARK_DDM_CPLDQ) -
			  sizeof(struct ark_ddm_cpldq_t)];
	struct ark_ddm_mrd_ps_t mrd_ps;
	struct ark_ddm_qstats_t queue_stats;
	struct ark_ddm_cpld_ps_t cpld_ps;
	uint8_t reserved5[(ARK_DDM_SETUP - ARK_DDM_CPLD_PS) -
			  sizeof(struct ark_ddm_cpld_ps_t)];
	struct ark_ddm_setup_t setup;
	uint8_t reserved_p[(ARK_DDM_EXPECTED_SIZE - ARK_DDM_SETUP) -
			   sizeof(struct ark_ddm_setup_t)];
};


/* DDM function prototype */
int ark_ddm_verify(struct ark_ddm_t *ddm);
void ark_ddm_start(struct ark_ddm_t *ddm);
int ark_ddm_stop(struct ark_ddm_t *ddm, const int wait);
void ark_ddm_reset(struct ark_ddm_t *ddm);
void ark_ddm_stats_reset(struct ark_ddm_t *ddm);
void ark_ddm_setup(struct ark_ddm_t *ddm, rte_iova_t cons_addr,
		   uint32_t interval);
void ark_ddm_dump_stats(struct ark_ddm_t *ddm, const char *msg);
void ark_ddm_dump(struct ark_ddm_t *ddm, const char *msg);
int ark_ddm_is_stopped(struct ark_ddm_t *ddm);
uint64_t ark_ddm_queue_byte_count(struct ark_ddm_t *ddm);
uint64_t ark_ddm_queue_pkt_count(struct ark_ddm_t *ddm);
void ark_ddm_queue_reset_stats(struct ark_ddm_t *ddm);

#endif
