/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_COMPAT_H_
#define _HINIC_COMPAT_H_

#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <rte_common.h>
#include <rte_bitops.h>
#include <rte_byteorder.h>
#include <rte_memzone.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_atomic.h>
#include <rte_spinlock.h>
#include <rte_cycles.h>
#include <rte_log.h>

typedef uint8_t   u8;
typedef int8_t    s8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef int32_t   s32;
typedef uint64_t  u64;

#ifndef dma_addr_t
typedef uint64_t  dma_addr_t;
#endif

#ifndef gfp_t
#define gfp_t unsigned
#endif

#ifndef bool
#define bool int
#endif

#ifndef FALSE
#define FALSE	(0)
#endif

#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef false
#define false	(0)
#endif

#ifndef true
#define true	(1)
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define HINIC_ERROR	(-1)
#define HINIC_OK	(0)

#ifndef BIT
#define BIT(n) (1 << (n))
#endif

#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))
#define lower_32_bits(n) ((u32)(n))

/* Returns X / Y, rounding up.  X must be nonnegative to round correctly. */
#define DIV_ROUND_UP(X, Y) (((X) + ((Y) - 1)) / (Y))

/* Returns X rounded up to the nearest multiple of Y. */
#define ROUND_UP(X, Y) (DIV_ROUND_UP(X, Y) * (Y))

#undef  ALIGN
#define ALIGN(x, a)  RTE_ALIGN(x, a)

#define PTR_ALIGN(p, a)		((typeof(p))ALIGN((unsigned long)(p), (a)))

/* Reported driver name. */
#define HINIC_DRIVER_NAME "net_hinic"

extern int hinic_logtype;

#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, hinic_logtype, \
		HINIC_DRIVER_NAME": " fmt "\n", ##args)

/* common definition */
#ifndef ETH_ALEN
#define ETH_ALEN		6
#endif
#define ETH_HLEN		14
#define ETH_CRC_LEN		4
#define VLAN_PRIO_SHIFT		13
#define VLAN_N_VID		4096

/* bit order interface */
#define cpu_to_be16(o) rte_cpu_to_be_16(o)
#define cpu_to_be32(o) rte_cpu_to_be_32(o)
#define cpu_to_be64(o) rte_cpu_to_be_64(o)
#define cpu_to_le32(o) rte_cpu_to_le_32(o)
#define be16_to_cpu(o) rte_be_to_cpu_16(o)
#define be32_to_cpu(o) rte_be_to_cpu_32(o)
#define be64_to_cpu(o) rte_be_to_cpu_64(o)
#define le32_to_cpu(o) rte_le_to_cpu_32(o)

/* virt memory and dma phy memory */
#define __iomem
#define GFP_KERNEL		RTE_MEMZONE_IOVA_CONTIG
#define HINIC_PAGE_SHIFT	12
#define HINIC_PAGE_SIZE		RTE_PGSIZE_4K
#define HINIC_MEM_ALLOC_ALIGN_MIN	8

#define HINIC_PAGE_SIZE_DPDK	6

void *dma_zalloc_coherent(void *dev, size_t size, dma_addr_t *dma_handle,
			  unsigned int socket_id);

void *dma_zalloc_coherent_aligned(void *hwdev, size_t size,
		dma_addr_t *dma_handle, unsigned int socket_id);

void *dma_zalloc_coherent_aligned256k(void *hwdev, size_t size,
			      dma_addr_t *dma_handle, unsigned int socket_id);

void dma_free_coherent(void *dev, size_t size, void *virt, dma_addr_t phys);

/* dma pool alloc and free */
#define	pci_pool dma_pool
#define	pci_pool_alloc(pool, handle) dma_pool_alloc(pool, handle)
#define	pci_pool_free(pool, vaddr, addr) dma_pool_free(pool, vaddr, addr)

struct dma_pool *dma_pool_create(const char *name, void *dev, size_t size,
				size_t align, size_t boundary);
void dma_pool_destroy(struct dma_pool *pool);
void *dma_pool_alloc(struct pci_pool *pool, dma_addr_t *dma_addr);
void dma_pool_free(struct pci_pool *pool, void *vaddr, dma_addr_t dma);

#define kzalloc(size, flag) rte_zmalloc(NULL, size, HINIC_MEM_ALLOC_ALIGN_MIN)
#define kzalloc_aligned(size, flag) rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE)
#define kfree(ptr)            rte_free(ptr)

/* mmio interface */
static inline void writel(u32 value, volatile void  *addr)
{
	*(volatile u32 *)addr = value;
}

static inline u32 readl(const volatile void *addr)
{
	return *(const volatile u32 *)addr;
}

#define __raw_writel(value, reg) writel((value), (reg))
#define __raw_readl(reg) readl((reg))

/* Spinlock related interface */
#define hinic_spinlock_t rte_spinlock_t

#define spinlock_t rte_spinlock_t
#define spin_lock_init(spinlock_prt)	rte_spinlock_init(spinlock_prt)
#define spin_lock_deinit(lock)
#define spin_lock(spinlock_prt)		rte_spinlock_lock(spinlock_prt)
#define spin_unlock(spinlock_prt)	rte_spinlock_unlock(spinlock_prt)

#ifdef CLOCK_MONOTONIC_RAW /* Defined in glibc bits/time.h */
#define CLOCK_TYPE CLOCK_MONOTONIC_RAW
#else
#define CLOCK_TYPE CLOCK_MONOTONIC
#endif
#define HINIC_MUTEX_TIMEOUT  10

static inline unsigned long clock_gettime_ms(void)
{
	struct timespec tv;

	(void)clock_gettime(CLOCK_TYPE, &tv);

	return (unsigned long)tv.tv_sec * 1000 +
	       (unsigned long)tv.tv_nsec / 1000000;
}

#define jiffies	clock_gettime_ms()
#define msecs_to_jiffies(ms)	(ms)
#define time_before(now, end)	((now) < (end))

/* misc kernel utils */
static inline u16 ilog2(u32 n)
{
	u16 res = 0;

	while (n > 1) {
		n >>= 1;
		res++;
	}

	return res;
}

static inline int hinic_mutex_init(pthread_mutex_t *pthreadmutex,
					const pthread_mutexattr_t *mattr)
{
	int err;

	err = pthread_mutex_init(pthreadmutex, mattr);
	if (unlikely(err))
		PMD_DRV_LOG(ERR, "Fail to initialize mutex, error: %d", err);

	return err;
}

static inline int hinic_mutex_destroy(pthread_mutex_t *pthreadmutex)
{
	int err;

	err = pthread_mutex_destroy(pthreadmutex);
	if (unlikely(err))
		PMD_DRV_LOG(ERR, "Fail to destroy mutex, error: %d", err);

	return err;
}

static inline int hinic_mutex_lock(pthread_mutex_t *pthreadmutex)
{
	int err;
	struct timespec tout;

	(void)clock_gettime(CLOCK_TYPE, &tout);

	tout.tv_sec += HINIC_MUTEX_TIMEOUT;
	err = pthread_mutex_timedlock(pthreadmutex, &tout);
	if (err)
		PMD_DRV_LOG(ERR, "Mutex lock failed. (ErrorNo=%d)", err);

	return err;
}

static inline int hinic_mutex_unlock(pthread_mutex_t *pthreadmutex)
{
	return pthread_mutex_unlock(pthreadmutex);
}

#endif /* _HINIC_COMPAT_H_ */
