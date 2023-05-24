/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Intel Corporation
 */

#ifndef _GVE_OSDEP_H_
#define _GVE_OSDEP_H_

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdbool.h>

#include <rte_bitops.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_io.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_version.h>

#include "../gve_logs.h"

#ifdef RTE_EXEC_ENV_LINUX
#include <sys/utsname.h>
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef rte_be16_t __sum16;

typedef rte_be16_t __be16;
typedef rte_be32_t __be32;
typedef rte_be64_t __be64;

typedef rte_le16_t __le16;
typedef rte_le32_t __le32;
typedef rte_le64_t __le64;

typedef rte_iova_t dma_addr_t;

#define ETH_MIN_MTU	RTE_ETHER_MIN_MTU
#define ETH_ALEN	RTE_ETHER_ADDR_LEN

#ifndef PAGE_SHIFT
#define PAGE_SHIFT	12
#endif
#ifndef PAGE_SIZE
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#endif

#define BIT(nr)		RTE_BIT32(nr)

#define be16_to_cpu(x) rte_be_to_cpu_16(x)
#define be32_to_cpu(x) rte_be_to_cpu_32(x)
#define be64_to_cpu(x) rte_be_to_cpu_64(x)

#define cpu_to_be16(x) rte_cpu_to_be_16(x)
#define cpu_to_be32(x) rte_cpu_to_be_32(x)
#define cpu_to_be64(x) rte_cpu_to_be_64(x)

#define READ_ONCE32(x) rte_read32(&(x))

#ifndef ____cacheline_aligned
#define ____cacheline_aligned	__rte_cache_aligned
#endif
#ifndef __packed
#define __packed		__rte_packed
#endif
#define __iomem

#define msleep(ms)		rte_delay_ms(ms)

#define OS_VERSION_STRLEN 128
struct os_version_string {
	char os_version_str1[OS_VERSION_STRLEN];
	char os_version_str2[OS_VERSION_STRLEN];
};

/* These macros are used to generate compilation errors if a struct/union
 * is not exactly the correct length. It gives a divide by zero error if
 * the struct/union is not of the correct size, otherwise it creates an
 * enum that is never used.
 */
#define GVE_CHECK_STRUCT_LEN(n, X) enum gve_static_assert_enum_##X \
	{ gve_static_assert_##X = (n) / ((sizeof(struct X) == (n)) ? 1 : 0) }
#define GVE_CHECK_UNION_LEN(n, X) enum gve_static_asset_enum_##X \
	{ gve_static_assert_##X = (n) / ((sizeof(union X) == (n)) ? 1 : 0) }

static __rte_always_inline u8
readb(volatile void *addr)
{
	return rte_read8(addr);
}

static __rte_always_inline void
writeb(u8 value, volatile void *addr)
{
	rte_write8(value, addr);
}

static __rte_always_inline void
writel(u32 value, volatile void *addr)
{
	rte_write32(value, addr);
}

static __rte_always_inline u32
ioread32be(const volatile void *addr)
{
	return rte_be_to_cpu_32(rte_read32(addr));
}

static __rte_always_inline void
iowrite32be(u32 value, volatile void *addr)
{
	writel(rte_cpu_to_be_32(value), addr);
}

/* DMA memory allocation tracking */
struct gve_dma_mem {
	void *va;
	rte_iova_t pa;
	uint32_t size;
	const void *zone;
};

static inline void *
gve_alloc_dma_mem(struct gve_dma_mem *mem, u64 size)
{
	static uint16_t gve_dma_memzone_id;
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	if (!mem)
		return NULL;

	snprintf(z_name, sizeof(z_name), "gve_dma_%u",
		 __atomic_fetch_add(&gve_dma_memzone_id, 1, __ATOMIC_RELAXED));
	mz = rte_memzone_reserve_aligned(z_name, size, SOCKET_ID_ANY,
					 RTE_MEMZONE_IOVA_CONTIG,
					 PAGE_SIZE);
	if (!mz)
		return NULL;

	mem->size = size;
	mem->va = mz->addr;
	mem->pa = mz->iova;
	mem->zone = mz;
	PMD_DRV_LOG(DEBUG, "memzone %s is allocated", mz->name);

	return mem->va;
}

static inline void
gve_free_dma_mem(struct gve_dma_mem *mem)
{
	PMD_DRV_LOG(DEBUG, "memzone %s to be freed",
		    ((const struct rte_memzone *)mem->zone)->name);

	rte_memzone_free(mem->zone);
	mem->zone = NULL;
	mem->va = NULL;
	mem->pa = 0;
}

static inline void
populate_driver_version_strings(char *str1, char *str2)
{
	struct utsname uts;
	if (uname(&uts) >= 0) {
		/* release */
		rte_strscpy(str1, uts.release,
			OS_VERSION_STRLEN);
		/* version */
		rte_strscpy(str2, uts.version,
			OS_VERSION_STRLEN);
	}
}
#endif /* _GVE_OSDEP_H_ */
