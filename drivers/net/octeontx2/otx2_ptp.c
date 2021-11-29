/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <ethdev_driver.h>

#include "otx2_ethdev.h"

#define PTP_FREQ_ADJUST (1 << 9)

/* Function to enable ptp config for VFs */
void
otx2_nix_ptp_enable_vf(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	if (otx2_nix_recalc_mtu(eth_dev))
		otx2_err("Failed to set MTU size for ptp");

	dev->scalar_ena = true;
	dev->rx_offload_flags |= NIX_RX_OFFLOAD_TSTAMP_F;

	/* Setting up the function pointers as per new offload flags */
	otx2_eth_set_rx_function(eth_dev);
	otx2_eth_set_tx_function(eth_dev);
}

static uint16_t
nix_eth_ptp_vf_burst(void *queue, struct rte_mbuf **mbufs, uint16_t pkts)
{
	struct otx2_eth_rxq *rxq = queue;
	struct rte_eth_dev *eth_dev;

	RTE_SET_USED(mbufs);
	RTE_SET_USED(pkts);

	eth_dev = rxq->eth_dev;
	otx2_nix_ptp_enable_vf(eth_dev);

	return 0;
}

static int
nix_read_raw_clock(struct otx2_eth_dev *dev, uint64_t *clock, uint64_t *tsc,
		   uint8_t is_pmu)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct ptp_req *req;
	struct ptp_rsp *rsp;
	int rc;

	req = otx2_mbox_alloc_msg_ptp_op(mbox);
	req->op = PTP_OP_GET_CLOCK;
	req->is_pmu = is_pmu;
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto fail;

	if (clock)
		*clock = rsp->clk;
	if (tsc)
		*tsc = rsp->tsc;

fail:
	return rc;
}

/* This function calculates two parameters "clk_freq_mult" and
 * "clk_delta" which is useful in deriving PTP HI clock from
 * timestamp counter (tsc) value.
 */
int
otx2_nix_raw_clock_tsc_conv(struct otx2_eth_dev *dev)
{
	uint64_t ticks_base = 0, ticks = 0, tsc = 0, t_freq;
	int rc, val;

	/* Calculating the frequency at which PTP HI clock is running */
	rc = nix_read_raw_clock(dev, &ticks_base, &tsc, false);
	if (rc) {
		otx2_err("Failed to read the raw clock value: %d", rc);
		goto fail;
	}

	rte_delay_ms(100);

	rc = nix_read_raw_clock(dev, &ticks, &tsc, false);
	if (rc) {
		otx2_err("Failed to read the raw clock value: %d", rc);
		goto fail;
	}

	t_freq = (ticks - ticks_base) * 10;

	/* Calculating the freq multiplier viz the ratio between the
	 * frequency at which PTP HI clock works and tsc clock runs
	 */
	dev->clk_freq_mult =
		(double)pow(10, floor(log10(t_freq))) / rte_get_timer_hz();

	val = false;
#ifdef RTE_ARM_EAL_RDTSC_USE_PMU
	val = true;
#endif
	rc = nix_read_raw_clock(dev, &ticks, &tsc, val);
	if (rc) {
		otx2_err("Failed to read the raw clock value: %d", rc);
		goto fail;
	}

	/* Calculating delta between PTP HI clock and tsc */
	dev->clk_delta = ((uint64_t)(ticks / dev->clk_freq_mult) - tsc);

fail:
	return rc;
}

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
	uint8_t rc = -EINVAL;

	if (otx2_dev_is_vf_or_sdp(dev) || otx2_dev_is_lbk(dev))
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
	struct rte_eth_dev *eth_dev;
	int i;

	if (!dev)
		return -EINVAL;

	eth_dev = otx2_dev->eth_dev;
	if (!eth_dev)
		return -EINVAL;

	otx2_dev->ptp_en = ptp_en;
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		struct otx2_eth_rxq *rxq = eth_dev->data->rx_queues[i];
		rxq->mbuf_initializer =
			otx2_nix_rxq_mbuf_setup(otx2_dev,
						eth_dev->data->port_id);
	}
	if (otx2_dev_is_vf(otx2_dev) && !(otx2_dev_is_sdp(otx2_dev)) &&
	    !(otx2_dev_is_lbk(otx2_dev))) {
		/* In case of VF, setting of MTU cant be done directly in this
		 * function as this is running as part of MBOX request(PF->VF)
		 * and MTU setting also requires MBOX message to be
		 * sent(VF->PF)
		 */
		eth_dev->rx_pkt_burst = nix_eth_ptp_vf_burst;
		rte_mb();
	}

	return 0;
}

int
otx2_nix_timesync_enable(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int i, rc = 0;

	/* If we are VF/SDP/LBK, ptp cannot not be enabled */
	if (otx2_dev_is_vf_or_sdp(dev) || otx2_dev_is_lbk(dev)) {
		otx2_info("PTP cannot be enabled in case of VF/SDP/LBK");
		return -EINVAL;
	}

	if (otx2_ethdev_is_ptp_en(dev)) {
		otx2_info("PTP mode is already enabled");
		return -EINVAL;
	}

	if (!(dev->rx_offload_flags & NIX_RX_OFFLOAD_PTYPE_F)) {
		otx2_err("Ptype offload is disabled, it should be enabled");
		return -EINVAL;
	}

	if (dev->npc_flow.switch_header_type == OTX2_PRIV_FLAGS_HIGIG) {
		otx2_err("Both PTP and switch header enabled");
		return -EINVAL;
	}

	/* Allocating a iova address for tx tstamp */
	const struct rte_memzone *ts;
	ts = rte_eth_dma_zone_reserve(eth_dev, "otx2_ts",
				      0, OTX2_ALIGN, OTX2_ALIGN,
				      dev->node);
	if (ts == NULL) {
		otx2_err("Failed to allocate mem for tx tstamp addr");
		return -ENOMEM;
	}

	dev->tstamp.tx_tstamp_iova = ts->iova;
	dev->tstamp.tx_tstamp = ts->addr;

	rc = rte_mbuf_dyn_rx_timestamp_register(
			&dev->tstamp.tstamp_dynfield_offset,
			&dev->tstamp.rx_tstamp_dynflag);
	if (rc != 0) {
		otx2_err("Failed to register Rx timestamp field/flag");
		return -rte_errno;
	}

	/* System time should be already on by default */
	nix_start_timecounters(eth_dev);

	dev->rx_offloads |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;
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

	rc = otx2_nix_recalc_mtu(eth_dev);
	if (rc)
		otx2_err("Failed to set MTU size for ptp");

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

	if (otx2_dev_is_vf_or_sdp(dev) || otx2_dev_is_lbk(dev))
		return -EINVAL;

	dev->rx_offloads &= ~RTE_ETH_RX_OFFLOAD_TIMESTAMP;
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

	rc = otx2_nix_recalc_mtu(eth_dev);
	if (rc)
		otx2_err("Failed to set MTU size for ptp");

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

	otx2_nix_dbg("rx timestamp: %"PRIu64" sec: %"PRIu64" nsec %"PRIu64"",
		     (uint64_t)tstamp->rx_tstamp, (uint64_t)timestamp->tv_sec,
		     (uint64_t)timestamp->tv_nsec);

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

	otx2_nix_dbg("tx timestamp: %"PRIu64" sec: %"PRIu64" nsec %"PRIu64"",
		     *tstamp->tx_tstamp, (uint64_t)timestamp->tv_sec,
		     (uint64_t)timestamp->tv_nsec);

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
		/* Since the frequency of PTP comp register is tuned, delta and
		 * freq mult calculation for deriving PTP_HI from timestamp
		 * counter should be done again.
		 */
		rc = otx2_nix_raw_clock_tsc_conv(dev);
		if (rc)
			otx2_err("Failed to calculate delta and freq mult");
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

	otx2_nix_dbg("PTP time read: %"PRIu64" .%09"PRIu64"",
		     (uint64_t)ts->tv_sec, (uint64_t)ts->tv_nsec);

	return 0;
}


int
otx2_nix_read_clock(struct rte_eth_dev *eth_dev, uint64_t *clock)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	/* This API returns the raw PTP HI clock value. Since LFs doesn't
	 * have direct access to PTP registers and it requires mbox msg
	 * to AF for this value. In fastpath reading this value for every
	 * packet (which involves mbox call) becomes very expensive, hence
	 * we should be able to derive PTP HI clock value from tsc by
	 * using freq_mult and clk_delta calculated during configure stage.
	 */
	*clock = (rte_get_tsc_cycles() + dev->clk_delta) * dev->clk_freq_mult;

	return 0;
}
