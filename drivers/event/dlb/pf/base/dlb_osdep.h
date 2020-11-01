/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB_OSDEP_H__
#define __DLB_OSDEP_H__

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <cpuid.h>
#include <pthread.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_io.h>
#include <rte_log.h>
#include <rte_spinlock.h>
#include "../dlb_main.h"
#include "dlb_resource.h"
#include "../../dlb_log.h"
#include "../../dlb_user.h"


#define DLB_PCI_REG_READ(reg)        rte_read32((void *)reg)
#define DLB_PCI_REG_WRITE(reg, val)   rte_write32(val, (void *)reg)

#define DLB_CSR_REG_ADDR(a, reg) ((void *)((uintptr_t)(a)->csr_kva + (reg)))
#define DLB_CSR_RD(hw, reg) \
	DLB_PCI_REG_READ(DLB_CSR_REG_ADDR((hw), (reg)))
#define DLB_CSR_WR(hw, reg, val) \
	DLB_PCI_REG_WRITE(DLB_CSR_REG_ADDR((hw), (reg)), (val))

#define DLB_FUNC_REG_ADDR(a, reg) ((void *)((uintptr_t)(a)->func_kva + (reg)))
#define DLB_FUNC_RD(hw, reg) \
	DLB_PCI_REG_READ(DLB_FUNC_REG_ADDR((hw), (reg)))
#define DLB_FUNC_WR(hw, reg, val) \
	DLB_PCI_REG_WRITE(DLB_FUNC_REG_ADDR((hw), (reg)), (val))

extern unsigned int dlb_unregister_timeout_s;
/**
 * os_queue_unregister_timeout_s() - timeout (in seconds) to wait for queue
 *                                   unregister acknowledgments.
 */
static inline unsigned int os_queue_unregister_timeout_s(void)
{
	return dlb_unregister_timeout_s;
}

static inline size_t os_strlcpy(char *dst, const char *src, size_t sz)
{
	return rte_strlcpy(dst, src, sz);
}

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

#define DLB_PP_BASE(__is_ldb) ((__is_ldb) ? DLB_LDB_PP_BASE : DLB_DIR_PP_BASE)
/**
 * os_map_producer_port() - map a producer port into the caller's address space
 * @hw: dlb_hw handle for a particular device.
 * @port_id: port ID
 * @is_ldb: true for load-balanced port, false for a directed port
 *
 * This function maps the requested producer port memory into the caller's
 * address space.
 *
 * Return:
 * Returns the base address at which the PP memory was mapped, else NULL.
 */
static inline void *os_map_producer_port(struct dlb_hw *hw,
					 u8 port_id,
					 bool is_ldb)
{
	uint64_t addr;
	uint64_t pp_dma_base;


	pp_dma_base = (uintptr_t)hw->func_kva + DLB_PP_BASE(is_ldb);
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

/* PFPMD - Nothing to do here, since memory was not actually mapped by us */
static inline void os_unmap_producer_port(struct dlb_hw *hw, void *addr)
{
	RTE_SET_USED(hw);
	RTE_SET_USED(addr);
}

/**
 * os_fence_hcw() - fence an HCW to ensure it arrives at the device
 * @hw: dlb_hw handle for a particular device.
 * @pp_addr: producer port address
 */
static inline void os_fence_hcw(struct dlb_hw *hw, u64 *pp_addr)
{
	RTE_SET_USED(hw);

	/* To ensure outstanding HCWs reach the device, read the PP address. IA
	 * memory ordering prevents reads from passing older writes, and the
	 * mfence also ensures this.
	 */
	rte_mb();

	*(volatile u64 *)pp_addr;
}

/* Map to PMDs logging interface */
#define DLB_ERR(dev, fmt, args...) \
	DLB_LOG_ERR(fmt, ## args)

#define DLB_INFO(dev, fmt, args...) \
	DLB_LOG_INFO(fmt, ## args)

#define DLB_DEBUG(dev, fmt, args...) \
	DLB_LOG_DEBUG(fmt, ## args)

/**
 * DLB_HW_ERR() - log an error message
 * @dlb: dlb_hw handle for a particular device.
 * @...: variable string args.
 */
#define DLB_HW_ERR(dlb, ...) do {	\
	RTE_SET_USED(dlb);		\
	DLB_ERR(dlb, __VA_ARGS__);	\
} while (0)

/**
 * DLB_HW_INFO() - log an info message
 * @dlb: dlb_hw handle for a particular device.
 * @...: variable string args.
 */
#define DLB_HW_INFO(dlb, ...) do {	\
	RTE_SET_USED(dlb);		\
	DLB_INFO(dlb, __VA_ARGS__);	\
} while (0)

/*** scheduling functions ***/

/* The callback runs until it completes all outstanding QID->CQ
 * map and unmap requests. To prevent deadlock, this function gives other
 * threads a chance to grab the resource mutex and configure hardware.
 */
static void *dlb_complete_queue_map_unmap(void *__args)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)__args;
	int ret;

	while (1) {
		rte_spinlock_lock(&dlb_dev->resource_mutex);

		ret = dlb_finish_unmap_qid_procedures(&dlb_dev->hw);
		ret += dlb_finish_map_qid_procedures(&dlb_dev->hw);

		if (ret != 0) {
			rte_spinlock_unlock(&dlb_dev->resource_mutex);
			/* Relinquish the CPU so the application can process
			 * its CQs, so this function does not deadlock.
			 */
			sched_yield();
		} else
			break;
	}

	dlb_dev->worker_launched = false;

	rte_spinlock_unlock(&dlb_dev->resource_mutex);

	return NULL;
}


/**
 * os_schedule_work() - launch a thread to process pending map and unmap work
 * @hw: dlb_hw handle for a particular device.
 *
 * This function launches a thread that will run until all pending
 * map and unmap procedures are complete.
 */
static inline void os_schedule_work(struct dlb_hw *hw)
{
	struct dlb_dev *dlb_dev;
	pthread_t complete_queue_map_unmap_thread;
	int ret;

	dlb_dev = container_of(hw, struct dlb_dev, hw);

	ret = rte_ctrl_thread_create(&complete_queue_map_unmap_thread,
				     "dlb_queue_unmap_waiter",
				     NULL,
				     dlb_complete_queue_map_unmap,
				     dlb_dev);
	if (ret)
		DLB_ERR(dlb_dev,
		"Could not create queue complete map/unmap thread, err=%d\n",
			  ret);
	else
		dlb_dev->worker_launched = true;
}

/**
 * os_worker_active() - query whether the map/unmap worker thread is active
 * @hw: dlb_hw handle for a particular device.
 *
 * This function returns a boolean indicating whether a thread (launched by
 * os_schedule_work()) is active. This function is used to determine
 * whether or not to launch a worker thread.
 */
static inline bool os_worker_active(struct dlb_hw *hw)
{
	struct dlb_dev *dlb_dev;

	dlb_dev = container_of(hw, struct dlb_dev, hw);

	return dlb_dev->worker_launched;
}

/**
 * os_notify_user_space() - notify user space
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: ID of domain to notify.
 * @alert_id: alert ID.
 * @aux_alert_data: additional alert data.
 *
 * This function notifies user space of an alert (such as a remote queue
 * unregister or hardware alarm).
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 */
static inline int os_notify_user_space(struct dlb_hw *hw,
				       u32 domain_id,
				       u64 alert_id,
				       u64 aux_alert_data)
{
	RTE_SET_USED(hw);
	RTE_SET_USED(domain_id);
	RTE_SET_USED(alert_id);
	RTE_SET_USED(aux_alert_data);

	/* Not called for PF PMD */
	return -1;
}

enum dlb_dev_revision {
	DLB_A0,
	DLB_A1,
	DLB_A2,
	DLB_A3,
	DLB_B0,
};

/**
 * os_get_dev_revision() - query the device_revision
 * @hw: dlb_hw handle for a particular device.
 */
static inline enum dlb_dev_revision os_get_dev_revision(struct dlb_hw *hw)
{
	uint32_t a, b, c, d, stepping;

	RTE_SET_USED(hw);

	__cpuid(0x1, a, b, c, d);

	stepping = a & 0xf;

	switch (stepping) {
	case 0:
		return DLB_A0;
	case 1:
		return DLB_A1;
	case 2:
		return DLB_A2;
	case 3:
		return DLB_A3;
	default:
		/* Treat all revisions >= 4 as B0 */
		return DLB_B0;
	}
}

#endif /*  __DLB_OSDEP_H__ */
