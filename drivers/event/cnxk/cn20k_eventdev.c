/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include "roc_api.h"

#include "cnxk_eventdev.h"

static void
cn20k_sso_set_rsrc(void *arg)
{
	struct cnxk_sso_evdev *dev = arg;

	dev->max_event_ports = dev->sso.max_hws;
	dev->max_event_queues = dev->sso.max_hwgrp > RTE_EVENT_MAX_QUEUES_PER_DEV ?
					RTE_EVENT_MAX_QUEUES_PER_DEV :
					dev->sso.max_hwgrp;
}

static int
cn20k_sso_rsrc_init(void *arg, uint8_t hws, uint8_t hwgrp)
{
	struct cnxk_tim_evdev *tim_dev = cnxk_tim_priv_get();
	struct cnxk_sso_evdev *dev = arg;
	uint16_t nb_tim_lfs;

	nb_tim_lfs = tim_dev ? tim_dev->nb_rings : 0;
	return roc_sso_rsrc_init(&dev->sso, hws, hwgrp, nb_tim_lfs);
}

static void
cn20k_sso_info_get(struct rte_eventdev *event_dev, struct rte_event_dev_info *dev_info)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_CN20K_PMD);
	cnxk_sso_info_get(dev, dev_info);
	dev_info->max_event_port_enqueue_depth = UINT32_MAX;
}

static int
cn20k_sso_dev_configure(const struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = cnxk_sso_dev_validate(event_dev, 1, UINT32_MAX);
	if (rc < 0) {
		plt_err("Invalid event device configuration");
		return -EINVAL;
	}

	rc = cn20k_sso_rsrc_init(dev, dev->nb_event_ports, dev->nb_event_queues);
	if (rc < 0) {
		plt_err("Failed to initialize SSO resources");
		return -ENODEV;
	}

	return rc;
}

static struct eventdev_ops cn20k_sso_dev_ops = {
	.dev_infos_get = cn20k_sso_info_get,
	.dev_configure = cn20k_sso_dev_configure,

	.queue_def_conf = cnxk_sso_queue_def_conf,
	.port_def_conf = cnxk_sso_port_def_conf,
};

static int
cn20k_sso_init(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = roc_plt_init();
	if (rc < 0) {
		plt_err("Failed to initialize platform model");
		return rc;
	}

	event_dev->dev_ops = &cn20k_sso_dev_ops;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rc = cnxk_sso_init(event_dev);
	if (rc < 0)
		return rc;

	cn20k_sso_set_rsrc(cnxk_sso_pmd_priv(event_dev));
	if (!dev->max_event_ports || !dev->max_event_queues) {
		plt_err("Not enough eventdev resource queues=%d ports=%d", dev->max_event_queues,
			dev->max_event_ports);
		cnxk_sso_fini(event_dev);
		return -ENODEV;
	}

	plt_sso_dbg("Initializing %s max_queues=%d max_ports=%d", event_dev->data->name,
		    dev->max_event_queues, dev->max_event_ports);

	return 0;
}

static int
cn20k_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(pci_drv, pci_dev, sizeof(struct cnxk_sso_evdev),
				       cn20k_sso_init);
}

static const struct rte_pci_id cn20k_pci_sso_map[] = {
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN20KA, PCI_DEVID_CNXK_RVU_SSO_TIM_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN20KA, PCI_DEVID_CNXK_RVU_SSO_TIM_VF),
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn20k_pci_sso = {
	.id_table = cn20k_pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cn20k_sso_probe,
	.remove = cnxk_sso_remove,
};

RTE_PMD_REGISTER_PCI(event_cn20k, cn20k_pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_cn20k, cn20k_pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_cn20k, "vfio-pci");
