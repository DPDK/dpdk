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
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_eventdev.h>
#include <rte_eventdev_pmd.h>
#include <rte_eventdev_pmd_pci.h>
#include <rte_memory.h>
#include <rte_string_fns.h>

#include "../dlb2_priv.h"
#include "../dlb2_iface.h"
#include "../dlb2_inline_fns.h"
#include "dlb2_main.h"
#include "base/dlb2_hw_types.h"
#include "base/dlb2_osdep.h"
#include "base/dlb2_resource.h"

static const char *event_dlb2_pf_name = RTE_STR(EVDEV_DLB2_NAME_PMD);

static void
dlb2_pf_low_level_io_init(void)
{
	int i;
	/* Addresses will be initialized at port create */
	for (i = 0; i < DLB2_MAX_NUM_PORTS; i++) {
		/* First directed ports */
		dlb2_port[i][DLB2_DIR_PORT].pp_addr = NULL;
		dlb2_port[i][DLB2_DIR_PORT].cq_base = NULL;
		dlb2_port[i][DLB2_DIR_PORT].mmaped = true;

		/* Now load balanced ports */
		dlb2_port[i][DLB2_LDB_PORT].pp_addr = NULL;
		dlb2_port[i][DLB2_LDB_PORT].cq_base = NULL;
		dlb2_port[i][DLB2_LDB_PORT].mmaped = true;
	}
}

static int
dlb2_pf_open(struct dlb2_hw_dev *handle, const char *name)
{
	RTE_SET_USED(handle);
	RTE_SET_USED(name);

	return 0;
}

static int
dlb2_pf_get_device_version(struct dlb2_hw_dev *handle,
			   uint8_t *revision)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;

	*revision = dlb2_dev->revision;

	return 0;
}

static void
dlb2_pf_hardware_init(struct dlb2_hw_dev *handle)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;

	dlb2_hw_enable_sparse_ldb_cq_mode(&dlb2_dev->hw);
	dlb2_hw_enable_sparse_dir_cq_mode(&dlb2_dev->hw);
}

static int
dlb2_pf_get_num_resources(struct dlb2_hw_dev *handle,
			  struct dlb2_get_num_resources_args *rsrcs)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;

	return dlb2_hw_get_num_resources(&dlb2_dev->hw, rsrcs, false, 0);
}

static int
dlb2_pf_get_cq_poll_mode(struct dlb2_hw_dev *handle,
			 enum dlb2_cq_poll_modes *mode)
{
	RTE_SET_USED(handle);

	*mode = DLB2_CQ_POLL_MODE_SPARSE;

	return 0;
}

static void
dlb2_pf_iface_fn_ptrs_init(void)
{
	dlb2_iface_low_level_io_init = dlb2_pf_low_level_io_init;
	dlb2_iface_open = dlb2_pf_open;
	dlb2_iface_get_device_version = dlb2_pf_get_device_version;
	dlb2_iface_hardware_init = dlb2_pf_hardware_init;
	dlb2_iface_get_num_resources = dlb2_pf_get_num_resources;
	dlb2_iface_get_cq_poll_mode = dlb2_pf_get_cq_poll_mode;
}

/* PCI DEV HOOKS */
static int
dlb2_eventdev_pci_init(struct rte_eventdev *eventdev)
{
	int ret = 0;
	struct rte_pci_device *pci_dev;
	struct dlb2_devargs dlb2_args = {
		.socket_id = rte_socket_id(),
		.max_num_events = DLB2_MAX_NUM_LDB_CREDITS,
		.num_dir_credits_override = -1,
		.qid_depth_thresholds = { {0} },
		.cos_id = DLB2_COS_DEFAULT
	};
	struct dlb2_eventdev *dlb2;

	DLB2_LOG_DBG("Enter with dev_id=%d socket_id=%d",
		     eventdev->data->dev_id, eventdev->data->socket_id);

	dlb2_pf_iface_fn_ptrs_init();

	pci_dev = RTE_DEV_TO_PCI(eventdev->dev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		dlb2 = dlb2_pmd_priv(eventdev); /* rte_zmalloc_socket mem */

		/* Probe the DLB2 PF layer */
		dlb2->qm_instance.pf_dev = dlb2_probe(pci_dev);

		if (dlb2->qm_instance.pf_dev == NULL) {
			DLB2_LOG_ERR("DLB2 PF Probe failed with error %d\n",
				     rte_errno);
			ret = -rte_errno;
			goto dlb2_probe_failed;
		}

		/* Were we invoked with runtime parameters? */
		if (pci_dev->device.devargs) {
			ret = dlb2_parse_params(pci_dev->device.devargs->args,
						pci_dev->device.devargs->name,
						&dlb2_args);
			if (ret) {
				DLB2_LOG_ERR("PFPMD failed to parse args ret=%d, errno=%d\n",
					     ret, rte_errno);
				goto dlb2_probe_failed;
			}
		}

		ret = dlb2_primary_eventdev_probe(eventdev,
						  event_dlb2_pf_name,
						  &dlb2_args);
	} else {
		ret = dlb2_secondary_eventdev_probe(eventdev,
						    event_dlb2_pf_name);
	}
	if (ret)
		goto dlb2_probe_failed;

	DLB2_LOG_INFO("DLB2 PF Probe success\n");

	return 0;

dlb2_probe_failed:

	DLB2_LOG_INFO("DLB2 PF Probe failed, ret=%d\n", ret);

	return ret;
}

#define EVENTDEV_INTEL_VENDOR_ID 0x8086

static const struct rte_pci_id pci_id_dlb2_map[] = {
	{
		RTE_PCI_DEVICE(EVENTDEV_INTEL_VENDOR_ID,
			       PCI_DEVICE_ID_INTEL_DLB2_PF)
	},
	{
		.vendor_id = 0,
	},
};

static int
event_dlb2_pci_probe(struct rte_pci_driver *pci_drv,
		     struct rte_pci_device *pci_dev)
{
	int ret;

	ret = rte_event_pmd_pci_probe_named(pci_drv, pci_dev,
					     sizeof(struct dlb2_eventdev),
					     dlb2_eventdev_pci_init,
					     event_dlb2_pf_name);
	if (ret) {
		DLB2_LOG_INFO("rte_event_pmd_pci_probe_named() failed, "
				"ret=%d\n", ret);
	}

	return ret;
}

static int
event_dlb2_pci_remove(struct rte_pci_device *pci_dev)
{
	int ret;

	ret = rte_event_pmd_pci_remove(pci_dev, NULL);

	if (ret) {
		DLB2_LOG_INFO("rte_event_pmd_pci_remove() failed, "
				"ret=%d\n", ret);
	}

	return ret;

}

static struct rte_pci_driver pci_eventdev_dlb2_pmd = {
	.id_table = pci_id_dlb2_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = event_dlb2_pci_probe,
	.remove = event_dlb2_pci_remove,
};

RTE_PMD_REGISTER_PCI(event_dlb2_pf, pci_eventdev_dlb2_pmd);
RTE_PMD_REGISTER_PCI_TABLE(event_dlb2_pf, pci_id_dlb2_map);
