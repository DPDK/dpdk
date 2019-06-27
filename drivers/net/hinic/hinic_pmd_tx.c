/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_udp.h>
#include <rte_ip.h>

#include "base/hinic_compat.h"
#include "base/hinic_pmd_hwdev.h"
#include "base/hinic_pmd_hwif.h"
#include "base/hinic_pmd_wq.h"
#include "base/hinic_pmd_nicio.h"
#include "hinic_pmd_ethdev.h"
#include "hinic_pmd_tx.h"


void hinic_free_all_tx_skbs(struct hinic_txq *txq)
{
	u16 ci;
	struct hinic_nic_dev *nic_dev = txq->nic_dev;
	struct hinic_tx_info *tx_info;
	int free_wqebbs = hinic_get_sq_free_wqebbs(nic_dev->hwdev,
						   txq->q_id) + 1;

	while (free_wqebbs < txq->q_depth) {
		ci = hinic_get_sq_local_ci(nic_dev->hwdev, txq->q_id);

		tx_info = &txq->tx_info[ci];

		if (unlikely(tx_info->cpy_mbuf != NULL)) {
			rte_pktmbuf_free(tx_info->cpy_mbuf);
			tx_info->cpy_mbuf = NULL;
		}

		rte_pktmbuf_free(tx_info->mbuf);
		hinic_update_sq_local_ci(nic_dev->hwdev, txq->q_id,
					 tx_info->wqebb_cnt);

		free_wqebbs += tx_info->wqebb_cnt;
		tx_info->mbuf = NULL;
	}
}

void hinic_free_all_tx_resources(struct rte_eth_dev *eth_dev)
{
	u16 q_id;
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);

	for (q_id = 0; q_id < nic_dev->num_sq; q_id++) {
		eth_dev->data->tx_queues[q_id] = NULL;

		if (nic_dev->txqs[q_id] == NULL)
			continue;

		/* stop tx queue free tx mbuf */
		hinic_free_all_tx_skbs(nic_dev->txqs[q_id]);
		hinic_free_tx_resources(nic_dev->txqs[q_id]);

		/* free txq */
		kfree(nic_dev->txqs[q_id]);
		nic_dev->txqs[q_id] = NULL;
	}
}

void hinic_free_tx_resources(struct hinic_txq *txq)
{
	if (txq->tx_info == NULL)
		return;

	kfree(txq->tx_info);
	txq->tx_info = NULL;
}

void hinic_destroy_sq(struct hinic_hwdev *hwdev, u16 q_id)
{
	struct hinic_nic_io *nic_io;
	struct hinic_qp *qp;

	nic_io = hwdev->nic_io;
	qp = &nic_io->qps[q_id];

	if (qp->sq.wq == NULL)
		return;

	hinic_free_db_addr(nic_io->hwdev, qp->sq.db_addr);
	hinic_wq_free(nic_io->hwdev, qp->sq.wq);
	qp->sq.wq = NULL;
}
