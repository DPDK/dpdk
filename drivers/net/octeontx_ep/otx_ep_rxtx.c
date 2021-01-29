/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <unistd.h>

#include <rte_eal.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <ethdev_pci.h>

#include "otx_ep_common.h"
#include "otx_ep_vf.h"
#include "otx2_ep_vf.h"
#include "otx_ep_rxtx.h"

static void
otx_ep_dmazone_free(const struct rte_memzone *mz)
{
	const struct rte_memzone *mz_tmp;
	int ret = 0;

	if (mz == NULL) {
		otx_ep_err("Memzone %s : NULL\n", mz->name);
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

	iq = otx_ep->instr_queue[iq_no];
	if (iq == NULL) {
		otx_ep_err("Invalid IQ[%d]\n", iq_no);
		return -EINVAL;
	}

	rte_free(iq->req_list);
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
	uint32_t q_size;

	conf = otx_ep->conf;
	iq = otx_ep->instr_queue[iq_no];
	q_size = conf->iq.instr_type * num_descs;

	/* IQ memory creation for Instruction submission to OCTEON TX2 */
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
	 * posted to OCTEON TX2. This list will be used for freeing the IQ
	 * data buffer(s) later once the OCTEON TX2 fetched the requests.
	 */
	iq->req_list = rte_zmalloc_socket("request_list",
			(iq->nb_desc * OTX_EP_IQREQ_LIST_SIZE),
			RTE_CACHE_LINE_SIZE,
			rte_socket_id());
	if (iq->req_list == NULL) {
		otx_ep_err("IQ[%d] req_list alloc failed\n", iq_no);
		goto iq_init_fail;
	}

	otx_ep_info("IQ[%d]: base: %p basedma: %lx count: %d\n",
		     iq_no, iq->base_addr, (unsigned long)iq->base_addr_dma,
		     iq->nb_desc);

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
	otx_ep->fn_list.setup_iq_regs(otx_ep, iq_no);

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
	otx_ep->fn_list.setup_oq_regs(otx_ep, q_no);

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
