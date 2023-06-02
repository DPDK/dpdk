/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2021 Hisilicon Limited.
 */

#include <ethdev_pci.h>
#include <rte_io.h>
#include <rte_time.h>

#include "hns3_ethdev.h"
#include "hns3_ptp.h"
#include "hns3_logs.h"

uint64_t hns3_timestamp_rx_dynflag;
int hns3_timestamp_dynfield_offset = -1;

int
hns3_mbuf_dyn_rx_timestamp_register(struct rte_eth_dev *dev,
				    struct rte_eth_conf *conf)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (!(conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP))
		return 0;

	ret = rte_mbuf_dyn_rx_timestamp_register
			(&hns3_timestamp_dynfield_offset,
			 &hns3_timestamp_rx_dynflag);
	if (ret) {
		hns3_err(hw,
			"failed to register Rx timestamp field/flag");
		return ret;
	}

	return 0;
}

static int
hns3_ptp_int_en(struct hns3_hw *hw, bool en)
{
	struct hns3_ptp_int_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	req = (struct hns3_ptp_int_cmd *)desc.data;
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_PTP_INT_EN, false);
	req->int_en = en ? 1 : 0;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw,
			"failed to %s ptp interrupt, ret = %d\n",
			en ? "enable" : "disable", ret);

	return ret;
}

static void
hns3_ptp_timesync_write_time(struct hns3_hw *hw, const struct timespec *ts)
{
	uint64_t sec = ts->tv_sec;
	uint64_t ns = ts->tv_nsec;

	/* Set the timecounters to a new value. */
	hns3_write_dev(hw, HNS3_CFG_TIME_SYNC_H, upper_32_bits(sec));
	hns3_write_dev(hw, HNS3_CFG_TIME_SYNC_M, lower_32_bits(sec));
	hns3_write_dev(hw, HNS3_CFG_TIME_SYNC_L, lower_32_bits(ns));
	hns3_write_dev(hw, HNS3_CFG_TIME_SYNC_RDY, 1);
}

int
hns3_ptp_init(struct hns3_hw *hw)
{
	struct timespec sys_time;
	int ret;

	if (!hns3_dev_get_support(hw, PTP))
		return 0;

	ret = hns3_ptp_int_en(hw, true);
	if (ret)
		return ret;

	/* Start PTP timer */
	hns3_write_dev(hw, HNS3_CFG_TIME_CYC_EN, 1);

	/* Initializing the RTC. */
	clock_gettime(CLOCK_REALTIME, &sys_time);
	hns3_ptp_timesync_write_time(hw, &sys_time);

	return 0;
}

static int
hns3_timesync_configure(struct hns3_adapter *hns, bool en)
{
	struct hns3_ptp_mode_cfg_cmd *req;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_cmd_desc desc;
	uint32_t val;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CFG_PTP_MODE, false);

	req = (struct hns3_ptp_mode_cfg_cmd *)desc.data;

	val = en ? 1 : 0;
	hns3_set_bit(req->enable, HNS3_PTP_ENABLE_B, val);
	hns3_set_bit(req->enable, HNS3_PTP_TX_ENABLE_B, val);
	hns3_set_bit(req->enable, HNS3_PTP_RX_ENABLE_B, val);

	if (en) {
		hns3_set_field(req->ptp_type, HNS3_PTP_TYPE_M, HNS3_PTP_TYPE_S,
			       PTP_TYPE_L2_V2_TYPE);
		hns3_set_field(req->v2_message_type_1, HNS3_PTP_MESSAGE_TYPE_M,
			       HNS3_PTP_MESSAGE_TYPE_S, ALL_PTP_V2_TYPE);
	}

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "configure PTP time failed, en = %d, ret = %d",
			 en, ret);
		return ret;
	}

	pf->ptp_enable = en;

	return 0;
}

int
hns3_timesync_enable(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_pf *pf = &hns->pf;
	int ret;

	if (!hns3_dev_get_support(hw, PTP))
		return -ENOTSUP;

	if (pf->ptp_enable)
		return 0;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_timesync_configure(hns, true);
	rte_spinlock_unlock(&hw->lock);
	return ret;
}

int
hns3_timesync_disable(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_pf *pf = &hns->pf;
	int ret;

	if (!hns3_dev_get_support(hw, PTP))
		return -ENOTSUP;

	if (!pf->ptp_enable)
		return 0;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_timesync_configure(hns, false);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

int
hns3_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp,
				uint32_t flags __rte_unused)
{
#define TIME_RX_STAMP_NS_MASK 0x3FFFFFFF
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_pf *pf = &hns->pf;
	uint64_t ns, sec;

	if (!hns3_dev_get_support(hw, PTP))
		return -ENOTSUP;

	ns = pf->rx_timestamp & TIME_RX_STAMP_NS_MASK;
	sec = upper_32_bits(pf->rx_timestamp);

	ns += sec * NSEC_PER_SEC;
	*timestamp = rte_ns_to_timespec(ns);

	return 0;
}

int
hns3_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp)
{
#define TIME_TX_STAMP_NS_MASK 0x3FFFFFFF
#define TIME_TX_STAMP_VALID   24
#define TIME_TX_STAMP_CNT_MASK 0x7
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint64_t sec;
	uint64_t tmp;
	uint64_t ns;
	int ts_cnt;

	if (!hns3_dev_get_support(hw, PTP))
		return -ENOTSUP;

	ts_cnt = hns3_read_dev(hw, HNS3_TX_1588_BACK_TSP_CNT) &
			TIME_TX_STAMP_CNT_MASK;
	if (ts_cnt == 0)
		return -EINVAL;

	ns = hns3_read_dev(hw, HNS3_TX_1588_TSP_BACK_0) & TIME_TX_STAMP_NS_MASK;
	sec = hns3_read_dev(hw, HNS3_TX_1588_TSP_BACK_1);
	tmp = hns3_read_dev(hw, HNS3_TX_1588_TSP_BACK_2) & 0xFFFF;
	sec = (tmp << 32) | sec;

	ns += sec * NSEC_PER_SEC;

	*timestamp = rte_ns_to_timespec(ns);

	/* Clear current timestamp hardware stores */
	hns3_read_dev(hw, HNS3_TX_1588_SEQID_BACK);

	return 0;
}

int
hns3_timesync_read_time(struct rte_eth_dev *dev, struct timespec *ts)
{
#define HNS3_PTP_SEC_H_OFFSET	32
#define HNS3_PTP_SEC_H_MASK	0xFFFF

	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t sec_hi, sec_lo;
	uint64_t ns, sec;

	if (!hns3_dev_get_support(hw, PTP))
		return -ENOTSUP;

	ns = hns3_read_dev(hw, HNS3_CURR_TIME_OUT_NS);
	sec_hi = hns3_read_dev(hw, HNS3_CURR_TIME_OUT_H) & HNS3_PTP_SEC_H_MASK;
	sec_lo = hns3_read_dev(hw, HNS3_CURR_TIME_OUT_L);
	sec = ((uint64_t)sec_hi << HNS3_PTP_SEC_H_OFFSET) | sec_lo;

	ns += sec * NSEC_PER_SEC;
	*ts = rte_ns_to_timespec(ns);

	return 0;
}

int
hns3_timesync_write_time(struct rte_eth_dev *dev, const struct timespec *ts)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!hns3_dev_get_support(hw, PTP))
		return -ENOTSUP;

	hns3_ptp_timesync_write_time(hw, ts);

	return 0;
}

int
hns3_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta)
{
#define TIME_SYNC_L_MASK 0x7FFFFFFF
#define SYMBOL_BIT_OFFSET 31
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct timespec cur_time;
	uint64_t ns;

	if (!hns3_dev_get_support(hw, PTP))
		return -ENOTSUP;

	(void)hns3_timesync_read_time(dev, &cur_time);
	ns = rte_timespec_to_ns((const struct timespec *)&cur_time);
	cur_time = rte_ns_to_timespec(ns + delta);
	(void)hns3_timesync_write_time(dev, (const struct timespec *)&cur_time);

	return 0;
}

int
hns3_restore_ptp(struct hns3_adapter *hns)
{
	struct hns3_pf *pf = &hns->pf;
	struct hns3_hw *hw = &hns->hw;
	bool en = pf->ptp_enable;
	int ret;

	if (!hns3_dev_get_support(hw, PTP))
		return 0;

	ret = hns3_timesync_configure(hns, en);
	if (ret)
		hns3_err(hw, "restore PTP enable state(%d) failed, ret = %d",
			 en, ret);

	return ret;
}

void
hns3_ptp_uninit(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	int ret;

	if (!hns3_dev_get_support(hw, PTP))
		return;

	ret = hns3_ptp_int_en(hw, false);
	if (ret != 0)
		hns3_err(hw, "disable PTP interrupt failed, ret = %d.", ret);

	ret = hns3_timesync_configure(hns, false);
	if (ret != 0)
		hns3_err(hw, "disable timesync failed, ret = %d.", ret);
}
