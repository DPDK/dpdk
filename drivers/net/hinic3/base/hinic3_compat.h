/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_COMPAT_H_
#define _HINIC3_COMPAT_H_

#include <rte_ethdev.h>
#include <rte_io.h>
#include <rte_log.h>

#define upper_32_bits(n) ((uint32_t)((n) >> 32))
#define lower_32_bits(n) ((uint32_t)(n))

#define HINIC3_MEM_ALLOC_ALIGN_MIN 1

extern int hinic3_logtype;
#define RTE_LOGTYPE_HINIC3_DRIVER hinic3_logtype

#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, HINIC3_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#ifdef HW_CONVERT_ENDIAN
/* If csrs to enable endianness converting are configured, hw will do the
 * endianness converting for stateless SQ ci, the fields less than 4B for
 * doorbell, the fields less than 4B in the CQE data.
 */
#define hinic3_hw_be32(val)  (val)
#define hinic3_hw_cpu32(val) (val)
#define hinic3_hw_cpu16(val) (val)
#else
#define hinic3_hw_be32(val)  rte_cpu_to_be_32(val)
#define hinic3_hw_cpu32(val) rte_be_to_cpu_32(val)
#define hinic3_hw_cpu16(val) rte_be_to_cpu_16(val)
#endif

static inline void
hinic3_cpu_to_hw(void *data, uint32_t len)
{
	uint32_t i, seg = len / sizeof(uint32_t);
	uint32_t *mem = data;

	for (i = 0; i < seg; i++) {
		*mem = hinic3_hw_be32(*mem);
		mem++;
	}
}

static inline int
hinic3_get_bit(uint32_t nr, volatile RTE_ATOMIC(uint64_t) *addr)
{
	return rte_atomic_fetch_or_explicit(addr, RTE_BIT64(nr),
				     rte_memory_order_relaxed);
}

static inline void
hinic3_set_bit(uint32_t nr, volatile RTE_ATOMIC(uint64_t) *addr)
{
	rte_atomic_fetch_or_explicit(addr, RTE_BIT64(nr),
				     rte_memory_order_seq_cst);
}

static inline void
hinic3_clear_bit(uint32_t nr, volatile RTE_ATOMIC(uint64_t) *addr)
{
	rte_atomic_fetch_and_explicit(addr, ~RTE_BIT64(nr),
				      rte_memory_order_seq_cst);
}

static inline int
hinic3_test_and_clear_bit(uint32_t nr, volatile RTE_ATOMIC(uint64_t) *addr)
{
	unsigned long mask = RTE_BIT64(nr);

	return rte_atomic_fetch_and_explicit(addr, ~mask,
				      rte_memory_order_seq_cst) & mask;
}

static inline int
hinic3_test_and_set_bit(uint32_t nr, volatile RTE_ATOMIC(uint64_t) *addr)
{
	unsigned long mask = RTE_BIT64(nr);

	return rte_atomic_fetch_or_explicit(addr, mask,
				      rte_memory_order_seq_cst) & mask;
}

#define HINIC3_S_TO_MS_UNIT  1000

#define cycles		      rte_get_timer_cycles()
#define msecs_to_cycles(ms)  ((ms) * rte_get_timer_hz() / HINIC3_S_TO_MS_UNIT)
#define time_before(now, end) ((int64_t)((now) - (end)) < 0)

/**
 * Convert data to big endian 32 bit format.
 *
 * @param data
 * The data to convert.
 * @param len
 * Length of data to convert, must be Multiple of 4B.
 */
static inline void
hinic3_cpu_to_be32(void *data, uint32_t len)
{
	uint32_t i, seg = len / sizeof(uint32_t);
	uint32_t *mem = data;

	for (i = 0; i < seg; i++) {
		*mem = rte_cpu_to_be_32(*mem);
		mem++;
	}
}

/**
 * Convert data from big endian 32 bit format.
 *
 * @param data
 * The data to convert.
 * @param len
 * Length of data to convert, must be Multiple of 4B.
 */
static inline void
hinic3_be32_to_cpu(void *data, uint32_t len)
{
	uint32_t i, seg = len / sizeof(uint32_t);
	uint32_t *mem = data;

	for (i = 0; i < seg; i++) {
		*mem = rte_be_to_cpu_32(*mem);
		mem++;
	}
}
#endif /* _HINIC3_COMPAT_H_ */
