/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef __SKELETON_EVENTDEV_H__
#define __SKELETON_EVENTDEV_H__

#include <eventdev_pmd_pci.h>
#include <eventdev_pmd_vdev.h>

extern int skeleton_eventdev_logtype;
#define RTE_LOGTYPE_SKELETON_EVENTDEV skeleton_eventdev_logtype

#ifdef RTE_LIBRTE_PMD_SKELETON_EVENTDEV_DEBUG
#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, SKELETON_EVENTDEV, "%s(): ", __func__, __VA_ARGS__)
#define PMD_DRV_FUNC_TRACE() PMD_DRV_LOG(DEBUG, ">>")
#else
#define PMD_DRV_LOG(...) do { } while (0)
#define PMD_DRV_FUNC_TRACE() do { } while (0)
#endif

#define PMD_DRV_ERR(...) \
	RTE_LOG_LINE_PREFIX(ERR, SKELETON_EVENTDEV, "%s(): ", __func__, __VA_ARGS__)

struct __rte_cache_aligned skeleton_eventdev {
	uintptr_t reg_base;
	uint16_t device_id;
	uint16_t vendor_id;
	uint16_t subsystem_device_id;
	uint16_t subsystem_vendor_id;
};

struct __rte_cache_aligned skeleton_port {
	uint8_t port_id;
};

static inline struct skeleton_eventdev *
skeleton_pmd_priv(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

#endif /* __SKELETON_EVENTDEV_H__ */
