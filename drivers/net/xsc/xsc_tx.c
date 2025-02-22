/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#include <rte_io.h>

#include "xsc_log.h"
#include "xsc_defs.h"
#include "xsc_dev.h"
#include "xsc_ethdev.h"
#include "xsc_cmd.h"
#include "xsc_tx.h"
#include "xsc_np.h"

void
xsc_txq_elts_alloc(struct xsc_txq_data *txq_data)
{
	const uint32_t elts_s = 1 << txq_data->elts_n;
	uint32_t i;

	for (i = 0; i < elts_s; ++i)
		txq_data->elts[i] = NULL;
	txq_data->elts_head = 0;
	txq_data->elts_tail = 0;
	txq_data->elts_comp = 0;
}

int
xsc_txq_obj_new(struct xsc_dev *xdev, struct xsc_txq_data *txq_data,
		uint64_t offloads, uint16_t idx)
{
	int ret = 0;
	struct xsc_tx_cq_params cq_params = {0};
	struct xsc_tx_cq_info cq_info = {0};
	struct xsc_tx_qp_params qp_params = {0};
	struct xsc_tx_qp_info qp_info = {0};

	cq_params.port_id = txq_data->port_id;
	cq_params.qp_id = txq_data->idx;
	cq_params.elts_n = txq_data->elts_n;
	ret = xsc_dev_tx_cq_create(xdev, &cq_params, &cq_info);
	if (ret) {
		rte_errno = errno;
		goto error;
	}

	txq_data->cq = cq_info.cq;
	txq_data->cqe_n = cq_info.cqe_n;
	txq_data->cqe_s = cq_info.cqe_s;
	txq_data->cq_db = cq_info.cq_db;
	txq_data->cqn = cq_info.cqn;
	txq_data->cqes = cq_info.cqes;
	txq_data->cqe_m = (uint16_t)(1 << cq_info.cqe_n) - 1;

	PMD_DRV_LOG(INFO, "Create tx cq, cqe_s:%d, cqe_n:%d, cq_db=%p, cqn:%u",
		    txq_data->cqe_s, txq_data->cqe_n,
		    txq_data->cq_db, txq_data->cqn);

	qp_params.cq = txq_data->cq;
	qp_params.tx_offloads = offloads;
	qp_params.port_id = txq_data->port_id;
	qp_params.qp_id = idx;
	qp_params.elts_n = txq_data->elts_n;
	ret = xsc_dev_tx_qp_create(xdev, &qp_params, &qp_info);

	if (ret != 0) {
		rte_errno = errno;
		goto error;
	}

	txq_data->qp = qp_info.qp;
	txq_data->qpn = qp_info.qpn;
	txq_data->wqes = qp_info.wqes;
	txq_data->wqe_n = qp_info.wqe_n;
	txq_data->wqe_s = 1 << txq_data->wqe_n;
	txq_data->wqe_m = txq_data->wqe_s - 1;
	txq_data->wqe_ds_n = rte_log2_u32(xdev->hwinfo.send_seg_num);
	txq_data->qp_db =  qp_info.qp_db;

	txq_data->cq_ci = 0;
	txq_data->cq_pi = 0;
	txq_data->wqe_ci = 0;
	txq_data->wqe_pi = 0;
	txq_data->wqe_comp = 0;

	PMD_DRV_LOG(INFO, "Create tx qp, wqe_s:%d, wqe_n:%d, qp_db=%p, qpn:%u",
		    txq_data->wqe_s, txq_data->wqe_n,
		    txq_data->qp_db, txq_data->qpn);
	return 0;

error:
	return -rte_errno;
}

void
xsc_txq_obj_release(struct xsc_dev *xdev, struct xsc_txq_data *txq_data)
{
	PMD_DRV_LOG(DEBUG, "Destroy tx queue %u, portid %u",
		    txq_data->idx, txq_data->port_id);
	if (txq_data->qp != NULL)
		xsc_dev_destroy_qp(xdev, txq_data->qp);
	if (txq_data->cq != NULL)
		xsc_dev_destroy_cq(xdev, txq_data->cq);
}

void
xsc_txq_elts_free(struct xsc_txq_data *txq_data)
{
	const uint16_t elts_n = 1 << txq_data->elts_n;
	const uint16_t elts_m = elts_n - 1;
	uint16_t elts_head = txq_data->elts_head;
	uint16_t elts_tail = txq_data->elts_tail;
	struct rte_mbuf *(*elts)[elts_n] = &txq_data->elts;

	txq_data->elts_head = 0;
	txq_data->elts_tail = 0;
	txq_data->elts_comp = 0;

	while (elts_tail != elts_head) {
		struct rte_mbuf *elt = (*elts)[elts_tail & elts_m];

		rte_pktmbuf_free_seg(elt);
		++elts_tail;
	}
	PMD_DRV_LOG(DEBUG, "Port %u txq %u free elts", txq_data->port_id, txq_data->idx);
}

static __rte_always_inline void
xsc_tx_elts_flush(struct xsc_txq_data *__rte_restrict txq, uint16_t tail)
{
	uint16_t elts_n = tail - txq->elts_tail;
	uint32_t free_n;

	do {
		free_n = txq->elts_s - (txq->elts_tail & txq->elts_m);
		free_n = RTE_MIN(free_n, elts_n);
		rte_pktmbuf_free_bulk(&txq->elts[txq->elts_tail & txq->elts_m], free_n);
		txq->elts_tail += free_n;
		elts_n -= free_n;
	} while (elts_n > 0);
}

static void
xsc_tx_cqes_handle(struct xsc_txq_data *__rte_restrict txq)
{
	uint32_t count = XSC_TX_COMP_CQE_HANDLE_MAX;
	volatile struct xsc_cqe *last_cqe = NULL;
	volatile struct xsc_cqe *cqe;
	bool doorbell = false;
	int ret;
	uint16_t tail;

	do {
		cqe = &txq->cqes[txq->cq_ci & txq->cqe_m];
		ret = xsc_check_cqe_own(cqe, txq->cqe_n, txq->cq_ci);
		if (unlikely(ret != XSC_CQE_OWNER_SW)) {
			if (likely(ret != XSC_CQE_OWNER_ERR))
				/* No new CQEs in completion queue. */
				break;
			doorbell = true;
			++txq->cq_ci;
			txq->cq_pi = txq->cq_ci;
			last_cqe = NULL;
			++txq->stats.tx_errors;
			continue;
		}

		doorbell = true;
		++txq->cq_ci;
		last_cqe = cqe;
	} while (--count > 0);

	if (likely(doorbell)) {
		union xsc_cq_doorbell cq_db = {
			.cq_data = 0
		};
		cq_db.next_cid = txq->cq_ci;
		cq_db.cq_num = txq->cqn;

		/* Ring doorbell */
		rte_write32(rte_cpu_to_le_32(cq_db.cq_data), txq->cq_db);

		/* Release completed elts */
		if (likely(last_cqe != NULL)) {
			txq->wqe_pi = rte_le_to_cpu_16(last_cqe->wqe_id) >> txq->wqe_ds_n;
			tail = txq->fcqs[(txq->cq_ci - 1) & txq->cqe_m];
			if (likely(tail != txq->elts_tail))
				xsc_tx_elts_flush(txq, tail);
		}
	}
}

static __rte_always_inline void
xsc_tx_wqe_ctrl_seg_init(struct xsc_txq_data *__rte_restrict txq,
			 struct rte_mbuf *__rte_restrict mbuf,
			 struct xsc_wqe *__rte_restrict wqe)
{
	struct xsc_send_wqe_ctrl_seg *cs = &wqe->cseg;
	int i = 0;
	int ds_max = (1 << txq->wqe_ds_n) - 1;

	cs->msg_opcode = XSC_OPCODE_RAW;
	cs->wqe_id = rte_cpu_to_le_16(txq->wqe_ci << txq->wqe_ds_n);
	cs->has_pph = 0;
	/* Clear dseg's seg len */
	if (cs->ds_data_num > 1 && cs->ds_data_num <= ds_max) {
		for (i = 1; i < cs->ds_data_num; i++)
			wqe->dseg[i].seg_len = 0;
	}

	cs->ds_data_num = mbuf->nb_segs;
	if (mbuf->ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
		cs->csum_en = 0x2;
	else
		cs->csum_en = 0;

	if (txq->tso_en == 1 && (mbuf->ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
		cs->so_type = 1;
		cs->so_hdr_len = mbuf->l2_len + mbuf->l3_len + mbuf->l4_len;
		cs->so_data_size = rte_cpu_to_le_16(mbuf->tso_segsz);
	}

	cs->msg_len = rte_cpu_to_le_32(rte_pktmbuf_pkt_len(mbuf));
	if (unlikely(cs->msg_len == 0))
		cs->msg_len = rte_cpu_to_le_32(rte_pktmbuf_data_len(mbuf));

	/* Do not generate cqe for every pkts */
	cs->ce = 0;
}

static __rte_always_inline void
xsc_tx_wqe_data_seg_init(struct rte_mbuf *mbuf, struct xsc_wqe *wqe)
{
	uint16_t i, nb_segs = mbuf->nb_segs;
	uint32_t data_len;
	rte_iova_t iova;
	struct xsc_wqe_data_seg *dseg;

	for (i = 0; i < nb_segs; ++i) {
		dseg = &wqe->dseg[i];
		iova = rte_pktmbuf_iova(mbuf);
		data_len = rte_pktmbuf_data_len(mbuf);

		dseg->in_line = 0;
		dseg->seg_len = rte_cpu_to_le_32(data_len);
		dseg->lkey = 0;
		dseg->va = rte_cpu_to_le_64(iova);
		mbuf = mbuf->next;
	}
}

static __rte_always_inline struct xsc_wqe *
xsc_tx_wqes_fill(struct xsc_txq_data *__rte_restrict txq,
		 struct rte_mbuf **__rte_restrict pkts,
		 uint32_t pkts_n)
{
	uint32_t i;
	struct xsc_wqe *wqe = NULL;
	struct rte_mbuf *mbuf;

	for (i = 0; i < pkts_n; i++) {
		rte_prefetch0(pkts[i]);
		mbuf = pkts[i];
		wqe = (struct xsc_wqe *)((struct xsc_send_wqe_ctrl_seg *)txq->wqes +
		      (txq->wqe_ci & txq->wqe_m) * (1 << txq->wqe_ds_n));

		/* Init wqe ctrl seg */
		xsc_tx_wqe_ctrl_seg_init(txq, mbuf, wqe);
		/* Init wqe data segs */
		xsc_tx_wqe_data_seg_init(mbuf, wqe);
		++txq->wqe_ci;
		txq->stats.tx_bytes += rte_pktmbuf_pkt_len(mbuf);
	}

	return wqe;
}

static __rte_always_inline void
xsc_tx_doorbell_ring(volatile uint32_t *db, uint32_t index,
		     uint32_t qpn, uint16_t ds_n)
{
	union xsc_send_doorbell tx_db;

	tx_db.next_pid = index << ds_n;
	tx_db.qp_num = qpn;

	rte_write32(rte_cpu_to_le_32(tx_db.send_data), db);
}

static __rte_always_inline void
xsc_tx_elts_store(struct xsc_txq_data *__rte_restrict txq,
		  struct rte_mbuf **__rte_restrict pkts,
		  uint32_t pkts_n)
{
	uint32_t part;
	struct rte_mbuf **elts = (struct rte_mbuf **)txq->elts;

	part = txq->elts_s - (txq->elts_head & txq->elts_m);
	rte_memcpy((void *)(elts + (txq->elts_head & txq->elts_m)),
		   (void *)pkts,
		   RTE_MIN(part, pkts_n) * sizeof(struct rte_mbuf *));

	if (unlikely(part < pkts_n))
		rte_memcpy((void *)elts, (void *)(pkts + part),
			   (pkts_n - part) * sizeof(struct rte_mbuf *));
}

uint16_t
xsc_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct xsc_txq_data *txq = dpdk_txq;
	uint32_t tx_n, remain_n = pkts_n;
	uint16_t idx, elts_free, wqe_free;
	uint16_t elts_head;
	struct xsc_wqe *last_wqe;

	if (unlikely(!pkts_n))
		return 0;

	do {
		xsc_tx_cqes_handle(txq);

		elts_free = txq->elts_s - (uint16_t)(txq->elts_head - txq->elts_tail);
		wqe_free = txq->wqe_s - ((uint16_t)((txq->wqe_ci << txq->wqe_ds_n) -
			   (txq->wqe_pi << txq->wqe_ds_n)) >> txq->wqe_ds_n);
		if (unlikely(elts_free == 0 || wqe_free == 0))
			break;

		/* Fill in WQEs */
		tx_n = RTE_MIN(remain_n, wqe_free);
		idx = pkts_n - remain_n;
		last_wqe = xsc_tx_wqes_fill(txq, &pkts[idx], tx_n);
		remain_n -= tx_n;
		last_wqe->cseg.ce = 1;

		/* Update free-cqs, elts_comp */
		elts_head = txq->elts_head;
		elts_head += tx_n;
		if ((uint16_t)(elts_head - txq->elts_comp) > 0) {
			txq->elts_comp = elts_head;
			txq->fcqs[txq->cq_pi++ & txq->cqe_m] = elts_head;
		}

		/* Ring tx doorbell */
		xsc_tx_doorbell_ring(txq->qp_db, txq->wqe_ci, txq->qpn, txq->wqe_ds_n);

		xsc_tx_elts_store(txq, &pkts[idx], tx_n);
		txq->elts_head += tx_n;
	} while (remain_n > 0);

	txq->stats.tx_pkts += (pkts_n - remain_n);
	return pkts_n - remain_n;
}
