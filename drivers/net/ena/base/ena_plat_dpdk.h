/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) Amazon.com, Inc. or its affiliates.
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

#include <ethdev_driver.h>
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

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct rte_eth_dev ena_netdev;
typedef uint64_t dma_addr_t;

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif

#define ENA_PRIu64 PRIu64
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
#define ENA_COM_DEVICE_BUSY	-EBUSY

#define ____cacheline_aligned __rte_cache_aligned

#define ENA_CDESC_RING_SIZE_ALIGNMENT  (1 << 12) /* 4K */

#define ENA_MSLEEP(x) rte_delay_us_sleep(x * 1000)
#define ENA_USLEEP(x) rte_delay_us_sleep(x)
#define ENA_UDELAY(x) rte_delay_us_block(x)

#define ENA_TOUCH(x) ((void)(x))

#define wmb rte_wmb
#define rmb rte_rmb
#define mb rte_mb
#define mmiowb rte_io_wmb
#define __iomem

#ifndef READ_ONCE
#define READ_ONCE(var) (*((volatile typeof(var) *)(&(var))))
#endif

#define READ_ONCE8(var) READ_ONCE(var)
#define READ_ONCE16(var) READ_ONCE(var)
#define READ_ONCE32(var) READ_ONCE(var)

#define US_PER_S 1000000
#define ENA_GET_SYSTEM_USECS()						       \
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
#define BIT(nr)	RTE_BIT32(nr)
#define BIT64(nr)	RTE_BIT64(nr)
#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#define GENMASK(h, l)	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define GENMASK_ULL(h, l) (((~0ULL) - (1ULL << (l)) + 1) &		       \
			  (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

#define ena_trc_log(dev, level, fmt, ...)				       \
	(								       \
		ENA_TOUCH(dev),						       \
		rte_log(RTE_LOG_ ## level, ena_logtype_com,		       \
			"[ENA_COM: %s]" fmt, __func__, ##__VA_ARGS__)	       \
	)

#if (defined RTE_ETHDEV_DEBUG_TX) || (defined RTE_ETHDEV_DEBUG_RX)
#define ena_trc_dbg(dev, format, ...) ena_trc_log(dev, DEBUG, format, ##__VA_ARGS__)
#else
#define ena_trc_dbg(dev, format, ...)
#endif
#define ena_trc_info(dev, format, ...) ena_trc_log(dev, INFO, format, ##__VA_ARGS__)
#define ena_trc_warn(dev, format, ...) ena_trc_log(dev, WARNING, format, ##__VA_ARGS__)
#define ena_trc_err(dev, format, ...) ena_trc_log(dev, ERR, format, ##__VA_ARGS__)

#define ENA_WARN(cond, dev, format, ...)				       \
	do {								       \
		if (unlikely(cond)) {					       \
			ena_trc_err(dev,				       \
				"Warn failed on %s:%s:%d:" format,	       \
				__FILE__, __func__, __LINE__, ##__VA_ARGS__);  \
		}							       \
	} while (0)

/* Spinlock related methods */
#define ena_spinlock_t rte_spinlock_t
#define ENA_SPINLOCK_INIT(spinlock) rte_spinlock_init(&(spinlock))
#define ENA_SPINLOCK_LOCK(spinlock, flags)				       \
	__extension__ ({(void)(flags); rte_spinlock_lock(&(spinlock)); })
#define ENA_SPINLOCK_UNLOCK(spinlock, flags)				       \
	__extension__ ({(void)(flags); rte_spinlock_unlock(&(spinlock)); })
#define ENA_SPINLOCK_DESTROY(spinlock) ((void)(spinlock))

typedef struct {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	uint8_t flag;
} ena_wait_event_t;

#define ENA_WAIT_EVENT_INIT(waitevent)					       \
	do {								       \
		ena_wait_event_t *_we = &(waitevent);			       \
		pthread_mutex_init(&_we->mutex, NULL);			       \
		pthread_cond_init(&_we->cond, NULL);			       \
		_we->flag = 0;						       \
	} while (0)

#define ENA_WAIT_EVENT_WAIT(waitevent, timeout)				       \
	do {								       \
		ena_wait_event_t *_we = &(waitevent);			       \
		typeof(timeout) _tmo = (timeout);			       \
		int ret = 0;						       \
		struct timespec wait;					       \
		struct timeval now;					       \
		unsigned long timeout_us;				       \
		gettimeofday(&now, NULL);				       \
		wait.tv_sec = now.tv_sec + _tmo / 1000000UL;		       \
		timeout_us = _tmo % 1000000UL;				       \
		wait.tv_nsec = (now.tv_usec + timeout_us) * 1000UL;	       \
		pthread_mutex_lock(&_we->mutex);			       \
		while (ret == 0 && !_we->flag) {			       \
			ret = pthread_cond_timedwait(&_we->cond,	       \
				&_we->mutex, &wait);			       \
		}							       \
		/* Asserts only if not working on ena_wait_event_t */	       \
		if (unlikely(ret != 0 && ret != ETIMEDOUT))		       \
			ena_trc_err(NULL,				       \
				"Invalid wait event. pthread ret: %d\n", ret); \
		else if (unlikely(ret == ETIMEDOUT))			       \
			ena_trc_err(NULL,				       \
				"Timeout waiting for " #waitevent "\n");       \
		_we->flag = 0;						       \
		pthread_mutex_unlock(&_we->mutex);			       \
	} while (0)
#define ENA_WAIT_EVENT_SIGNAL(waitevent)				       \
	do {								       \
		ena_wait_event_t *_we = &(waitevent);			       \
		pthread_mutex_lock(&_we->mutex);			       \
		_we->flag = 1;						       \
		pthread_cond_signal(&_we->cond);			       \
		pthread_mutex_unlock(&_we->mutex);			       \
	} while (0)
/* pthread condition doesn't need to be rearmed after usage */
#define ENA_WAIT_EVENT_CLEAR(...)
#define ENA_WAIT_EVENT_DESTROY(waitevent) ((void)(waitevent))

#define ENA_MIGHT_SLEEP()

#define ena_time_t uint64_t
#define ena_time_high_res_t uint64_t

/* Note that high resolution timers are not used by the ENA PMD for now.
 * Although these macro definitions compile, it shall fail the
 * compilation in case the unimplemented API is called prematurely.
 */
#define ENA_TIME_EXPIRE(timeout)  ((timeout) < rte_get_timer_cycles())
#define ENA_TIME_EXPIRE_HIGH_RES(timeout) (RTE_SET_USED(timeout), 0)
#define ENA_TIME_INIT_HIGH_RES() 0
#define ENA_TIME_COMPARE_HIGH_RES(time1, time2) (RTE_SET_USED(time1), RTE_SET_USED(time2), 0)
#define ENA_GET_SYSTEM_TIMEOUT(timeout_us) \
	((timeout_us) * rte_get_timer_hz() / 1000000 + rte_get_timer_cycles())
#define ENA_GET_SYSTEM_TIMEOUT_HIGH_RES(current_time, timeout_us) \
	(RTE_SET_USED(current_time), RTE_SET_USED(timeout_us), 0)
#define ENA_GET_SYSTEM_TIME_HIGH_RES() 0

const struct rte_memzone *
ena_mem_alloc_coherent(struct rte_eth_dev_data *data, size_t size,
		       int socket_id, unsigned int alignment, void **virt_addr,
		       dma_addr_t *phys_addr);

#define ENA_MEM_ALLOC_COHERENT_ALIGNED(					       \
	dmadev, size, virt, phys, mem_handle, alignment)		       \
	do {								       \
		void *virt_addr;					       \
		dma_addr_t phys_addr;					       \
		(mem_handle) = ena_mem_alloc_coherent((dmadev), (size),	       \
			SOCKET_ID_ANY, (alignment), &virt_addr, &phys_addr);   \
		(virt) = virt_addr;					       \
		(phys) = phys_addr;					       \
	} while (0)
#define ENA_MEM_ALLOC_COHERENT(dmadev, size, virt, phys, mem_handle)	       \
		ENA_MEM_ALLOC_COHERENT_ALIGNED(dmadev, size, virt, phys,       \
			mem_handle, RTE_CACHE_LINE_SIZE)
#define ENA_MEM_FREE_COHERENT(dmadev, size, virt, phys, mem_handle)	       \
		__extension__ ({ ENA_TOUCH(size); ENA_TOUCH(phys); ENA_TOUCH(dmadev);	       \
		   rte_memzone_free(mem_handle); })

#define ENA_MEM_ALLOC_COHERENT_NODE_ALIGNED(				       \
	dmadev, size, virt, phys, mem_handle, node, alignment)			\
	do {								       \
		void *virt_addr;					       \
		dma_addr_t phys_addr;					       \
		(mem_handle) = ena_mem_alloc_coherent((dmadev), (size),	       \
			(node), (alignment), &virt_addr, &phys_addr);      \
		(virt) = virt_addr;					       \
		(phys) = phys_addr;					       \
	} while (0)
#define ENA_MEM_ALLOC_COHERENT_NODE(					       \
	dmadev, size, virt, phys, mem_handle, node)				\
		ENA_MEM_ALLOC_COHERENT_NODE_ALIGNED(dmadev, size, virt,	phys,  \
			mem_handle, node, RTE_CACHE_LINE_SIZE)
#define ENA_MEM_ALLOC_NODE(dmadev, size, virt, node)				\
	do {								       \
		ENA_TOUCH(dmadev);						\
		virt = rte_zmalloc_socket(NULL, size, 0, node);		       \
	} while (0)

#define ENA_MEM_ALLOC(dmadev, size) rte_zmalloc(NULL, size, 1)
#define ENA_MEM_FREE(dmadev, ptr, size)					       \
	__extension__ ({ ENA_TOUCH(dmadev); ENA_TOUCH(size); rte_free(ptr); })

#define ENA_DB_SYNC(mem_handle) ((void)mem_handle)

#define ENA_REG_WRITE32(bus, value, reg)				       \
	__extension__ ({ (void)(bus); rte_write32((value), (reg)); })
#define ENA_REG_WRITE32_RELAXED(bus, value, reg)			       \
	__extension__ ({ (void)(bus); rte_write32_relaxed((value), (reg)); })
#define ENA_REG_READ32(bus, reg)					       \
	__extension__ ({ (void)(bus); rte_read32_relaxed((reg)); })

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
#define prefetchw(x) rte_prefetch0_write(x)

#define lower_32_bits(x) ((uint32_t)(x))
#define upper_32_bits(x) ((uint32_t)(((x) >> 16) >> 16))

#define ENA_GET_SYSTEM_TIMEOUT(timeout_us)				       \
	((timeout_us) * rte_get_timer_hz() / 1000000 + rte_get_timer_cycles())
#define ENA_WAIT_EVENTS_DESTROY(admin_queue) ((void)(admin_queue))

/* The size must be 8 byte align */
#define ENA_MEMCPY_TO_DEVICE_64(dst, src, size)				       \
	do {								       \
		int count, i;						       \
		uint64_t *to = (uint64_t *)(dst);			       \
		const uint64_t *from = (const uint64_t *)(src);		       \
		count = (size) / 8;					       \
		for (i = 0; i < count; i++, from++, to++)		       \
			rte_write64_relaxed(*from, to);			       \
	} while(0)

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define ENA_FFS(x) ffs(x)

void ena_rss_key_fill(void *key, size_t size);

#define ENA_RSS_FILL_KEY(key, size) ena_rss_key_fill(key, size)

#define ENA_INTR_INITIAL_TX_INTERVAL_USECS_PLAT 0
#define ENA_INTR_INITIAL_RX_INTERVAL_USECS_PLAT 0

#include "ena_includes.h"

#define ENA_BITS_PER_U64(bitmap) (ena_bits_per_u64(bitmap))

#define ENA_FIELD_GET(value, mask, offset) (((value) & (mask)) >> (offset))
#define ENA_FIELD_PREP(value, mask, offset) (((value) << (offset)) & (mask))

#define ENA_ZERO_SHIFT 0

static __rte_always_inline int ena_bits_per_u64(uint64_t bitmap)
{
	int count = 0;

	while (bitmap) {
		bitmap &= (bitmap - 1);
		count++;
	}

	return count;
}

#define ENA_ADMIN_OS_DPDK 3

#endif /* DPDK_ENA_COM_ENA_PLAT_DPDK_H_ */
