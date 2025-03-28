/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2024 Intel Corporation
 */

#ifndef _IXGBE_OS_H_
#define _IXGBE_OS_H_

#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <malloc.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_byteorder.h>
#include <rte_io.h>
#include <rte_ip.h>

#include "../ixgbe_logs.h"
#include "../ixgbe_bypass_defines.h"

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#define ASSERT(x) if(!(x)) rte_panic("IXGBE: x")

#define DELAY(x) rte_delay_us_sleep(x)
#define usec_delay(x) DELAY(x)
#define msec_delay(x) DELAY(1000*(x))

#define DEBUGFUNC(F)            DEBUGOUT(F "\n");
#define DEBUGOUT(S, ...)        RTE_LOG(DEBUG, IXGBE_DRIVER, "%s(): " S, __func__, ## __VA_ARGS__)
#define DEBUGOUT1(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT2(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT3(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT6(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT7(S, args...)   DEBUGOUT(S, ##args)

#define ERROR_REPORT1(e, S, args...)   DEBUGOUT(S, ##args)
#define ERROR_REPORT2(e, S, args...)   DEBUGOUT(S, ##args)
#define ERROR_REPORT3(e, S, args...)   DEBUGOUT(S, ##args)

#define FALSE               0
#define TRUE                1

#define false               0
#define true                1
#ifndef RTE_EXEC_ENV_WINDOWS
#define min(a,b)	RTE_MIN(a,b)
#endif

#define EWARN(hw, S, args...)     DEBUGOUT1(S, ##args)

/* Bunch of defines for shared code bogosity */
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(_p) (void)(_p)
#endif
#define UNREFERENCED_1PARAMETER(_p) (void)(_p)
#define UNREFERENCED_2PARAMETER(_p, _q) do { (void)(_p); (void)(_q); } while(0)
#define UNREFERENCED_3PARAMETER(_p, _q, _r) \
	do { (void)(_p); (void)(_q); (void)(_r); } while(0)
#define UNREFERENCED_4PARAMETER(_p, _q, _r, _s) \
	do { (void)(_p); (void)(_q); (void)(_r); (void)(_s); } while(0)
#define UNREFERENCED_5PARAMETER(_p, _q, _r, _s, _t) \
	do { (void)(_p); (void)(_q); (void)(_r); (void)(_s); (void)(_t); } while(0)

/* Shared code error reporting */
enum {
	IXGBE_ERROR_SOFTWARE,
	IXGBE_ERROR_POLLING,
	IXGBE_ERROR_INVALID_STATE,
	IXGBE_ERROR_UNSUPPORTED,
	IXGBE_ERROR_ARGUMENT,
	IXGBE_ERROR_CAUTION,
};

#define STATIC static
#define IXGBE_NTOHL(_i)	rte_be_to_cpu_32(_i)
#define IXGBE_NTOHS(_i)	rte_be_to_cpu_16(_i)
#define IXGBE_CPU_TO_LE16(_i)  rte_cpu_to_le_16(_i)
#define IXGBE_CPU_TO_LE32(_i)  rte_cpu_to_le_32(_i)
#define IXGBE_LE16_TO_CPU(_i)  rte_le_to_cpu_16(_i)
#define IXGBE_LE32_TO_CPU(_i)  rte_le_to_cpu_32(_i)
#define IXGBE_LE64_TO_CPU(_i)  rte_le_to_cpu_64(_i)
#define IXGBE_LE32_TO_CPUS(_i) do { *_i = rte_le_to_cpu_32(*_i); } while(0)
#define IXGBE_CPU_TO_BE16(_i)  rte_cpu_to_be_16(_i)
#define IXGBE_CPU_TO_BE32(_i)  rte_cpu_to_be_32(_i)
#define IXGBE_BE32_TO_CPU(_i)  rte_be_to_cpu_32(_i)

typedef uint8_t		u8;
typedef int8_t		s8;
typedef uint16_t	u16;
typedef int16_t		s16;
typedef uint32_t	u32;
typedef int32_t		s32;
typedef uint64_t	u64;

#define mb()	rte_mb()
#define wmb()	rte_wmb()
#define rmb()	rte_rmb()

#define IOMEM

#define prefetch(x) rte_prefetch0(x)

#define IXGBE_PCI_REG(reg) rte_read32(reg)

static inline uint32_t ixgbe_read_addr(volatile void* addr)
{
	return rte_le_to_cpu_32(IXGBE_PCI_REG(addr));
}

#define IXGBE_PCI_REG_WRITE(reg, value)			\
	rte_write32((rte_cpu_to_le_32(value)), reg)

#define IXGBE_PCI_REG_WRITE_RELAXED(reg, value)		\
	rte_write32_relaxed((rte_cpu_to_le_32(value)), reg)

#define IXGBE_PCI_REG_WC_WRITE(reg, value)			\
	rte_write32_wc((rte_cpu_to_le_32(value)), reg)

#define IXGBE_PCI_REG_WC_WRITE_RELAXED(reg, value)		\
	rte_write32_wc_relaxed((rte_cpu_to_le_32(value)), reg)

#define IXGBE_PCI_REG_ADDR(hw, reg) \
	((volatile uint32_t *)((char *)(hw)->hw_addr + (reg)))

#define IXGBE_PCI_REG_ARRAY_ADDR(hw, reg, index) \
	IXGBE_PCI_REG_ADDR((hw), (reg) + ((index) << 2))

/* Not implemented !! */
#define IXGBE_READ_PCIE_WORD(hw, reg)  ((void)hw, (void)(reg), 0)
#define IXGBE_WRITE_PCIE_WORD(hw, reg, value) do { (void)hw; (void)reg; (void)value; } while(0)

#define IXGBE_WRITE_FLUSH(a) IXGBE_READ_REG(a, IXGBE_STATUS)

#define IXGBE_READ_REG(hw, reg) \
	ixgbe_read_addr(IXGBE_PCI_REG_ADDR((hw), (reg)))

#define IXGBE_WRITE_REG(hw, reg, value) \
	IXGBE_PCI_REG_WRITE(IXGBE_PCI_REG_ADDR((hw), (reg)), (value))

#define IXGBE_READ_REG_ARRAY(hw, reg, index) \
	IXGBE_PCI_REG(IXGBE_PCI_REG_ARRAY_ADDR((hw), (reg), (index)))

#define IXGBE_WRITE_REG_ARRAY(hw, reg, index, value) \
	IXGBE_PCI_REG_WRITE(IXGBE_PCI_REG_ARRAY_ADDR((hw), (reg), (index)), (value))

#define IXGBE_WRITE_REG_THEN_POLL_MASK(hw, reg, val, mask, poll_ms)	\
do {									\
	uint32_t cnt = poll_ms;						\
	IXGBE_WRITE_REG(hw, (reg), (val));				\
	while (((IXGBE_READ_REG(hw, (reg))) & (mask)) && (cnt--))	\
		rte_delay_ms(1);					\
} while (0)

struct ixgbe_hw;
struct ixgbe_lock {
	pthread_mutex_t mutex;
};

#define ixgbe_calloc(hw, c, s) ((void)hw, calloc(c, s))
#define ixgbe_malloc(hw, s) ((void)hw, malloc(s))
#define ixgbe_free(hw, a) ((void)hw, free(a))

#define ixgbe_init_lock(lock) pthread_mutex_init(&(lock)->mutex, NULL)
#define ixgbe_destroy_lock(lock) pthread_mutex_destroy(&(lock)->mutex)
#define ixgbe_acquire_lock(lock) pthread_mutex_lock(&(lock)->mutex)
#define ixgbe_release_lock(lock)	pthread_mutex_unlock(&(lock)->mutex)

#endif /* _IXGBE_OS_H_ */
