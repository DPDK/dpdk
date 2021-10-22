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

	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER) {
			rc = octeontx_vlan_hw_filter(nic, true);
			if (rc)
				goto done;

			nic->rx_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_FILTER;
			nic->rx_offload_flags |= OCCTX_RX_VLAN_FLTR_F;
		} else {
			rc = octeontx_vlan_hw_filter(nic, false);
			if (rc)
				goto done;

			nic->rx_offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_FILTER;
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

	rc = octeontx_dev_vlan_offload_set(dev, RTE_ETH_VLAN_FILTER_MASK);
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

int
octeontx_dev_set_link_up(struct rte_eth_dev *eth_dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(eth_dev);
	int rc, i;

	rc = octeontx_bgx_port_set_link_state(nic->port_id, true);
	if (rc)
		goto done;

	/* Start tx queues  */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		octeontx_dev_tx_queue_start(eth_dev, i);

done:
	return rc;
}

int
octeontx_dev_set_link_down(struct rte_eth_dev *eth_dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(eth_dev);
	int i;

	/* Stop tx queues  */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		octeontx_dev_tx_queue_stop(eth_dev, i);

	return octeontx_bgx_port_set_link_state(nic->port_id, false);
}

int
octeontx_dev_flow_ctrl_get(struct rte_eth_dev *dev,
			   struct rte_eth_fc_conf *fc_conf)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	octeontx_mbox_bgx_port_fc_cfg_t conf;
	int rc;

	memset(&conf, 0, sizeof(octeontx_mbox_bgx_port_fc_cfg_t));

	rc = octeontx_bgx_port_flow_ctrl_cfg(nic->port_id, &conf);
	if (rc)
		return rc;

	if (conf.rx_pause && conf.tx_pause)
		fc_conf->mode = RTE_ETH_FC_FULL;
	else if (conf.rx_pause)
		fc_conf->mode = RTE_ETH_FC_RX_PAUSE;
	else if (conf.tx_pause)
		fc_conf->mode = RTE_ETH_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_ETH_FC_NONE;

	/* low_water & high_water values are in Bytes */
	fc_conf->low_water = conf.low_water;
	fc_conf->high_water = conf.high_water;

	return rc;
}

int
octeontx_dev_flow_ctrl_set(struct rte_eth_dev *dev,
			   struct rte_eth_fc_conf *fc_conf)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct octeontx_fc_info *fc = &nic->fc;
	octeontx_mbox_bgx_port_fc_cfg_t conf;
	uint8_t tx_pause, rx_pause;
	uint16_t max_high_water;
	int rc;

	if (fc_conf->pause_time || fc_conf->mac_ctrl_frame_fwd ||
	    fc_conf->autoneg) {
		octeontx_log_err("Below flowctrl parameters are not supported "
				 "pause_time, mac_ctrl_frame_fwd and autoneg");
		return -EINVAL;
	}

	if (fc_conf->high_water == fc->high_water &&
	    fc_conf->low_water == fc->low_water &&
	    fc_conf->mode == fc->mode)
		return 0;

	max_high_water = fc->rx_fifosz - OCTEONTX_BGX_RSVD_RX_FIFOBYTES;

	if (fc_conf->high_water > max_high_water ||
	    fc_conf->high_water < fc_conf->low_water) {
		octeontx_log_err("Invalid high/low water values "
				 "High_water(in Bytes) must <= 0x%x ",
				 max_high_water);
		return -EINVAL;
	}

	if (fc_conf->high_water % BIT(4) || fc_conf->low_water % BIT(4)) {
		octeontx_log_err("High/low water value must be multiple of 16");
		return -EINVAL;
	}

	rx_pause = (fc_conf->mode == RTE_ETH_FC_FULL) ||
			(fc_conf->mode == RTE_ETH_FC_RX_PAUSE);
	tx_pause = (fc_conf->mode == RTE_ETH_FC_FULL) ||
			(fc_conf->mode == RTE_ETH_FC_TX_PAUSE);

	conf.high_water = fc_conf->high_water;
	conf.low_water = fc_conf->low_water;
	conf.fc_cfg = BGX_PORT_FC_CFG_SET;
	conf.rx_pause = rx_pause;
	conf.tx_pause = tx_pause;

	rc = octeontx_bgx_port_flow_ctrl_cfg(nic->port_id, &conf);
	if (rc)
		return rc;

	fc->high_water = fc_conf->high_water;
	fc->low_water = fc_conf->low_water;
	fc->mode = fc_conf->mode;

	return rc;
}

int
octeontx_dev_flow_ctrl_init(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct octeontx_fc_info *fc = &nic->fc;
	struct rte_eth_fc_conf fc_conf;
	int rc;

	rc = octeontx_dev_flow_ctrl_get(dev, &fc_conf);
	if (rc) {
		octeontx_log_err("Failed to get flow control info");
		return rc;
	}

	fc->def_highmark = fc_conf.high_water;
	fc->def_lowmark = fc_conf.low_water;
	fc->def_mode = fc_conf.mode;

	return rc;
}

int
octeontx_dev_flow_ctrl_fini(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct octeontx_fc_info *fc = &nic->fc;
	struct rte_eth_fc_conf fc_conf;

	memset(&fc_conf, 0, sizeof(struct rte_eth_fc_conf));

	/* Restore flow control parameters with default values */
	fc_conf.high_water = fc->def_highmark;
	fc_conf.low_water = fc->def_lowmark;
	fc_conf.mode = fc->def_mode;

	return octeontx_dev_flow_ctrl_set(dev, &fc_conf);
}
