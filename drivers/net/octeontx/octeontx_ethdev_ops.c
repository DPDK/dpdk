/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_malloc.h>

#include "octeontx_ethdev.h"
#include "octeontx_logs.h"
#include "octeontx_rxtx.h"

static int
octeontx_vlan_hw_filter(struct octeontx_nic *nic, uint8_t flag)
{
	struct octeontx_vlan_info *vlan = &nic->vlan_info;
	pki_port_vlan_filter_config_t fltr_conf;
	int rc = 0;

	if (vlan->filter_on == flag)
		return rc;

	fltr_conf.port_type = OCTTX_PORT_TYPE_NET;
	fltr_conf.fltr_conf = flag;

	rc = octeontx_pki_port_vlan_fltr_config(nic->port_id, &fltr_conf);
	if (rc != 0) {
		octeontx_log_err("Fail to configure vlan hw filter for port %d",
				 nic->port_id);
		goto done;
	}

	vlan->filter_on = flag;

done:
	return rc;
}

int
octeontx_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct rte_eth_rxmode *rxmode;
	int rc = 0;

	rxmode = &dev->data->dev_conf.rxmode;

	if (mask & ETH_VLAN_EXTEND_MASK) {
		octeontx_log_err("Extend offload not supported");
		return -ENOTSUP;
	}

	if (mask & ETH_VLAN_STRIP_MASK) {
		octeontx_log_err("VLAN strip offload not supported");
		return -ENOTSUP;
	}

	if (mask & ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_FILTER) {
			rc = octeontx_vlan_hw_filter(nic, true);
			if (rc)
				goto done;

			nic->rx_offloads |= DEV_RX_OFFLOAD_VLAN_FILTER;
			nic->rx_offload_flags |= OCCTX_RX_VLAN_FLTR_F;
		} else {
			rc = octeontx_vlan_hw_filter(nic, false);
			if (rc)
				goto done;

			nic->rx_offloads &= ~DEV_RX_OFFLOAD_VLAN_FILTER;
			nic->rx_offload_flags &= ~OCCTX_RX_VLAN_FLTR_F;
		}
	}

done:
	return rc;
}

int
octeontx_dev_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct octeontx_vlan_info *vlan = &nic->vlan_info;
	pki_port_vlan_filter_entry_config_t fltr_entry;
	struct vlan_entry *entry = NULL;
	int entry_count = 0;
	int rc = -EINVAL;

	if (on) {
		TAILQ_FOREACH(entry, &vlan->fltr_tbl, next)
			if (entry->vlan_id == vlan_id) {
				octeontx_log_dbg("Vlan Id is already set");
				return 0;
			}
	} else {
		TAILQ_FOREACH(entry, &vlan->fltr_tbl, next)
			entry_count++;

		if (!entry_count)
			return 0;
	}

	fltr_entry.port_type = OCTTX_PORT_TYPE_NET;
	fltr_entry.vlan_tpid = RTE_ETHER_TYPE_VLAN;
	fltr_entry.vlan_id = vlan_id;
	fltr_entry.entry_conf = on;

	if (on) {
		entry = rte_zmalloc("octeontx_nic_vlan_entry",
				    sizeof(struct vlan_entry), 0);
		if (!entry) {
			octeontx_log_err("Failed to allocate memory");
			return -ENOMEM;
		}
	}

	rc = octeontx_pki_port_vlan_fltr_entry_config(nic->port_id,
						      &fltr_entry);
	if (rc != 0) {
		octeontx_log_err("Fail to configure vlan filter entry "
				 "for port %d", nic->port_id);
		if (entry)
			rte_free(entry);

		goto done;
	}

	if (on) {
		entry->vlan_id  = vlan_id;
		TAILQ_INSERT_HEAD(&vlan->fltr_tbl, entry, next);
	} else {
		TAILQ_FOREACH(entry, &vlan->fltr_tbl, next) {
			if (entry->vlan_id == vlan_id) {
				TAILQ_REMOVE(&vlan->fltr_tbl, entry, next);
				rte_free(entry);
				break;
			}
		}
	}

done:
	return rc;
}

int
octeontx_dev_vlan_offload_init(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	int rc;

	TAILQ_INIT(&nic->vlan_info.fltr_tbl);

	rc = octeontx_dev_vlan_offload_set(dev, ETH_VLAN_FILTER_MASK);
	if (rc)
		octeontx_log_err("Failed to set vlan offload rc=%d", rc);

	return rc;
}

int
octeontx_dev_vlan_offload_fini(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct octeontx_vlan_info *vlan = &nic->vlan_info;
	pki_port_vlan_filter_entry_config_t fltr_entry;
	struct vlan_entry *entry;
	int rc = 0;

	TAILQ_FOREACH(entry, &vlan->fltr_tbl, next) {
		fltr_entry.port_type = OCTTX_PORT_TYPE_NET;
		fltr_entry.vlan_tpid = RTE_ETHER_TYPE_VLAN;
		fltr_entry.vlan_id = entry->vlan_id;
		fltr_entry.entry_conf = 0;

		rc = octeontx_pki_port_vlan_fltr_entry_config(nic->port_id,
							      &fltr_entry);
		if (rc != 0) {
			octeontx_log_err("Fail to configure vlan filter entry "
					 "for port %d", nic->port_id);
			break;
		}
	}

	return rc;
}
