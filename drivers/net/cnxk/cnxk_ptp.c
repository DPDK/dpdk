/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"

int
cnxk_nix_read_clock(struct rte_eth_dev *eth_dev, uint64_t *clock)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	/* This API returns the raw PTP HI clock value. Since LFs do not
	 * have direct access to PTP registers and it requires mbox msg
	 * to AF for this value. In fastpath reading this value for every
	 * packet (which involves mbox call) becomes very expensive, hence
	 * we should be able to derive PTP HI clock value from tsc by
	 * using freq_mult and clk_delta calculated during configure stage.
	 */
	*clock = (rte_get_tsc_cycles() + dev->clk_delta) * dev->clk_freq_mult;

	return 0;
}

/* This function calculates two parameters "clk_freq_mult" and
 * "clk_delta" which is useful in deriving PTP HI clock from
 * timestamp counter (tsc) value.
 */
int
cnxk_nix_tsc_convert(struct cnxk_eth_dev *dev)
{
	uint64_t ticks_base = 0, ticks = 0, tsc = 0, t_freq;
	struct roc_nix *nix = &dev->nix;
	int rc, val;

	/* Calculating the frequency at which PTP HI clock is running */
	rc = roc_nix_ptp_clock_read(nix, &ticks_base, &tsc, false);
	if (rc) {
		plt_err("Failed to read the raw clock value: %d", rc);
		goto fail;
	}

	rte_delay_ms(100);

	rc = roc_nix_ptp_clock_read(nix, &ticks, &tsc, false);
	if (rc) {
		plt_err("Failed to read the raw clock value: %d", rc);
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
	rc = roc_nix_ptp_clock_read(nix, &ticks, &tsc, val);
	if (rc) {
		plt_err("Failed to read the raw clock value: %d", rc);
		goto fail;
	}

	/* Calculating delta between PTP HI clock and tsc */
	dev->clk_delta = ((uint64_t)(ticks / dev->clk_freq_mult) - tsc);

fail:
	return rc;
}

int
cnxk_nix_timesync_read_time(struct rte_eth_dev *eth_dev, struct timespec *ts)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	uint64_t clock, ns;
	int rc;

	rc = roc_nix_ptp_clock_read(nix, &clock, NULL, false);
	if (rc)
		return rc;

	ns = rte_timecounter_update(&dev->systime_tc, clock);
	*ts = rte_ns_to_timespec(ns);
	return 0;
}

int
cnxk_nix_timesync_write_time(struct rte_eth_dev *eth_dev,
			     const struct timespec *ts)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint64_t ns;

	ns = rte_timespec_to_ns(ts);
	/* Set the time counters to a new value. */
	dev->systime_tc.nsec = ns;
	dev->rx_tstamp_tc.nsec = ns;
	dev->tx_tstamp_tc.nsec = ns;

	return 0;
}

int
cnxk_nix_timesync_adjust_time(struct rte_eth_dev *eth_dev, int64_t delta)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc;

	/* Adjust the frequent to make tics increments in 10^9 tics per sec */
	if (delta < ROC_NIX_PTP_FREQ_ADJUST &&
	    delta > -ROC_NIX_PTP_FREQ_ADJUST) {
		rc = roc_nix_ptp_sync_time_adjust(nix, delta);
		if (rc)
			return rc;

		/* Since the frequency of PTP comp register is tuned, delta and
		 * freq mult calculation for deriving PTP_HI from timestamp
		 * counter should be done again.
		 */
		rc = cnxk_nix_tsc_convert(dev);
		if (rc)
			plt_err("Failed to calculate delta and freq mult");
	}

	dev->systime_tc.nsec += delta;
	dev->rx_tstamp_tc.nsec += delta;
	dev->tx_tstamp_tc.nsec += delta;

	return 0;
}

int
cnxk_nix_timesync_read_rx_timestamp(struct rte_eth_dev *eth_dev,
				    struct timespec *timestamp, uint32_t flags)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_timesync_info *tstamp = &dev->tstamp;
	uint64_t ns;

	PLT_SET_USED(flags);

	if (!tstamp->rx_ready)
		return -EINVAL;

	ns = rte_timecounter_update(&dev->rx_tstamp_tc, tstamp->rx_tstamp);
	*timestamp = rte_ns_to_timespec(ns);
	tstamp->rx_ready = 0;
	return 0;
}

int
cnxk_nix_timesync_read_tx_timestamp(struct rte_eth_dev *eth_dev,
				    struct timespec *timestamp)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_timesync_info *tstamp = &dev->tstamp;
	uint64_t ns;

	if (*tstamp->tx_tstamp == 0)
		return -EINVAL;

	ns = rte_timecounter_update(&dev->tx_tstamp_tc, *tstamp->tx_tstamp);
	*timestamp = rte_ns_to_timespec(ns);
	*tstamp->tx_tstamp = 0;
	rte_wmb();

	return 0;
}

int
cnxk_nix_timesync_enable(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_timesync_info *tstamp = &dev->tstamp;
	struct roc_nix *nix = &dev->nix;
	const struct rte_memzone *ts;
	int rc = 0;

	/* If we are VF/SDP/LBK, ptp cannot not be enabled */
	if (roc_nix_is_vf_or_sdp(nix) || roc_nix_is_lbk(nix)) {
		plt_err("PTP cannot be enabled for VF/SDP/LBK");
		return -EINVAL;
	}

	if (dev->ptp_en)
		return rc;

	if (dev->ptype_disable) {
		plt_err("Ptype offload is disabled, it should be enabled");
		return -EINVAL;
	}

	if (dev->npc.switch_header_type == ROC_PRIV_FLAGS_HIGIG) {
		plt_err("Both PTP and switch header cannot be enabled");
		return -EINVAL;
	}

	/* Allocating a iova address for tx tstamp */
	ts = rte_eth_dma_zone_reserve(eth_dev, "cnxk_ts", 0, 128, 128, 0);
	if (ts == NULL) {
		plt_err("Failed to allocate mem for tx tstamp addr");
		return -ENOMEM;
	}

	tstamp->tx_tstamp_iova = ts->iova;
	tstamp->tx_tstamp = ts->addr;

	rc = rte_mbuf_dyn_rx_timestamp_register(&tstamp->tstamp_dynfield_offset,
						&tstamp->rx_tstamp_dynflag);
	if (rc) {
		plt_err("Failed to register Rx timestamp field/flag");
		goto error;
	}

	/* System time should be already on by default */
	memset(&dev->systime_tc, 0, sizeof(struct rte_timecounter));
	memset(&dev->rx_tstamp_tc, 0, sizeof(struct rte_timecounter));
	memset(&dev->tx_tstamp_tc, 0, sizeof(struct rte_timecounter));

	dev->systime_tc.cc_mask = CNXK_CYCLECOUNTER_MASK;
	dev->rx_tstamp_tc.cc_mask = CNXK_CYCLECOUNTER_MASK;
	dev->tx_tstamp_tc.cc_mask = CNXK_CYCLECOUNTER_MASK;

	dev->rx_offloads |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	rc = roc_nix_ptp_rx_ena_dis(nix, true);
	if (!rc) {
		rc = roc_nix_ptp_tx_ena_dis(nix, true);
		if (rc) {
			roc_nix_ptp_rx_ena_dis(nix, false);
			goto error;
		}
	}

	rc = nix_recalc_mtu(eth_dev);
	if (rc) {
		plt_err("Failed to set MTU size for ptp");
		goto error;
	}

	return rc;

error:
	rte_eth_dma_zone_free(eth_dev, "cnxk_ts", 0);
	dev->tstamp.tx_tstamp_iova = 0;
	dev->tstamp.tx_tstamp = NULL;
	return rc;
}

int
cnxk_nix_timesync_disable(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint64_t rx_offloads = RTE_ETH_RX_OFFLOAD_TIMESTAMP;
	struct roc_nix *nix = &dev->nix;
	int rc = 0;

	/* If we are VF/SDP/LBK, ptp cannot not be disabled */
	if (roc_nix_is_vf_or_sdp(nix) || roc_nix_is_lbk(nix))
		return -EINVAL;

	if (!dev->ptp_en)
		return rc;

	dev->rx_offloads &= ~rx_offloads;

	rc = roc_nix_ptp_rx_ena_dis(nix, false);
	if (!rc) {
		rc = roc_nix_ptp_tx_ena_dis(nix, false);
		if (rc) {
			roc_nix_ptp_rx_ena_dis(nix, true);
			return rc;
		}
	}

	rc = nix_recalc_mtu(eth_dev);
	if (rc)
		plt_err("Failed to set MTU size for ptp");

	return rc;
}
