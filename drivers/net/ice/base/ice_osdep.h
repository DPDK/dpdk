/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _ICE_OSDEP_H_
#define _ICE_OSDEP_H_

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

#include "../ice_logs.h"

#define INLINE inline
#define STATIC static

typedef uint8_t         u8;
typedef int8_t          s8;
typedef uint16_t        u16;
typedef int16_t         s16;
typedef uint32_t        u32;
typedef int32_t         s32;
typedef uint64_t        u64;
typedef uint64_t        s64;

#define __iomem
#define hw_dbg(hw, S, A...) do {} while (0)
#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))
#define lower_32_bits(n) ((u32)(n))
#define low_16_bits(x)   ((x) & 0xFFFF)
#define high_16_bits(x)  (((x) & 0xFFFF0000) >> 16)

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

#ifndef __always_unused
#define __always_unused  __attribute__((unused))
#endif
#ifndef __maybe_unused
#define __maybe_unused  __attribute__((unused))
#endif
#ifndef __packed
#define __packed  __attribute__((packed))
#endif

#ifndef BIT_ULL
#define BIT_ULL(a) (1ULL << (a))
#endif

#define FALSE           0
#define TRUE            1
#define false           0
#define true            1

#define min(a, b) RTE_MIN(a, b)
#define max(a, b) RTE_MAX(a, b)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define FIELD_SIZEOF(t, f) (sizeof(((t *)0)->f))
#define MAKEMASK(m, s) ((m) << (s))

#define DEBUGOUT(S, A...) PMD_DRV_LOG_RAW(DEBUG, S, ##A)
#define DEBUGFUNC(F) PMD_DRV_LOG_RAW(DEBUG, F)

#define ice_debug(h, m, s, ...)					\
do {								\
	if (((m) & (h)->debug_mask))				\
		PMD_DRV_LOG_RAW(DEBUG, "ice %02x.%x " s,	\
			(h)->bus.device, (h)->bus.func,		\
					##__VA_ARGS__);		\
} while (0)

#define ice_info(hw, fmt, args...) ice_debug(hw, ICE_DBG_ALL, fmt, ##args)
#define ice_warn(hw, fmt, args...) ice_debug(hw, ICE_DBG_ALL, fmt, ##args)
#define ice_debug_array(hw, type, rowsize, groupsize, buf, len)		\
do {									\
	struct ice_hw *hw_l = hw;					\
		u16 len_l = len;					\
		u8 *buf_l = buf;					\
		int i;							\
		for (i = 0; i < len_l; i += 8)				\
			ice_debug(hw_l, type,				\
				  "0x%04X  0x%016"PRIx64"\n",		\
				  i, *((u64 *)((buf_l) + i)));		\
} while (0)
#define ice_snprintf snprintf
#ifndef SNPRINTF
#define SNPRINTF ice_snprintf
#endif

#define ICE_PCI_REG(reg)     rte_read32(reg)
#define ICE_PCI_REG_ADDR(a, reg) \
	((volatile uint32_t *)((char *)(a)->hw_addr + (reg)))
static inline uint32_t ice_read_addr(volatile void *addr)
{
	return rte_le_to_cpu_32(ICE_PCI_REG(addr));
}

#define ICE_PCI_REG_WRITE(reg, value) \
	rte_write32((rte_cpu_to_le_32(value)), reg)

#define ice_flush(a)   ICE_READ_REG((a), GLGEN_STAT)
#define icevf_flush(a) ICE_READ_REG((a), VFGEN_RSTAT)
#define ICE_READ_REG(hw, reg) ice_read_addr(ICE_PCI_REG_ADDR((hw), (reg)))
#define ICE_WRITE_REG(hw, reg, value) \
	ICE_PCI_REG_WRITE(ICE_PCI_REG_ADDR((hw), (reg)), (value))

#define rd32(a, reg) ice_read_addr(ICE_PCI_REG_ADDR((a), (reg)))
#define wr32(a, reg, value) \
	ICE_PCI_REG_WRITE(ICE_PCI_REG_ADDR((a), (reg)), (value))
#define flush(a) ice_read_addr(ICE_PCI_REG_ADDR((a), (GLGEN_STAT)))
#define div64_long(n, d) ((n) / (d))

#define BITS_PER_BYTE       8

/* memory allocation tracking */
struct ice_dma_mem {
	void *va;
	u64 pa;
	u32 size;
	const void *zone;
} __attribute__((packed));

struct ice_virt_mem {
	void *va;
	u32 size;
} __attribute__((packed));

#define ice_malloc(h, s)    rte_zmalloc(NULL, s, 0)
#define ice_calloc(h, c, s) rte_zmalloc(NULL, (c) * (s), 0)
#define ice_free(h, m)         rte_free(m)

#define ice_memset(a, b, c, d) memset((a), (b), (c))
#define ice_memcpy(a, b, c, d) rte_memcpy((a), (b), (c))
#define ice_memdup(a, b, c, d) rte_memcpy(ice_malloc(a, c), b, c)

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
struct ice_lock {
	rte_spinlock_t spinlock;
};

static inline void
ice_init_lock(struct ice_lock *sp)
{
	rte_spinlock_init(&sp->spinlock);
}

static inline void
ice_acquire_lock(struct ice_lock *sp)
{
	rte_spinlock_lock(&sp->spinlock);
}

static inline void
ice_release_lock(struct ice_lock *sp)
{
	rte_spinlock_unlock(&sp->spinlock);
}

static inline void
ice_destroy_lock(__attribute__((unused)) struct ice_lock *sp)
{
}

struct ice_hw;

static inline void *
ice_alloc_dma_mem(__attribute__((unused)) struct ice_hw *hw,
		  struct ice_dma_mem *mem, u64 size)
{
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	if (!mem)
		return NULL;

	snprintf(z_name, sizeof(z_name), "ice_dma_%"PRIu64, rte_rand());
	mz = rte_memzone_reserve_bounded(z_name, size, SOCKET_ID_ANY, 0,
					 0, RTE_PGSIZE_2M);
	if (!mz)
		return NULL;

	mem->size = size;
	mem->va = mz->addr;
	mem->pa = mz->phys_addr;
	mem->zone = (const void *)mz;
	PMD_DRV_LOG(DEBUG, "memzone %s allocated with physical address: "
		    "%"PRIu64, mz->name, mem->pa);

	return mem->va;
}

static inline void
ice_free_dma_mem(__attribute__((unused)) struct ice_hw *hw,
		 struct ice_dma_mem *mem)
{
	PMD_DRV_LOG(DEBUG, "memzone %s to be freed with physical address: "
		    "%"PRIu64, ((const struct rte_memzone *)mem->zone)->name,
		    mem->pa);
	rte_memzone_free((const struct rte_memzone *)mem->zone);
	mem->zone = NULL;
	mem->va = NULL;
	mem->pa = (u64)0;
}

static inline u8
ice_hweight8(u32 num)
{
	u8 bits = 0;
	u32 i;

	for (i = 0; i < 8; i++) {
		bits += (u8)(num & 0x1);
		num >>= 1;
	}

	return bits;
}

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define DELAY(x) rte_delay_us(x)
#define ice_usec_delay(x) rte_delay_us(x)
#define ice_msec_delay(x, y) rte_delay_us(1000 * (x))
#define udelay(x) DELAY(x)
#define msleep(x) DELAY(1000 * (x))
#define usleep_range(min, max) msleep(DIV_ROUND_UP(min, 1000))

struct ice_list_entry {
	LIST_ENTRY(ice_list_entry) next;
};

LIST_HEAD(ice_list_head, ice_list_entry);

#define LIST_ENTRY_TYPE    ice_list_entry
#define LIST_HEAD_TYPE     ice_list_head
#define INIT_LIST_HEAD(list_head)  LIST_INIT(list_head)
#define LIST_DEL(entry)            LIST_REMOVE(entry, next)
/* LIST_EMPTY(list_head)) the same in sys/queue.h */

/*Note parameters are swapped*/
#define LIST_FIRST_ENTRY(head, type, field) (type *)((head)->lh_first)
#define LIST_NEXT_ENTRY(entry, type, field) \
	((type *)(entry)->field.next.le_next)
#define LIST_ADD(entry, list_head)    LIST_INSERT_HEAD(list_head, entry, next)
#define LIST_ADD_AFTER(entry, list_entry) \
	LIST_INSERT_AFTER(list_entry, entry, next)

static inline void list_add_tail(struct ice_list_entry *entry,
				 struct ice_list_head *head)
{
	struct ice_list_entry *tail = head->lh_first;

	if (tail == NULL) {
		LIST_INSERT_HEAD(head, entry, next);
		return;
	}
	while (tail->next.le_next != NULL)
		tail = tail->next.le_next;
	LIST_INSERT_AFTER(tail, entry, next);
}

#define LIST_ADD_TAIL(entry, head) list_add_tail(entry, head)
#define LIST_FOR_EACH_ENTRY(pos, head, type, member)			       \
	for ((pos) = (head)->lh_first ?					       \
		     container_of((head)->lh_first, struct type, member) :     \
		     0;							       \
	     (pos);							       \
	     (pos) = (pos)->member.next.le_next ?			       \
		     container_of((pos)->member.next.le_next, struct type,     \
				  member) :				       \
		     0)

#define LIST_REPLACE_INIT(list_head, head) do {				\
	(head)->lh_first = (list_head)->lh_first;			\
	INIT_LIST_HEAD(list_head);					\
} while (0)

#define HLIST_NODE_TYPE         LIST_ENTRY_TYPE
#define HLIST_HEAD_TYPE         LIST_HEAD_TYPE
#define INIT_HLIST_HEAD(list_head)             INIT_LIST_HEAD(list_head)
#define HLIST_ADD_HEAD(entry, list_head)       LIST_ADD(entry, list_head)
#define HLIST_EMPTY(list_head)                 LIST_EMPTY(list_head)
#define HLIST_DEL(entry)                       LIST_DEL(entry)
#define HLIST_FOR_EACH_ENTRY(pos, head, type, member) \
	LIST_FOR_EACH_ENTRY(pos, head, type, member)
#define LIST_FOR_EACH_ENTRY_SAFE(pos, tmp, head, type, member) \
	LIST_FOR_EACH_ENTRY(pos, head, type, member)

#ifndef ICE_DBG_TRACE
#define ICE_DBG_TRACE		BIT_ULL(0)
#endif

#ifndef DIVIDE_AND_ROUND_UP
#define DIVIDE_AND_ROUND_UP(a, b) (((a) + (b) - 1) / (b))
#endif

#ifndef ICE_INTEL_VENDOR_ID
#define ICE_INTEL_VENDOR_ID		0x8086
#endif

#ifndef IS_UNICAST_ETHER_ADDR
#define IS_UNICAST_ETHER_ADDR(addr) \
	((bool)((((u8 *)(addr))[0] % ((u8)0x2)) == 0))
#endif

#ifndef IS_MULTICAST_ETHER_ADDR
#define IS_MULTICAST_ETHER_ADDR(addr) \
	((bool)((((u8 *)(addr))[0] % ((u8)0x2)) == 1))
#endif

#ifndef IS_BROADCAST_ETHER_ADDR
/* Check whether an address is broadcast. */
#define IS_BROADCAST_ETHER_ADDR(addr)	\
	((bool)((((u16 *)(addr))[0] == ((u16)0xffff))))
#endif

#ifndef IS_ZERO_ETHER_ADDR
#define IS_ZERO_ETHER_ADDR(addr) \
	(((bool)((((u16 *)(addr))[0] == ((u16)0x0)))) && \
	 ((bool)((((u16 *)(addr))[1] == ((u16)0x0)))) && \
	 ((bool)((((u16 *)(addr))[2] == ((u16)0x0)))))
#endif

#endif /* _ICE_OSDEP_H_ */
