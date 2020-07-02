/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_REPS_H_
#define _BNXT_REPS_H_

#include <rte_malloc.h>
#include <rte_ethdev.h>

#define BNXT_MAX_CFA_CODE               65536
#define BNXT_VF_IDX_INVALID             0xffff

uint16_t
bnxt_vfr_recv(uint16_t port_id, uint16_t queue_id, struct rte_mbuf *mbuf);
int bnxt_vf_representor_init(struct rte_eth_dev *eth_dev, void *params);
int bnxt_vf_representor_uninit(struct rte_eth_dev *eth_dev);
int bnxt_vf_rep_dev_info_get_op(struct rte_eth_dev *eth_dev,
				struct rte_eth_dev_info *dev_info);
int bnxt_vf_rep_dev_configure_op(struct rte_eth_dev *eth_dev);

int bnxt_vf_rep_link_update_op(struct rte_eth_dev *eth_dev, int wait_to_compl);
int bnxt_vf_rep_dev_start_op(struct rte_eth_dev *eth_dev);
int bnxt_vf_rep_rx_queue_setup_op(struct rte_eth_dev *eth_dev,
				  __rte_unused uint16_t queue_idx,
				  __rte_unused uint16_t nb_desc,
				  __rte_unused unsigned int socket_id,
				  __rte_unused const struct rte_eth_rxconf *
				  rx_conf,
				  __rte_unused struct rte_mempool *mp);
int bnxt_vf_rep_tx_queue_setup_op(struct rte_eth_dev *eth_dev,
				  __rte_unused uint16_t queue_idx,
				  __rte_unused uint16_t nb_desc,
				  __rte_unused unsigned int socket_id,
				  __rte_unused const struct rte_eth_txconf *
				  tx_conf);
void bnxt_vf_rep_rx_queue_release_op(void *rx_queue);
void bnxt_vf_rep_tx_queue_release_op(void *tx_queue);
void bnxt_vf_rep_dev_stop_op(struct rte_eth_dev *eth_dev);
void bnxt_vf_rep_dev_close_op(struct rte_eth_dev *eth_dev);
int bnxt_vf_rep_stats_get_op(struct rte_eth_dev *eth_dev,
			     struct rte_eth_stats *stats);
int bnxt_vf_rep_stats_reset_op(struct rte_eth_dev *eth_dev);
#endif /* _BNXT_REPS_H_ */
