/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 Advanced Micro Devices, Inc.
 */

#ifndef _IONIC_RXTX_H_
#define _IONIC_RXTX_H_

#include <stdint.h>

#include "ionic_if.h"

struct ionic_rx_qcq;
struct ionic_tx_qcq;
struct rte_eth_dev;
struct rte_eth_rxconf;
struct rte_eth_rxq_info;
struct rte_eth_txconf;
struct rte_eth_txq_info;
struct rte_mbuf;
struct rte_mempool;

struct ionic_rx_service {
	/* cb in */
	struct rte_mbuf **rx_pkts;
	/* cb out */
	uint16_t nb_rx;
};

#define IONIC_CSUM_FLAG_MASK	(IONIC_RXQ_COMP_CSUM_F_VLAN - 1)

extern const uint64_t ionic_csum_flags[IONIC_CSUM_FLAG_MASK];
extern const uint32_t ionic_ptype_table[IONIC_RXQ_COMP_PKT_TYPE_MASK];

/* ionic_rxtx.c */
int ionic_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
	uint16_t nb_desc, uint32_t socket_id,
	const struct rte_eth_rxconf *rx_conf, struct rte_mempool *mp);
void ionic_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
int ionic_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int ionic_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int ionic_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
	uint16_t nb_desc,  uint32_t socket_id,
	const struct rte_eth_txconf *tx_conf);
void ionic_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
int ionic_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int ionic_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);

/* Helpers for optimized dev_start() */
int ionic_dev_rx_queue_start_firsthalf(struct rte_eth_dev *dev,
	uint16_t rx_queue_id);
int ionic_dev_rx_queue_start_secondhalf(struct rte_eth_dev *dev,
	uint16_t rx_queue_id);
int ionic_dev_tx_queue_start_firsthalf(struct rte_eth_dev *dev,
	uint16_t tx_queue_id);
int ionic_dev_tx_queue_start_secondhalf(struct rte_eth_dev *dev,
	uint16_t tx_queue_id);

/* Helpers for optimized dev_stop() */
void ionic_dev_rx_queue_stop_firsthalf(struct rte_eth_dev *dev,
	uint16_t rx_queue_id);
void ionic_dev_rx_queue_stop_secondhalf(struct rte_eth_dev *dev,
	uint16_t rx_queue_id);
void ionic_dev_tx_queue_stop_firsthalf(struct rte_eth_dev *dev,
	uint16_t tx_queue_id);
void ionic_dev_tx_queue_stop_secondhalf(struct rte_eth_dev *dev,
	uint16_t tx_queue_id);

void ionic_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);
void ionic_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);

int ionic_dev_rx_descriptor_status(void *rx_queue, uint16_t offset);
int ionic_dev_tx_descriptor_status(void *tx_queue, uint16_t offset);

const uint32_t *ionic_dev_supported_ptypes_get(struct rte_eth_dev *dev,
					       size_t *no_of_elements);

int ionic_tx_tso(struct ionic_tx_qcq *txq, struct rte_mbuf *txm);

uint16_t ionic_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts);

/* ionic_rxtx_simple.c */
uint16_t ionic_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts);
uint16_t ionic_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts);

int ionic_rx_fill(struct ionic_rx_qcq *rxq);

/* ionic_rxtx_sg.c */
uint16_t ionic_recv_pkts_sg(void *rx_queue, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts);
uint16_t ionic_xmit_pkts_sg(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts);

int ionic_rx_fill_sg(struct ionic_rx_qcq *rxq);

static inline void
ionic_rxq_flush(struct ionic_queue *q)
{
#ifndef RTE_LIBRTE_IONIC_PMD_EMBEDDED
	struct ionic_rxq_desc *desc_base = q->base;
	struct ionic_rxq_desc *cmb_desc_base = q->cmb_base;

	if (q->cmb_base) {
		if (q->head_idx < q->cmb_head_idx) {
			/* copy [cmb_head, num_descs) */
			rte_memcpy((void *)&cmb_desc_base[q->cmb_head_idx],
				(void *)&desc_base[q->cmb_head_idx],
				(q->num_descs - q->cmb_head_idx) * sizeof(*desc_base));
			/* copy [0, head) */
			rte_memcpy((void *)&cmb_desc_base[0],
				(void *)&desc_base[0],
				q->head_idx * sizeof(*desc_base));
		} else {
			/* copy [cmb_head, head) */
			rte_memcpy((void *)&cmb_desc_base[q->cmb_head_idx],
				(void *)&desc_base[q->cmb_head_idx],
				(q->head_idx - q->cmb_head_idx) * sizeof(*desc_base));
		}
		q->cmb_head_idx = q->head_idx;
	}
#endif /* RTE_LIBRTE_IONIC_PMD_EMBEDDED */

	ionic_q_flush(q);
}

static inline void
ionic_txq_flush(struct ionic_queue *q)
{
#ifndef RTE_LIBRTE_IONIC_PMD_EMBEDDED
	struct ionic_txq_desc *desc_base = q->base;
	struct ionic_txq_desc *cmb_desc_base = q->cmb_base;

	if (q->cmb_base) {
		if (q->head_idx < q->cmb_head_idx) {
			/* copy [cmb_head, num_descs) */
			rte_memcpy((void *)&cmb_desc_base[q->cmb_head_idx],
				(void *)&desc_base[q->cmb_head_idx],
				(q->num_descs - q->cmb_head_idx) * sizeof(*desc_base));
			/* copy [0, head) */
			rte_memcpy((void *)&cmb_desc_base[0],
				(void *)&desc_base[0],
				q->head_idx * sizeof(*desc_base));
		} else {
			/* copy [cmb_head, head) */
			rte_memcpy((void *)&cmb_desc_base[q->cmb_head_idx],
				(void *)&desc_base[q->cmb_head_idx],
				(q->head_idx - q->cmb_head_idx) * sizeof(*desc_base));
		}
		q->cmb_head_idx = q->head_idx;
	}
#endif /* RTE_LIBRTE_IONIC_PMD_EMBEDDED */

	ionic_q_flush(q);
}

#endif /* _IONIC_RXTX_H_ */
