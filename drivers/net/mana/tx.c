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

	for (i = 0; i < priv->num_queues; i++)
		if (dev->data->tx_queue_state[i] == RTE_ETH_QUEUE_STATE_STOPPED)
			return -EINVAL;

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
			txq->desc_ring_len--;
		}
		txq->desc_ring_head = 0;
		txq->desc_ring_tail = 0;
		txq->desc_ring_len = 0;

		memset(&txq->gdma_sq, 0, sizeof(txq->gdma_sq));
		memset(&txq->gdma_cq, 0, sizeof(txq->gdma_cq));

		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}

	return 0;
}

int
mana_start_tx_queues(struct rte_eth_dev *dev)
{
	struct mana_priv *priv = dev->data->dev_private;
	int ret, i;

	/* start TX queues */

	for (i = 0; i < priv->num_queues; i++)
		if (dev->data->tx_queue_state[i] == RTE_ETH_QUEUE_STATE_STARTED)
			return -EINVAL;

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

		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
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

uint16_t
mana_tx_burst(void *dpdk_txq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct mana_txq *txq = dpdk_txq;
	struct mana_priv *priv = txq->priv;
	int ret;
	void *db_page;
	uint16_t pkt_sent = 0;
	uint32_t num_comp, i;
#ifdef RTE_ARCH_32
	uint32_t wqe_count = 0;
#endif

	/* Process send completions from GDMA */
	num_comp = gdma_poll_completion_queue(&txq->gdma_cq,
			txq->gdma_comp_buf, txq->num_desc);

	i = 0;
	while (i < num_comp) {
		struct mana_txq_desc *desc =
			&txq->desc_ring[txq->desc_ring_tail];
		struct mana_tx_comp_oob *oob = (struct mana_tx_comp_oob *)
			txq->gdma_comp_buf[i].cqe_data;

		if (oob->cqe_hdr.cqe_type != CQE_TX_OKAY) {
			DP_LOG(ERR,
			       "mana_tx_comp_oob cqe_type %u vendor_err %u",
			       oob->cqe_hdr.cqe_type, oob->cqe_hdr.vendor_err);
			txq->stats.errors++;
		} else {
			DP_LOG(DEBUG, "mana_tx_comp_oob CQE_TX_OKAY");
			txq->stats.packets++;
		}

		if (!desc->pkt) {
			DP_LOG(ERR, "mana_txq_desc has a NULL pkt");
		} else {
			txq->stats.bytes += desc->pkt->pkt_len;
			rte_pktmbuf_free(desc->pkt);
		}

		desc->pkt = NULL;
		txq->desc_ring_tail = (txq->desc_ring_tail + 1) % txq->num_desc;
		txq->desc_ring_len--;
		txq->gdma_sq.tail += desc->wqe_size_in_bu;

		/* If TX CQE suppression is used, don't read more CQE but move
		 * on to the next packet
		 */
		if (desc->suppress_tx_cqe)
			continue;

		i++;
	}

	/* Post send requests to GDMA */
	for (uint16_t pkt_idx = 0; pkt_idx < nb_pkts; pkt_idx++) {
		struct rte_mbuf *m_pkt = tx_pkts[pkt_idx];
		struct rte_mbuf *m_seg = m_pkt;
		struct transmit_oob_v2 tx_oob;
		struct one_sgl sgl;
		uint16_t seg_idx;

		if (txq->desc_ring_len >= txq->num_desc)
			break;

		/* Drop the packet if it exceeds max segments */
		if (m_pkt->nb_segs > priv->max_send_sge) {
			DP_LOG(ERR, "send packet segments %d exceeding max",
			       m_pkt->nb_segs);
			continue;
		}

		/* Fill in the oob */
		tx_oob.short_oob.packet_format = SHORT_PACKET_FORMAT;
		tx_oob.short_oob.tx_is_outer_ipv4 =
			m_pkt->ol_flags & RTE_MBUF_F_TX_IPV4 ? 1 : 0;
		tx_oob.short_oob.tx_is_outer_ipv6 =
			m_pkt->ol_flags & RTE_MBUF_F_TX_IPV6 ? 1 : 0;

		tx_oob.short_oob.tx_compute_IP_header_checksum =
			m_pkt->ol_flags & RTE_MBUF_F_TX_IP_CKSUM ? 1 : 0;

		if ((m_pkt->ol_flags & RTE_MBUF_F_TX_L4_MASK) ==
				RTE_MBUF_F_TX_TCP_CKSUM) {
			struct rte_tcp_hdr *tcp_hdr;

			/* HW needs partial TCP checksum */

			tcp_hdr = rte_pktmbuf_mtod_offset(m_pkt,
					  struct rte_tcp_hdr *,
					  m_pkt->l2_len + m_pkt->l3_len);

			if (m_pkt->ol_flags & RTE_MBUF_F_TX_IPV4) {
				struct rte_ipv4_hdr *ip_hdr;

				ip_hdr = rte_pktmbuf_mtod_offset(m_pkt,
						struct rte_ipv4_hdr *,
						m_pkt->l2_len);
				tcp_hdr->cksum = rte_ipv4_phdr_cksum(ip_hdr,
							m_pkt->ol_flags);

			} else if (m_pkt->ol_flags & RTE_MBUF_F_TX_IPV6) {
				struct rte_ipv6_hdr *ip_hdr;

				ip_hdr = rte_pktmbuf_mtod_offset(m_pkt,
						struct rte_ipv6_hdr *,
						m_pkt->l2_len);
				tcp_hdr->cksum = rte_ipv6_phdr_cksum(ip_hdr,
							m_pkt->ol_flags);
			} else {
				DP_LOG(ERR, "Invalid input for TCP CKSUM");
			}

			tx_oob.short_oob.tx_compute_TCP_checksum = 1;
			tx_oob.short_oob.tx_transport_header_offset =
				m_pkt->l2_len + m_pkt->l3_len;
		} else {
			tx_oob.short_oob.tx_compute_TCP_checksum = 0;
		}

		if ((m_pkt->ol_flags & RTE_MBUF_F_TX_L4_MASK) ==
				RTE_MBUF_F_TX_UDP_CKSUM) {
			struct rte_udp_hdr *udp_hdr;

			/* HW needs partial UDP checksum */
			udp_hdr = rte_pktmbuf_mtod_offset(m_pkt,
					struct rte_udp_hdr *,
					m_pkt->l2_len + m_pkt->l3_len);

			if (m_pkt->ol_flags & RTE_MBUF_F_TX_IPV4) {
				struct rte_ipv4_hdr *ip_hdr;

				ip_hdr = rte_pktmbuf_mtod_offset(m_pkt,
						struct rte_ipv4_hdr *,
						m_pkt->l2_len);

				udp_hdr->dgram_cksum =
					rte_ipv4_phdr_cksum(ip_hdr,
							    m_pkt->ol_flags);

			} else if (m_pkt->ol_flags & RTE_MBUF_F_TX_IPV6) {
				struct rte_ipv6_hdr *ip_hdr;

				ip_hdr = rte_pktmbuf_mtod_offset(m_pkt,
						struct rte_ipv6_hdr *,
						m_pkt->l2_len);

				udp_hdr->dgram_cksum =
					rte_ipv6_phdr_cksum(ip_hdr,
							    m_pkt->ol_flags);

			} else {
				DP_LOG(ERR, "Invalid input for UDP CKSUM");
			}

			tx_oob.short_oob.tx_compute_UDP_checksum = 1;
		} else {
			tx_oob.short_oob.tx_compute_UDP_checksum = 0;
		}

		tx_oob.short_oob.VCQ_number = txq->gdma_cq.id;

		tx_oob.short_oob.VSQ_frame_num =
			get_vsq_frame_num(txq->gdma_sq.id);
		tx_oob.short_oob.short_vport_offset = txq->tx_vp_offset;

		DP_LOG(DEBUG, "tx_oob packet_format %u ipv4 %u ipv6 %u",
		       tx_oob.short_oob.packet_format,
		       tx_oob.short_oob.tx_is_outer_ipv4,
		       tx_oob.short_oob.tx_is_outer_ipv6);

		DP_LOG(DEBUG, "tx_oob checksum ip %u tcp %u udp %u offset %u",
		       tx_oob.short_oob.tx_compute_IP_header_checksum,
		       tx_oob.short_oob.tx_compute_TCP_checksum,
		       tx_oob.short_oob.tx_compute_UDP_checksum,
		       tx_oob.short_oob.tx_transport_header_offset);

		DP_LOG(DEBUG, "pkt[%d]: buf_addr 0x%p, nb_segs %d, pkt_len %d",
		       pkt_idx, m_pkt->buf_addr, m_pkt->nb_segs,
		       m_pkt->pkt_len);

		/* Create SGL for packet data buffers */
		for (seg_idx = 0; seg_idx < m_pkt->nb_segs; seg_idx++) {
			struct mana_mr_cache *mr =
				mana_find_pmd_mr(&txq->mr_btree, priv, m_seg);

			if (!mr) {
				DP_LOG(ERR, "failed to get MR, pkt_idx %u",
				       pkt_idx);
				break;
			}

			sgl.gdma_sgl[seg_idx].address =
				rte_cpu_to_le_64(rte_pktmbuf_mtod(m_seg,
								  uint64_t));
			sgl.gdma_sgl[seg_idx].size = m_seg->data_len;
			sgl.gdma_sgl[seg_idx].memory_key = mr->lkey;

			DP_LOG(DEBUG,
			       "seg idx %u addr 0x%" PRIx64 " size %x key %x",
			       seg_idx, sgl.gdma_sgl[seg_idx].address,
			       sgl.gdma_sgl[seg_idx].size,
			       sgl.gdma_sgl[seg_idx].memory_key);

			m_seg = m_seg->next;
		}

		/* Skip this packet if we can't populate all segments */
		if (seg_idx != m_pkt->nb_segs)
			continue;

		/* If we can at least queue post two WQEs and there are at
		 * least two packets to send, use TX CQE suppression for the
		 * current WQE
		 */
		if (txq->desc_ring_len + 1 < txq->num_desc &&
		    pkt_idx + 1 < nb_pkts)
			tx_oob.short_oob.suppress_tx_CQE_generation = 1;
		else
			tx_oob.short_oob.suppress_tx_CQE_generation = 0;

		struct gdma_work_request work_req;
		uint32_t wqe_size_in_bu;

		work_req.gdma_header.struct_size = sizeof(work_req);

		work_req.sgl = sgl.gdma_sgl;
		work_req.num_sgl_elements = m_pkt->nb_segs;
		work_req.inline_oob_size_in_bytes =
			sizeof(struct transmit_short_oob_v2);
		work_req.inline_oob_data = &tx_oob;
		work_req.flags = 0;
		work_req.client_data_unit = NOT_USING_CLIENT_DATA_UNIT;

		ret = gdma_post_work_request(&txq->gdma_sq, &work_req,
					     &wqe_size_in_bu);
		if (!ret) {
			struct mana_txq_desc *desc =
				&txq->desc_ring[txq->desc_ring_head];

			/* Update queue for tracking pending requests */
			desc->pkt = m_pkt;
			desc->wqe_size_in_bu = wqe_size_in_bu;
			desc->suppress_tx_cqe =
				tx_oob.short_oob.suppress_tx_CQE_generation;
			txq->desc_ring_head =
				(txq->desc_ring_head + 1) % txq->num_desc;
			txq->desc_ring_len++;

			pkt_sent++;

			DP_LOG(DEBUG, "nb_pkts %u pkt[%d] sent",
			       nb_pkts, pkt_idx);
#ifdef RTE_ARCH_32
			wqe_count += wqe_size_in_bu;
			if (wqe_count > TX_WQE_SHORT_DB_THRESHOLD) {
				/* wqe_count approaching to short doorbell
				 * increment limit. Stop processing further
				 * more packets and just ring short
				 * doorbell.
				 */
				DP_LOG(DEBUG, "wqe_count %u reaching limit, "
				       "pkt_sent %d",
				       wqe_count, pkt_sent);
				break;
			}
#endif
		} else {
			DP_LOG(DEBUG, "pkt[%d] failed to post send ret %d",
			       pkt_idx, ret);
			break;
		}
	}

	/* Ring hardware door bell */
	db_page = priv->db_page;
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		struct rte_eth_dev *dev =
			&rte_eth_devices[priv->dev_data->port_id];
		struct mana_process_priv *process_priv = dev->process_private;

		db_page = process_priv->db_page;
	}

	if (pkt_sent) {
#ifdef RTE_ARCH_32
		ret = mana_ring_short_doorbell(db_page, GDMA_QUEUE_SEND,
					       txq->gdma_sq.id,
					       wqe_count *
						GDMA_WQE_ALIGNMENT_UNIT_SIZE,
					       0);
#else
		ret = mana_ring_doorbell(db_page, GDMA_QUEUE_SEND,
					 txq->gdma_sq.id,
					 txq->gdma_sq.head *
						GDMA_WQE_ALIGNMENT_UNIT_SIZE,
					 0);
#endif
		if (ret)
			DP_LOG(ERR, "mana_ring_doorbell failed ret %d", ret);
	}

	return pkt_sent;
}
