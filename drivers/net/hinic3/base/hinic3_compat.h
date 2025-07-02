/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_COMPAT_H_
#define _HINIC3_COMPAT_H_

#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <ethdev_pci.h>
#include <eal_interrupts.h>
#include <rte_io.h>
#include <rte_atomic.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_spinlock.h>

typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))
#define lower_32_bits(n) ((u32)(n))

#define HINIC3_MEM_ALLOC_ALIGN_MIN 1

extern int hinic3_logtype;
#define RTE_LOGTYPE_NET_HINIC3 hinic3_logtype

#define PMD_DRV_LOG(level, ...) RTE_LOG_LINE(level, NET_HINIC3, __VA_ARGS__)

/* Bit order interface. */
#define cpu_to_be16(o) rte_cpu_to_be_16(o)
#define cpu_to_be32(o) rte_cpu_to_be_32(o)
#define cpu_to_be64(o) rte_cpu_to_be_64(o)
#define cpu_to_le32(o) rte_cpu_to_le_32(o)
#define be16_to_cpu(o) rte_be_to_cpu_16(o)
#define be32_to_cpu(o) rte_be_to_cpu_32(o)
#define be64_to_cpu(o) rte_be_to_cpu_64(o)
#define le32_to_cpu(o) rte_le_to_cpu_32(o)

#ifdef HW_CONVERT_ENDIAN
/* If csrs to enable endianness converting are configured, hw will do the
 * endianness converting for stateless SQ ci, the fields less than 4B for
 * doorbell, the fields less than 4B in the CQE data.
 */
#define hinic3_hw_be32(val)  (val)
#define hinic3_hw_cpu32(val) (val)
#define hinic3_hw_cpu16(val) (val)
#else
#define hinic3_hw_be32(val)  cpu_to_be32(val)
#define hinic3_hw_cpu32(val) be32_to_cpu(val)
#define hinic3_hw_cpu16(val) be16_to_cpu(val)
#endif

#define ARRAY_LEN(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

static inline void
hinic3_hw_be32_len(void *data, int len)
{
	int i, chunk_sz = sizeof(u32);
	u32 *mem = data;

	if (!data)
		return;

	len = len / chunk_sz;

	for (i = 0; i < len; i++) {
		*mem = hinic3_hw_be32(*mem);
		mem++;
	}
}

static inline int
hinic3_get_bit(int nr, volatile RTE_ATOMIC(u64) *addr)
{
	RTE_ASSERT(nr < 0x20);

	uint32_t mask = UINT32_C(1) << nr;
	return (*addr) & mask;
}

static inline void
hinic3_set_bit(unsigned int nr, volatile RTE_ATOMIC(u64) *addr)
{
	rte_atomic_fetch_or_explicit(addr, (1UL << nr),
				     rte_memory_order_seq_cst);
}

static inline void
hinic3_clear_bit(int nr, volatile RTE_ATOMIC(u64) *addr)
{
	rte_atomic_fetch_and_explicit(addr, ~(1UL << nr),
				      rte_memory_order_seq_cst);
}

static inline int
hinic3_test_and_clear_bit(int nr, volatile RTE_ATOMIC(u64) *addr)
{
	unsigned long mask = (1UL << nr);

	return (int)(rte_atomic_fetch_and_explicit(addr, ~mask,
						   rte_memory_order_seq_cst) &
		     mask);
}

static inline int
hinic3_test_and_set_bit(int nr, volatile RTE_ATOMIC(u64) *addr)
{
	unsigned long mask = (1UL << nr);

	return (int)(rte_atomic_fetch_or_explicit(addr, mask,
						  rte_memory_order_seq_cst) &
		     mask);
}

#ifdef CLOCK_MONOTONIC_RAW /**< Defined in glibc bits/time.h . */
#define CLOCK_TYPE CLOCK_MONOTONIC_RAW
#else
#define CLOCK_TYPE CLOCK_MONOTONIC
#endif

#define HINIC3_MUTEX_TIMEOUT 10
#define HINIC3_S_TO_MS_UNIT  1000
#define HINIC3_S_TO_NS_UNIT  1000000

static inline unsigned long
clock_gettime_ms(void)
{
	struct timespec tv;

	clock_gettime(CLOCK_TYPE, &tv);

	return (unsigned long)tv.tv_sec * HINIC3_S_TO_MS_UNIT +
	       (unsigned long)tv.tv_nsec / HINIC3_S_TO_NS_UNIT;
}

#define jiffies		      clock_gettime_ms()
#define msecs_to_jiffies(ms)  (ms)
#define time_before(now, end) ((now) < (end))

/**
 * Convert data to big endian 32 bit format.
 *
 * @param data
 * The data to convert.
 * @param len
 * Length of data to convert, must be Multiple of 4B.
 */
static inline void
hinic3_cpu_to_be32(void *data, int len)
{
	int i, chunk_sz = sizeof(u32);
	u32 *mem = data;

	if (!data)
		return;

	len = len / chunk_sz;

	for (i = 0; i < len; i++) {
		*mem = cpu_to_be32(*mem);
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
hinic3_be32_to_cpu(void *data, int len)
{
	int i, chunk_sz = sizeof(u32);
	u32 *mem = data;

	if (!data)
		return;

	len = len / chunk_sz;

	for (i = 0; i < len; i++) {
		*mem = be32_to_cpu(*mem);
		mem++;
	}
}

static inline u16
ilog2(u32 n)
{
	u16 res = 0;

	while (n > 1) {
		n >>= 1;
		res++;
	}

	return res;
}

static inline int
hinic3_mutex_init(pthread_mutex_t *pthreadmutex,
		  const pthread_mutexattr_t *mattr)
{
	int err;

	err = pthread_mutex_init(pthreadmutex, mattr);
	if (unlikely(err))
		PMD_DRV_LOG(ERR, "Initialize mutex failed, error: %d", err);

	return err;
}

static inline int
hinic3_mutex_destroy(pthread_mutex_t *pthreadmutex)
{
	int err;

	err = pthread_mutex_destroy(pthreadmutex);
	if (unlikely(err))
		PMD_DRV_LOG(ERR, "Destroy mutex failed, error: %d", err);

	return err;
}

static inline int
hinic3_mutex_lock(pthread_mutex_t *pthreadmutex)
{
	int err;

	err = pthread_mutex_lock(pthreadmutex);
	if (err)
		PMD_DRV_LOG(ERR, "Mutex lock failed, err: %d", err);

	return err;
}

static inline void
hinic3_mutex_unlock(pthread_mutex_t *pthreadmutex)
{
	pthread_mutex_unlock(pthreadmutex);
}

#endif /* _HINIC3_COMPAT_H_ */
