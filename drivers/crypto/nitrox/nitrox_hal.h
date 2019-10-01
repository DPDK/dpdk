/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_HAL_H_
#define _NITROX_HAL_H_

#include <rte_cycles.h>
#include <rte_byteorder.h>

#include "nitrox_csr.h"

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

enum nitrox_vf_mode {
	NITROX_MODE_PF = 0x0,
	NITROX_MODE_VF16 = 0x1,
	NITROX_MODE_VF32 = 0x2,
	NITROX_MODE_VF64 = 0x3,
	NITROX_MODE_VF128 = 0x4,
};

int vf_get_vf_config_mode(uint8_t *bar_addr);
int vf_config_mode_to_nr_queues(enum nitrox_vf_mode vf_mode);

#endif /* _NITROX_HAL_H_ */
