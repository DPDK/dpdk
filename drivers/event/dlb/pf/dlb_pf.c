/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <rte_debug.h>
#include <rte_log.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_io.h>
#include <rte_memory.h>
#include <rte_string_fns.h>

#include "../dlb_priv.h"
#include "../dlb_iface.h"
#include "../dlb_inline_fns.h"
#include "dlb_main.h"
#include "base/dlb_hw_types.h"
#include "base/dlb_osdep.h"
#include "base/dlb_resource.h"

static void
dlb_pf_iface_fn_ptrs_init(void)
{

}

/* PCI DEV HOOKS */
static int
dlb_eventdev_pci_init(struct rte_eventdev *eventdev)
{
	int ret = 0;
	struct rte_pci_device *pci_dev;
	struct dlb_devargs dlb_args = {
		.socket_id = rte_socket_id(),
		.max_num_events = DLB_MAX_NUM_LDB_CREDITS,
		.num_dir_credits_override = -1,
		.defer_sched = 0,
		.num_atm_inflights = DLB_NUM_ATOMIC_INFLIGHTS_PER_QUEUE,
	};
	struct dlb_eventdev *dlb;

	DLB_LOG_DBG("Enter with dev_id=%d socket_id=%d",
		    eventdev->data->dev_id, eventdev->data->socket_id);

	dlb_entry_points_init(eventdev);

	dlb_pf_iface_fn_ptrs_init();

	pci_dev = RTE_DEV_TO_PCI(eventdev->dev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		dlb = dlb_pmd_priv(eventdev); /* rte_zmalloc_socket mem */

		/* Probe the DLB PF layer */
		dlb->qm_instance.pf_dev = dlb_probe(pci_dev);

		if (dlb->qm_instance.pf_dev == NULL) {
			DLB_LOG_ERR("DLB PF Probe failed with error %d\n",
				    rte_errno);
			ret = -rte_errno;
			goto dlb_probe_failed;
		}

		/* Were we invoked with runtime parameters? */
		if (pci_dev->device.devargs) {
			ret = dlb_parse_params(pci_dev->device.devargs->args,
					       pci_dev->device.devargs->name,
					       &dlb_args);
			if (ret) {
				DLB_LOG_ERR("PFPMD failed to parse args ret=%d, errno=%d\n",
					    ret, rte_errno);
				goto dlb_probe_failed;
			}
		}

		ret = dlb_primary_eventdev_probe(eventdev,
						 EVDEV_DLB_NAME_PMD_STR,
						 &dlb_args);
	} else {
		ret = dlb_secondary_eventdev_probe(eventdev,
						   EVDEV_DLB_NAME_PMD_STR);
	}
	if (ret)
		goto dlb_probe_failed;

	DLB_LOG_INFO("DLB PF Probe success\n");

	return 0;

dlb_probe_failed:

	DLB_LOG_INFO("DLB PF Probe failed, ret=%d\n", ret);

	return ret;
}

#define EVENTDEV_INTEL_VENDOR_ID 0x8086

static const struct rte_pci_id pci_id_dlb_map[] = {
	{
		RTE_PCI_DEVICE(EVENTDEV_INTEL_VENDOR_ID,
			       DLB_PF_DEV_ID)
	},
	{
		.vendor_id = 0,
	},
};

static int
event_dlb_pci_probe(struct rte_pci_driver *pci_drv,
		    struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe_named(pci_drv, pci_dev,
		sizeof(struct dlb_eventdev), dlb_eventdev_pci_init,
		EVDEV_DLB_NAME_PMD_STR);
}

static int
event_dlb_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_remove(pci_dev, NULL);
}

static struct rte_pci_driver pci_eventdev_dlb_pmd = {
	.id_table = pci_id_dlb_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = event_dlb_pci_probe,
	.remove = event_dlb_pci_remove,
};

RTE_PMD_REGISTER_PCI(event_dlb_pf, pci_eventdev_dlb_pmd);
RTE_PMD_REGISTER_PCI_TABLE(event_dlb_pf, pci_id_dlb_map);
