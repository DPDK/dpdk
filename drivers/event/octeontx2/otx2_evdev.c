/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_eventdev_pmd_pci.h>
#include <rte_pci.h>

#include "otx2_evdev.h"

static void
otx2_sso_info_get(struct rte_eventdev *event_dev,
		  struct rte_event_dev_info *dev_info)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_OCTEONTX2_PMD);
	dev_info->min_dequeue_timeout_ns = dev->min_dequeue_timeout_ns;
	dev_info->max_dequeue_timeout_ns = dev->max_dequeue_timeout_ns;
	dev_info->max_event_queues = dev->max_event_queues;
	dev_info->max_event_queue_flows = (1ULL << 20);
	dev_info->max_event_queue_priority_levels = 8;
	dev_info->max_event_priority_levels = 1;
	dev_info->max_event_ports = dev->max_event_ports;
	dev_info->max_event_port_dequeue_depth = 1;
	dev_info->max_event_port_enqueue_depth = 1;
	dev_info->max_num_events =  dev->max_num_events;
	dev_info->event_dev_cap = RTE_EVENT_DEV_CAP_QUEUE_QOS |
					RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
					RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES |
					RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK |
					RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT |
					RTE_EVENT_DEV_CAP_NONSEQ_MODE;
}

/* Initialize and register event driver with DPDK Application */
static struct rte_eventdev_ops otx2_sso_ops = {
	.dev_infos_get    = otx2_sso_info_get,
};

static int
otx2_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(pci_drv, pci_dev,
				       sizeof(struct otx2_sso_evdev),
				       otx2_sso_init);
}

static int
otx2_sso_remove(struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_remove(pci_dev, otx2_sso_fini);
}

static const struct rte_pci_id pci_sso_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_RVU_SSO_TIM_PF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_sso = {
	.id_table = pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA,
	.probe = otx2_sso_probe,
	.remove = otx2_sso_remove,
};

int
otx2_sso_init(struct rte_eventdev *event_dev)
{
	struct free_rsrcs_rsp *rsrc_cnt;
	struct rte_pci_device *pci_dev;
	struct otx2_sso_evdev *dev;
	int rc;

	event_dev->dev_ops = &otx2_sso_ops;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dev = sso_pmd_priv(event_dev);

	pci_dev = container_of(event_dev->dev, struct rte_pci_device, device);

	/* Initialize the base otx2_dev object */
	rc = otx2_dev_init(pci_dev, dev);
	if (rc < 0) {
		otx2_err("Failed to initialize otx2_dev rc=%d", rc);
		goto error;
	}

	/* Get SSO and SSOW MSIX rsrc cnt */
	otx2_mbox_alloc_msg_free_rsrc_cnt(dev->mbox);
	rc = otx2_mbox_process_msg(dev->mbox, (void *)&rsrc_cnt);
	if (rc < 0) {
		otx2_err("Unable to get free rsrc count");
		goto otx2_dev_uninit;
	}
	otx2_sso_dbg("SSO %d SSOW %d NPA %d provisioned", rsrc_cnt->sso,
		     rsrc_cnt->ssow, rsrc_cnt->npa);

	dev->max_event_ports = RTE_MIN(rsrc_cnt->ssow, OTX2_SSO_MAX_VHWS);
	dev->max_event_queues = RTE_MIN(rsrc_cnt->sso, OTX2_SSO_MAX_VHGRP);
	/* Grab the NPA LF if required */
	rc = otx2_npa_lf_init(pci_dev, dev);
	if (rc < 0) {
		otx2_err("Unable to init NPA lf. It might not be provisioned");
		goto otx2_dev_uninit;
	}

	dev->drv_inited = true;
	dev->is_timeout_deq = 0;
	dev->min_dequeue_timeout_ns = USEC2NSEC(1);
	dev->max_dequeue_timeout_ns = USEC2NSEC(0x3FF);
	dev->max_num_events = -1;
	dev->nb_event_queues = 0;
	dev->nb_event_ports = 0;

	if (!dev->max_event_ports || !dev->max_event_queues) {
		otx2_err("Not enough eventdev resource queues=%d ports=%d",
			 dev->max_event_queues, dev->max_event_ports);
		rc = -ENODEV;
		goto otx2_npa_lf_uninit;
	}

	otx2_sso_pf_func_set(dev->pf_func);
	otx2_sso_dbg("Initializing %s max_queues=%d max_ports=%d",
		     event_dev->data->name, dev->max_event_queues,
		     dev->max_event_ports);


	return 0;

otx2_npa_lf_uninit:
	otx2_npa_lf_fini();
otx2_dev_uninit:
	otx2_dev_fini(pci_dev, dev);
error:
	return rc;
}

int
otx2_sso_fini(struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct rte_pci_device *pci_dev;

	/* For secondary processes, nothing to be done */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = container_of(event_dev->dev, struct rte_pci_device, device);

	if (!dev->drv_inited)
		goto dev_fini;

	dev->drv_inited = false;
	otx2_npa_lf_fini();

dev_fini:
	if (otx2_npa_lf_active(dev)) {
		otx2_info("Common resource in use by other devices");
		return -EAGAIN;
	}

	otx2_dev_fini(pci_dev, dev);

	return 0;
}

RTE_PMD_REGISTER_PCI(event_octeontx2, pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_octeontx2, pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_octeontx2, "vfio-pci");
