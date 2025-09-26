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
	u64 (*restore_abnormal_ring)(void *priv, u16 local_queue_id, int type);
	int (*restart_abnormal_ring)(void *priv, int ring_index, int type);
	void (*cfg_txrx_vlan)(void *priv, u16 vlan_tci, u16 vlan_proto);
	int (*txrx_burst_mode_get)(void *priv, struct rte_eth_dev *dev,
				   struct rte_eth_burst_mode *mode, bool is_tx);
	int (*get_txrx_xstats_cnt)(void *priv, u16 *xstats_cnt);
	int (*get_txrx_xstats)(void *priv, struct rte_eth_xstat *xstats, u16 *xstats_cnt);
	int (*get_txrx_xstats_names)(void *priv, struct rte_eth_xstat_name *xstats_names,
				     u16 *xstats_cnt);
};

struct nbl_resource_ops_tbl {
	const struct nbl_resource_ops *ops;
	void *priv;
};

int nbl_res_init_leonis(void *p, const struct rte_eth_dev *eth_dev);
void nbl_res_remove_leonis(void *p);

#endif
