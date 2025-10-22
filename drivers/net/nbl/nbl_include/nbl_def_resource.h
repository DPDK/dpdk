/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_DEF_RESOURCE_H_
#define _NBL_DEF_RESOURCE_H_

#include "nbl_include.h"
#include "rte_ethdev_core.h"

#define NBL_RES_OPS_TBL_TO_OPS(res_ops_tbl)		((res_ops_tbl)->ops)
#define NBL_RES_OPS_TBL_TO_PRIV(res_ops_tbl)		((res_ops_tbl)->priv)

struct nbl_resource_ops {
	int (*configure_msix_map)(void *priv, u16 func_id, u16 num_net_msix, u16 num_others_msix,
				  bool net_msix_mask_en);
	int (*destroy_msix_map)(void *priv, u16 func_id);
	int (*enable_mailbox_irq)(void *priv, u16 func_id, u16 vector_id, bool enable_msix);
	int (*register_net)(void *priv,
			    struct nbl_register_net_param *register_param,
			    struct nbl_register_net_result *register_result);
	int (*unregister_net)(void *priv);
	u16 (*get_vsi_id)(void *priv);
	void (*get_eth_id)(void *priv, u16 vsi_id, u8 *eth_mode, u8 *eth_id);
	u16 (*get_vsi_global_qid)(void *priv, u16 vsi_id, u16 local_qid);
	int (*setup_q2vsi)(void *priv, u16 vsi_id);
	void (*remove_q2vsi)(void *priv, u16 vsi_id);
	int (*register_vsi2q)(void *priv, u16 vsi_index, u16 vsi_id,
			      u16 queue_offset, u16 queue_num);
	int (*setup_rss)(void *priv, u16 vsi_id);
	void (*remove_rss)(void *priv, u16 vsi_id);
	int (*alloc_rings)(void *priv, u16 tx_num, u16 rx_num, u16 queue_offset);
	void (*remove_rings)(void *priv);
	int (*start_tx_ring)(void *priv, struct nbl_start_tx_ring_param *param, u64 *dma_addr);
	void (*stop_tx_ring)(void *priv, u16 queue_idx);
	void (*release_tx_ring)(void *priv, u16 queue_idx);
	int (*start_rx_ring)(void *priv, struct nbl_start_rx_ring_param *param, u64 *dma_addr);
	int (*alloc_rx_bufs)(void *priv, u16 queue_idx);
	void (*stop_rx_ring)(void *priv, u16 queue_idx);
	void (*release_rx_ring)(void *priv, u16 queue_idx);
	int (*get_stats)(void *priv, struct rte_eth_stats *rte_stats);
	int (*reset_stats)(void *priv);
	void (*update_rx_ring)(void *priv, u16 queue_idx);
	u16 (*get_tx_ehdr_len)(void *priv);
	int (*alloc_txrx_queues)(void *priv, u16 vsi_id, u16 queue_num);
	void (*free_txrx_queues)(void *priv, u16 vsi_id);
	void (*clear_queues)(void *priv, u16 vsi_id);
	int (*setup_queue)(void *priv, struct nbl_txrx_queue_param *param, bool is_tx);
	void (*remove_all_queues)(void *priv, u16 vsi_id);
	u64 (*restore_abnormal_ring)(void *priv, u16 local_queue_id, int type);
	int (*restart_abnormal_ring)(void *priv, int ring_index, int type);
	void (*cfg_txrx_vlan)(void *priv, u16 vlan_tci, u16 vlan_proto);
	int (*txrx_burst_mode_get)(void *priv, struct rte_eth_dev *dev,
				   struct rte_eth_burst_mode *mode, bool is_tx);
	int (*get_txrx_xstats_cnt)(void *priv, u16 *xstats_cnt);
	int (*get_txrx_xstats)(void *priv, struct rte_eth_xstat *xstats, u16 *xstats_cnt);
	int (*get_txrx_xstats_names)(void *priv, struct rte_eth_xstat_name *xstats_names,
				     u16 *xstats_cnt);
	int (*add_macvlan)(void *priv, u8 *mac, u16 vlan_id, u16 vsi_id);
	void (*del_macvlan)(void *priv, u8 *mac, u16 vlan_id, u16 vsi_id);
	int (*add_multi_rule)(void *priv, u16 vsi_id);
	void (*del_multi_rule)(void *priv, u16 vsi_id);
	int (*cfg_multi_mcast)(void *priv, u16 vsi_id, u16 enable);
	void (*clear_flow)(void *priv, u16 vsi_id);
	int (*cfg_dsch)(void *priv, u16 vsi_id, bool vld);
	int (*setup_cqs)(void *priv, u16 vsi_id, u16 real_qps, bool rss_indir_set);
	void (*remove_cqs)(void *priv, u16 vsi_id);
};

struct nbl_resource_ops_tbl {
	struct nbl_resource_ops *ops;
	void *priv;
};

int nbl_res_init_leonis(void *p, const struct rte_eth_dev *eth_dev);
void nbl_res_remove_leonis(void *p);

#endif
