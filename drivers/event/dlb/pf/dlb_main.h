/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB_MAIN_H
#define __DLB_MAIN_H

#include <rte_debug.h>
#include <rte_log.h>
#include <rte_spinlock.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE (sysconf(_SC_PAGESIZE))
#endif

#include "base/dlb_hw_types.h"
#include "../dlb_user.h"

#define DLB_DEFAULT_UNREGISTER_TIMEOUT_S 5

struct dlb_dev {
	struct rte_pci_device *pdev;
	struct dlb_hw hw;
	/* struct list_head list; */
	struct device *dlb_device;
	bool domain_reset_failed;
	/* The resource mutex serializes access to driver data structures and
	 * hardware registers.
	 */
	rte_spinlock_t resource_mutex;
	rte_spinlock_t measurement_lock;
	bool worker_launched;
	u8 revision;
};

struct dlb_dev *dlb_probe(struct rte_pci_device *pdev);
void dlb_reset_done(struct dlb_dev *dlb_dev);

/* pf_ops */
int dlb_pf_init_driver_state(struct dlb_dev *dev);
void dlb_pf_free_driver_state(struct dlb_dev *dev);
void dlb_pf_init_hardware(struct dlb_dev *dev);
int dlb_pf_reset(struct dlb_dev *dlb_dev);

#endif /* __DLB_MAIN_H */
