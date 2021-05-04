/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_eventdev.h"

int
cnxk_sso_init(struct rte_eventdev *event_dev)
{
	const struct rte_memzone *mz = NULL;
	struct rte_pci_device *pci_dev;
	struct cnxk_sso_evdev *dev;
	int rc;

	mz = rte_memzone_reserve(CNXK_SSO_MZ_NAME, sizeof(uint64_t),
				 SOCKET_ID_ANY, 0);
	if (mz == NULL) {
		plt_err("Failed to create eventdev memzone");
		return -ENOMEM;
	}

	dev = cnxk_sso_pmd_priv(event_dev);
	pci_dev = container_of(event_dev->dev, struct rte_pci_device, device);
	dev->sso.pci_dev = pci_dev;

	*(uint64_t *)mz->addr = (uint64_t)dev;

	/* Initialize the base cnxk_dev object */
	rc = roc_sso_dev_init(&dev->sso);
	if (rc < 0) {
		plt_err("Failed to initialize RoC SSO rc=%d", rc);
		goto error;
	}

	dev->is_timeout_deq = 0;
	dev->min_dequeue_timeout_ns = USEC2NSEC(1);
	dev->max_dequeue_timeout_ns = USEC2NSEC(0x3FF);
	dev->max_num_events = -1;
	dev->nb_event_queues = 0;
	dev->nb_event_ports = 0;

	return 0;

error:
	rte_memzone_free(mz);
	return rc;
}

int
cnxk_sso_fini(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	/* For secondary processes, nothing to be done */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	roc_sso_rsrc_fini(&dev->sso);
	roc_sso_dev_fini(&dev->sso);

	return 0;
}

int
cnxk_sso_remove(struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_remove(pci_dev, cnxk_sso_fini);
}
