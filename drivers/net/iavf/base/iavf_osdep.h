/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _IAVF_OSDEP_H_
#define _IAVF_OSDEP_H_

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include <rte_common.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_io.h>

#include "../iavf_log.h"

#define INLINE inline
#define STATIC static

typedef uint8_t         u8;
typedef int8_t          s8;
typedef uint16_t        u16;
typedef uint32_t        u32;
typedef int32_t         s32;
typedef uint64_t        u64;

#define __iomem
#define hw_dbg(hw, S, A...) do {} while (0)
#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))
#define lower_32_bits(n) ((u32)(n))

#ifndef ETH_ADDR_LEN
#define ETH_ADDR_LEN                  6
#endif

#ifndef __le16
#define __le16          uint16_t
#endif
#ifndef __le32
#define __le32          uint32_t
#endif
#ifndef __le64
#define __le64          uint64_t
#endif
#ifndef __be16
#define __be16          uint16_t
#endif
#ifndef __be32
#define __be32          uint32_t
#endif
#ifndef __be64
#define __be64          uint64_t
#endif

#define FALSE           0
#define TRUE            1
#define false           0
#define true            1

#define min(a,b) RTE_MIN(a,b)
#define max(a,b) RTE_MAX(a,b)

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#define ASSERT(x) if(!(x)) rte_panic("IAVF: x")

#define DEBUGOUT(S)             PMD_DRV_LOG_RAW(DEBUG, S)
#define DEBUGOUT2(S, A...)      PMD_DRV_LOG_RAW(DEBUG, S, ##A)
#define DEBUGFUNC(F)            DEBUGOUT(F "\n")

#define CPU_TO_LE16(o) rte_cpu_to_le_16(o)
#define CPU_TO_LE32(s) rte_cpu_to_le_32(s)
#define CPU_TO_LE64(h) rte_cpu_to_le_64(h)
#define LE16_TO_CPU(a) rte_le_to_cpu_16(a)
#define LE32_TO_CPU(c) rte_le_to_cpu_32(c)
#define LE64_TO_CPU(k) rte_le_to_cpu_64(k)

#define cpu_to_le16(o) rte_cpu_to_le_16(o)
#define cpu_to_le32(s) rte_cpu_to_le_32(s)
#define cpu_to_le64(h) rte_cpu_to_le_64(h)
#define le16_to_cpu(a) rte_le_to_cpu_16(a)
#define le32_to_cpu(c) rte_le_to_cpu_32(c)
#define le64_to_cpu(k) rte_le_to_cpu_64(k)

#define iavf_memset(a, b, c, d) memset((a), (b), (c))
#define iavf_memcpy(a, b, c, d) rte_memcpy((a), (b), (c))

#define iavf_usec_delay(x) rte_delay_us_sleep(x)
#define iavf_msec_delay(x) iavf_usec_delay(1000 * (x))

#define IAVF_PCI_REG(reg)		rte_read32(reg)
#define IAVF_PCI_REG_ADDR(a, reg) \
	((volatile uint32_t *)((char *)(a)->hw_addr + (reg)))

#define IAVF_PCI_REG_WRITE(reg, value)		\
	rte_write32((rte_cpu_to_le_32(value)), reg)
#define IAVF_PCI_REG_WRITE_RELAXED(reg, value)	\
	rte_write32_relaxed((rte_cpu_to_le_32(value)), reg)
static inline
uint32_t iavf_read_addr(volatile void *addr)
{
	return rte_le_to_cpu_32(IAVF_PCI_REG(addr));
}

#define IAVF_READ_REG(hw, reg) \
	iavf_read_addr(IAVF_PCI_REG_ADDR((hw), (reg)))
#define IAVF_WRITE_REG(hw, reg, value) \
	IAVF_PCI_REG_WRITE(IAVF_PCI_REG_ADDR((hw), (reg)), (value))
#define IAVF_WRITE_FLUSH(a) \
	IAVF_READ_REG(a, IAVFGEN_RSTAT)

#define rd32(a, reg) iavf_read_addr(IAVF_PCI_REG_ADDR((a), (reg)))
#define wr32(a, reg, value) \
	IAVF_PCI_REG_WRITE(IAVF_PCI_REG_ADDR((a), (reg)), (value))

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

#define iavf_debug(h, m, s, ...)                                \
do {                                                            \
	if (((m) & (h)->debug_mask))                            \
		PMD_DRV_LOG_RAW(DEBUG, "iavf %02x.%x " s,       \
			(h)->bus.device, (h)->bus.func,         \
					##__VA_ARGS__);         \
} while (0)

/* memory allocation tracking */
struct iavf_dma_mem {
	void *va;
	u64 pa;
	u32 size;
	const void *zone;
} __attribute__((packed));

struct iavf_virt_mem {
	void *va;
	u32 size;
} __attribute__((packed));

/* SW spinlock */
struct iavf_spinlock {
	rte_spinlock_t spinlock;
};

#define iavf_allocate_dma_mem(h, m, unused, s, a) \
			iavf_allocate_dma_mem_d(h, m, s, a)
#define iavf_free_dma_mem(h, m) iavf_free_dma_mem_d(h, m)

#define iavf_allocate_virt_mem(h, m, s) iavf_allocate_virt_mem_d(h, m, s)
#define iavf_free_virt_mem(h, m) iavf_free_virt_mem_d(h, m)

static inline void
iavf_init_spinlock_d(struct iavf_spinlock *sp)
{
	rte_spinlock_init(&sp->spinlock);
}

static inline void
iavf_acquire_spinlock_d(struct iavf_spinlock *sp)
{
	rte_spinlock_lock(&sp->spinlock);
}

static inline void
iavf_release_spinlock_d(struct iavf_spinlock *sp)
{
	rte_spinlock_unlock(&sp->spinlock);
}

static inline void
iavf_destroy_spinlock_d(__rte_unused struct iavf_spinlock *sp)
{
}

#define iavf_init_spinlock(_sp) iavf_init_spinlock_d(_sp)
#define iavf_acquire_spinlock(_sp) iavf_acquire_spinlock_d(_sp)
#define iavf_release_spinlock(_sp) iavf_release_spinlock_d(_sp)
#define iavf_destroy_spinlock(_sp) iavf_destroy_spinlock_d(_sp)

#endif /* _IAVF_OSDEP_H_ */
