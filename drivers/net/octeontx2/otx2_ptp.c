/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_ethdev_driver.h>

#include "otx2_ethdev.h"

#define PTP_FREQ_ADJUST (1 << 9)

static void
nix_start_timecounters(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	memset(&dev->systime_tc, 0, sizeof(struct rte_timecounter));
	memset(&dev->rx_tstamp_tc, 0, sizeof(struct rte_timecounter));
	memset(&dev->tx_tstamp_tc, 0, sizeof(struct rte_timecounter));

	dev->systime_tc.cc_mask = OTX2_CYCLECOUNTER_MASK;
	dev->rx_tstamp_tc.cc_mask = OTX2_CYCLECOUNTER_MASK;
	dev->tx_tstamp_tc.cc_mask = OTX2_CYCLECOUNTER_MASK;
}

static int
nix_ptp_config(struct rte_eth_dev *eth_dev, int en)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	uint8_t rc = 0;

	if (otx2_dev_is_vf(dev))
		return rc;

	if (en) {
		/* Enable time stamping of sent PTP packets. */
		otx2_mbox_alloc_msg_nix_lf_ptp_tx_enable(mbox);
		rc = otx2_mbox_process(mbox);
		if (rc) {
			otx2_err("MBOX ptp tx conf enable failed: err %d", rc);
			return rc;
		}
		/* Enable time stamping of received PTP packets. */
		otx2_mbox_alloc_msg_cgx_ptp_rx_enable(mbox);
	} else {
		/* Disable time stamping of sent PTP packets. */
		otx2_mbox_alloc_msg_nix_lf_ptp_tx_disable(mbox);
		rc = otx2_mbox_process(mbox);
		if (rc) {
			otx2_err("MBOX ptp tx conf disable failed: err %d", rc);
			return rc;
		}
		/* Disable time stamping of received PTP packets. */
		otx2_mbox_alloc_msg_cgx_ptp_rx_disable(mbox);
	}

	return otx2_mbox_process(mbox);
}

int
otx2_nix_timesync_enable(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int i, rc = 0;

	if (otx2_ethdev_is_ptp_en(dev)) {
		otx2_info("PTP mode is already enabled ");
		return -EINVAL;
	}

	/* If we are VF, no further action can be taken */
	if (otx2_dev_is_vf(dev))
		return -EINVAL;

	if (!(dev->rx_offload_flags & NIX_RX_OFFLOAD_PTYPE_F)) {
		otx2_err("Ptype offload is disabled, it should be enabled");
		return -EINVAL;
	}

	/* Allocating a iova address for tx tstamp */
	const struct rte_memzone *ts;
	ts = rte_eth_dma_zone_reserve(eth_dev, "otx2_ts",
				      0, OTX2_ALIGN, OTX2_ALIGN,
				      dev->node);
	if (ts == NULL)
		otx2_err("Failed to allocate mem for tx tstamp addr");

	dev->tstamp.tx_tstamp_iova = ts->iova;
	dev->tstamp.tx_tstamp = ts->addr;

	/* System time should be already on by default */
	nix_start_timecounters(eth_dev);

	dev->rx_offloads |= DEV_RX_OFFLOAD_TIMESTAMP;
	dev->rx_offload_flags |= NIX_RX_OFFLOAD_TSTAMP_F;
	dev->tx_offload_flags |= NIX_TX_OFFLOAD_TSTAMP_F;

	rc = nix_ptp_config(eth_dev, 1);
	if (!rc) {
		for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
			struct otx2_eth_txq *txq = eth_dev->data->tx_queues[i];
			otx2_nix_form_default_desc(txq);
		}
	}
	return rc;
}

int
otx2_nix_timesync_disable(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int i, rc = 0;

	if (!otx2_ethdev_is_ptp_en(dev)) {
		otx2_nix_dbg("PTP mode is disabled");
		return -EINVAL;
	}

	/* If we are VF, nothing else can be done */
	if (otx2_dev_is_vf(dev))
		return -EINVAL;

	dev->rx_offloads &= ~DEV_RX_OFFLOAD_TIMESTAMP;
	dev->rx_offload_flags &= ~NIX_RX_OFFLOAD_TSTAMP_F;
	dev->tx_offload_flags &= ~NIX_TX_OFFLOAD_TSTAMP_F;

	rc = nix_ptp_config(eth_dev, 0);
	if (!rc) {
		for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
			struct otx2_eth_txq *txq = eth_dev->data->tx_queues[i];
			otx2_nix_form_default_desc(txq);
		}
	}
	return rc;
}
