/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB2_OSDEP_H
#define __DLB2_OSDEP_H

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_io.h>
#include <rte_log.h>
#include <rte_spinlock.h>
#include "../dlb2_main.h"
#include "dlb2_resource.h"
#include "../../dlb2_log.h"
#include "../../dlb2_user.h"


#define DLB2_PCI_REG_READ(addr)        rte_read32((void *)addr)
#define DLB2_PCI_REG_WRITE(reg, value) rte_write32(value, (void *)reg)

/* Read/write register 'reg' in the CSR BAR space */
#define DLB2_CSR_REG_ADDR(a, reg) ((void *)((uintptr_t)(a)->csr_kva + (reg)))
#define DLB2_CSR_RD(hw, reg) \
	DLB2_PCI_REG_READ(DLB2_CSR_REG_ADDR((hw), (reg)))
#define DLB2_CSR_WR(hw, reg, value) \
	DLB2_PCI_REG_WRITE(DLB2_CSR_REG_ADDR((hw), (reg)), (value))

/* Read/write register 'reg' in the func BAR space */
#define DLB2_FUNC_REG_ADDR(a, reg) ((void *)((uintptr_t)(a)->func_kva + (reg)))
#define DLB2_FUNC_RD(hw, reg) \
	DLB2_PCI_REG_READ(DLB2_FUNC_REG_ADDR((hw), (reg)))
#define DLB2_FUNC_WR(hw, reg, value) \
	DLB2_PCI_REG_WRITE(DLB2_FUNC_REG_ADDR((hw), (reg)), (value))

/* Map to PMDs logging interface */
#define DLB2_ERR(dev, fmt, args...) \
	DLB2_LOG_ERR(fmt, ## args)

#define DLB2_INFO(dev, fmt, args...) \
	DLB2_LOG_INFO(fmt, ## args)

#define DLB2_DEBUG(dev, fmt, args...) \
	DLB2_LOG_DBG(fmt, ## args)

/**
 * os_udelay() - busy-wait for a number of microseconds
 * @usecs: delay duration.
 */
static inline void os_udelay(int usecs)
{
	rte_delay_us(usecs);
}

/**
 * os_msleep() - sleep for a number of milliseconds
 * @usecs: delay duration.
 */
static inline void os_msleep(int msecs)
{
	rte_delay_ms(msecs);
}

#define DLB2_PP_BASE(__is_ldb) \
	((__is_ldb) ? DLB2_LDB_PP_BASE : DLB2_DIR_PP_BASE)

/**
 * os_map_producer_port() - map a producer port into the caller's address space
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: port ID
 * @is_ldb: true for load-balanced port, false for a directed port
 *
 * This function maps the requested producer port memory into the caller's
 * address space.
 *
 * Return:
 * Returns the base address at which the PP memory was mapped, else NULL.
 */
static inline void *os_map_producer_port(struct dlb2_hw *hw,
					 u8 port_id,
					 bool is_ldb)
{
	uint64_t addr;
	uint64_t pp_dma_base;

	pp_dma_base = (uintptr_t)hw->func_kva + DLB2_PP_BASE(is_ldb);
	addr = (pp_dma_base + (PAGE_SIZE * port_id));

	return (void *)(uintptr_t)addr;
}

/**
 * os_unmap_producer_port() - unmap a producer port
 * @addr: mapped producer port address
 *
 * This function undoes os_map_producer_port() by unmapping the producer port
 * memory from the caller's address space.
 *
 * Return:
 * Returns the base address at which the PP memory was mapped, else NULL.
 */
static inline void os_unmap_producer_port(struct dlb2_hw *hw, void *addr)
{
	RTE_SET_USED(hw);
	RTE_SET_USED(addr);
}

/**
 * os_fence_hcw() - fence an HCW to ensure it arrives at the device
 * @hw: dlb2_hw handle for a particular device.
 * @pp_addr: producer port address
 */
static inline void os_fence_hcw(struct dlb2_hw *hw, u64 *pp_addr)
{
	RTE_SET_USED(hw);

	/* To ensure outstanding HCWs reach the device, read the PP address. IA
	 * memory ordering prevents reads from passing older writes, and the
	 * mfence also ensures this.
	 */
	rte_mb();

	*(volatile u64 *)pp_addr;
}

/**
 * DLB2_HW_ERR() - log an error message
 * @dlb2: dlb2_hw handle for a particular device.
 * @...: variable string args.
 */
#define DLB2_HW_ERR(dlb2, ...) do {	\
	RTE_SET_USED(dlb2);		\
	DLB2_ERR(dlb2, __VA_ARGS__);	\
} while (0)

/**
 * DLB2_HW_DBG() - log an info message
 * @dlb2: dlb2_hw handle for a particular device.
 * @...: variable string args.
 */
#define DLB2_HW_DBG(dlb2, ...) do {	\
	RTE_SET_USED(dlb2);		\
	DLB2_DEBUG(dlb2, __VA_ARGS__);	\
} while (0)

/* The callback runs until it completes all outstanding QID->CQ
 * map and unmap requests. To prevent deadlock, this function gives other
 * threads a chance to grab the resource mutex and configure hardware.
 */
static void *dlb2_complete_queue_map_unmap(void *__args)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)__args;
	int ret;

	while (1) {
		rte_spinlock_lock(&dlb2_dev->resource_mutex);

		ret = dlb2_finish_unmap_qid_procedures(&dlb2_dev->hw);
		ret += dlb2_finish_map_qid_procedures(&dlb2_dev->hw);

		if (ret != 0) {
			rte_spinlock_unlock(&dlb2_dev->resource_mutex);
			/* Relinquish the CPU so the application can process
			 * its CQs, so this function doesn't deadlock.
			 */
			sched_yield();
		} else {
			break;
		}
	}

	dlb2_dev->worker_launched = false;

	rte_spinlock_unlock(&dlb2_dev->resource_mutex);

	return NULL;
}


/**
 * os_schedule_work() - launch a thread to process pending map and unmap work
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function launches a kernel thread that will run until all pending
 * map and unmap procedures are complete.
 */
static inline void os_schedule_work(struct dlb2_hw *hw)
{
	struct dlb2_dev *dlb2_dev;
	pthread_t complete_queue_map_unmap_thread;
	int ret;

	dlb2_dev = container_of(hw, struct dlb2_dev, hw);

	ret = rte_ctrl_thread_create(&complete_queue_map_unmap_thread,
				     "dlb_queue_unmap_waiter",
				     NULL,
				     dlb2_complete_queue_map_unmap,
				     dlb2_dev);
	if (ret)
		DLB2_ERR(dlb2_dev,
			 "Could not create queue complete map/unmap thread, err=%d\n",
			 ret);
	else
		dlb2_dev->worker_launched = true;
}

/**
 * os_worker_active() - query whether the map/unmap worker thread is active
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function returns a boolean indicating whether a thread (launched by
 * os_schedule_work()) is active. This function is used to determine
 * whether or not to launch a worker thread.
 */
static inline bool os_worker_active(struct dlb2_hw *hw)
{
	struct dlb2_dev *dlb2_dev;

	dlb2_dev = container_of(hw, struct dlb2_dev, hw);

	return dlb2_dev->worker_launched;
}

#endif /*  __DLB2_OSDEP_H */
