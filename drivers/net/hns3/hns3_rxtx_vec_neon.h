/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Hisilicon Limited.
 */

#ifndef _HNS3_RXTX_VEC_NEON_H_
#define _HNS3_RXTX_VEC_NEON_H_

#include <arm_neon.h>

#pragma GCC diagnostic ignored "-Wcast-qual"

static inline void
hns3_vec_tx(volatile struct hns3_desc *desc, struct rte_mbuf *pkt)
{
	uint64x2_t val1 = {
		pkt->buf_iova + pkt->data_off,
		((uint64_t)pkt->data_len) << HNS3_TXD_SEND_SIZE_SHIFT
	};
	uint64x2_t val2 = {
		0,
		((uint64_t)HNS3_TXD_DEFAULT_VLD_FE_BDTYPE) << HNS3_UINT32_BIT
	};
	vst1q_u64((uint64_t *)&desc->addr, val1);
	vst1q_u64((uint64_t *)&desc->tx.outer_vlan_tag, val2);
}

static uint16_t
hns3_xmit_fixed_burst_vec(void *__restrict tx_queue,
			  struct rte_mbuf **__restrict tx_pkts,
			  uint16_t nb_pkts)
{
	struct hns3_tx_queue *txq = (struct hns3_tx_queue *)tx_queue;
	volatile struct hns3_desc *tx_desc;
	struct hns3_entry *tx_entry;
	uint16_t next_to_use;
	uint16_t nb_commit;
	uint16_t nb_tx;
	uint16_t n, i;

	if (txq->tx_bd_ready < txq->tx_free_thresh)
		hns3_tx_free_buffers(txq);

	nb_commit = RTE_MIN(txq->tx_bd_ready, nb_pkts);
	if (unlikely(nb_commit == 0)) {
		txq->queue_full_cnt++;
		return 0;
	}
	nb_tx = nb_commit;

	next_to_use = txq->next_to_use;
	tx_desc = &txq->tx_ring[next_to_use];
	tx_entry = &txq->sw_ring[next_to_use];

	/*
	 * We need to deal with n descriptors first for better performance,
	 * if nb_commit is greater than the difference between txq->nb_tx_desc
	 * and next_to_use in sw_ring and tx_ring.
	 */
	n = txq->nb_tx_desc - next_to_use;
	if (nb_commit >= n) {
		for (i = 0; i < n; i++, tx_pkts++, tx_desc++) {
			hns3_vec_tx(tx_desc, *tx_pkts);
			tx_entry[i].mbuf = *tx_pkts;
		}

		nb_commit -= n;
		next_to_use = 0;
		tx_desc = &txq->tx_ring[next_to_use];
		tx_entry = &txq->sw_ring[next_to_use];
	}

	for (i = 0; i < nb_commit; i++, tx_pkts++, tx_desc++) {
		hns3_vec_tx(tx_desc, *tx_pkts);
		tx_entry[i].mbuf = *tx_pkts;
	}

	next_to_use += nb_commit;
	txq->next_to_use = next_to_use;
	txq->tx_bd_ready -= nb_tx;

	hns3_write_reg_opt(txq->io_tail_reg, nb_tx);

	return nb_tx;
}
#endif /* _HNS3_RXTX_VEC_NEON_H_ */
