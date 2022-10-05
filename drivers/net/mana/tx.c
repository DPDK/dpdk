/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022 Microsoft Corporation
 */

#include <ethdev_driver.h>

#include <infiniband/verbs.h>
#include <infiniband/manadv.h>

#include "mana.h"

int
mana_stop_tx_queues(struct rte_eth_dev *dev)
{
	struct mana_priv *priv = dev->data->dev_private;
	int i, ret;

	for (i = 0; i < priv->num_queues; i++) {
		struct mana_txq *txq = dev->data->tx_queues[i];

		if (txq->qp) {
			ret = ibv_destroy_qp(txq->qp);
			if (ret)
				DRV_LOG(ERR, "tx_queue destroy_qp failed %d",
					ret);
			txq->qp = NULL;
		}

		if (txq->cq) {
			ret = ibv_destroy_cq(txq->cq);
			if (ret)
				DRV_LOG(ERR, "tx_queue destroy_cp failed %d",
					ret);
			txq->cq = NULL;
		}

		/* Drain and free posted WQEs */
		while (txq->desc_ring_tail != txq->desc_ring_head) {
			struct mana_txq_desc *desc =
				&txq->desc_ring[txq->desc_ring_tail];

			rte_pktmbuf_free(desc->pkt);

			txq->desc_ring_tail =
				(txq->desc_ring_tail + 1) % txq->num_desc;
		}
		txq->desc_ring_head = 0;
		txq->desc_ring_tail = 0;

		memset(&txq->gdma_sq, 0, sizeof(txq->gdma_sq));
		memset(&txq->gdma_cq, 0, sizeof(txq->gdma_cq));
	}

	return 0;
}

int
mana_start_tx_queues(struct rte_eth_dev *dev)
{
	struct mana_priv *priv = dev->data->dev_private;
	int ret, i;

	/* start TX queues */
	for (i = 0; i < priv->num_queues; i++) {
		struct mana_txq *txq;
		struct ibv_qp_init_attr qp_attr = { 0 };
		struct manadv_obj obj = {};
		struct manadv_qp dv_qp;
		struct manadv_cq dv_cq;

		txq = dev->data->tx_queues[i];

		manadv_set_context_attr(priv->ib_ctx,
			MANADV_CTX_ATTR_BUF_ALLOCATORS,
			(void *)((uintptr_t)&(struct manadv_ctx_allocators){
				.alloc = &mana_alloc_verbs_buf,
				.free = &mana_free_verbs_buf,
				.data = (void *)(uintptr_t)txq->socket,
			}));

		txq->cq = ibv_create_cq(priv->ib_ctx, txq->num_desc,
					NULL, NULL, 0);
		if (!txq->cq) {
			DRV_LOG(ERR, "failed to create cq queue index %d", i);
			ret = -errno;
			goto fail;
		}

		qp_attr.send_cq = txq->cq;
		qp_attr.recv_cq = txq->cq;
		qp_attr.cap.max_send_wr = txq->num_desc;
		qp_attr.cap.max_send_sge = priv->max_send_sge;

		/* Skip setting qp_attr.cap.max_inline_data */

		qp_attr.qp_type = IBV_QPT_RAW_PACKET;
		qp_attr.sq_sig_all = 0;

		txq->qp = ibv_create_qp(priv->ib_parent_pd, &qp_attr);
		if (!txq->qp) {
			DRV_LOG(ERR, "Failed to create qp queue index %d", i);
			ret = -errno;
			goto fail;
		}

		/* Get the addresses of CQ, QP and DB */
		obj.qp.in = txq->qp;
		obj.qp.out = &dv_qp;
		obj.cq.in = txq->cq;
		obj.cq.out = &dv_cq;
		ret = manadv_init_obj(&obj, MANADV_OBJ_QP | MANADV_OBJ_CQ);
		if (ret) {
			DRV_LOG(ERR, "Failed to get manadv objects");
			goto fail;
		}

		txq->gdma_sq.buffer = obj.qp.out->sq_buf;
		txq->gdma_sq.count = obj.qp.out->sq_count;
		txq->gdma_sq.size = obj.qp.out->sq_size;
		txq->gdma_sq.id = obj.qp.out->sq_id;

		txq->tx_vp_offset = obj.qp.out->tx_vp_offset;
		priv->db_page = obj.qp.out->db_page;
		DRV_LOG(INFO, "txq sq id %u vp_offset %u db_page %p "
				" buf %p count %u size %u",
				txq->gdma_sq.id, txq->tx_vp_offset,
				priv->db_page,
				txq->gdma_sq.buffer, txq->gdma_sq.count,
				txq->gdma_sq.size);

		txq->gdma_cq.buffer = obj.cq.out->buf;
		txq->gdma_cq.count = obj.cq.out->count;
		txq->gdma_cq.size = txq->gdma_cq.count * COMP_ENTRY_SIZE;
		txq->gdma_cq.id = obj.cq.out->cq_id;

		/* CQ head starts with count (not 0) */
		txq->gdma_cq.head = txq->gdma_cq.count;

		DRV_LOG(INFO, "txq cq id %u buf %p count %u size %u head %u",
			txq->gdma_cq.id, txq->gdma_cq.buffer,
			txq->gdma_cq.count, txq->gdma_cq.size,
			txq->gdma_cq.head);
	}

	return 0;

fail:
	mana_stop_tx_queues(dev);
	return ret;
}

static inline uint16_t
get_vsq_frame_num(uint32_t vsq)
{
	union {
		uint32_t gdma_txq_id;
		struct {
			uint32_t reserved1	: 10;
			uint32_t vsq_frame	: 14;
			uint32_t reserved2	: 8;
		};
	} v;

	v.gdma_txq_id = vsq;
	return v.vsq_frame;
}
