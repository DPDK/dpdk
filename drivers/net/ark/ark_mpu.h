/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_MPU_H_
#define _ARK_MPU_H_

#include <stdint.h>

#include <rte_memory.h>

/* The MPU or Memory Prefetch Unit is an internal Arkville hardware
 * module for moving data between host memory and the hardware FPGA.
 * This module is *not* intended for end-user manipulation, hence
 * there is minimal documentation.
 */

/*
 * MPU hardware structures
 * These are overlay structures to a memory mapped FPGA device.  These
 * structs will never be instantiated in ram memory
 */

#define ARK_MPU_ID 0x00
struct ark_mpu_id_t {
	union {
		char id[4];
		uint32_t idnum;
	};
	union {
		char ver[4];
		uint32_t vernum;
	};
	uint32_t phys_id;
	uint32_t mrr_code;
};

#define ARK_MPU_HW 0x010
struct ark_mpu_hw_t {
	uint16_t num_queues;
	uint16_t reserved;
	uint32_t hw_depth;
	uint32_t obj_size;
	uint32_t obj_per_mrr;
};

#define ARK_MPU_CFG 0x040
struct ark_mpu_cfg_t {
	rte_iova_t ring_base;	/* rte_iova_t is a uint64_t */
	uint32_t ring_size;
	uint32_t ring_mask;
	uint32_t min_host_move;
	uint32_t min_hw_move;
	volatile uint32_t sw_prod_index;
	volatile uint32_t hw_cons_index;
	volatile uint32_t command;
};
enum ARK_MPU_COMMAND {
	MPU_CMD_IDLE = 1,
	MPU_CMD_RUN = 2,
	MPU_CMD_STOP = 4,
	MPU_CMD_RESET =	8,
	MPU_CMD_FORCE_RESET = 16,
	MPU_COMMAND_LIMIT = 0xfFFFFFFF
};

#define ARK_MPU_STATS 0x080
struct ark_mpu_stats_t {
	volatile uint64_t pci_request;
	volatile uint64_t q_empty;
	volatile uint64_t q_q1;
	volatile uint64_t q_q2;
	volatile uint64_t q_q3;
	volatile uint64_t q_q4;
	volatile uint64_t q_full;
};

#define ARK_MPU_DEBUG 0x0C0
struct ark_mpu_debug_t {
	volatile uint32_t state;
	uint32_t reserved;
	volatile uint32_t count;
	volatile uint32_t take;
	volatile uint32_t peek[4];
};

/*  Consolidated structure */
struct ark_mpu_t {
	struct ark_mpu_id_t id;
	uint8_t reserved0[(ARK_MPU_HW - ARK_MPU_ID)
			  - sizeof(struct ark_mpu_id_t)];
	struct ark_mpu_hw_t hw;
	uint8_t reserved1[(ARK_MPU_CFG - ARK_MPU_HW) -
			  sizeof(struct ark_mpu_hw_t)];
	struct ark_mpu_cfg_t cfg;
	uint8_t reserved2[(ARK_MPU_STATS - ARK_MPU_CFG) -
			  sizeof(struct ark_mpu_cfg_t)];
	struct ark_mpu_stats_t stats;
	uint8_t reserved3[(ARK_MPU_DEBUG - ARK_MPU_STATS) -
			  sizeof(struct ark_mpu_stats_t)];
	struct ark_mpu_debug_t debug;
};

uint16_t ark_api_num_queues(struct ark_mpu_t *mpu);
uint16_t ark_api_num_queues_per_port(struct ark_mpu_t *mpu,
				     uint16_t ark_ports);
int ark_mpu_verify(struct ark_mpu_t *mpu, uint32_t obj_size);
void ark_mpu_stop(struct ark_mpu_t *mpu);
void ark_mpu_start(struct ark_mpu_t *mpu);
int ark_mpu_reset(struct ark_mpu_t *mpu);
int ark_mpu_configure(struct ark_mpu_t *mpu, rte_iova_t ring,
		      uint32_t ring_size, int is_tx);

void ark_mpu_dump(struct ark_mpu_t *mpu, const char *msg, uint16_t idx);
void ark_mpu_dump_setup(struct ark_mpu_t *mpu, uint16_t qid);
void ark_mpu_reset_stats(struct ark_mpu_t *mpu);

/*  this action is in a performance critical path */
static inline void
ark_mpu_set_producer(struct ark_mpu_t *mpu, uint32_t idx)
{
	mpu->cfg.sw_prod_index = idx;
}

#endif
