/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_byteorder.h>

#include "nitrox_hal.h"
#include "nitrox_csr.h"

#define MAX_VF_QUEUES	8
#define MAX_PF_QUEUES	64

int
vf_get_vf_config_mode(uint8_t *bar_addr)
{
	union aqmq_qsz aqmq_qsz;
	uint64_t reg_addr;
	int q, vf_mode;

	aqmq_qsz.u64 = 0;
	aqmq_qsz.s.host_queue_size = 0xDEADBEEF;
	reg_addr = AQMQ_QSZX(0);
	nitrox_write_csr(bar_addr, reg_addr, aqmq_qsz.u64);
	rte_delay_us_block(CSR_DELAY);

	aqmq_qsz.u64 = 0;
	for (q = 1; q < MAX_VF_QUEUES; q++) {
		reg_addr = AQMQ_QSZX(q);
		aqmq_qsz.u64 = nitrox_read_csr(bar_addr, reg_addr);
		if (aqmq_qsz.s.host_queue_size == 0xDEADBEEF)
			break;
	}

	switch (q) {
	case 1:
		vf_mode = NITROX_MODE_VF128;
		break;
	case 2:
		vf_mode = NITROX_MODE_VF64;
		break;
	case 4:
		vf_mode = NITROX_MODE_VF32;
		break;
	case 8:
		vf_mode = NITROX_MODE_VF16;
		break;
	default:
		vf_mode = 0;
		break;
	}

	return vf_mode;
}

int
vf_config_mode_to_nr_queues(enum nitrox_vf_mode vf_mode)
{
	int nr_queues;

	switch (vf_mode) {
	case NITROX_MODE_PF:
		nr_queues = MAX_PF_QUEUES;
		break;
	case NITROX_MODE_VF16:
		nr_queues = 8;
		break;
	case NITROX_MODE_VF32:
		nr_queues = 4;
		break;
	case NITROX_MODE_VF64:
		nr_queues = 2;
		break;
	case NITROX_MODE_VF128:
		nr_queues = 1;
		break;
	default:
		nr_queues = 0;
		break;
	}

	return nr_queues;
}
