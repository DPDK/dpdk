/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2020 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#ifndef DPDK_ENA_COM_ENA_PLAT_DPDK_H_
#define DPDK_ENA_COM_ENA_PLAT_DPDK_H_

#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_cycles.h>
#include <rte_io.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_prefetch.h>
#include <rte_spinlock.h>

#include <sys/time.h>
#include <rte_memcpy.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef uint64_t dma_addr_t;
#ifndef ETIME
#define ETIME ETIMEDOUT
#endif

#define ena_atomic32_t rte_atomic32_t
#define ena_mem_handle_t const struct rte_memzone *

#define SZ_256 (256U)
#define SZ_4K (4096U)

#define ENA_COM_OK	0
#define ENA_COM_NO_MEM	-ENOMEM
#define ENA_COM_INVAL	-EINVAL
#define ENA_COM_NO_SPACE	-ENOSPC
#define ENA_COM_NO_DEVICE	-ENODEV
#define ENA_COM_TIMER_EXPIRED	-ETIME
#define ENA_COM_FAULT	-EFAULT
#define ENA_COM_TRY_AGAIN	-EAGAIN
#define ENA_COM_UNSUPPORTED    -EOPNOTSUPP
#define ENA_COM_EIO    -EIO

#define ____cacheline_aligned __rte_cache_aligned

#define ENA_ABORT() abort()

#define ENA_MSLEEP(x) rte_delay_us_sleep(x * 1000)
#define ENA_USLEEP(x) rte_delay_us_sleep(x)
#define ENA_UDELAY(x) rte_delay_us_block(x)

#define ENA_TOUCH(x) ((void)(x))
/* Avoid nested declaration on arm64, as it may define rte_memcpy as memcpy. */
#if defined(RTE_ARCH_X86)
#undef memcpy
#define memcpy rte_memcpy
#endif
#define wmb rte_wmb
#define rmb rte_rmb
#define mb rte_mb
#define mmiowb rte_io_wmb
#define __iomem

#define US_PER_S 1000000
#define ENA_GET_SYSTEM_USECS()						\
	(rte_get_timer_cycles() * US_PER_S / rte_get_timer_hz())

extern int ena_logtype_com;

#define ENA_MAX_T(type, x, y) RTE_MAX((type)(x), (type)(y))
#define ENA_MAX32(x, y) ENA_MAX_T(uint32_t, (x), (y))
#define ENA_MAX16(x, y) ENA_MAX_T(uint16_t, (x), (y))
#define ENA_MAX8(x, y) ENA_MAX_T(uint8_t, (x), (y))
#define ENA_MIN_T(type, x, y) RTE_MIN((type)(x), (type)(y))
#define ENA_MIN32(x, y) ENA_MIN_T(uint32_t, (x), (y))
#define ENA_MIN16(x, y) ENA_MIN_T(uint16_t, (x), (y))
#define ENA_MIN8(x, y) ENA_MIN_T(uint8_t, (x), (y))

#define BITS_PER_LONG_LONG (__SIZEOF_LONG_LONG__ * 8)
#define U64_C(x) x ## ULL
#define BIT(nr)         (1UL << (nr))
#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#define GENMASK(h, l)	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define GENMASK_ULL(h, l) (((~0ULL) - (1ULL << (l)) + 1) & \
			  (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

#ifdef RTE_LIBRTE_ENA_COM_DEBUG
#define ena_trc_log(level, fmt, arg...) \
	rte_log(RTE_LOG_ ## level, ena_logtype_com, \
		"[ENA_COM: %s]" fmt, __func__, ##arg)

#define ena_trc_dbg(format, arg...)	ena_trc_log(DEBUG, format, ##arg)
#define ena_trc_info(format, arg...)	ena_trc_log(INFO, format, ##arg)
#define ena_trc_warn(format, arg...)	ena_trc_log(WARNING, format, ##arg)
#define ena_trc_err(format, arg...)	ena_trc_log(ERR, format, ##arg)
#else
#define ena_trc_dbg(format, arg...) do { } while (0)
#define ena_trc_info(format, arg...) do { } while (0)
#define ena_trc_warn(format, arg...) do { } while (0)
#define ena_trc_err(format, arg...) do { } while (0)
#endif /* RTE_LIBRTE_ENA_COM_DEBUG */

#define ENA_WARN(cond, format, arg...)                                 \
do {                                                                   \
       if (unlikely(cond)) {                                           \
               ena_trc_err(                                            \
                       "Warn failed on %s:%s:%d:" format,              \
                       __FILE__, __func__, __LINE__, ##arg);           \
       }                                                               \
} while (0)

/* Spinlock related methods */
#define ena_spinlock_t rte_spinlock_t
#define ENA_SPINLOCK_INIT(spinlock) rte_spinlock_init(&spinlock)
#define ENA_SPINLOCK_LOCK(spinlock, flags)				\
	({(void)flags; rte_spinlock_lock(&spinlock); })
#define ENA_SPINLOCK_UNLOCK(spinlock, flags)				\
	({(void)flags; rte_spinlock_unlock(&(spinlock)); })
#define ENA_SPINLOCK_DESTROY(spinlock) ((void)spinlock)

#define q_waitqueue_t			\
	struct {			\
		pthread_cond_t cond;	\
		pthread_mutex_t mutex;	\
	}

#define ena_wait_queue_t q_waitqueue_t

#define ENA_WAIT_EVENT_INIT(waitqueue)					\
	do {								\
		pthread_mutex_init(&(waitqueue).mutex, NULL);		\
		pthread_cond_init(&(waitqueue).cond, NULL);		\
	} while (0)

#define ENA_WAIT_EVENT_WAIT(waitevent, timeout)				\
	do {								\
		struct timespec wait;					\
		struct timeval now;					\
		unsigned long timeout_us;				\
		gettimeofday(&now, NULL);				\
		wait.tv_sec = now.tv_sec + timeout / 1000000UL;		\
		timeout_us = timeout % 1000000UL;			\
		wait.tv_nsec = (now.tv_usec + timeout_us) * 1000UL;	\
		pthread_mutex_lock(&waitevent.mutex);			\
		pthread_cond_timedwait(&waitevent.cond,			\
				&waitevent.mutex, &wait);		\
		pthread_mutex_unlock(&waitevent.mutex);			\
	} while (0)
#define ENA_WAIT_EVENT_SIGNAL(waitevent) pthread_cond_signal(&waitevent.cond)
/* pthread condition doesn't need to be rearmed after usage */
#define ENA_WAIT_EVENT_CLEAR(...)
#define ENA_WAIT_EVENT_DESTROY(waitqueue) ((void)(waitqueue))

#define ena_wait_event_t ena_wait_queue_t
#define ENA_MIGHT_SLEEP()

#define ena_time_t uint64_t
#define ENA_TIME_EXPIRE(timeout)  (timeout < rte_get_timer_cycles())
#define ENA_GET_SYSTEM_TIMEOUT(timeout_us)                             \
       (timeout_us * rte_get_timer_hz() / 1000000 + rte_get_timer_cycles())

/*
 * Each rte_memzone should have unique name.
 * To satisfy it, count number of allocations and add it to name.
 */
extern rte_atomic32_t ena_alloc_cnt;

#define ENA_MEM_ALLOC_COHERENT_ALIGNED(					\
	dmadev, size, virt, phys, handle, alignment)			\
	do {								\
		const struct rte_memzone *mz = NULL;			\
		ENA_TOUCH(dmadev); ENA_TOUCH(handle);			\
		if (size > 0) {						\
			char z_name[RTE_MEMZONE_NAMESIZE];		\
			snprintf(z_name, sizeof(z_name),		\
			 "ena_alloc_%d",				\
			 rte_atomic32_add_return(&ena_alloc_cnt, 1));	\
			mz = rte_memzone_reserve_aligned(z_name, size,	\
					SOCKET_ID_ANY,			\
					RTE_MEMZONE_IOVA_CONTIG,	\
					alignment);			\
			handle = mz;					\
		}							\
		if (mz == NULL) {					\
			virt = NULL;					\
			phys = 0;					\
		} else {						\
			memset(mz->addr, 0, size);			\
			virt = mz->addr;				\
			phys = mz->iova;				\
		}							\
	} while (0)
#define ENA_MEM_ALLOC_COHERENT(dmadev, size, virt, phys, handle)	\
		ENA_MEM_ALLOC_COHERENT_ALIGNED(				\
			dmadev,						\
			size,						\
			virt,						\
			phys,						\
			handle,						\
			RTE_CACHE_LINE_SIZE)
#define ENA_MEM_FREE_COHERENT(dmadev, size, virt, phys, handle) 	\
		({ ENA_TOUCH(size); ENA_TOUCH(phys);			\
		   ENA_TOUCH(dmadev);					\
		   rte_memzone_free(handle); })

#define ENA_MEM_ALLOC_COHERENT_NODE_ALIGNED(				\
	dmadev, size, virt, phys, mem_handle, node, dev_node, alignment) \
	do {								\
		const struct rte_memzone *mz = NULL;			\
		ENA_TOUCH(dmadev); ENA_TOUCH(dev_node);			\
		if (size > 0) {						\
			char z_name[RTE_MEMZONE_NAMESIZE];		\
			snprintf(z_name, sizeof(z_name),		\
			 "ena_alloc_%d",				\
			 rte_atomic32_add_return(&ena_alloc_cnt, 1));   \
			mz = rte_memzone_reserve_aligned(z_name, size, node, \
				RTE_MEMZONE_IOVA_CONTIG, alignment);	\
			mem_handle = mz;				\
		}							\
		if (mz == NULL) {					\
			virt = NULL;					\
			phys = 0;					\
		} else {						\
			memset(mz->addr, 0, size);			\
			virt = mz->addr;				\
			phys = mz->iova;				\
		}							\
	} while (0)
#define ENA_MEM_ALLOC_COHERENT_NODE(					\
	dmadev, size, virt, phys, mem_handle, node, dev_node)		\
		ENA_MEM_ALLOC_COHERENT_NODE_ALIGNED(			\
			dmadev,						\
			size,						\
			virt,						\
			phys,						\
			mem_handle,					\
			node,						\
			dev_node,					\
			RTE_CACHE_LINE_SIZE)
#define ENA_MEM_ALLOC_NODE(dmadev, size, virt, node, dev_node) \
	do {								\
		ENA_TOUCH(dmadev); ENA_TOUCH(dev_node);			\
		virt = rte_zmalloc_socket(NULL, size, 0, node);		\
	} while (0)

#define ENA_MEM_ALLOC(dmadev, size) rte_zmalloc(NULL, size, 1)
#define ENA_MEM_FREE(dmadev, ptr, size)					\
	({ ENA_TOUCH(dmadev); ENA_TOUCH(size); rte_free(ptr); })

#define ENA_DB_SYNC(mem_handle) ((void)mem_handle)

#define ENA_REG_WRITE32(bus, value, reg)				\
	({ (void)(bus); rte_write32((value), (reg)); })
#define ENA_REG_WRITE32_RELAXED(bus, value, reg)			\
	({ (void)(bus); rte_write32_relaxed((value), (reg)); })
#define ENA_REG_READ32(bus, reg)					\
	({ (void)(bus); rte_read32_relaxed((reg)); })

#define ATOMIC32_INC(i32_ptr) rte_atomic32_inc(i32_ptr)
#define ATOMIC32_DEC(i32_ptr) rte_atomic32_dec(i32_ptr)
#define ATOMIC32_SET(i32_ptr, val) rte_atomic32_set(i32_ptr, val)
#define ATOMIC32_READ(i32_ptr) rte_atomic32_read(i32_ptr)

#define msleep(x) rte_delay_us(x * 1000)
#define udelay(x) rte_delay_us(x)

#define dma_rmb() rmb()

#define MAX_ERRNO       4095
#define IS_ERR(x) (((unsigned long)x) >= (unsigned long)-MAX_ERRNO)
#define ERR_PTR(error) ((void *)(long)error)
#define PTR_ERR(error) ((long)(void *)error)
#define might_sleep()

#define prefetch(x) rte_prefetch0(x)
#define prefetchw(x) prefetch(x)

#define lower_32_bits(x) ((uint32_t)(x))
#define upper_32_bits(x) ((uint32_t)(((x) >> 16) >> 16))

#define ENA_TIME_EXPIRE(timeout)  (timeout < rte_get_timer_cycles())
#define ENA_GET_SYSTEM_TIMEOUT(timeout_us)				\
    (timeout_us * rte_get_timer_hz() / 1000000 + rte_get_timer_cycles())
#define ENA_WAIT_EVENTS_DESTROY(admin_queue) ((void)(admin_queue))

#ifndef READ_ONCE
#define READ_ONCE(var) (*((volatile typeof(var) *)(&(var))))
#endif

#define READ_ONCE8(var) READ_ONCE(var)
#define READ_ONCE16(var) READ_ONCE(var)
#define READ_ONCE32(var) READ_ONCE(var)

/* The size must be 8 byte align */
#define ENA_MEMCPY_TO_DEVICE_64(dst, src, size)				\
	do {								\
		int count, i;						\
		uint64_t *to = (uint64_t *)(dst);			\
		const uint64_t *from = (const uint64_t *)(src);		\
		count = (size) / 8;					\
		for (i = 0; i < count; i++, from++, to++)		\
			rte_write64_relaxed(*from, to);			\
	} while(0)

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define ENA_FFS(x) ffs(x)

void ena_rss_key_fill(void *key, size_t size);

#define ENA_RSS_FILL_KEY(key, size) ena_rss_key_fill(key, size)

#define ENA_INTR_INITIAL_TX_INTERVAL_USECS_PLAT 0

#define ENA_PRIu64 PRIu64

#include "ena_includes.h"
#endif /* DPDK_ENA_COM_ENA_PLAT_DPDK_H_ */
