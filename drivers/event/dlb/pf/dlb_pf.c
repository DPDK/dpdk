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
dlb_pf_low_level_io_init(struct dlb_eventdev *dlb __rte_unused)
{
	int i;

	/* Addresses will be initialized at port create */
	for (i = 0; i < DLB_MAX_NUM_PORTS; i++) {
		/* First directed ports */

		/* producer port */
		dlb_port[i][DLB_DIR].pp_addr = NULL;

		/* popcount */
		dlb_port[i][DLB_DIR].ldb_popcount = NULL;
		dlb_port[i][DLB_DIR].dir_popcount = NULL;

		/* consumer queue */
		dlb_port[i][DLB_DIR].cq_base = NULL;
		dlb_port[i][DLB_DIR].mmaped = true;

		/* Now load balanced ports */

		/* producer port */
		dlb_port[i][DLB_LDB].pp_addr = NULL;

		/* popcount */
		dlb_port[i][DLB_LDB].ldb_popcount = NULL;
		dlb_port[i][DLB_LDB].dir_popcount = NULL;

		/* consumer queue */
		dlb_port[i][DLB_LDB].cq_base = NULL;
		dlb_port[i][DLB_LDB].mmaped = true;
	}
}

static int
dlb_pf_open(struct dlb_hw_dev *handle, const char *name)
{
	RTE_SET_USED(handle);
	RTE_SET_USED(name);

	return 0;
}

static void
dlb_pf_domain_close(struct dlb_eventdev *dlb)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)dlb->qm_instance.pf_dev;
	int ret;

	ret = dlb_reset_domain(&dlb_dev->hw, dlb->qm_instance.domain_id);
	if (ret)
		DLB_LOG_ERR("dlb_pf_reset_domain err %d", ret);
}

static int
dlb_pf_get_device_version(struct dlb_hw_dev *handle,
			  uint8_t *revision)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;

	*revision = dlb_dev->revision;

	return 0;
}

static int
dlb_pf_get_num_resources(struct dlb_hw_dev *handle,
			 struct dlb_get_num_resources_args *rsrcs)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;

	dlb_hw_get_num_resources(&dlb_dev->hw, rsrcs);

	return 0;
}

static int
dlb_pf_sched_domain_create(struct dlb_hw_dev *handle,
			   struct dlb_create_sched_domain_args *arg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	if (dlb_dev->domain_reset_failed) {
		response.status = DLB_ST_DOMAIN_RESET_FAILED;
		ret = -EINVAL;
		goto done;
	}

	ret = dlb_hw_create_sched_domain(&dlb_dev->hw, arg, &response);
	if (ret)
		goto done;

done:

	*(struct dlb_cmd_response *)arg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_ldb_credit_pool_create(struct dlb_hw_dev *handle,
			      struct dlb_create_ldb_pool_args *cfg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_create_ldb_pool(&dlb_dev->hw,
				     handle->domain_id,
				     cfg,
				     &response);

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_dir_credit_pool_create(struct dlb_hw_dev *handle,
			      struct dlb_create_dir_pool_args *cfg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_create_dir_pool(&dlb_dev->hw,
				     handle->domain_id,
				     cfg,
				     &response);

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_get_cq_poll_mode(struct dlb_hw_dev *handle,
			enum dlb_cq_poll_modes *mode)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;

	if (dlb_dev->revision >= DLB_REV_B0)
		*mode = DLB_CQ_POLL_MODE_SPARSE;
	else
		*mode = DLB_CQ_POLL_MODE_STD;

	return 0;
}

static void
dlb_pf_iface_fn_ptrs_init(void)
{
	dlb_iface_low_level_io_init = dlb_pf_low_level_io_init;
	dlb_iface_open = dlb_pf_open;
	dlb_iface_domain_close = dlb_pf_domain_close;
	dlb_iface_get_device_version = dlb_pf_get_device_version;
	dlb_iface_get_num_resources = dlb_pf_get_num_resources;
	dlb_iface_sched_domain_create = dlb_pf_sched_domain_create;
	dlb_iface_ldb_credit_pool_create = dlb_pf_ldb_credit_pool_create;
	dlb_iface_dir_credit_pool_create = dlb_pf_dir_credit_pool_create;
	dlb_iface_get_cq_poll_mode = dlb_pf_get_cq_poll_mode;
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
