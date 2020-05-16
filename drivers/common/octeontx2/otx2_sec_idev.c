/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_atomic.h>
#include <rte_bus_pci.h>
#include <rte_ethdev.h>
#include <rte_spinlock.h>

#include "otx2_common.h"
#include "otx2_sec_idev.h"

static struct otx2_sec_idev_cfg sec_cfg[OTX2_MAX_INLINE_PORTS];

/**
 * @internal
 * Check if rte_eth_dev is security offload capable otx2_eth_dev
 */
uint8_t
otx2_eth_dev_is_sec_capable(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	if (pci_dev->id.device_id == PCI_DEVID_OCTEONTX2_RVU_PF ||
	    pci_dev->id.device_id == PCI_DEVID_OCTEONTX2_RVU_VF ||
	    pci_dev->id.device_id == PCI_DEVID_OCTEONTX2_RVU_AF_VF)
		return 1;

	return 0;
}

int
otx2_sec_idev_cfg_init(int port_id)
{
	struct otx2_sec_idev_cfg *cfg;
	int i;

	cfg = &sec_cfg[port_id];
	cfg->tx_cpt_idx = 0;
	rte_spinlock_init(&cfg->tx_cpt_lock);

	for (i = 0; i < OTX2_MAX_CPT_QP_PER_PORT; i++) {
		cfg->tx_cpt[i].qp = NULL;
		rte_atomic16_set(&cfg->tx_cpt[i].ref_cnt, 0);
	}

	return 0;
}

int
otx2_sec_idev_tx_cpt_qp_add(uint16_t port_id, struct otx2_cpt_qp *qp)
{
	struct otx2_sec_idev_cfg *cfg;
	int i, ret;

	if (qp == NULL || port_id >= OTX2_MAX_INLINE_PORTS)
		return -EINVAL;

	cfg = &sec_cfg[port_id];

	/* Find a free slot to save CPT LF */

	rte_spinlock_lock(&cfg->tx_cpt_lock);

	for (i = 0; i < OTX2_MAX_CPT_QP_PER_PORT; i++) {
		if (cfg->tx_cpt[i].qp == NULL) {
			cfg->tx_cpt[i].qp = qp;
			ret = 0;
			goto unlock;
		}
	}

	ret = -EINVAL;

unlock:
	rte_spinlock_unlock(&cfg->tx_cpt_lock);
	return ret;
}

int
otx2_sec_idev_tx_cpt_qp_remove(struct otx2_cpt_qp *qp)
{
	struct otx2_sec_idev_cfg *cfg;
	uint16_t port_id;
	int i, ret;

	if (qp == NULL)
		return -EINVAL;

	for (port_id = 0; port_id < OTX2_MAX_INLINE_PORTS; port_id++) {
		cfg = &sec_cfg[port_id];

		rte_spinlock_lock(&cfg->tx_cpt_lock);

		for (i = 0; i < OTX2_MAX_CPT_QP_PER_PORT; i++) {
			if (cfg->tx_cpt[i].qp != qp)
				continue;

			/* Don't free if the QP is in use by any sec session */
			if (rte_atomic16_read(&cfg->tx_cpt[i].ref_cnt)) {
				ret = -EBUSY;
			} else {
				cfg->tx_cpt[i].qp = NULL;
				ret = 0;
			}

			goto unlock;
		}

		rte_spinlock_unlock(&cfg->tx_cpt_lock);
	}

	return -ENOENT;

unlock:
	rte_spinlock_unlock(&cfg->tx_cpt_lock);
	return ret;
}

int
otx2_sec_idev_tx_cpt_qp_get(uint16_t port_id, struct otx2_cpt_qp **qp)
{
	struct otx2_sec_idev_cfg *cfg;
	uint16_t index;
	int i, ret;

	if (port_id >= OTX2_MAX_INLINE_PORTS || qp == NULL)
		return -EINVAL;

	cfg = &sec_cfg[port_id];

	rte_spinlock_lock(&cfg->tx_cpt_lock);

	index = cfg->tx_cpt_idx;

	/* Get the next index with valid data */
	for (i = 0; i < OTX2_MAX_CPT_QP_PER_PORT; i++) {
		if (cfg->tx_cpt[index].qp != NULL)
			break;
		index = (index + 1) % OTX2_MAX_CPT_QP_PER_PORT;
	}

	if (i >= OTX2_MAX_CPT_QP_PER_PORT) {
		ret = -EINVAL;
		goto unlock;
	}

	*qp = cfg->tx_cpt[index].qp;
	rte_atomic16_inc(&cfg->tx_cpt[index].ref_cnt);

	cfg->tx_cpt_idx = (index + 1) % OTX2_MAX_CPT_QP_PER_PORT;

	ret = 0;

unlock:
	rte_spinlock_unlock(&cfg->tx_cpt_lock);
	return ret;
}

int
otx2_sec_idev_tx_cpt_qp_put(struct otx2_cpt_qp *qp)
{
	struct otx2_sec_idev_cfg *cfg;
	uint16_t port_id;
	int i;

	if (qp == NULL)
		return -EINVAL;

	for (port_id = 0; port_id < OTX2_MAX_INLINE_PORTS; port_id++) {
		cfg = &sec_cfg[port_id];
		for (i = 0; i < OTX2_MAX_CPT_QP_PER_PORT; i++) {
			if (cfg->tx_cpt[i].qp == qp) {
				rte_atomic16_dec(&cfg->tx_cpt[i].ref_cnt);
				return 0;
			}
		}
	}

	return -EINVAL;
}
