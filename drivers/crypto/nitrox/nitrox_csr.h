/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_CSR_H_
#define _NITROX_CSR_H_

#include <rte_common.h>
#include <rte_io.h>

#define CSR_DELAY	30

/* AQM Virtual Function Registers */
#define AQMQ_QSZX(_i)			(0x20008 + ((_i)*0x40000))

static inline uint64_t
nitrox_read_csr(uint8_t *bar_addr, uint64_t offset)
{
	return rte_read64(bar_addr + offset);
}

static inline void
nitrox_write_csr(uint8_t *bar_addr, uint64_t offset, uint64_t value)
{
	rte_write64(value, (bar_addr + offset));
}

#endif /* _NITROX_CSR_H_ */
