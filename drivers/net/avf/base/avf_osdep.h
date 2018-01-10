/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _AVF_OSDEP_H_
#define _AVF_OSDEP_H_

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

#include "../avf_log.h"

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
#define ASSERT(x) if(!(x)) rte_panic("AVF: x")

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

#define avf_memset(a, b, c, d) memset((a), (b), (c))
#define avf_memcpy(a, b, c, d) rte_memcpy((a), (b), (c))

#define avf_usec_delay(x) rte_delay_us(x)
#define avf_msec_delay(x) rte_delay_us(1000*(x))

#define AVF_PCI_REG(reg)		rte_read32(reg)
#define AVF_PCI_REG_ADDR(a, reg) \
	((volatile uint32_t *)((char *)(a)->hw_addr + (reg)))

#define AVF_PCI_REG_WRITE(reg, value)		\
	rte_write32((rte_cpu_to_le_32(value)), reg)
#define AVF_PCI_REG_WRITE_RELAXED(reg, value)	\
	rte_write32_relaxed((rte_cpu_to_le_32(value)), reg)
static inline
uint32_t avf_read_addr(volatile void *addr)
{
	return rte_le_to_cpu_32(AVF_PCI_REG(addr));
}

#define AVF_READ_REG(hw, reg) \
	avf_read_addr(AVF_PCI_REG_ADDR((hw), (reg)))
#define AVF_WRITE_REG(hw, reg, value) \
	AVF_PCI_REG_WRITE(AVF_PCI_REG_ADDR((hw), (reg)), (value))
#define AVF_WRITE_FLUSH(a) \
	AVF_READ_REG(a, AVFGEN_RSTAT)

#define rd32(a, reg) avf_read_addr(AVF_PCI_REG_ADDR((a), (reg)))
#define wr32(a, reg, value) \
	AVF_PCI_REG_WRITE(AVF_PCI_REG_ADDR((a), (reg)), (value))

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

#define avf_debug(h, m, s, ...)                                \
do {                                                            \
	if (((m) & (h)->debug_mask))                            \
		PMD_DRV_LOG_RAW(DEBUG, "avf %02x.%x " s,       \
			(h)->bus.device, (h)->bus.func,         \
					##__VA_ARGS__);         \
} while (0)

/* memory allocation tracking */
struct avf_dma_mem {
	void *va;
	u64 pa;
	u32 size;
	const void *zone;
} __attribute__((packed));

struct avf_virt_mem {
	void *va;
	u32 size;
} __attribute__((packed));

/* SW spinlock */
struct avf_spinlock {
	rte_spinlock_t spinlock;
};

#define avf_allocate_dma_mem(h, m, unused, s, a) \
			avf_allocate_dma_mem_d(h, m, s, a)
#define avf_free_dma_mem(h, m) avf_free_dma_mem_d(h, m)

#define avf_allocate_virt_mem(h, m, s) avf_allocate_virt_mem_d(h, m, s)
#define avf_free_virt_mem(h, m) avf_free_virt_mem_d(h, m)

static inline void
avf_init_spinlock_d(struct avf_spinlock *sp)
{
	rte_spinlock_init(&sp->spinlock);
}

static inline void
avf_acquire_spinlock_d(struct avf_spinlock *sp)
{
	rte_spinlock_lock(&sp->spinlock);
}

static inline void
avf_release_spinlock_d(struct avf_spinlock *sp)
{
	rte_spinlock_unlock(&sp->spinlock);
}

static inline void
avf_destroy_spinlock_d(__rte_unused struct avf_spinlock *sp)
{
}

#define avf_init_spinlock(_sp) avf_init_spinlock_d(_sp)
#define avf_acquire_spinlock(_sp) avf_acquire_spinlock_d(_sp)
#define avf_release_spinlock(_sp) avf_release_spinlock_d(_sp)
#define avf_destroy_spinlock(_sp) avf_destroy_spinlock_d(_sp)

#endif /* _AVF_OSDEP_H_ */
