/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_REPS_H_
#define _BNXT_REPS_H_

#include <rte_malloc.h>
#include <rte_ethdev.h>

#define BNXT_MAX_CFA_CODE               65536
#define BNXT_VF_IDX_INVALID             0xffff

/* Switchdev Port ID Mapping (Per switch domain id).
 * Lower 15 bits map the VFs (VF_ID). Upper bit maps the PF.
 */
#define	BNXT_SWITCH_PORT_ID_PF		0x8000
#define	BNXT_SWITCH_PORT_ID_TRUSTED_VF	0x0
#define BNXT_SWITCH_PORT_ID_VF_MASK	0x7FFF

uint16_t
bnxt_vfr_recv(uint16_t port_id, uint16_t queue_id, struct rte_mbuf *mbuf);
int bnxt_representor_init(struct rte_eth_dev *eth_dev, void *params);
int bnxt_representor_uninit(struct rte_eth_dev *eth_dev);
int bnxt_rep_dev_info_get_op(struct rte_eth_dev *eth_dev,
				struct rte_eth_dev_info *dev_info);
int bnxt_rep_dev_configure_op(struct rte_eth_dev *eth_dev);

int bnxt_rep_link_update_op(struct rte_eth_dev *eth_dev, int wait_to_compl);
int bnxt_rep_dev_start_op(struct rte_eth_dev *eth_dev);
int bnxt_rep_rx_queue_setup_op(struct rte_eth_dev *eth_dev,
				  __rte_unused uint16_t queue_idx,
				  __rte_unused uint16_t nb_desc,
				  __rte_unused unsigned int socket_id,
				  __rte_unused const struct rte_eth_rxconf *
				  rx_conf,
				  __rte_unused struct rte_mempool *mp);
int bnxt_rep_tx_queue_setup_op(struct rte_eth_dev *eth_dev,
				  __rte_unused uint16_t queue_idx,
				  __rte_unused uint16_t nb_desc,
				  __rte_unused unsigned int socket_id,
				  __rte_unused const struct rte_eth_txconf *
				  tx_conf);
void bnxt_rep_rx_queue_release_op(struct rte_eth_dev *dev, uint16_t queue_idx);
void bnxt_rep_tx_queue_release_op(struct rte_eth_dev *dev, uint16_t queue_idx);
int  bnxt_rep_dev_stop_op(struct rte_eth_dev *eth_dev);
int bnxt_rep_dev_close_op(struct rte_eth_dev *eth_dev);
int bnxt_rep_stats_get_op(struct rte_eth_dev *eth_dev,
			     struct rte_eth_stats *stats);
int bnxt_rep_stats_reset_op(struct rte_eth_dev *eth_dev);
int bnxt_rep_stop_all(struct bnxt *bp);
#endif /* _BNXT_REPS_H_ */
