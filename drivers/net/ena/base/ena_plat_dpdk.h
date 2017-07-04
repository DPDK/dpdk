/*-
* BSD LICENSE
*
* Copyright (c) 2015-2016 Amazon.com, Inc. or its affiliates.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in
* the documentation and/or other materials provided with the
* distribution.
* * Neither the name of copyright holder nor the names of its
* contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef DPDK_ENA_COM_ENA_PLAT_DPDK_H_
#define DPDK_ENA_COM_ENA_PLAT_DPDK_H_

#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_cycles.h>
#include <rte_io.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_spinlock.h>

#include <sys/time.h>

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
#define ENA_COM_PERMISSION	-EPERM
#define ENA_COM_TIMER_EXPIRED	-ETIME
#define ENA_COM_FAULT	-EFAULT
#define ENA_COM_TRY_AGAIN	-EAGAIN

#define ____cacheline_aligned __rte_cache_aligned

#define ENA_ABORT() abort()

#define ENA_MSLEEP(x) rte_delay_ms(x)
#define ENA_UDELAY(x) rte_delay_us(x)

#define ENA_TOUCH(x) ((void)(x))
#define memcpy_toio memcpy
#define wmb rte_wmb
#define rmb rte_wmb
#define mb rte_mb
#define __iomem

#define US_PER_S 1000000
#define ENA_GET_SYSTEM_USECS()						\
	(rte_get_timer_cycles() * US_PER_S / rte_get_timer_hz())

#if RTE_LOG_LEVEL >= RTE_LOG_DEBUG
#define ENA_ASSERT(cond, format, arg...)				\
	do {								\
		if (unlikely(!(cond))) {				\
			RTE_LOG(ERR, PMD, format, ##arg);		\
			rte_panic("line %d\tassert \"" #cond "\""	\
					"failed\n", __LINE__);		\
		}							\
	} while (0)
#else
#define ENA_ASSERT(cond, format, arg...) do {} while (0)
#endif

#define ENA_MAX32(x, y) RTE_MAX((x), (y))
#define ENA_MAX16(x, y) RTE_MAX((x), (y))
#define ENA_MAX8(x, y) RTE_MAX((x), (y))
#define ENA_MIN32(x, y) RTE_MIN((x), (y))
#define ENA_MIN16(x, y) RTE_MIN((x), (y))
#define ENA_MIN8(x, y) RTE_MIN((x), (y))

#define U64_C(x) x ## ULL
#define BIT(nr)         (1UL << (nr))
#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#define GENMASK(h, l)	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define GENMASK_ULL(h, l) (((U64_C(1) << ((h) - (l) + 1)) - 1) << (l))

#ifdef RTE_LIBRTE_ENA_COM_DEBUG
#define ena_trc_dbg(format, arg...)					\
	RTE_LOG(DEBUG, PMD, "[ENA_COM: %s] " format, __func__, ##arg)
#define ena_trc_info(format, arg...)					\
	RTE_LOG(INFO, PMD, "[ENA_COM: %s] " format, __func__, ##arg)
#define ena_trc_warn(format, arg...)					\
	RTE_LOG(ERR, PMD, "[ENA_COM: %s] " format, __func__, ##arg)
#define ena_trc_err(format, arg...)					\
	RTE_LOG(ERR, PMD, "[ENA_COM: %s] " format, __func__, ##arg)
#else
#define ena_trc_dbg(format, arg...) do { } while (0)
#define ena_trc_info(format, arg...) do { } while (0)
#define ena_trc_warn(format, arg...) do { } while (0)
#define ena_trc_err(format, arg...) do { } while (0)
#endif /* RTE_LIBRTE_ENA_COM_DEBUG */

/* Spinlock related methods */
#define ena_spinlock_t rte_spinlock_t
#define ENA_SPINLOCK_INIT(spinlock) rte_spinlock_init(&spinlock)
#define ENA_SPINLOCK_LOCK(spinlock, flags)				\
	({(void)flags; rte_spinlock_lock(&spinlock); })
#define ENA_SPINLOCK_UNLOCK(spinlock, flags)				\
	({(void)flags; rte_spinlock_unlock(&(spinlock)); })

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

#define ena_wait_event_t ena_wait_queue_t
#define ENA_MIGHT_SLEEP()

#define ENA_MEM_ALLOC_COHERENT(dmadev, size, virt, phys, handle)	\
	do {								\
		const struct rte_memzone *mz;				\
		char z_name[RTE_MEMZONE_NAMESIZE];			\
		ENA_TOUCH(dmadev); ENA_TOUCH(handle);			\
		snprintf(z_name, sizeof(z_name),			\
				"ena_alloc_%d", ena_alloc_cnt++);	\
		mz = rte_memzone_reserve(z_name, size, SOCKET_ID_ANY, 0); \
		memset(mz->addr, 0, size);				\
		virt = mz->addr;					\
		phys = mz->phys_addr;					\
		handle = mz;						\
	} while (0)
#define ENA_MEM_FREE_COHERENT(dmadev, size, virt, phys, handle) 	\
		({ ENA_TOUCH(size); ENA_TOUCH(phys);			\
		   ENA_TOUCH(dmadev);					\
		   rte_memzone_free(handle); })

#define ENA_MEM_ALLOC_COHERENT_NODE(dmadev, size, virt, phys, node, dev_node) \
	do {								\
		const struct rte_memzone *mz;				\
		char z_name[RTE_MEMZONE_NAMESIZE];			\
		ENA_TOUCH(dmadev); ENA_TOUCH(dev_node);			\
		snprintf(z_name, sizeof(z_name),			\
				"ena_alloc_%d", ena_alloc_cnt++);	\
		mz = rte_memzone_reserve(z_name, size, node, 0); \
		memset(mz->addr, 0, size);				\
		virt = mz->addr;					\
		phys = mz->phys_addr;					\
	} while (0)

#define ENA_MEM_ALLOC_NODE(dmadev, size, virt, node, dev_node) \
	do {								\
		const struct rte_memzone *mz;				\
		char z_name[RTE_MEMZONE_NAMESIZE];			\
		ENA_TOUCH(dmadev); ENA_TOUCH(dev_node);			\
		snprintf(z_name, sizeof(z_name),			\
				"ena_alloc_%d", ena_alloc_cnt++);	\
		mz = rte_memzone_reserve(z_name, size, node, 0); \
		memset(mz->addr, 0, size);				\
		virt = mz->addr;					\
	} while (0)

#define ENA_MEM_ALLOC(dmadev, size) rte_zmalloc(NULL, size, 1)
#define ENA_MEM_FREE(dmadev, ptr) ({ENA_TOUCH(dmadev); rte_free(ptr); })

#define ENA_REG_WRITE32(value, reg) rte_write32_relaxed((value), (reg))
#define ENA_REG_READ32(reg) rte_read32_relaxed((reg))

#define ATOMIC32_INC(i32_ptr) rte_atomic32_inc(i32_ptr)
#define ATOMIC32_DEC(i32_ptr) rte_atomic32_dec(i32_ptr)
#define ATOMIC32_SET(i32_ptr, val) rte_atomic32_set(i32_ptr, val)
#define ATOMIC32_READ(i32_ptr) rte_atomic32_read(i32_ptr)

#define msleep(x) rte_delay_us(x * 1000)
#define udelay(x) rte_delay_us(x)

#define MAX_ERRNO       4095
#define IS_ERR(x) (((unsigned long)x) >= (unsigned long)-MAX_ERRNO)
#define ERR_PTR(error) ((void *)(long)error)
#define PTR_ERR(error) ((long)(void *)error)
#define might_sleep()

#endif /* DPDK_ENA_COM_ENA_PLAT_DPDK_H_ */
