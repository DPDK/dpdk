/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <cnxk_ethdev.h>
#include <cnxk_ethdev_mcs.h>
#include <roc_mcs.h>

static int
cnxk_mcs_event_cb(void *userdata, struct roc_mcs_event_desc *desc, void *cb_arg)
{
	struct rte_eth_event_macsec_desc d = {0};

	d.metadata = (uint64_t)userdata;

	switch (desc->type) {
	case ROC_MCS_EVENT_SECTAG_VAL_ERR:
		d.type = RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR;
		switch (desc->subtype) {
		case ROC_MCS_EVENT_RX_SECTAG_V_EQ1:
			d.subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_V_EQ1;
			break;
		case ROC_MCS_EVENT_RX_SECTAG_E_EQ0_C_EQ1:
			d.subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_E_EQ0_C_EQ1;
			break;
		case ROC_MCS_EVENT_RX_SECTAG_SL_GTE48:
			d.subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SL_GTE48;
			break;
		case ROC_MCS_EVENT_RX_SECTAG_ES_EQ1_SC_EQ1:
			d.subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_ES_EQ1_SC_EQ1;
			break;
		case ROC_MCS_EVENT_RX_SECTAG_SC_EQ1_SCB_EQ1:
			d.subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SC_EQ1_SCB_EQ1;
			break;
		default:
			plt_err("Unknown MACsec sub event : %d", desc->subtype);
		}
		break;
	case ROC_MCS_EVENT_RX_SA_PN_HARD_EXP:
		d.type = RTE_ETH_EVENT_MACSEC_RX_SA_PN_HARD_EXP;
		break;
	case ROC_MCS_EVENT_RX_SA_PN_SOFT_EXP:
		d.type = RTE_ETH_EVENT_MACSEC_RX_SA_PN_SOFT_EXP;
		break;
	case ROC_MCS_EVENT_TX_SA_PN_HARD_EXP:
		d.type = RTE_ETH_EVENT_MACSEC_TX_SA_PN_HARD_EXP;
		break;
	case ROC_MCS_EVENT_TX_SA_PN_SOFT_EXP:
		d.type = RTE_ETH_EVENT_MACSEC_TX_SA_PN_SOFT_EXP;
		break;
	default:
		plt_err("Unknown MACsec event type: %d", desc->type);
	}

	rte_eth_dev_callback_process(cb_arg, RTE_ETH_EVENT_MACSEC, &d);

	return 0;
}

void
cnxk_mcs_dev_fini(struct cnxk_eth_dev *dev)
{
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	int rc;

	rc = roc_mcs_event_cb_unregister(mcs_dev->mdev, ROC_MCS_EVENT_SECTAG_VAL_ERR);
	if (rc)
		plt_err("Failed to unregister MCS event callback: rc: %d", rc);

	rc = roc_mcs_event_cb_unregister(mcs_dev->mdev, ROC_MCS_EVENT_TX_SA_PN_SOFT_EXP);
	if (rc)
		plt_err("Failed to unregister MCS event callback: rc: %d", rc);

	rc = roc_mcs_event_cb_unregister(mcs_dev->mdev, ROC_MCS_EVENT_RX_SA_PN_SOFT_EXP);
	if (rc)
		plt_err("Failed to unregister MCS event callback: rc: %d", rc);

	/* Cleanup MACsec dev */
	roc_mcs_dev_fini(mcs_dev->mdev);

	plt_free(mcs_dev);
}

int
cnxk_mcs_dev_init(struct cnxk_eth_dev *dev, uint8_t mcs_idx)
{
	struct roc_mcs_intr_cfg intr_cfg = {0};
	struct roc_mcs_hw_info hw_info = {0};
	struct cnxk_mcs_dev *mcs_dev;
	int rc;

	rc = roc_mcs_hw_info_get(&hw_info);
	if (rc) {
		plt_err("MCS HW info get failed: rc: %d ", rc);
		return rc;
	}

	mcs_dev = plt_zmalloc(sizeof(struct cnxk_mcs_dev), PLT_CACHE_LINE_SIZE);
	if (!mcs_dev)
		return -ENOMEM;

	mcs_dev->idx = mcs_idx;
	mcs_dev->mdev = roc_mcs_dev_init(mcs_dev->idx);
	if (!mcs_dev->mdev) {
		plt_free(mcs_dev);
		return rc;
	}
	mcs_dev->port_id = dev->eth_dev->data->port_id;

	intr_cfg.intr_mask =
		ROC_MCS_CPM_RX_SECTAG_V_EQ1_INT | ROC_MCS_CPM_RX_SECTAG_E_EQ0_C_EQ1_INT |
		ROC_MCS_CPM_RX_SECTAG_SL_GTE48_INT | ROC_MCS_CPM_RX_SECTAG_ES_EQ1_SC_EQ1_INT |
		ROC_MCS_CPM_RX_SECTAG_SC_EQ1_SCB_EQ1_INT | ROC_MCS_CPM_RX_PACKET_XPN_EQ0_INT |
		ROC_MCS_CPM_RX_PN_THRESH_REACHED_INT | ROC_MCS_CPM_TX_PACKET_XPN_EQ0_INT |
		ROC_MCS_CPM_TX_PN_THRESH_REACHED_INT | ROC_MCS_CPM_TX_SA_NOT_VALID_INT |
		ROC_MCS_BBE_RX_DFIFO_OVERFLOW_INT | ROC_MCS_BBE_RX_PLFIFO_OVERFLOW_INT |
		ROC_MCS_BBE_TX_DFIFO_OVERFLOW_INT | ROC_MCS_BBE_TX_PLFIFO_OVERFLOW_INT |
		ROC_MCS_PAB_RX_CHAN_OVERFLOW_INT | ROC_MCS_PAB_TX_CHAN_OVERFLOW_INT;

	rc = roc_mcs_intr_configure(mcs_dev->mdev, &intr_cfg);
	if (rc) {
		plt_err("Failed to configure MCS interrupts: rc: %d", rc);
		plt_free(mcs_dev);
		return rc;
	}

	rc = roc_mcs_event_cb_register(mcs_dev->mdev, ROC_MCS_EVENT_SECTAG_VAL_ERR,
				       cnxk_mcs_event_cb, dev->eth_dev, mcs_dev);
	if (rc) {
		plt_err("Failed to register MCS event callback: rc: %d", rc);
		plt_free(mcs_dev);
		return rc;
	}
	rc = roc_mcs_event_cb_register(mcs_dev->mdev, ROC_MCS_EVENT_TX_SA_PN_SOFT_EXP,
				       cnxk_mcs_event_cb, dev->eth_dev, mcs_dev);
	if (rc) {
		plt_err("Failed to register MCS event callback: rc: %d", rc);
		plt_free(mcs_dev);
		return rc;
	}
	rc = roc_mcs_event_cb_register(mcs_dev->mdev, ROC_MCS_EVENT_RX_SA_PN_SOFT_EXP,
				       cnxk_mcs_event_cb, dev->eth_dev, mcs_dev);
	if (rc) {
		plt_err("Failed to register MCS event callback: rc: %d", rc);
		plt_free(mcs_dev);
		return rc;
	}
	dev->mcs_dev = mcs_dev;

	return 0;
}
