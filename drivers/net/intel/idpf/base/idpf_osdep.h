/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2024 Intel Corporation
 */

#ifndef _IDPF_OSDEP_H_
#define _IDPF_OSDEP_H_

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_random.h>
#include <rte_io.h>
#include <rte_compat.h>

#include "../idpf_common_logs.h"

#define INLINE inline
#define STATIC static

typedef uint8_t		u8;
typedef int8_t		s8;
typedef uint16_t	u16;
typedef int16_t		s16;
typedef uint32_t	u32;
typedef int32_t		s32;
typedef uint64_t	u64;
typedef uint64_t	s64;

typedef struct idpf_lock idpf_lock;

#define __iomem
#define hw_dbg(hw, S, ...)	do {} while (0)
#define upper_32_bits(n)	((u32)(((n) >> 16) >> 16))
#define lower_32_bits(n)	((u32)(n))
#define low_16_bits(x)		((x) & 0xFFFF)
#define high_16_bits(x)		(((x) & 0xFFFF0000) >> 16)

#define IDPF_M(m, s)		((m) << (s))

#define BITS_PER_LONG (8 * sizeof(long))
#define BITS_PER_LONG_LONG (8 * sizeof(long long))
#define GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define GENMASK_ULL(h, l) \
	(((~0ULL) << (l)) & (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

#ifndef ETH_ADDR_LEN
#define ETH_ADDR_LEN		6
#endif

#ifndef __le16
#define __le16	uint16_t
#endif
#ifndef __le32
#define __le32	uint32_t
#endif
#ifndef __le64
#define __le64	uint64_t
#endif
#ifndef __be16
#define __be16	uint16_t
#endif
#ifndef __be32
#define __be32	uint32_t
#endif
#ifndef __be64
#define __be64	uint64_t
#endif

#ifndef BIT_ULL
#define BIT_ULL(a) RTE_BIT64(a)
#endif

#ifndef BIT
#define BIT(a) RTE_BIT32(a)
#endif

#define FALSE	0
#define TRUE	1
#define false	0
#define true	1

/* Avoid macro redefinition warning on Windows */
#ifdef RTE_EXEC_ENV_WINDOWS
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#define min(a, b) RTE_MIN(a, b)
#define max(a, b) RTE_MAX(a, b)

#define ARRAY_SIZE(arr)  RTE_DIM(arr)
#define FIELD_SIZEOF(t, f) (sizeof(((t *)0)->(f)))
#define MAKEMASK(m, s) ((m) << (s))

#define DEBUGOUT(S, ...)	RTE_LOG(DEBUG, IDPF_COMMON, S, ## __VA_ARGS__)
#define DEBUGOUT2(S, ...)	DEBUGOUT(S, ## __VA_ARGS__)
#define DEBUGFUNC(F)		DEBUGOUT(F "\n")

#define idpf_debug(h, m, s, ...)					\
	do {								\
		if (((m) & (h)->debug_mask))				\
			DEBUGOUT("idpf %02x.%x " s "\n",		\
					(h)->bus.device, (h)->bus.func,	\
					##__VA_ARGS__);			\
	} while (0)

#define idpf_info(hw, fmt, ...) \
	idpf_debug(hw, IDPF_DBG_ALL, fmt, ##__VA_ARGS__)
#define idpf_warn(hw, fmt, ...) \
	idpf_debug(hw, IDPF_DBG_ALL, fmt, ##__VA_ARGS__)
#define idpf_debug_array(hw, type, rowsize, groupsize, buf, len)	\
	do {								\
		struct idpf_hw *hw_l = hw;				\
		u16 len_l = len;					\
		u8 *buf_l = buf;					\
		int i;							\
		for (i = 0; i < len_l; i += 8)				\
			idpf_debug(hw_l, type,				\
				   "0x%04X  0x%016"PRIx64"\n",		\
				   i, *((u64 *)((buf_l) + i)));		\
	} while (0)
#define idpf_snprintf snprintf
#ifndef SNPRINTF
#define SNPRINTF idpf_snprintf
#endif

#define IDPF_PCI_REG(reg)     rte_read32(reg)
#define IDPF_PCI_REG_ADDR(a, reg)				\
	((volatile uint32_t *)((char *)(a)->hw_addr + (reg)))
#define IDPF_PCI_REG64(reg)     rte_read64(reg)
#define IDPF_PCI_REG_ADDR64(a, reg)				\
	((volatile uint64_t *)((char *)(a)->hw_addr + (reg)))

#define idpf_wmb() rte_io_wmb()
#define idpf_rmb() rte_io_rmb()
#define idpf_mb() rte_io_mb()

static inline uint32_t idpf_read_addr(volatile void *addr)
{
	return rte_le_to_cpu_32(IDPF_PCI_REG(addr));
}

static inline uint64_t idpf_read_addr64(volatile void *addr)
{
	return rte_le_to_cpu_64(IDPF_PCI_REG64(addr));
}

#define IDPF_PCI_REG_WRITE(reg, value)			\
	rte_write32((rte_cpu_to_le_32(value)), reg)

#define IDPF_PCI_REG_WRITE64(reg, value)		\
	rte_write64((rte_cpu_to_le_64(value)), reg)

#define IDPF_READ_REG(hw, reg) idpf_read_addr(IDPF_PCI_REG_ADDR((hw), (reg)))
#define IDPF_WRITE_REG(hw, reg, value)					\
	IDPF_PCI_REG_WRITE(IDPF_PCI_REG_ADDR((hw), (reg)), (value))

#define rd32(a, reg) idpf_read_addr(IDPF_PCI_REG_ADDR((a), (reg)))
#define wr32(a, reg, value)						\
	IDPF_PCI_REG_WRITE(IDPF_PCI_REG_ADDR((a), (reg)), (value))
#define div64_long(n, d) ((n) / (d))
#define rd64(a, reg) idpf_read_addr64(IDPF_PCI_REG_ADDR64((a), (reg)))

#define BITS_PER_BYTE       8

/* memory allocation tracking */
struct __rte_packed_begin idpf_dma_mem {
	void *va;
	u64 pa;
	u32 size;
	const void *zone;
} __rte_packed_end;

struct __rte_packed_begin idpf_virt_mem {
	void *va;
	u32 size;
} __rte_packed_end;

#define idpf_malloc(h, s)	rte_zmalloc(NULL, s, 0)
#define idpf_calloc(h, c, s)	rte_zmalloc(NULL, (c) * (s), 0)
#define idpf_free(h, m)		rte_free(m)

#define idpf_memset(a, b, c, d)	memset((a), (b), (c))
#define idpf_memcpy(a, b, c, d)	rte_memcpy((a), (b), (c))
#define idpf_memdup(a, b, c, d)	rte_memcpy(idpf_malloc(a, c), b, c)

#define CPU_TO_BE16(o) rte_cpu_to_be_16(o)
#define CPU_TO_BE32(o) rte_cpu_to_be_32(o)
#define CPU_TO_BE64(o) rte_cpu_to_be_64(o)
#define CPU_TO_LE16(o) rte_cpu_to_le_16(o)
#define CPU_TO_LE32(s) rte_cpu_to_le_32(s)
#define CPU_TO_LE64(h) rte_cpu_to_le_64(h)
#define LE16_TO_CPU(a) rte_le_to_cpu_16(a)
#define LE32_TO_CPU(c) rte_le_to_cpu_32(c)
#define LE64_TO_CPU(k) rte_le_to_cpu_64(k)

#define NTOHS(a) rte_be_to_cpu_16(a)
#define NTOHL(a) rte_be_to_cpu_32(a)
#define HTONS(a) rte_cpu_to_be_16(a)
#define HTONL(a) rte_cpu_to_be_32(a)

/* SW spinlock */
struct idpf_lock {
	rte_spinlock_t spinlock;
};

#define idpf_init_lock(sp) rte_spinlock_init(&(sp)->spinlock)
#define idpf_acquire_lock(sp) rte_spinlock_lock(&(sp)->spinlock)
#define idpf_release_lock(sp) rte_spinlock_unlock(&(sp)->spinlock)
#define idpf_destroy_lock(sp) RTE_SET_USED(sp)

struct idpf_hw;

static inline void *
idpf_alloc_dma_mem(__rte_unused struct idpf_hw *hw,
		   struct idpf_dma_mem *mem, u64 size)
{
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	if (!mem)
		return NULL;

	snprintf(z_name, sizeof(z_name), "idpf_dma_%"PRIu64, rte_rand());
	mz = rte_memzone_reserve_aligned(z_name, size, SOCKET_ID_ANY,
					 RTE_MEMZONE_IOVA_CONTIG, RTE_PGSIZE_4K);
	if (!mz)
		return NULL;

	mem->size = size;
	mem->va = mz->addr;
	mem->pa = mz->iova;
	mem->zone = (const void *)mz;
	memset(mem->va, 0, size);

	return mem->va;
}

static inline void
idpf_free_dma_mem(__rte_unused struct idpf_hw *hw,
		  struct idpf_dma_mem *mem)
{
	rte_memzone_free((const struct rte_memzone *)mem->zone);
	mem->size = 0;
	mem->va = NULL;
	mem->pa = 0;
}

static inline u8
idpf_hweight8(u32 num)
{
	u8 bits = 0;
	u32 i;

	for (i = 0; i < 8; i++) {
		bits += (u8)(num & 0x1);
		num >>= 1;
	}

	return bits;
}

static inline u8
idpf_hweight32(u32 num)
{
	u8 bits = 0;
	u32 i;

	for (i = 0; i < 32; i++) {
		bits += (u8)(num & 0x1);
		num >>= 1;
	}

	return bits;
}

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define DELAY(x) rte_delay_us(x)
#define idpf_usec_delay(x) rte_delay_us(x)
#define idpf_msec_delay(x, y) rte_delay_us(1000 * (x))
#define udelay(x) DELAY(x)
#define msleep(x) DELAY(1000 * (x))
#define usleep_range(min, max) msleep(DIV_ROUND_UP(min, 1000))

#ifndef IDPF_DBG_TRACE
#define IDPF_DBG_TRACE	  BIT_ULL(0)
#endif

#ifndef DIVIDE_AND_ROUND_UP
#define DIVIDE_AND_ROUND_UP(a, b) (((a) + (b) - 1) / (b))
#endif

#ifndef IDPF_INTEL_VENDOR_ID
#define IDPF_INTEL_VENDOR_ID	    0x8086
#endif

#ifndef IS_UNICAST_ETHER_ADDR
#define IS_UNICAST_ETHER_ADDR(addr)			\
	((bool)((((u8 *)(addr))[0] % ((u8)0x2)) == 0))
#endif

#ifndef IS_MULTICAST_ETHER_ADDR
#define IS_MULTICAST_ETHER_ADDR(addr)			\
	((bool)((((u8 *)(addr))[0] % ((u8)0x2)) == 1))
#endif

#ifndef IS_BROADCAST_ETHER_ADDR
/* Check whether an address is broadcast. */
#define IS_BROADCAST_ETHER_ADDR(addr)			\
	((bool)((((u16 *)(addr))[0] == ((u16)0xffff))))
#endif

#ifndef IS_ZERO_ETHER_ADDR
#define IS_ZERO_ETHER_ADDR(addr)				\
	(((bool)((((u16 *)(addr))[0] == ((u16)0x0)))) &&	\
	 ((bool)((((u16 *)(addr))[1] == ((u16)0x0)))) &&	\
	 ((bool)((((u16 *)(addr))[2] == ((u16)0x0)))))
#endif

#ifndef LIST_HEAD_TYPE
#define LIST_HEAD_TYPE(list_name, type) LIST_HEAD(list_name, type)
#endif

#ifndef LIST_ENTRY_TYPE
#define LIST_ENTRY_TYPE(type)	   LIST_ENTRY(type)
#endif

#ifndef LIST_FOREACH_SAFE
#define LIST_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = LIST_FIRST((head));				\
	    (var) && ((tvar) = LIST_NEXT((var), field), 1);		\
	    (var) = (tvar))
#endif

#ifndef LIST_FOR_EACH_ENTRY_SAFE
#define LIST_FOR_EACH_ENTRY_SAFE(pos, temp, head, entry_type, list)	\
	LIST_FOREACH_SAFE(pos, head, list, temp)
#endif

#ifndef LIST_FOR_EACH_ENTRY
#define LIST_FOR_EACH_ENTRY(pos, head, entry_type, list)		\
	LIST_FOREACH(pos, head, list)

#endif

#define idpf_struct_size(ptr, field, num) \
	(sizeof(*(ptr)) + sizeof(*(ptr)->field) * (num))

enum idpf_mac_type {
	IDPF_MAC_UNKNOWN = 0,
	IDPF_MAC_PF,
	IDPF_MAC_VF,
	IDPF_MAC_GENERIC
};

#define ETH_ALEN 6

struct idpf_mac_info {
	enum idpf_mac_type type;
	u8 addr[ETH_ALEN];
	u8 perm_addr[ETH_ALEN];
};

#define IDPF_AQ_LINK_UP 0x1

/* PCI bus types */
enum idpf_bus_type {
	idpf_bus_type_unknown = 0,
	idpf_bus_type_pci,
	idpf_bus_type_pcix,
	idpf_bus_type_pci_express,
	idpf_bus_type_reserved
};

/* PCI bus speeds */
enum idpf_bus_speed {
	idpf_bus_speed_unknown	= 0,
	idpf_bus_speed_33	= 33,
	idpf_bus_speed_66	= 66,
	idpf_bus_speed_100	= 100,
	idpf_bus_speed_120	= 120,
	idpf_bus_speed_133	= 133,
	idpf_bus_speed_2500	= 2500,
	idpf_bus_speed_5000	= 5000,
	idpf_bus_speed_8000	= 8000,
	idpf_bus_speed_reserved
};

/* PCI bus widths */
enum idpf_bus_width {
	idpf_bus_width_unknown	= 0,
	idpf_bus_width_pcie_x1	= 1,
	idpf_bus_width_pcie_x2	= 2,
	idpf_bus_width_pcie_x4	= 4,
	idpf_bus_width_pcie_x8	= 8,
	idpf_bus_width_32	= 32,
	idpf_bus_width_64	= 64,
	idpf_bus_width_reserved
};

/* Bus parameters */
struct idpf_bus_info {
	enum idpf_bus_speed speed;
	enum idpf_bus_width width;
	enum idpf_bus_type type;

	u16 func;
	u16 device;
	u16 lan_id;
	u16 bus_id;
};

/* Function specific capabilities */
struct idpf_hw_func_caps {
	u32 num_alloc_vfs;
	u32 vf_base_id;
};

#endif /* _IDPF_OSDEP_H_ */
