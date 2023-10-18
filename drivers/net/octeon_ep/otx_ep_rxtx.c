/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <unistd.h>
#include <assert.h>
#include <rte_eal.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_io.h>
#include <rte_net.h>
#include <ethdev_pci.h>

#include "otx_ep_common.h"
#include "otx_ep_vf.h"
#include "otx_ep_rxtx.h"

static void
otx_ep_dmazone_free(const struct rte_memzone *mz)
{
	const struct rte_memzone *mz_tmp;
	int ret = 0;

	if (mz == NULL) {
		otx_ep_err("Memzone: NULL\n");
		return;
	}

	mz_tmp = rte_memzone_lookup(mz->name);
	if (mz_tmp == NULL) {
		otx_ep_err("Memzone %s Not Found\n", mz->name);
		return;
	}

	ret = rte_memzone_free(mz);
	if (ret)
		otx_ep_err("Memzone free failed : ret = %d\n", ret);
}

/* Free IQ resources */
int
otx_ep_delete_iqs(struct otx_ep_device *otx_ep, uint32_t iq_no)
{
	struct otx_ep_instr_queue *iq;
	uint32_t i;

	iq = otx_ep->instr_queue[iq_no];
	if (iq == NULL) {
		otx_ep_err("Invalid IQ[%d]\n", iq_no);
		return -EINVAL;
	}

	if (iq->req_list) {
		for (i = 0; i < iq->nb_desc; i++)
			rte_free(iq->req_list[i].finfo.g.sg);
		rte_free(iq->req_list);
	}

	iq->req_list = NULL;

	if (iq->iq_mz) {
		otx_ep_dmazone_free(iq->iq_mz);
		iq->iq_mz = NULL;
	}

	rte_free(otx_ep->instr_queue[iq_no]);
	otx_ep->instr_queue[iq_no] = NULL;

	otx_ep->nb_tx_queues--;

	otx_ep_info("IQ[%d] is deleted\n", iq_no);

	return 0;
}

/* IQ initialization */
static int
otx_ep_init_instr_queue(struct otx_ep_device *otx_ep, int iq_no, int num_descs,
		     unsigned int socket_id)
{
	const struct otx_ep_config *conf;
	struct otx_ep_instr_queue *iq;
	struct otx_ep_sg_entry *sg;
	uint32_t i, q_size;
	int ret;

	conf = otx_ep->conf;
	iq = otx_ep->instr_queue[iq_no];
	q_size = conf->iq.instr_type * num_descs;

	/* IQ memory creation for Instruction submission to OCTEON 9 */
	iq->iq_mz = rte_eth_dma_zone_reserve(otx_ep->eth_dev,
					     "instr_queue", iq_no, q_size,
					     OTX_EP_PCI_RING_ALIGN,
					     socket_id);
	if (iq->iq_mz == NULL) {
		otx_ep_err("IQ[%d] memzone alloc failed\n", iq_no);
		goto iq_init_fail;
	}

	iq->base_addr_dma = iq->iq_mz->iova;
	iq->base_addr = (uint8_t *)iq->iq_mz->addr;

	if (num_descs & (num_descs - 1)) {
		otx_ep_err("IQ[%d] descs not in power of 2\n", iq_no);
		goto iq_init_fail;
	}

	iq->nb_desc = num_descs;

	/* Create a IQ request list to hold requests that have been
	 * posted to OCTEON 9. This list will be used for freeing the IQ
	 * data buffer(s) later once the OCTEON 9 fetched the requests.
	 */
	iq->req_list = rte_zmalloc_socket("request_list",
			(iq->nb_desc * OTX_EP_IQREQ_LIST_SIZE),
			RTE_CACHE_LINE_SIZE,
			rte_socket_id());
	if (iq->req_list == NULL) {
		otx_ep_err("IQ[%d] req_list alloc failed\n", iq_no);
		goto iq_init_fail;
	}

	for (i = 0; i < iq->nb_desc; i++) {
		sg = rte_zmalloc_socket("sg_entry", (OTX_EP_MAX_SG_LISTS * OTX_EP_SG_ENTRY_SIZE),
			OTX_EP_SG_ALIGN, rte_socket_id());
		if (sg == NULL) {
			otx_ep_err("IQ[%d] sg_entries alloc failed\n", iq_no);
			goto iq_init_fail;
		}

		iq->req_list[i].finfo.g.num_sg = OTX_EP_MAX_SG_LISTS;
		iq->req_list[i].finfo.g.sg = sg;
	}

	otx_ep_info("IQ[%d]: base: %p basedma: %lx count: %d\n",
		     iq_no, iq->base_addr, (unsigned long)iq->base_addr_dma,
		     iq->nb_desc);

	iq->mbuf_list = rte_zmalloc_socket("mbuf_list",	(iq->nb_desc * sizeof(struct rte_mbuf *)),
					   RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (!iq->mbuf_list) {
		otx_ep_err("IQ[%d] mbuf_list alloc failed\n", iq_no);
		goto iq_init_fail;
	}

	iq->otx_ep_dev = otx_ep;
	iq->q_no = iq_no;
	iq->fill_cnt = 0;
	iq->host_write_index = 0;
	iq->otx_read_index = 0;
	iq->flush_index = 0;
	iq->instr_pending = 0;

	otx_ep->io_qmask.iq |= (1ull << iq_no);

	/* Set 32B/64B mode for each input queue */
	if (conf->iq.instr_type == 64)
		otx_ep->io_qmask.iq64B |= (1ull << iq_no);

	iq->iqcmd_64B = (conf->iq.instr_type == 64);

	/* Set up IQ registers */
	ret = otx_ep->fn_list.setup_iq_regs(otx_ep, iq_no);
	if (ret)
		return ret;

	return 0;

iq_init_fail:
	return -ENOMEM;
}

int
otx_ep_setup_iqs(struct otx_ep_device *otx_ep, uint32_t iq_no, int num_descs,
		 unsigned int socket_id)
{
	struct otx_ep_instr_queue *iq;

	iq = (struct otx_ep_instr_queue *)rte_zmalloc("otx_ep_IQ", sizeof(*iq),
						RTE_CACHE_LINE_SIZE);
	if (iq == NULL)
		return -ENOMEM;

	otx_ep->instr_queue[iq_no] = iq;

	if (otx_ep_init_instr_queue(otx_ep, iq_no, num_descs, socket_id)) {
		otx_ep_err("IQ init is failed\n");
		goto delete_IQ;
	}
	otx_ep->nb_tx_queues++;

	otx_ep_info("IQ[%d] is created.\n", iq_no);

	return 0;

delete_IQ:
	otx_ep_delete_iqs(otx_ep, iq_no);
	return -ENOMEM;
}

static void
otx_ep_droq_reset_indices(struct otx_ep_droq *droq)
{
	droq->read_idx  = 0;
	droq->write_idx = 0;
	droq->refill_idx = 0;
	droq->refill_count = 0;
	droq->last_pkt_count = 0;
	droq->pkts_pending = 0;
}

static void
otx_ep_droq_destroy_ring_buffers(struct otx_ep_droq *droq)
{
	uint32_t idx;

	for (idx = 0; idx < droq->nb_desc; idx++) {
		if (droq->recv_buf_list[idx]) {
			rte_pktmbuf_free(droq->recv_buf_list[idx]);
			droq->recv_buf_list[idx] = NULL;
		}
	}

	otx_ep_droq_reset_indices(droq);
}

/* Free OQs resources */
int
otx_ep_delete_oqs(struct otx_ep_device *otx_ep, uint32_t oq_no)
{
	struct otx_ep_droq *droq;

	droq = otx_ep->droq[oq_no];
	if (droq == NULL) {
		otx_ep_err("Invalid droq[%d]\n", oq_no);
		return -EINVAL;
	}

	otx_ep_droq_destroy_ring_buffers(droq);
	rte_free(droq->recv_buf_list);
	droq->recv_buf_list = NULL;

	if (droq->desc_ring_mz) {
		otx_ep_dmazone_free(droq->desc_ring_mz);
		droq->desc_ring_mz = NULL;
	}

	memset(droq, 0, OTX_EP_DROQ_SIZE);

	rte_free(otx_ep->droq[oq_no]);
	otx_ep->droq[oq_no] = NULL;

	otx_ep->nb_rx_queues--;

	otx_ep_info("OQ[%d] is deleted\n", oq_no);
	return 0;
}

static int
otx_ep_droq_setup_ring_buffers(struct otx_ep_droq *droq)
{
	struct otx_ep_droq_desc *desc_ring = droq->desc_ring;
	struct otx_ep_droq_info *info;
	struct rte_mbuf *buf;
	uint32_t idx;

	for (idx = 0; idx < droq->nb_desc; idx++) {
		buf = rte_pktmbuf_alloc(droq->mpool);
		if (buf == NULL) {
			otx_ep_err("OQ buffer alloc failed\n");
			droq->stats.rx_alloc_failure++;
			return -ENOMEM;
		}

		droq->recv_buf_list[idx] = buf;
		info = rte_pktmbuf_mtod(buf, struct otx_ep_droq_info *);
		memset(info, 0, sizeof(*info));
		desc_ring[idx].buffer_ptr = rte_mbuf_data_iova_default(buf);
	}

	otx_ep_droq_reset_indices(droq);

	return 0;
}

/* OQ initialization */
static int
otx_ep_init_droq(struct otx_ep_device *otx_ep, uint32_t q_no,
	      uint32_t num_descs, uint32_t desc_size,
	      struct rte_mempool *mpool, unsigned int socket_id)
{
	const struct otx_ep_config *conf = otx_ep->conf;
	uint32_t c_refill_threshold;
	struct otx_ep_droq *droq;
	uint32_t desc_ring_size;
	int ret;

	otx_ep_info("OQ[%d] Init start\n", q_no);

	droq = otx_ep->droq[q_no];
	droq->otx_ep_dev = otx_ep;
	droq->q_no = q_no;
	droq->mpool = mpool;

	droq->nb_desc      = num_descs;
	droq->buffer_size  = desc_size;
	c_refill_threshold = RTE_MAX(conf->oq.refill_threshold,
				     droq->nb_desc / 2);

	/* OQ desc_ring set up */
	desc_ring_size = droq->nb_desc * OTX_EP_DROQ_DESC_SIZE;
	droq->desc_ring_mz = rte_eth_dma_zone_reserve(otx_ep->eth_dev, "droq",
						      q_no, desc_ring_size,
						      OTX_EP_PCI_RING_ALIGN,
						      socket_id);

	if (droq->desc_ring_mz == NULL) {
		otx_ep_err("OQ:%d desc_ring allocation failed\n", q_no);
		goto init_droq_fail;
	}

	droq->desc_ring_dma = droq->desc_ring_mz->iova;
	droq->desc_ring = (struct otx_ep_droq_desc *)droq->desc_ring_mz->addr;

	otx_ep_dbg("OQ[%d]: desc_ring: virt: 0x%p, dma: %lx\n",
		    q_no, droq->desc_ring, (unsigned long)droq->desc_ring_dma);
	otx_ep_dbg("OQ[%d]: num_desc: %d\n", q_no, droq->nb_desc);

	/* OQ buf_list set up */
	droq->recv_buf_list = rte_zmalloc_socket("recv_buf_list",
				(droq->nb_desc * sizeof(struct rte_mbuf *)),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (droq->recv_buf_list == NULL) {
		otx_ep_err("OQ recv_buf_list alloc failed\n");
		goto init_droq_fail;
	}

	if (otx_ep_droq_setup_ring_buffers(droq))
		goto init_droq_fail;

	droq->refill_threshold = c_refill_threshold;

	/* Set up OQ registers */
	ret = otx_ep->fn_list.setup_oq_regs(otx_ep, q_no);
	if (ret)
		return ret;

	otx_ep->io_qmask.oq |= (1ull << q_no);

	return 0;

init_droq_fail:
	return -ENOMEM;
}

/* OQ configuration and setup */
int
otx_ep_setup_oqs(struct otx_ep_device *otx_ep, int oq_no, int num_descs,
		 int desc_size, struct rte_mempool *mpool,
		 unsigned int socket_id)
{
	struct otx_ep_droq *droq;

	/* Allocate new droq. */
	droq = (struct otx_ep_droq *)rte_zmalloc("otx_ep_OQ",
				sizeof(*droq), RTE_CACHE_LINE_SIZE);
	if (droq == NULL) {
		otx_ep_err("Droq[%d] Creation Failed\n", oq_no);
		return -ENOMEM;
	}
	otx_ep->droq[oq_no] = droq;

	if (otx_ep_init_droq(otx_ep, oq_no, num_descs, desc_size, mpool,
			     socket_id)) {
		otx_ep_err("Droq[%d] Initialization failed\n", oq_no);
		goto delete_OQ;
	}
	otx_ep_info("OQ[%d] is created.\n", oq_no);

	otx_ep->nb_rx_queues++;

	return 0;

delete_OQ:
	otx_ep_delete_oqs(otx_ep, oq_no);
	return -ENOMEM;
}

static inline void
otx_ep_iqreq_delete(struct otx_ep_instr_queue *iq, uint32_t idx)
{
	struct rte_mbuf *mbuf;
	uint32_t reqtype;

	mbuf    = iq->req_list[idx].finfo.mbuf;
	reqtype = iq->req_list[idx].reqtype;

	switch (reqtype) {
	case OTX_EP_REQTYPE_NORESP_NET:
	case OTX_EP_REQTYPE_NORESP_GATHER:
		/* This will take care of multiple segments also */
		rte_pktmbuf_free(mbuf);
		otx_ep_dbg("IQ buffer freed at idx[%d]\n", idx);
		break;

	case OTX_EP_REQTYPE_NONE:
	default:
		otx_ep_info("This iqreq mode is not supported:%d\n", reqtype);
	}

	/* Reset the request list at this index */
	iq->req_list[idx].finfo.mbuf = NULL;
	iq->req_list[idx].reqtype = 0;
}

static inline void
otx_ep_iqreq_add(struct otx_ep_instr_queue *iq, struct rte_mbuf *mbuf,
		uint32_t reqtype, int index)
{
	iq->req_list[index].finfo.mbuf = mbuf;
	iq->req_list[index].reqtype = reqtype;
}

static uint32_t
otx_vf_update_read_index(struct otx_ep_instr_queue *iq)
{
	uint32_t val;

	/*
	 * Batch subtractions from the HW counter to reduce PCIe traffic
	 * This adds an extra local variable, but almost halves the
	 * number of PCIe writes.
	 */
	val = *iq->inst_cnt_ism;
	iq->inst_cnt += val - iq->inst_cnt_ism_prev;
	iq->inst_cnt_ism_prev = val;

	if (val > (uint32_t)(1 << 31)) {
		/*
		 * Only subtract the packet count in the HW counter
		 * when count above halfway to saturation.
		 */
		rte_write32(val, iq->inst_cnt_reg);
		rte_mb();

		rte_write64(OTX2_SDP_REQUEST_ISM, iq->inst_cnt_reg);
		while (__atomic_load_n(iq->inst_cnt_ism, __ATOMIC_RELAXED) >= val) {
			rte_write64(OTX2_SDP_REQUEST_ISM, iq->inst_cnt_reg);
			rte_mb();
		}

		iq->inst_cnt_ism_prev = 0;
	}
	rte_write64(OTX2_SDP_REQUEST_ISM, iq->inst_cnt_reg);

	/* Modulo of the new index with the IQ size will give us
	 * the new index.
	 */
	return iq->inst_cnt & (iq->nb_desc - 1);
}

static void
otx_ep_flush_iq(struct otx_ep_instr_queue *iq)
{
	uint32_t instr_processed = 0;

	iq->otx_read_index = otx_vf_update_read_index(iq);
	while (iq->flush_index != iq->otx_read_index) {
		/* Free the IQ data buffer to the pool */
		otx_ep_iqreq_delete(iq, iq->flush_index);
		iq->flush_index =
			otx_ep_incr_index(iq->flush_index, 1, iq->nb_desc);

		instr_processed++;
	}

	iq->stats.instr_processed = instr_processed;
	iq->instr_pending -= instr_processed;
}

static inline void
otx_ep_ring_doorbell(struct otx_ep_device *otx_ep __rte_unused,
		struct otx_ep_instr_queue *iq)
{
	rte_wmb();
	rte_write64(iq->fill_cnt, iq->doorbell_reg);
	iq->fill_cnt = 0;
}

static inline int
post_iqcmd(struct otx_ep_instr_queue *iq, uint8_t *iqcmd)
{
	uint8_t *iqptr, cmdsize;

	/* This ensures that the read index does not wrap around to
	 * the same position if queue gets full before OCTEON 9 could
	 * fetch any instr.
	 */
	if (iq->instr_pending > (iq->nb_desc - 1))
		return OTX_EP_IQ_SEND_FAILED;

	/* Copy cmd into iq */
	cmdsize = 64;
	iqptr   = iq->base_addr + (iq->host_write_index << 6);

	rte_memcpy(iqptr, iqcmd, cmdsize);

	/* Increment the host write index */
	iq->host_write_index =
		otx_ep_incr_index(iq->host_write_index, 1, iq->nb_desc);

	iq->fill_cnt++;

	/* Flush the command into memory. We need to be sure the data
	 * is in memory before indicating that the instruction is
	 * pending.
	 */
	iq->instr_pending++;
	/* OTX_EP_IQ_SEND_SUCCESS */
	return 0;
}


static int
otx_ep_send_data(struct otx_ep_device *otx_ep, struct otx_ep_instr_queue *iq,
		 void *cmd, int dbell)
{
	uint32_t ret;

	/* Submit IQ command */
	ret = post_iqcmd(iq, cmd);

	if (ret == OTX_EP_IQ_SEND_SUCCESS) {
		if (dbell)
			otx_ep_ring_doorbell(otx_ep, iq);
		iq->stats.instr_posted++;

	} else {
		iq->stats.instr_dropped++;
		if (iq->fill_cnt)
			otx_ep_ring_doorbell(otx_ep, iq);
	}
	return ret;
}

static inline void
set_sg_size(struct otx_ep_sg_entry *sg_entry, uint16_t size, uint32_t pos)
{
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	sg_entry->u.size[pos] = size;
#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	sg_entry->u.size[(OTX_EP_NUM_SG_PTRS - 1) - pos] = size;
#endif
}

static inline int
prepare_xmit_gather_list(struct otx_ep_instr_queue *iq, struct rte_mbuf *m, uint64_t *dptr,
			 union otx_ep_instr_ih *ih)
{
	uint16_t j = 0, frags, num_sg, mask = OTX_EP_NUM_SG_PTRS - 1;
	struct otx_ep_buf_free_info *finfo;
	uint32_t pkt_len;
	int rc = -1;

	pkt_len = rte_pktmbuf_pkt_len(m);
	frags = m->nb_segs;
	num_sg = (frags + mask) / OTX_EP_NUM_SG_PTRS;

	if (unlikely(pkt_len > OTX_EP_MAX_PKT_SZ && num_sg > OTX_EP_MAX_SG_LISTS)) {
		otx_ep_err("Failed to xmit the pkt, pkt_len is higher or pkt has more segments\n");
		goto exit;
	}

	finfo = &iq->req_list[iq->host_write_index].finfo;
	*dptr = rte_mem_virt2iova(finfo->g.sg);
	ih->u64 |= ((1ULL << 62) | ((uint64_t)frags << 48) | (pkt_len + ih->s.fsz));

	while (frags--) {
		finfo->g.sg[(j >> 2)].ptr[(j & mask)] = rte_mbuf_data_iova(m);
		set_sg_size(&finfo->g.sg[(j >> 2)], m->data_len, (j & mask));
		j++;
		m = m->next;
	}

	return 0;

exit:
	return rc;
}

/* Enqueue requests/packets to OTX_EP IQ queue.
 * returns number of requests enqueued successfully
 */
uint16_t
otx_ep_xmit_pkts(void *tx_queue, struct rte_mbuf **pkts, uint16_t nb_pkts)
{
	struct otx_ep_instr_queue *iq = (struct otx_ep_instr_queue *)tx_queue;
	struct otx_ep_device *otx_ep = iq->otx_ep_dev;
	struct otx_ep_instr_64B iqcmd;
	int dbell, index, count = 0;
	uint32_t iqreq_type;
	uint32_t pkt_len, i;
	struct rte_mbuf *m;

	iqcmd.ih.u64 = 0;
	iqcmd.pki_ih3.u64 = 0;
	iqcmd.irh.u64 = 0;

	/* ih invars */
	iqcmd.ih.s.fsz = OTX_EP_FSZ;
	iqcmd.ih.s.pkind = otx_ep->pkind; /* The SDK decided PKIND value */

	/* pki ih3 invars */
	iqcmd.pki_ih3.s.w = 1;
	iqcmd.pki_ih3.s.utt = 1;
	iqcmd.pki_ih3.s.tagtype = ORDERED_TAG;
	/* sl will be sizeof(pki_ih3) */
	iqcmd.pki_ih3.s.sl = OTX_EP_FSZ + OTX_CUST_DATA_LEN;

	/* irh invars */
	iqcmd.irh.s.opcode = OTX_EP_NW_PKT_OP;

	for (i = 0; i < nb_pkts; i++) {
		m = pkts[i];
		if (m->nb_segs == 1) {
			pkt_len = rte_pktmbuf_data_len(m);
			iqcmd.ih.s.tlen = pkt_len + iqcmd.ih.s.fsz;
			iqcmd.dptr = rte_mbuf_data_iova(m); /*dptr*/
			iqcmd.ih.s.gather = 0;
			iqcmd.ih.s.gsz = 0;
			iqreq_type = OTX_EP_REQTYPE_NORESP_NET;
		} else {
			if (!(otx_ep->tx_offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS))
				goto xmit_fail;

			if (unlikely(prepare_xmit_gather_list(iq, m, &iqcmd.dptr, &iqcmd.ih) < 0))
				goto xmit_fail;

			pkt_len = rte_pktmbuf_pkt_len(m);
			iqreq_type = OTX_EP_REQTYPE_NORESP_GATHER;
		}

		iqcmd.irh.u64 = rte_bswap64(iqcmd.irh.u64);

#ifdef OTX_EP_IO_DEBUG
		otx_ep_dbg("After swapping\n");
		otx_ep_dbg("Word0 [dptr]: 0x%016lx\n",
			   (unsigned long)iqcmd.dptr);
		otx_ep_dbg("Word1 [ihtx]: 0x%016lx\n", (unsigned long)iqcmd.ih);
		otx_ep_dbg("Word2 [pki_ih3]: 0x%016lx\n",
			   (unsigned long)iqcmd.pki_ih3);
		otx_ep_dbg("Word3 [rptr]: 0x%016lx\n",
			   (unsigned long)iqcmd.rptr);
		otx_ep_dbg("Word4 [irh]: 0x%016lx\n", (unsigned long)iqcmd.irh);
		otx_ep_dbg("Word5 [exhdr[0]]: 0x%016lx\n",
				(unsigned long)iqcmd.exhdr[0]);
		rte_pktmbuf_dump(stdout, m, rte_pktmbuf_pkt_len(m));
#endif
		dbell = (i == (unsigned int)(nb_pkts - 1)) ? 1 : 0;
		index = iq->host_write_index;
		if (otx_ep_send_data(otx_ep, iq, &iqcmd, dbell))
			goto xmit_fail;
		otx_ep_iqreq_add(iq, m, iqreq_type, index);
		iq->stats.tx_pkts++;
		iq->stats.tx_bytes += pkt_len;
		count++;
	}

xmit_fail:
	if (iq->instr_pending >= OTX_EP_MAX_INSTR)
		otx_ep_flush_iq(iq);

	/* Return no# of instructions posted successfully. */
	return count;
}

static uint32_t
otx_ep_droq_refill(struct otx_ep_droq *droq)
{
	struct otx_ep_droq_desc *desc_ring = droq->desc_ring;
	struct otx_ep_droq_info *info;
	struct rte_mbuf *buf = NULL;
	uint32_t desc_refilled = 0;

	while (droq->refill_count && (desc_refilled < droq->nb_desc)) {
		buf = rte_pktmbuf_alloc(droq->mpool);
		/* If a buffer could not be allocated, no point in
		 * continuing
		 */
		if (unlikely(!buf)) {
			droq->stats.rx_alloc_failure++;
			break;
		}
		info = rte_pktmbuf_mtod(buf, struct otx_ep_droq_info *);
		info->length = 0;

		droq->recv_buf_list[droq->refill_idx] = buf;
		desc_ring[droq->refill_idx].buffer_ptr =
					rte_mbuf_data_iova_default(buf);
		droq->refill_idx = otx_ep_incr_index(droq->refill_idx, 1,
				droq->nb_desc);

		desc_refilled++;
		droq->refill_count--;
	}

	return desc_refilled;
}

static struct rte_mbuf *
otx_ep_droq_read_packet(struct otx_ep_device *otx_ep, struct otx_ep_droq *droq, int next_fetch)
{
	volatile struct otx_ep_droq_info *info;
	struct rte_mbuf *mbuf_next = NULL;
	struct rte_mbuf *mbuf = NULL;
	uint64_t total_pkt_len;
	uint32_t pkt_len = 0;
	int next_idx;

	mbuf = droq->recv_buf_list[droq->read_idx];
	info = rte_pktmbuf_mtod(mbuf, struct otx_ep_droq_info *);

	/* make sure info is available */
	rte_rmb();
	if (unlikely(!info->length)) {
		int retry = OTX_EP_MAX_DELAYED_PKT_RETRIES;
		/* otx_ep_dbg("OCTEON DROQ[%d]: read_idx: %d; Data not ready "
		 * "yet, Retry; pending=%lu\n", droq->q_no, droq->read_idx,
		 * droq->pkts_pending);
		 */
		droq->stats.pkts_delayed_data++;
		while (retry && !info->length) {
			retry--;
			rte_delay_us_block(50);
		}
		if (!retry && !info->length) {
			otx_ep_err("OCTEON DROQ[%d]: read_idx: %d; Retry failed !!\n",
				   droq->q_no, droq->read_idx);
			/* May be zero length packet; drop it */
			assert(0);
		}
	}

	if (next_fetch) {
		next_idx = otx_ep_incr_index(droq->read_idx, 1, droq->nb_desc);
		mbuf_next = droq->recv_buf_list[next_idx];
		rte_prefetch0(rte_pktmbuf_mtod(mbuf_next, void *));
	}

	info->length = rte_bswap16(info->length >> 48);
	/* Deduce the actual data size */
	total_pkt_len = info->length + OTX_EP_INFO_SIZE;
	if (total_pkt_len <= droq->buffer_size) {
		mbuf->data_off += OTX_EP_INFO_SIZE;
		pkt_len = (uint32_t)info->length;
		mbuf->pkt_len  = pkt_len;
		mbuf->data_len  = pkt_len;
		mbuf->port = otx_ep->port_id;
		droq->recv_buf_list[droq->read_idx] = NULL;
		droq->read_idx = otx_ep_incr_index(droq->read_idx, 1, droq->nb_desc);
		droq->refill_count++;
	} else {
		struct rte_mbuf *first_buf = NULL;
		struct rte_mbuf *last_buf = NULL;

		/* csr read helps to flush pending dma */
		droq->sent_reg_val = rte_read32(droq->pkts_sent_reg);
		rte_rmb();

		while (pkt_len < total_pkt_len) {
			int cpy_len = 0;

			cpy_len = ((pkt_len + droq->buffer_size) > total_pkt_len)
					? ((uint32_t)total_pkt_len - pkt_len)
					: droq->buffer_size;

			mbuf = droq->recv_buf_list[droq->read_idx];
			droq->recv_buf_list[droq->read_idx] = NULL;

			if (likely(mbuf)) {
				/* Note the first seg */
				if (!pkt_len)
					first_buf = mbuf;

				mbuf->port = otx_ep->port_id;
				if (!pkt_len) {
					mbuf->data_off += OTX_EP_INFO_SIZE;
					mbuf->pkt_len = cpy_len - OTX_EP_INFO_SIZE;
					mbuf->data_len = cpy_len - OTX_EP_INFO_SIZE;
				} else {
					mbuf->pkt_len = cpy_len;
					mbuf->data_len = cpy_len;
				}

				if (pkt_len) {
					first_buf->nb_segs++;
					first_buf->pkt_len += mbuf->pkt_len;
				}

				if (last_buf)
					last_buf->next = mbuf;

				last_buf = mbuf;
			} else {
				otx_ep_err("no buf\n");
				assert(0);
			}

			pkt_len += cpy_len;
			droq->read_idx = otx_ep_incr_index(droq->read_idx, 1, droq->nb_desc);
			droq->refill_count++;
		}
		mbuf = first_buf;
	}

	return mbuf;
}

static inline uint32_t
otx_ep_check_droq_pkts(struct otx_ep_droq *droq)
{
	uint32_t new_pkts;
	uint32_t val;

	/*
	 * Batch subtractions from the HW counter to reduce PCIe traffic
	 * This adds an extra local variable, but almost halves the
	 * number of PCIe writes.
	 */
	val = *droq->pkts_sent_ism;
	new_pkts = val - droq->pkts_sent_ism_prev;
	droq->pkts_sent_ism_prev = val;

	if (val > (uint32_t)(1 << 31)) {
		/*
		 * Only subtract the packet count in the HW counter
		 * when count above halfway to saturation.
		 */
		rte_write32(val, droq->pkts_sent_reg);
		rte_mb();

		rte_write64(OTX2_SDP_REQUEST_ISM, droq->pkts_sent_reg);
		while (__atomic_load_n(droq->pkts_sent_ism, __ATOMIC_RELAXED) >= val) {
			rte_write64(OTX2_SDP_REQUEST_ISM, droq->pkts_sent_reg);
			rte_mb();
		}

		droq->pkts_sent_ism_prev = 0;
	}
	rte_write64(OTX2_SDP_REQUEST_ISM, droq->pkts_sent_reg);
	droq->pkts_pending += new_pkts;

	return new_pkts;
}

static inline int32_t __rte_hot
otx_ep_rx_pkts_to_process(struct otx_ep_droq *droq, uint16_t nb_pkts)
{
	if (unlikely(droq->pkts_pending < nb_pkts))
		otx_ep_check_droq_pkts(droq);

	return RTE_MIN(nb_pkts, droq->pkts_pending);
}

/* Check for response arrival from OCTEON 9
 * returns number of requests completed
 */
uint16_t
otx_ep_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_droq *droq = rx_queue;
	struct otx_ep_device *otx_ep;
	struct rte_mbuf *oq_pkt;
	uint16_t pkts, new_pkts;
	uint32_t valid_pkts = 0;
	int next_fetch;

	otx_ep = droq->otx_ep_dev;
	new_pkts = otx_ep_rx_pkts_to_process(droq, nb_pkts);

	for (pkts = 0; pkts < new_pkts; pkts++) {
		/* Push the received pkt to application */
		next_fetch = (pkts == new_pkts - 1) ? 0 : 1;
		oq_pkt = otx_ep_droq_read_packet(otx_ep, droq, next_fetch);
		if (!oq_pkt) {
			RTE_LOG_DP(ERR, PMD,
				   "DROQ read pkt failed pending %" PRIu64
				    "last_pkt_count %" PRIu64 "new_pkts %d.\n",
				   droq->pkts_pending, droq->last_pkt_count,
				   new_pkts);
			droq->stats.rx_err++;
			continue;
		} else {
			rx_pkts[valid_pkts] = oq_pkt;
			valid_pkts++;
			/* Stats */
			droq->stats.pkts_received++;
			droq->stats.bytes_received += oq_pkt->pkt_len;
		}
	}
	droq->pkts_pending -= pkts;

	/* Refill DROQ buffers */
	if (droq->refill_count >= DROQ_REFILL_THRESHOLD) {
		int desc_refilled = otx_ep_droq_refill(droq);

		/* Flush the droq descriptor data to memory to be sure
		 * that when we update the credits the data in memory is
		 * accurate.
		 */
		rte_io_wmb();
		rte_write32(desc_refilled, droq->pkts_credit_reg);
	} else {
		/*
		 * SDP output goes into DROP state when output doorbell count
		 * goes below drop count. When door bell count is written with
		 * a value greater than drop count SDP output should come out
		 * of DROP state. Due to a race condition this is not happening.
		 * Writing doorbell register with 0 again may make SDP output
		 * come out of this state.
		 */

		rte_write32(0, droq->pkts_credit_reg);
	}
	return valid_pkts;
}
