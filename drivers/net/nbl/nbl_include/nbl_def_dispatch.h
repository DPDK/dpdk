/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_DEF_DISPATCH_H_
#define _NBL_DEF_DISPATCH_H_

#include "nbl_include.h"

#define NBL_DISP_OPS_TBL_TO_OPS(disp_ops_tbl)	((disp_ops_tbl)->ops)
#define NBL_DISP_OPS_TBL_TO_PRIV(disp_ops_tbl)	((disp_ops_tbl)->priv)

enum {
	NBL_DISP_CTRL_LVL_NEVER = 0,
	NBL_DISP_CTRL_LVL_MGT,
	NBL_DISP_CTRL_LVL_NET,
	NBL_DISP_CTRL_LVL_ALWAYS,
	NBL_DISP_CTRL_LVL_MAX,
};

struct nbl_dispatch_ops {
	int (*configure_msix_map)(void *priv, u16 num_net_msix, u16 num_others_msix,
				  bool net_msix_mask_en);
	int (*destroy_msix_map)(void *priv);
	int (*enable_mailbox_irq)(void *p, u16 vector_id, bool enable_msix);
	void (*get_resource_pt_ops)(void *priv, struct nbl_resource_pt_ops *pt_ops, bool offload);
	int (*register_net)(void *priv,
			    struct nbl_register_net_param *register_param,
			    struct nbl_register_net_result *register_result);
	int (*unregister_net)(void *priv);
	int (*get_mac_addr)(void *priv, u8 *mac);
	int (*add_macvlan)(void *priv, u8 *mac, u16 vlan_id, u16 vsi_id);
	void (*del_macvlan)(void *priv, u8 *mac, u16 vlan_id, u16 vsi_id);
	int (*add_multi_rule)(void *priv, u16 vsi);
	void (*del_multi_rule)(void *priv, u16 vsi);
	int (*cfg_multi_mcast)(void *priv, u16 vsi, u16 enable);
	void (*clear_flow)(void *priv, u16 vsi_id);
	void (*get_firmware_version)(void *priv, char *firmware_version, u8 max_len);
	int (*set_promisc_mode)(void *priv, u16 vsi_id, u16 mode);
	int (*alloc_txrx_queues)(void *priv, u16 vsi_id, u16 queue_num);
	void (*free_txrx_queues)(void *priv, u16 vsi_id);
	u16 (*get_vsi_id)(void *priv);
	void (*get_eth_id)(void *priv, u16 vsi_id, u8 *eth_mode, u8 *eth_id);
	int (*setup_txrx_queues)(void *priv, u16 vsi_id, u16 queue_num);
	void (*remove_txrx_queues)(void *priv, u16 vsi_id);
	int (*alloc_rings)(void *priv, u16 tx_num, u16 rx_num, u16 queue_offset);
	void (*remove_rings)(void *priv);
	int (*start_tx_ring)(void *priv, struct nbl_start_tx_ring_param *param, u64 *dma_addr);
	void (*stop_tx_ring)(void *priv, u16 queue_idx);
	void (*release_tx_ring)(void *priv, u16 queue_idx);
	int (*start_rx_ring)(void *priv, struct nbl_start_rx_ring_param *param, u64 *dma_addr);
	int (*alloc_rx_bufs)(void *priv, u16 queue_idx);
	void (*stop_rx_ring)(void *priv, u16 queue_idx);
	void (*release_rx_ring)(void *priv, u16 queue_idx);
	void (*update_rx_ring)(void *priv, u16 index);
	u16 (*get_tx_ehdr_len)(void *priv);
	void (*cfg_txrx_vlan)(void *priv, u16 vlan_tci, u16 vlan_proto);
	int (*setup_queue)(void *priv, struct nbl_txrx_queue_param *param, bool is_tx);
	void (*remove_all_queues)(void *priv, u16 vsi_id);
	int (*register_vsi2q)(void *priv, u16 vsi_index, u16 vsi_id,
			      u16 queue_offset, u16 queue_num);
	int (*setup_q2vsi)(void *priv, u16 vsi_id);
	void (*remove_q2vsi)(void *priv, u16 vsi_id);
	int (*setup_rss)(void *priv, u16 vsi_id);
	void (*remove_rss)(void *priv, u16 vsi_id);
	int (*cfg_dsch)(void *priv, u16 vsi_id, bool vld);
	int (*setup_cqs)(void *priv, u16 vsi_id, u16 real_qps, bool rss_indir_set);
	void (*remove_cqs)(void *priv, u16 vsi_id);
	int (*set_rxfh_indir)(void *priv, u16 vsi_id, u32 *indir, u32 indir_size);
	void (*clear_queues)(void *priv, u16 vsi_id);
	u16 (*xmit_pkts)(void *priv, void *tx_queue, struct rte_mbuf **tx_pkts, u16 nb_pkts);
	u16 (*recv_pkts)(void *priv, void *rx_queue, struct rte_mbuf **rx_pkts, u16 nb_pkts);
	u16 (*get_vsi_global_qid)(void *priv, u16 vsi_id, u16 local_qid);
	void (*get_board_info)(void *priv, struct nbl_board_port_info *board_info);
	void (*get_link_state)(void *priv, u8 eth_id, struct nbl_eth_link_info *eth_link_info);
	int (*get_stats)(void *priv, struct rte_eth_stats *rte_stats);

	void (*dummy_func)(void *priv);
};

struct nbl_dispatch_ops_tbl {
	struct nbl_dispatch_ops *ops;
	void *priv;
};

int nbl_disp_init(void *p);
void nbl_disp_remove(void *p);

#endif
