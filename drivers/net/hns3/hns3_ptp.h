/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 HiSilicon Limited.
 */

#ifndef HNS3_PTP_H
#define HNS3_PTP_H

/* Register bit for 1588 event */
#define HNS3_VECTOR0_1588_INT_B			0

#define HNS3_PTP_BASE_ADDRESS			0x29000

#define HNS3_TX_1588_SEQID_BACK			(HNS3_PTP_BASE_ADDRESS + 0x0)
#define HNS3_TX_1588_TSP_BACK_0			(HNS3_PTP_BASE_ADDRESS + 0x4)
#define HNS3_TX_1588_TSP_BACK_1			(HNS3_PTP_BASE_ADDRESS + 0x8)
#define HNS3_TX_1588_TSP_BACK_2			(HNS3_PTP_BASE_ADDRESS + 0xc)

#define HNS3_TX_1588_BACK_TSP_CNT		(HNS3_PTP_BASE_ADDRESS + 0x30)

#define HNS3_CFG_TIME_SYNC_H			(HNS3_PTP_BASE_ADDRESS + 0x50)
#define HNS3_CFG_TIME_SYNC_M			(HNS3_PTP_BASE_ADDRESS + 0x54)
#define HNS3_CFG_TIME_SYNC_L			(HNS3_PTP_BASE_ADDRESS + 0x58)
#define HNS3_CFG_TIME_SYNC_RDY			(HNS3_PTP_BASE_ADDRESS + 0x5c)

#define HNS3_CFG_TIME_CYC_EN			(HNS3_PTP_BASE_ADDRESS + 0x70)

#define HNS3_CURR_TIME_OUT_H			(HNS3_PTP_BASE_ADDRESS + 0x74)
#define HNS3_CURR_TIME_OUT_L			(HNS3_PTP_BASE_ADDRESS + 0x78)
#define HNS3_CURR_TIME_OUT_NS			(HNS3_PTP_BASE_ADDRESS + 0x7c)

int hns3_restore_ptp(struct hns3_adapter *hns);
int hns3_mbuf_dyn_rx_timestamp_register(struct rte_eth_dev *dev,
				    struct rte_eth_conf *conf);
int hns3_ptp_init(struct hns3_hw *hw);
void hns3_ptp_uninit(struct hns3_hw *hw);
int hns3_timesync_enable(struct rte_eth_dev *dev);
int hns3_timesync_disable(struct rte_eth_dev *dev);
int hns3_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp,
				uint32_t flags __rte_unused);
int hns3_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp);
int hns3_timesync_read_time(struct rte_eth_dev *dev, struct timespec *ts);
int hns3_timesync_write_time(struct rte_eth_dev *dev,
			const struct timespec *ts);
int hns3_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta);

#endif /* HNS3_PTP_H */
