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
otx2_eth_dev_ptp_info_update(struct otx2_dev *dev, bool ptp_en)
{
	struct otx2_eth_dev *otx2_dev = (struct otx2_eth_dev *)dev;
	struct rte_eth_dev *eth_dev = otx2_dev->eth_dev;
	int i;

	otx2_dev->ptp_en = ptp_en;
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		struct otx2_eth_rxq *rxq = eth_dev->data->rx_queues[i];
		rxq->mbuf_initializer =
			otx2_nix_rxq_mbuf_setup(otx2_dev,
						eth_dev->data->port_id);
	}
	return 0;
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

		/* Setting up the function pointers as per new offload flags */
		otx2_eth_set_rx_function(eth_dev);
		otx2_eth_set_tx_function(eth_dev);
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

		/* Setting up the function pointers as per new offload flags */
		otx2_eth_set_rx_function(eth_dev);
		otx2_eth_set_tx_function(eth_dev);
	}
	return rc;
}

int
otx2_nix_timesync_read_rx_timestamp(struct rte_eth_dev *eth_dev,
				    struct timespec *timestamp,
				    uint32_t __rte_unused flags)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_timesync_info *tstamp = &dev->tstamp;
	uint64_t ns;

	if (!tstamp->rx_ready)
		return -EINVAL;

	ns = rte_timecounter_update(&dev->rx_tstamp_tc, tstamp->rx_tstamp);
	*timestamp = rte_ns_to_timespec(ns);
	tstamp->rx_ready = 0;

	otx2_nix_dbg("rx timestamp: %llu sec: %lu nsec %lu",
		     (unsigned long long)tstamp->rx_tstamp, timestamp->tv_sec,
		     timestamp->tv_nsec);

	return 0;
}

int
otx2_nix_timesync_read_tx_timestamp(struct rte_eth_dev *eth_dev,
				    struct timespec *timestamp)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_timesync_info *tstamp = &dev->tstamp;
	uint64_t ns;

	if (*tstamp->tx_tstamp == 0)
		return -EINVAL;

	ns = rte_timecounter_update(&dev->tx_tstamp_tc, *tstamp->tx_tstamp);
	*timestamp = rte_ns_to_timespec(ns);

	otx2_nix_dbg("tx timestamp: %llu sec: %lu nsec %lu",
		     *(unsigned long long *)tstamp->tx_tstamp,
		     timestamp->tv_sec, timestamp->tv_nsec);

	*tstamp->tx_tstamp = 0;
	rte_wmb();

	return 0;
}

int
otx2_nix_timesync_adjust_time(struct rte_eth_dev *eth_dev, int64_t delta)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct ptp_req *req;
	struct ptp_rsp *rsp;
	int rc;

	/* Adjust the frequent to make tics increments in 10^9 tics per sec */
	if (delta < PTP_FREQ_ADJUST && delta > -PTP_FREQ_ADJUST) {
		req = otx2_mbox_alloc_msg_ptp_op(mbox);
		req->op = PTP_OP_ADJFINE;
		req->scaled_ppm = delta;

		rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			return rc;
	}
	dev->systime_tc.nsec += delta;
	dev->rx_tstamp_tc.nsec += delta;
	dev->tx_tstamp_tc.nsec += delta;

	return 0;
}

int
otx2_nix_timesync_write_time(struct rte_eth_dev *eth_dev,
			     const struct timespec *ts)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint64_t ns;

	ns = rte_timespec_to_ns(ts);
	/* Set the time counters to a new value. */
	dev->systime_tc.nsec = ns;
	dev->rx_tstamp_tc.nsec = ns;
	dev->tx_tstamp_tc.nsec = ns;

	return 0;
}

int
otx2_nix_timesync_read_time(struct rte_eth_dev *eth_dev, struct timespec *ts)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct ptp_req *req;
	struct ptp_rsp *rsp;
	uint64_t ns;
	int rc;

	req = otx2_mbox_alloc_msg_ptp_op(mbox);
	req->op = PTP_OP_GET_CLOCK;
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	ns = rte_timecounter_update(&dev->systime_tc, rsp->clk);
	*ts = rte_ns_to_timespec(ns);

	otx2_nix_dbg("PTP time read: %ld.%09ld", ts->tv_sec, ts->tv_nsec);

	return 0;
}
