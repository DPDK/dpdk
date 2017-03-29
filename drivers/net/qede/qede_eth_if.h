/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef _QEDE_ETH_IF_H
#define _QEDE_ETH_IF_H

#include "qede_if.h"

/*forward decl */
struct eth_slow_path_rx_cqe;

#define INIT_STRUCT_FIELD(field, value) .field = value

#define QED_ETH_INTERFACE_VERSION       609

#define QEDE_MAX_MCAST_FILTERS		64

enum qed_filter_rx_mode_type {
	QED_FILTER_RX_MODE_TYPE_REGULAR,
	QED_FILTER_RX_MODE_TYPE_MULTI_PROMISC,
	QED_FILTER_RX_MODE_TYPE_PROMISC,
};

enum qed_filter_type {
	QED_FILTER_TYPE_UCAST,
	QED_FILTER_TYPE_MCAST,
	QED_FILTER_TYPE_RX_MODE,
	QED_MAX_FILTER_TYPES,
};

struct qed_dev_eth_info {
	struct qed_dev_info common;

	uint8_t num_queues;
	uint8_t num_tc;

	struct ether_addr port_mac;
	uint16_t num_vlan_filters;
	uint32_t num_mac_filters;

	/* Legacy VF - this affects the datapath */
	bool is_legacy;
};

struct qed_update_vport_params {
	uint8_t vport_id;
	uint8_t update_vport_active_flg;
	uint8_t vport_active_flg;
	uint8_t update_inner_vlan_removal_flg;
	uint8_t inner_vlan_removal_flg;
	uint8_t update_tx_switching_flg;
	uint8_t tx_switching_flg;
	uint8_t update_accept_any_vlan_flg;
	uint8_t accept_any_vlan;
	uint8_t update_rss_flg;
	uint16_t mtu;
	struct ecore_sge_tpa_params *sge_tpa_params;
};

struct qed_start_vport_params {
	bool remove_inner_vlan;
	bool handle_ptp_pkts;
	bool enable_lro;
	bool drop_ttl0;
	uint8_t vport_id;
	uint16_t mtu;
	bool clear_stats;
};

struct qed_eth_ops {
	const struct qed_common_ops *common;

	int (*fill_dev_info)(struct ecore_dev *edev,
			     struct qed_dev_eth_info *info);

	int (*vport_start)(struct ecore_dev *edev,
			   struct qed_start_vport_params *params);

	int (*vport_stop)(struct ecore_dev *edev, uint8_t vport_id);

	int (*vport_update)(struct ecore_dev *edev,
			    struct qed_update_vport_params *params);

	int (*q_rx_start)(struct ecore_dev *cdev,
			  uint8_t rss_num,
			  struct ecore_queue_start_common_params *p_params,
			  uint16_t bd_max_bytes,
			  dma_addr_t bd_chain_phys_addr,
			  dma_addr_t cqe_pbl_addr,
			  uint16_t cqe_pbl_size,
			  struct ecore_rxq_start_ret_params *ret_params);

	int (*q_rx_stop)(struct ecore_dev *edev,
			 uint8_t rss_id, void *handle);

	int (*q_tx_start)(struct ecore_dev *edev,
			  uint8_t rss_num,
			  struct ecore_queue_start_common_params *p_params,
			  dma_addr_t pbl_addr,
			  uint16_t pbl_size,
			  struct ecore_txq_start_ret_params *ret_params);

	int (*q_tx_stop)(struct ecore_dev *edev,
			 uint8_t rss_id, void *handle);

	int (*eth_cqe_completion)(struct ecore_dev *edev,
				  uint8_t rss_id,
				  struct eth_slow_path_rx_cqe *cqe);

	int (*fastpath_stop)(struct ecore_dev *edev);

	void (*fastpath_start)(struct ecore_dev *edev);

	void (*get_vport_stats)(struct ecore_dev *edev,
				struct ecore_eth_stats *stats);
};

/* externs */

extern const struct qed_common_ops qed_common_ops_pass;

const struct qed_eth_ops *qed_get_eth_ops(void);

int qed_configure_filter_rx_mode(struct rte_eth_dev *eth_dev,
				 enum qed_filter_rx_mode_type type);

bool qed_update_rss_parm_cmt(struct ecore_dev *edev, uint16_t *p_tbl);

#endif /* _QEDE_ETH_IF_H */
