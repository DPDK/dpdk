/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <cnxk_eswitch.h>

static __rte_always_inline struct rte_mbuf *
eswitch_nix_get_mbuf_from_cqe(void *cq, const uint64_t data_off)
{
	rte_iova_t buff;

	/* Skip CQE, NIX_RX_PARSE_S and SG HDR(9 DWORDs) and peek buff addr */
	buff = *((rte_iova_t *)((uint64_t *)cq + 9));
	return (struct rte_mbuf *)(buff - data_off);
}

static inline uint64_t
eswitch_nix_rx_nb_pkts(struct roc_nix_cq *cq, const uint64_t wdata, const uint32_t qmask)
{
	uint64_t reg, head, tail;
	uint32_t available;

	/* Update the available count if cached value is not enough */

	/* Use LDADDA version to avoid reorder */
	reg = roc_atomic64_add_sync(wdata, cq->status);
	/* CQ_OP_STATUS operation error */
	if (reg & BIT_ULL(NIX_CQ_OP_STAT_OP_ERR) || reg & BIT_ULL(NIX_CQ_OP_STAT_CQ_ERR))
		return 0;

	tail = reg & 0xFFFFF;
	head = (reg >> 20) & 0xFFFFF;
	if (tail < head)
		available = tail - head + qmask + 1;
	else
		available = tail - head;

	return available;
}

static inline void
nix_cn9k_xmit_one(uint64_t *cmd, void *lmt_addr, const plt_iova_t io_addr)
{
	uint64_t lmt_status;

	do {
		roc_lmt_mov(lmt_addr, cmd, 0);
		lmt_status = roc_lmt_submit_ldeor(io_addr);
	} while (lmt_status == 0);
}

uint16_t
cnxk_eswitch_dev_tx_burst(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid,
			  struct rte_mbuf **pkts, uint16_t nb_xmit, const uint16_t flags)
{
	struct roc_nix_sq *sq = &eswitch_dev->txq[qid].sqs;
	struct roc_nix_rq *rq = &eswitch_dev->rxq[qid].rqs;
	uint64_t aura_handle, cmd[6], data = 0;
	uint16_t lmt_id, pkt = 0, nb_tx = 0;
	struct nix_send_ext_s *send_hdr_ext;
	struct nix_send_hdr_s *send_hdr;
	uint16_t vlan_tci = qid;
	union nix_send_sg_s *sg;
	uintptr_t lmt_base, pa;
	int64_t fc_pkts, dw_m1;
	rte_iova_t io_addr;

	if (unlikely(eswitch_dev->txq[qid].state != CNXK_ESWITCH_QUEUE_STATE_STARTED))
		return 0;

	lmt_base = sq->roc_nix->lmt_base;
	io_addr = sq->io_addr;
	aura_handle = rq->aura_handle;
	/* Get LMT base address and LMT ID as per thread ID */
	lmt_id = roc_plt_control_lmt_id_get();
	lmt_base += ((uint64_t)lmt_id << ROC_LMT_LINE_SIZE_LOG2);
	/* Double word minus 1: LMTST size-1 in units of 128 bits */
	/* 2(HDR) + 2(EXT_HDR) + 1(SG) + 1(IOVA) = 6/2 - 1 = 2 */
	dw_m1 = cn10k_nix_tx_ext_subs(flags) + 1;

	memset(cmd, 0, sizeof(cmd));
	send_hdr = (struct nix_send_hdr_s *)&cmd[0];
	send_hdr->w0.sizem1 = dw_m1;
	send_hdr->w0.sq = sq->qid;

	if (dw_m1 >= 2) {
		send_hdr_ext = (struct nix_send_ext_s *)&cmd[2];
		send_hdr_ext->w0.subdc = NIX_SUBDC_EXT;
		if (flags & NIX_TX_OFFLOAD_VLAN_QINQ_F) {
			send_hdr_ext->w1.vlan0_ins_ena = true;
			/* 2B before end of l2 header */
			send_hdr_ext->w1.vlan0_ins_ptr = 12;
			send_hdr_ext->w1.vlan0_ins_tci = 0;
		}
		sg = (union nix_send_sg_s *)&cmd[4];
	} else {
		sg = (union nix_send_sg_s *)&cmd[2];
	}

	sg->subdc = NIX_SUBDC_SG;
	sg->segs = 1;
	sg->ld_type = NIX_SENDLDTYPE_LDD;

	/* Tx */
	fc_pkts = ((int64_t)sq->nb_sqb_bufs_adj - *((uint64_t *)sq->fc)) << sq->sqes_per_sqb_log2;

	if (fc_pkts < 0)
		nb_tx = 0;
	else
		nb_tx = PLT_MIN(nb_xmit, (uint64_t)fc_pkts);

	for (pkt = 0; pkt < nb_tx; pkt++) {
		send_hdr->w0.total = pkts[pkt]->pkt_len;
		if (pkts[pkt]->pool) {
			aura_handle = pkts[pkt]->pool->pool_id;
			send_hdr->w0.aura = roc_npa_aura_handle_to_aura(aura_handle);
		} else {
			send_hdr->w0.df = 1;
		}
		if (dw_m1 >= 2 && flags & NIX_TX_OFFLOAD_VLAN_QINQ_F)
			send_hdr_ext->w1.vlan0_ins_tci = vlan_tci;
		sg->seg1_size = pkts[pkt]->pkt_len;
		*(plt_iova_t *)(sg + 1) = rte_mbuf_data_iova(pkts[pkt]);

		plt_esw_dbg("Transmitting pkt %d (%p) vlan tci %x on sq %d esw qid %d", pkt,
			    pkts[pkt], vlan_tci, sq->qid, qid);
		if (roc_model_is_cn9k()) {
			nix_cn9k_xmit_one(cmd, sq->lmt_addr, sq->io_addr);
		} else {
			cn10k_nix_xmit_mv_lmt_base(lmt_base, cmd, flags);
			/* PA<6:4> = LMTST size-1 in units of 128 bits. Size of the first LMTST in
			 * burst.
			 */
			pa = io_addr | (dw_m1 << 4);
			data &= ~0x7ULL;
			/*<15:12> = CNTM1: Count minus one of LMTSTs in the burst */
			data = (0ULL << 12);
			/* *<10:0> = LMT_ID: Identifies which LMT line is used for the first LMTST
			 */
			data |= (uint64_t)lmt_id;

			/* STEOR0 */
			roc_lmt_submit_steorl(data, pa);
			rte_io_wmb();
		}
	}

	return nb_tx;
}

uint16_t
cnxk_eswitch_dev_rx_burst(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid,
			  struct rte_mbuf **pkts, uint16_t nb_pkts)
{
	struct roc_nix_rq *rq = &eswitch_dev->rxq[qid].rqs;
	struct roc_nix_cq *cq = &eswitch_dev->cxq[qid].cqs;
	const union nix_rx_parse_u *rx;
	struct nix_cqe_hdr_s *cqe;
	uint64_t pkt = 0, nb_rx;
	struct rte_mbuf *mbuf;
	uint64_t wdata;
	uint32_t qmask;
	uintptr_t desc;
	uint32_t head;

	if (unlikely(eswitch_dev->rxq[qid].state != CNXK_ESWITCH_QUEUE_STATE_STARTED))
		return 0;

	wdata = cq->wdata;
	qmask = cq->qmask;
	desc = (uintptr_t)cq->desc_base;
	nb_rx = eswitch_nix_rx_nb_pkts(cq, wdata, qmask);
	nb_rx = RTE_MIN(nb_rx, nb_pkts);
	head = cq->head;

	/* Nothing to receive */
	if (!nb_rx)
		return 0;

	/* Rx */
	for (pkt = 0; pkt < nb_rx; pkt++) {
		/* Prefetch N desc ahead */
		rte_prefetch_non_temporal((void *)(desc + (CQE_SZ((head + 2) & qmask))));
		cqe = (struct nix_cqe_hdr_s *)(desc + CQE_SZ(head));
		rx = (const union nix_rx_parse_u *)((const uint64_t *)cqe + 1);

		/* Skip	QE, NIX_RX_PARSE_S and SG HDR(9 DWORDs) and peek buff addr */
		mbuf = eswitch_nix_get_mbuf_from_cqe(cqe, rq->first_skip);
		mbuf->pkt_len = rx->pkt_lenm1 + 1;
		mbuf->data_len = rx->pkt_lenm1 + 1;
		mbuf->data_off = 128;
		/* Rx parse to capture vlan info */
		if (rx->vtag0_valid)
			mbuf->vlan_tci = rx->vtag0_tci;
		/* Populate RSS hash */
		mbuf->hash.rss = cqe->tag;
		mbuf->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		pkts[pkt] = mbuf;
		roc_prefetch_store_keep(mbuf);
		plt_esw_dbg("Packet %d rec on queue %d esw qid %d hash %x mbuf %p vlan tci %d",
			    (uint32_t)pkt, rq->qid, qid, mbuf->hash.rss, mbuf, mbuf->vlan_tci);
		head++;
		head &= qmask;
	}

	/* Free all the CQs that we've processed */
	rte_write64_relaxed((wdata | nb_rx), (void *)cq->door);
	cq->head = head;

	return nb_rx;
}
