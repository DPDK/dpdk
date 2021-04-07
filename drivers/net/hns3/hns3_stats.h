/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef _HNS3_STATS_H_
#define _HNS3_STATS_H_

/* stats macro */
#define HNS3_MAC_CMD_NUM		21
#define HNS3_RD_FIRST_STATS_NUM		2
#define HNS3_RD_OTHER_STATS_NUM		4

/* TQP stats */
struct hns3_tqp_stats {
	uint64_t rcb_tx_ring_pktnum_rcd; /* Total num of transmitted packets */
	uint64_t rcb_rx_ring_pktnum_rcd; /* Total num of received packets */
	uint64_t *rcb_rx_ring_pktnum;
	uint64_t *rcb_tx_ring_pktnum;
};

/* mac stats, Statistics counters collected by the MAC, opcode id: 0x0032 */
struct hns3_mac_stats {
	uint64_t mac_tx_mac_pause_num;
	uint64_t mac_rx_mac_pause_num;
	uint64_t mac_tx_pfc_pri0_pkt_num;
	uint64_t mac_tx_pfc_pri1_pkt_num;
	uint64_t mac_tx_pfc_pri2_pkt_num;
	uint64_t mac_tx_pfc_pri3_pkt_num;
	uint64_t mac_tx_pfc_pri4_pkt_num;
	uint64_t mac_tx_pfc_pri5_pkt_num;
	uint64_t mac_tx_pfc_pri6_pkt_num;
	uint64_t mac_tx_pfc_pri7_pkt_num;
	uint64_t mac_rx_pfc_pri0_pkt_num;
	uint64_t mac_rx_pfc_pri1_pkt_num;
	uint64_t mac_rx_pfc_pri2_pkt_num;
	uint64_t mac_rx_pfc_pri3_pkt_num;
	uint64_t mac_rx_pfc_pri4_pkt_num;
	uint64_t mac_rx_pfc_pri5_pkt_num;
	uint64_t mac_rx_pfc_pri6_pkt_num;
	uint64_t mac_rx_pfc_pri7_pkt_num;
	uint64_t mac_tx_total_pkt_num;
	uint64_t mac_tx_total_oct_num;
	uint64_t mac_tx_good_pkt_num;
	uint64_t mac_tx_bad_pkt_num;
	uint64_t mac_tx_good_oct_num;
	uint64_t mac_tx_bad_oct_num;
	uint64_t mac_tx_uni_pkt_num;
	uint64_t mac_tx_multi_pkt_num;
	uint64_t mac_tx_broad_pkt_num;
	uint64_t mac_tx_undersize_pkt_num;
	uint64_t mac_tx_oversize_pkt_num;
	uint64_t mac_tx_64_oct_pkt_num;
	uint64_t mac_tx_65_127_oct_pkt_num;
	uint64_t mac_tx_128_255_oct_pkt_num;
	uint64_t mac_tx_256_511_oct_pkt_num;
	uint64_t mac_tx_512_1023_oct_pkt_num;
	uint64_t mac_tx_1024_1518_oct_pkt_num;
	uint64_t mac_tx_1519_2047_oct_pkt_num;
	uint64_t mac_tx_2048_4095_oct_pkt_num;
	uint64_t mac_tx_4096_8191_oct_pkt_num;
	uint64_t rsv0;
	uint64_t mac_tx_8192_9216_oct_pkt_num;
	uint64_t mac_tx_9217_12287_oct_pkt_num;
	uint64_t mac_tx_12288_16383_oct_pkt_num;
	uint64_t mac_tx_1519_max_good_oct_pkt_num;
	uint64_t mac_tx_1519_max_bad_oct_pkt_num;

	uint64_t mac_rx_total_pkt_num;
	uint64_t mac_rx_total_oct_num;
	uint64_t mac_rx_good_pkt_num;
	uint64_t mac_rx_bad_pkt_num;
	uint64_t mac_rx_good_oct_num;
	uint64_t mac_rx_bad_oct_num;
	uint64_t mac_rx_uni_pkt_num;
	uint64_t mac_rx_multi_pkt_num;
	uint64_t mac_rx_broad_pkt_num;
	uint64_t mac_rx_undersize_pkt_num;
	uint64_t mac_rx_oversize_pkt_num;
	uint64_t mac_rx_64_oct_pkt_num;
	uint64_t mac_rx_65_127_oct_pkt_num;
	uint64_t mac_rx_128_255_oct_pkt_num;
	uint64_t mac_rx_256_511_oct_pkt_num;
	uint64_t mac_rx_512_1023_oct_pkt_num;
	uint64_t mac_rx_1024_1518_oct_pkt_num;
	uint64_t mac_rx_1519_2047_oct_pkt_num;
	uint64_t mac_rx_2048_4095_oct_pkt_num;
	uint64_t mac_rx_4096_8191_oct_pkt_num;
	uint64_t rsv1;
	uint64_t mac_rx_8192_9216_oct_pkt_num;
	uint64_t mac_rx_9217_12287_oct_pkt_num;
	uint64_t mac_rx_12288_16383_oct_pkt_num;
	uint64_t mac_rx_1519_max_good_oct_pkt_num;
	uint64_t mac_rx_1519_max_bad_oct_pkt_num;

	uint64_t mac_tx_fragment_pkt_num;
	uint64_t mac_tx_undermin_pkt_num;
	uint64_t mac_tx_jabber_pkt_num;
	uint64_t mac_tx_err_all_pkt_num;
	uint64_t mac_tx_from_app_good_pkt_num;
	uint64_t mac_tx_from_app_bad_pkt_num;
	uint64_t mac_rx_fragment_pkt_num;
	uint64_t mac_rx_undermin_pkt_num;
	uint64_t mac_rx_jabber_pkt_num;
	uint64_t mac_rx_fcs_err_pkt_num;
	uint64_t mac_rx_send_app_good_pkt_num;
	uint64_t mac_rx_send_app_bad_pkt_num;
	uint64_t mac_tx_pfc_pause_pkt_num;
	uint64_t mac_rx_pfc_pause_pkt_num;
	uint64_t mac_tx_ctrl_pkt_num;
	uint64_t mac_rx_ctrl_pkt_num;
};

/* store statistics names and its offset in stats structure */
struct hns3_xstats_name_offset {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint32_t offset;
};

#define HNS3_MAC_STATS_OFFSET(f) \
	(offsetof(struct hns3_mac_stats, f))

#define HNS3_ERR_INT_STATS_FIELD_OFFSET(f) \
	(offsetof(struct hns3_err_msix_intr_stats, f))

struct hns3_reset_stats;
#define HNS3_RESET_STATS_FIELD_OFFSET(f) \
	(offsetof(struct hns3_reset_stats, f))

#define HNS3_RX_BD_ERROR_STATS_FIELD_OFFSET(f) \
	(offsetof(struct hns3_rx_queue, f))

#define HNS3_TX_ERROR_STATS_FIELD_OFFSET(f) \
	(offsetof(struct hns3_tx_queue, f))

int hns3_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *rte_stats);
int hns3_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
			unsigned int n);
int hns3_dev_xstats_reset(struct rte_eth_dev *dev);
int hns3_dev_xstats_get_names(struct rte_eth_dev *dev,
			      struct rte_eth_xstat_name *xstats_names,
			      __rte_unused unsigned int size);
int hns3_dev_xstats_get_by_id(struct rte_eth_dev *dev,
			      const uint64_t *ids,
			      uint64_t *values,
			      uint32_t size);
int hns3_dev_xstats_get_names_by_id(struct rte_eth_dev *dev,
				    struct rte_eth_xstat_name *xstats_names,
				    const uint64_t *ids,
				    uint32_t size);
int hns3_stats_reset(struct rte_eth_dev *dev);
void hns3_error_int_stats_add(struct hns3_adapter *hns, const char *err);
int hns3_tqp_stats_init(struct hns3_hw *hw);
void hns3_tqp_stats_uninit(struct hns3_hw *hw);

#endif /* _HNS3_STATS_H_ */
