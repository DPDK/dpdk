/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_HAL_H_
#define _NITROX_HAL_H_

#include <rte_cycles.h>
#include <rte_byteorder.h>

#include "nitrox_csr.h"

union nps_pkt_slc_cnts {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t slc_int : 1;
		uint64_t uns_int : 1;
		uint64_t in_int : 1;
		uint64_t mbox_int : 1;
		uint64_t resend : 1;
		uint64_t raz : 5;
		uint64_t timer : 22;
		uint64_t cnt : 32;
#else
		uint64_t cnt : 32;
		uint64_t timer : 22;
		uint64_t raz : 5;
		uint64_t resend : 1;
		uint64_t mbox_int : 1;
		uint64_t in_int : 1;
		uint64_t uns_int : 1;
		uint64_t slc_int : 1;
#endif
	} s;
};

union nps_pkt_slc_int_levels {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t bmode : 1;
		uint64_t raz : 9;
		uint64_t timet : 22;
		uint64_t cnt : 32;
#else
		uint64_t cnt : 32;
		uint64_t timet : 22;
		uint64_t raz : 9;
		uint64_t bmode : 1;
#endif
	} s;
};

union nps_pkt_slc_ctl {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz : 61;
		uint64_t rh : 1;
		uint64_t z : 1;
		uint64_t enb : 1;
#else
		uint64_t enb : 1;
		uint64_t z : 1;
		uint64_t rh : 1;
		uint64_t raz : 61;
#endif
	} s;
};

union nps_pkt_in_instr_ctl {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz : 62;
		uint64_t is64b : 1;
		uint64_t enb : 1;
#else
		uint64_t enb : 1;
		uint64_t is64b : 1;
		uint64_t raz : 62;
#endif
	} s;
};

union nps_pkt_in_instr_rsize {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz : 32;
		uint64_t rsize : 32;
#else
		uint64_t rsize : 32;
		uint64_t raz : 32;
#endif
	} s;
};

union nps_pkt_in_instr_baoff_dbell {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t aoff : 32;
		uint64_t dbell : 32;
#else
		uint64_t dbell : 32;
		uint64_t aoff : 32;
#endif
	} s;
};

union nps_pkt_in_done_cnts {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t slc_int : 1;
		uint64_t uns_int : 1;
		uint64_t in_int : 1;
		uint64_t mbox_int : 1;
		uint64_t resend : 1;
		uint64_t raz : 27;
		uint64_t cnt : 32;
#else
		uint64_t cnt : 32;
		uint64_t raz : 27;
		uint64_t resend : 1;
		uint64_t mbox_int : 1;
		uint64_t in_int : 1;
		uint64_t uns_int : 1;
		uint64_t slc_int : 1;
#endif
	} s;
};

union aqmq_qsz {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz : 32;
		uint64_t host_queue_size : 32;
#else
		uint64_t host_queue_size : 32;
		uint64_t raz : 32;
#endif
	} s;
};

union zqmq_activity_stat {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz		: 63;
		uint64_t queue_active	: 1;
#else
		uint64_t queue_active	: 1;
		uint64_t raz		: 63;
#endif
	} s;
};

union zqmq_en {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz		: 63;
		uint64_t queue_enable	: 1;
#else
		uint64_t queue_enable	: 1;
		uint64_t raz		: 63;
#endif
	} s;
};

union zqmq_cmp_cnt {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz			: 30;
		uint64_t resend			: 1;
		uint64_t completion_status	: 1;
		uint64_t commands_completed_count: 32;
#else
		uint64_t commands_completed_count: 32;
		uint64_t completion_status	: 1;
		uint64_t resend			: 1;
		uint64_t raz			: 30;
#endif
	} s;
};

union zqmq_drbl {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz		: 32;
		uint64_t dbell_count	: 32;
#else
		uint64_t dbell_count	: 32;
		uint64_t raz		: 32;
#endif
	} s;
};

union zqmq_qsz {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz		: 32;
		uint64_t host_queue_size: 32;
#else
		uint64_t host_queue_size: 32;
		uint64_t raz		: 32;
#endif
	} s;
};

union zqmq_cmp_thr {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz				: 32;
		uint64_t commands_completed_threshold	: 32;
#else
		uint64_t commands_completed_threshold	: 32;
		uint64_t raz				: 32;
#endif
	} s;
};

union zqmq_timer_ld {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz		: 32;
		uint64_t timer_load_value: 32;
#else
		uint64_t timer_load_value: 32;
		uint64_t raz		: 32;
#endif
	} s;
};

enum nitrox_vf_mode {
	NITROX_MODE_PF = 0x0,
	NITROX_MODE_VF16 = 0x1,
	NITROX_MODE_VF32 = 0x2,
	NITROX_MODE_VF64 = 0x3,
	NITROX_MODE_VF128 = 0x4,
};

static inline int
inc_zqmq_next_cmd(uint8_t *bar_addr, uint16_t ring)
{
	uint64_t reg_addr = 0;
	uint64_t val;

	reg_addr = ZQMQ_NXT_CMDX(ring);
	val = nitrox_read_csr(bar_addr, reg_addr);
	val++;
	nitrox_write_csr(bar_addr, reg_addr, val);
	rte_delay_us_block(CSR_DELAY);
	if (nitrox_read_csr(bar_addr, reg_addr) != val)
		return -EIO;

	return 0;
}

int vf_get_vf_config_mode(uint8_t *bar_addr);
int vf_config_mode_to_nr_queues(enum nitrox_vf_mode vf_mode);
void setup_nps_pkt_input_ring(uint8_t *bar_addr, uint16_t ring, uint32_t rsize,
			      phys_addr_t raddr);
void setup_nps_pkt_solicit_output_port(uint8_t *bar_addr, uint16_t port);
void nps_pkt_input_ring_disable(uint8_t *bar_addr, uint16_t ring);
void nps_pkt_solicited_port_disable(uint8_t *bar_addr, uint16_t port);
int setup_zqmq_input_ring(uint8_t *bar_addr, uint16_t ring, uint32_t rsize,
			  phys_addr_t raddr);
int zqmq_input_ring_disable(uint8_t *bar_addr, uint16_t ring);

#endif /* _NITROX_HAL_H_ */
