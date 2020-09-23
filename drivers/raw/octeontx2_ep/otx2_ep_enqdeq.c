/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <rte_bus.h>
#include <rte_bus_pci.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_mempool.h>
#include <rte_pci.h>

#include <rte_common.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include "otx2_common.h"
#include "otx2_ep_enqdeq.h"

static void
sdp_dmazone_free(const struct rte_memzone *mz)
{
	const struct rte_memzone *mz_tmp;
	int ret = 0;

	if (mz == NULL) {
		otx2_err("Memzone %s : NULL", mz->name);
		return;
	}

	mz_tmp = rte_memzone_lookup(mz->name);
	if (mz_tmp == NULL) {
		otx2_err("Memzone %s Not Found", mz->name);
		return;
	}

	ret = rte_memzone_free(mz);
	if (ret)
		otx2_err("Memzone free failed : ret = %d", ret);

}

/* Free IQ resources */
int
sdp_delete_iqs(struct sdp_device *sdpvf, uint32_t iq_no)
{
	struct sdp_instr_queue *iq;

	iq = sdpvf->instr_queue[iq_no];
	if (iq == NULL) {
		otx2_err("Invalid IQ[%d]\n", iq_no);
		return -ENOMEM;
	}

	rte_free(iq->req_list);
	iq->req_list = NULL;

	if (iq->iq_mz) {
		sdp_dmazone_free(iq->iq_mz);
		iq->iq_mz = NULL;
	}

	rte_free(sdpvf->instr_queue[iq_no]);
	sdpvf->instr_queue[iq_no] = NULL;

	sdpvf->num_iqs--;

	otx2_info("IQ[%d] is deleted", iq_no);

	return 0;
}

/* IQ initialization */
static int
sdp_init_instr_queue(struct sdp_device *sdpvf, int iq_no)
{
	const struct sdp_config *conf;
	struct sdp_instr_queue *iq;
	uint32_t q_size;

	conf = sdpvf->conf;
	iq = sdpvf->instr_queue[iq_no];
	q_size = conf->iq.instr_type * conf->num_iqdef_descs;

	/* IQ memory creation for Instruction submission to OCTEON TX2 */
	iq->iq_mz = rte_memzone_reserve_aligned("iqmz",
					q_size,
					rte_socket_id(),
					RTE_MEMZONE_IOVA_CONTIG,
					RTE_CACHE_LINE_SIZE);
	if (iq->iq_mz == NULL) {
		otx2_err("IQ[%d] memzone alloc failed", iq_no);
		goto iq_init_fail;
	}

	iq->base_addr_dma = iq->iq_mz->iova;
	iq->base_addr = (uint8_t *)iq->iq_mz->addr;

	if (conf->num_iqdef_descs & (conf->num_iqdef_descs - 1)) {
		otx2_err("IQ[%d] descs not in power of 2", iq_no);
		goto iq_init_fail;
	}

	iq->nb_desc = conf->num_iqdef_descs;

	/* Create a IQ request list to hold requests that have been
	 * posted to OCTEON TX2. This list will be used for freeing the IQ
	 * data buffer(s) later once the OCTEON TX2 fetched the requests.
	 */
	iq->req_list = rte_zmalloc_socket("request_list",
			(iq->nb_desc * SDP_IQREQ_LIST_SIZE),
			RTE_CACHE_LINE_SIZE,
			rte_socket_id());
	if (iq->req_list == NULL) {
		otx2_err("IQ[%d] req_list alloc failed", iq_no);
		goto iq_init_fail;
	}

	otx2_info("IQ[%d]: base: %p basedma: %lx count: %d",
		     iq_no, iq->base_addr, (unsigned long)iq->base_addr_dma,
		     iq->nb_desc);

	iq->sdp_dev = sdpvf;
	iq->q_no = iq_no;
	iq->fill_cnt = 0;
	iq->host_write_index = 0;
	iq->otx_read_index = 0;
	iq->flush_index = 0;

	/* Initialize the spinlock for this instruction queue */
	rte_spinlock_init(&iq->lock);
	rte_spinlock_init(&iq->post_lock);

	rte_atomic64_clear(&iq->iq_flush_running);

	sdpvf->io_qmask.iq |= (1ull << iq_no);

	/* Set 32B/64B mode for each input queue */
	if (conf->iq.instr_type == 64)
		sdpvf->io_qmask.iq64B |= (1ull << iq_no);

	iq->iqcmd_64B = (conf->iq.instr_type == 64);

	/* Set up IQ registers */
	sdpvf->fn_list.setup_iq_regs(sdpvf, iq_no);

	return 0;

iq_init_fail:
	return -ENOMEM;

}

int
sdp_setup_iqs(struct sdp_device *sdpvf, uint32_t iq_no)
{
	struct sdp_instr_queue *iq;

	iq = (struct sdp_instr_queue *)rte_zmalloc("sdp_IQ", sizeof(*iq),
						RTE_CACHE_LINE_SIZE);
	if (iq == NULL)
		return -ENOMEM;

	sdpvf->instr_queue[iq_no] = iq;

	if (sdp_init_instr_queue(sdpvf, iq_no)) {
		otx2_err("IQ init is failed");
		goto delete_IQ;
	}
	otx2_info("IQ[%d] is created.", sdpvf->num_iqs);

	sdpvf->num_iqs++;


	return 0;

delete_IQ:
	sdp_delete_iqs(sdpvf, iq_no);
	return -ENOMEM;
}

static void
sdp_droq_reset_indices(struct sdp_droq *droq)
{
	droq->read_idx  = 0;
	droq->write_idx = 0;
	droq->refill_idx = 0;
	droq->refill_count = 0;
	rte_atomic64_set(&droq->pkts_pending, 0);
}

static void
sdp_droq_destroy_ring_buffers(struct sdp_device *sdpvf,
				struct sdp_droq *droq)
{
	uint32_t idx;

	for (idx = 0; idx < droq->nb_desc; idx++) {
		if (droq->recv_buf_list[idx].buffer) {
			rte_mempool_put(sdpvf->enqdeq_mpool,
				droq->recv_buf_list[idx].buffer);

			droq->recv_buf_list[idx].buffer = NULL;
		}
	}

	sdp_droq_reset_indices(droq);
}

/* Free OQs resources */
int
sdp_delete_oqs(struct sdp_device *sdpvf, uint32_t oq_no)
{
	struct sdp_droq *droq;

	droq = sdpvf->droq[oq_no];
	if (droq == NULL) {
		otx2_err("Invalid droq[%d]", oq_no);
		return -ENOMEM;
	}

	sdp_droq_destroy_ring_buffers(sdpvf, droq);
	rte_free(droq->recv_buf_list);
	droq->recv_buf_list = NULL;

	if (droq->info_mz) {
		sdp_dmazone_free(droq->info_mz);
		droq->info_mz = NULL;
	}

	if (droq->desc_ring_mz) {
		sdp_dmazone_free(droq->desc_ring_mz);
		droq->desc_ring_mz = NULL;
	}

	memset(droq, 0, SDP_DROQ_SIZE);

	rte_free(sdpvf->droq[oq_no]);
	sdpvf->droq[oq_no] = NULL;

	sdpvf->num_oqs--;

	otx2_info("OQ[%d] is deleted", oq_no);
	return 0;
}

static int
sdp_droq_setup_ring_buffers(struct sdp_device *sdpvf,
		struct sdp_droq *droq)
{
	struct sdp_droq_desc *desc_ring = droq->desc_ring;
	uint32_t idx;
	void *buf;

	for (idx = 0; idx < droq->nb_desc; idx++) {
		if (rte_mempool_get(sdpvf->enqdeq_mpool, &buf) ||
		    (buf == NULL)) {
			otx2_err("OQ buffer alloc failed");
			droq->stats.rx_alloc_failure++;
			/* sdp_droq_destroy_ring_buffers(droq);*/
			return -ENOMEM;
		}

		droq->recv_buf_list[idx].buffer = buf;
		droq->info_list[idx].length = 0;

		/* Map ring buffers into memory */
		desc_ring[idx].info_ptr = (uint64_t)(droq->info_list_dma +
			(idx * SDP_DROQ_INFO_SIZE));

		desc_ring[idx].buffer_ptr = rte_mem_virt2iova(buf);
	}

	sdp_droq_reset_indices(droq);

	return 0;
}

static void *
sdp_alloc_info_buffer(struct sdp_device *sdpvf __rte_unused,
	struct sdp_droq *droq)
{
	droq->info_mz = rte_memzone_reserve_aligned("OQ_info_list",
				(droq->nb_desc * SDP_DROQ_INFO_SIZE),
				rte_socket_id(),
				RTE_MEMZONE_IOVA_CONTIG,
				RTE_CACHE_LINE_SIZE);

	if (droq->info_mz == NULL)
		return NULL;

	droq->info_list_dma = droq->info_mz->iova;
	droq->info_alloc_size = droq->info_mz->len;
	droq->info_base_addr = (size_t)droq->info_mz->addr;

	return droq->info_mz->addr;
}

/* OQ initialization */
static int
sdp_init_droq(struct sdp_device *sdpvf, uint32_t q_no)
{
	const struct sdp_config *conf = sdpvf->conf;
	uint32_t c_refill_threshold;
	uint32_t desc_ring_size;
	struct sdp_droq *droq;

	otx2_info("OQ[%d] Init start", q_no);

	droq = sdpvf->droq[q_no];
	droq->sdp_dev = sdpvf;
	droq->q_no = q_no;

	c_refill_threshold = conf->oq.refill_threshold;
	droq->nb_desc      = conf->num_oqdef_descs;
	droq->buffer_size  = conf->oqdef_buf_size;

	/* OQ desc_ring set up */
	desc_ring_size = droq->nb_desc * SDP_DROQ_DESC_SIZE;
	droq->desc_ring_mz = rte_memzone_reserve_aligned("sdp_oqmz",
						desc_ring_size,
						rte_socket_id(),
						RTE_MEMZONE_IOVA_CONTIG,
						RTE_CACHE_LINE_SIZE);

	if (droq->desc_ring_mz == NULL) {
		otx2_err("OQ:%d desc_ring allocation failed", q_no);
		goto init_droq_fail;
	}

	droq->desc_ring_dma = droq->desc_ring_mz->iova;
	droq->desc_ring = (struct sdp_droq_desc *)droq->desc_ring_mz->addr;

	otx2_sdp_dbg("OQ[%d]: desc_ring: virt: 0x%p, dma: %lx",
		    q_no, droq->desc_ring, (unsigned long)droq->desc_ring_dma);
	otx2_sdp_dbg("OQ[%d]: num_desc: %d", q_no, droq->nb_desc);


	/* OQ info_list set up */
	droq->info_list = sdp_alloc_info_buffer(sdpvf, droq);
	if (droq->info_list == NULL) {
		otx2_err("memory allocation failed for OQ[%d] info_list", q_no);
		goto init_droq_fail;
	}

	/* OQ buf_list set up */
	droq->recv_buf_list = rte_zmalloc_socket("recv_buf_list",
				(droq->nb_desc * SDP_DROQ_RECVBUF_SIZE),
				 RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (droq->recv_buf_list == NULL) {
		otx2_err("OQ recv_buf_list alloc failed");
		goto init_droq_fail;
	}

	if (sdp_droq_setup_ring_buffers(sdpvf, droq))
		goto init_droq_fail;

	droq->refill_threshold = c_refill_threshold;
	rte_spinlock_init(&droq->lock);


	/* Set up OQ registers */
	sdpvf->fn_list.setup_oq_regs(sdpvf, q_no);

	sdpvf->io_qmask.oq |= (1ull << q_no);

	return 0;

init_droq_fail:
	return -ENOMEM;
}

/* OQ configuration and setup */
int
sdp_setup_oqs(struct sdp_device *sdpvf, uint32_t oq_no)
{
	struct sdp_droq *droq;

	/* Allocate new droq. */
	droq = (struct sdp_droq *)rte_zmalloc("sdp_OQ",
				sizeof(*droq), RTE_CACHE_LINE_SIZE);
	if (droq == NULL) {
		otx2_err("Droq[%d] Creation Failed", oq_no);
		return -ENOMEM;
	}
	sdpvf->droq[oq_no] = droq;

	if (sdp_init_droq(sdpvf, oq_no)) {
		otx2_err("Droq[%d] Initialization failed", oq_no);
		goto delete_OQ;
	}
	otx2_info("OQ[%d] is created.", oq_no);

	sdpvf->num_oqs++;

	return 0;

delete_OQ:
	sdp_delete_oqs(sdpvf, oq_no);
	return -ENOMEM;
}

static inline void
sdp_iqreq_delete(struct sdp_device *sdpvf,
		struct sdp_instr_queue *iq, uint32_t idx)
{
	uint32_t reqtype;
	void *buf;

	buf     = iq->req_list[idx].buf;
	reqtype = iq->req_list[idx].reqtype;

	switch (reqtype) {
	case SDP_REQTYPE_NORESP:
		rte_mempool_put(sdpvf->enqdeq_mpool, buf);
		otx2_sdp_dbg("IQ buffer freed at idx[%d]", idx);
		break;

	case SDP_REQTYPE_NORESP_GATHER:
	case SDP_REQTYPE_NONE:
	default:
		otx2_info("This iqreq mode is not supported:%d", reqtype);

	}

	/* Reset the request list at this index */
	iq->req_list[idx].buf = NULL;
	iq->req_list[idx].reqtype = 0;
}

static inline void
sdp_iqreq_add(struct sdp_instr_queue *iq, void *buf,
		uint32_t reqtype)
{
	iq->req_list[iq->host_write_index].buf = buf;
	iq->req_list[iq->host_write_index].reqtype = reqtype;

	otx2_sdp_dbg("IQ buffer added at idx[%d]", iq->host_write_index);

}

static void
sdp_flush_iq(struct sdp_device *sdpvf,
		struct sdp_instr_queue *iq,
		uint32_t pending_thresh __rte_unused)
{
	uint32_t instr_processed = 0;

	rte_spinlock_lock(&iq->lock);

	iq->otx_read_index = sdpvf->fn_list.update_iq_read_idx(iq);
	while (iq->flush_index != iq->otx_read_index) {
		/* Free the IQ data buffer to the pool */
		sdp_iqreq_delete(sdpvf, iq, iq->flush_index);
		iq->flush_index =
			sdp_incr_index(iq->flush_index, 1, iq->nb_desc);

		instr_processed++;
	}

	iq->stats.instr_processed = instr_processed;
	rte_atomic64_sub(&iq->instr_pending, instr_processed);

	rte_spinlock_unlock(&iq->lock);
}

static inline void
sdp_ring_doorbell(struct sdp_device *sdpvf __rte_unused,
		struct sdp_instr_queue *iq)
{
	otx2_write64(iq->fill_cnt, iq->doorbell_reg);

	/* Make sure doorbell writes observed by HW */
	rte_io_wmb();
	iq->fill_cnt = 0;

}

static inline int
post_iqcmd(struct sdp_instr_queue *iq, uint8_t *iqcmd)
{
	uint8_t *iqptr, cmdsize;

	/* This ensures that the read index does not wrap around to
	 * the same position if queue gets full before OCTEON TX2 could
	 * fetch any instr.
	 */
	if (rte_atomic64_read(&iq->instr_pending) >=
			      (int32_t)(iq->nb_desc - 1)) {
		otx2_err("IQ is full, pending:%ld",
			 (long)rte_atomic64_read(&iq->instr_pending));

		return SDP_IQ_SEND_FAILED;
	}

	/* Copy cmd into iq */
	cmdsize = ((iq->iqcmd_64B) ? 64 : 32);
	iqptr   = iq->base_addr + (cmdsize * iq->host_write_index);

	rte_memcpy(iqptr, iqcmd, cmdsize);

	otx2_sdp_dbg("IQ cmd posted @ index:%d", iq->host_write_index);

	/* Increment the host write index */
	iq->host_write_index =
		sdp_incr_index(iq->host_write_index, 1, iq->nb_desc);

	iq->fill_cnt++;

	/* Flush the command into memory. We need to be sure the data
	 * is in memory before indicating that the instruction is
	 * pending.
	 */
	rte_smp_wmb();
	rte_atomic64_inc(&iq->instr_pending);

	/* SDP_IQ_SEND_SUCCESS */
	return 0;
}


static int
sdp_send_data(struct sdp_device *sdpvf,
	      struct sdp_instr_queue *iq, void *cmd)
{
	uint32_t ret;

	/* Lock this IQ command queue before posting instruction */
	rte_spinlock_lock(&iq->post_lock);

	/* Submit IQ command */
	ret = post_iqcmd(iq, cmd);

	if (ret == SDP_IQ_SEND_SUCCESS) {
		sdp_ring_doorbell(sdpvf, iq);

		iq->stats.instr_posted++;
		otx2_sdp_dbg("Instr submit success posted: %ld\n",
			     (long)iq->stats.instr_posted);

	} else {
		iq->stats.instr_dropped++;
		otx2_err("Instr submit failed, dropped: %ld\n",
			 (long)iq->stats.instr_dropped);

	}

	rte_spinlock_unlock(&iq->post_lock);

	return ret;
}


/* Enqueue requests/packets to SDP IQ queue.
 * returns number of requests enqueued successfully
 */
int
sdp_rawdev_enqueue(struct rte_rawdev *rawdev,
		   struct rte_rawdev_buf **buffers __rte_unused,
		   unsigned int count, rte_rawdev_obj_t context)
{
	struct sdp_instr_64B *iqcmd;
	struct sdp_instr_queue *iq;
	struct sdp_soft_instr *si;
	struct sdp_device *sdpvf;

	struct sdp_instr_ih ihx;

	sdpvf = (struct sdp_device *)rawdev->dev_private;
	si = (struct sdp_soft_instr *)context;

	iq = sdpvf->instr_queue[si->q_no];

	if ((count > 1) || (count < 1)) {
		otx2_err("This mode not supported: req[%d]", count);
		goto enq_fail;
	}

	memset(&ihx, 0, sizeof(struct sdp_instr_ih));

	iqcmd = &si->command;
	memset(iqcmd, 0, sizeof(struct sdp_instr_64B));

	iqcmd->dptr = (uint64_t)si->dptr;

	/* Populate SDP IH */
	ihx.pkind  = sdpvf->pkind;
	ihx.fsz    = si->ih.fsz + 8; /* 8B for NIX IH */
	ihx.gather = si->ih.gather;

	/* Direct data instruction */
	ihx.tlen   = si->ih.tlen + ihx.fsz;

	switch (ihx.gather) {
	case 0: /* Direct data instr */
		ihx.tlen = si->ih.tlen + ihx.fsz;
		break;

	default: /* Gather */
		switch (si->ih.gsz) {
		case 0: /* Direct gather instr */
			otx2_err("Direct Gather instr : not supported");
			goto enq_fail;

		default: /* Indirect gather instr */
			otx2_err("Indirect Gather instr : not supported");
			goto enq_fail;
		}
	}

	rte_memcpy(&iqcmd->ih, &ihx, sizeof(uint64_t));
	iqcmd->rptr = (uint64_t)si->rptr;
	rte_memcpy(&iqcmd->irh, &si->irh, sizeof(uint64_t));

	/* Swap FSZ(front data) here, to avoid swapping on OCTEON TX2 side */
	sdp_swap_8B_data(&iqcmd->rptr, 1);
	sdp_swap_8B_data(&iqcmd->irh, 1);

	otx2_sdp_dbg("After swapping");
	otx2_sdp_dbg("Word0 [dptr]: 0x%016lx", (unsigned long)iqcmd->dptr);
	otx2_sdp_dbg("Word1 [ihtx]: 0x%016lx", (unsigned long)iqcmd->ih);
	otx2_sdp_dbg("Word2 [rptr]: 0x%016lx", (unsigned long)iqcmd->rptr);
	otx2_sdp_dbg("Word3 [irh]: 0x%016lx", (unsigned long)iqcmd->irh);
	otx2_sdp_dbg("Word4 [exhdr[0]]: 0x%016lx",
			(unsigned long)iqcmd->exhdr[0]);

	sdp_iqreq_add(iq, si->dptr, si->reqtype);

	if (sdp_send_data(sdpvf, iq, iqcmd)) {
		otx2_err("Data send failed :");
		sdp_iqreq_delete(sdpvf, iq, iq->host_write_index);
		goto enq_fail;
	}

	if (rte_atomic64_read(&iq->instr_pending) >= 1)
		sdp_flush_iq(sdpvf, iq, 1 /*(iq->nb_desc / 2)*/);

	/* Return no# of instructions posted successfully. */
	return count;

enq_fail:
	return SDP_IQ_SEND_FAILED;
}

static uint32_t
sdp_droq_refill(struct sdp_device *sdpvf, struct sdp_droq *droq)
{
	struct sdp_droq_desc *desc_ring;
	uint32_t desc_refilled = 0;
	void *buf = NULL;

	desc_ring = droq->desc_ring;

	while (droq->refill_count && (desc_refilled < droq->nb_desc)) {
		/* If a valid buffer exists (happens if there is no dispatch),
		 * reuse the buffer, else allocate.
		 */
		if (droq->recv_buf_list[droq->refill_idx].buffer != NULL)
			break;

		if (rte_mempool_get(sdpvf->enqdeq_mpool, &buf) ||
		    (buf == NULL)) {
			/* If a buffer could not be allocated, no point in
			 * continuing
			 */
			droq->stats.rx_alloc_failure++;
			break;
		}

		droq->recv_buf_list[droq->refill_idx].buffer = buf;
		desc_ring[droq->refill_idx].buffer_ptr = rte_mem_virt2iova(buf);

		/* Reset any previous values in the length field. */
		droq->info_list[droq->refill_idx].length = 0;

		droq->refill_idx = sdp_incr_index(droq->refill_idx, 1,
				droq->nb_desc);

		desc_refilled++;
		droq->refill_count--;

	}

	return desc_refilled;
}

static int
sdp_droq_read_packet(struct sdp_device *sdpvf __rte_unused,
		     struct sdp_droq *droq,
		     struct sdp_droq_pkt *droq_pkt)
{
	struct sdp_droq_info *info;
	uint32_t total_len = 0;
	uint32_t pkt_len = 0;

	info = &droq->info_list[droq->read_idx];
	sdp_swap_8B_data((uint64_t *)&info->length, 1);
	if (!info->length) {
		otx2_err("OQ info_list->length[%ld]", (long)info->length);
		goto oq_read_fail;
	}

	/* Deduce the actual data size */
	info->length -= SDP_RH_SIZE;
	total_len += (uint32_t)info->length;

	otx2_sdp_dbg("OQ: pkt_len[%ld], buffer_size %d",
			(long)info->length, droq->buffer_size);
	if (info->length > droq->buffer_size) {
		otx2_err("This mode is not supported: pkt_len > buffer_size");
		goto oq_read_fail;
	}

	if (info->length <= droq->buffer_size) {
		pkt_len = (uint32_t)info->length;
		droq_pkt->data = droq->recv_buf_list[droq->read_idx].buffer;
		droq_pkt->len  = pkt_len;

		droq->recv_buf_list[droq->read_idx].buffer = NULL;
		droq->read_idx = sdp_incr_index(droq->read_idx,	1,/* count */
						droq->nb_desc /* max rd idx */);
		droq->refill_count++;

	}

	info->length = 0;

	return SDP_OQ_RECV_SUCCESS;

oq_read_fail:
	return SDP_OQ_RECV_FAILED;
}

static inline uint32_t
sdp_check_droq_pkts(struct sdp_droq *droq, uint32_t burst_size)
{
	uint32_t min_pkts = 0;
	uint32_t new_pkts;
	uint32_t pkt_count;

	/* Latest available OQ packets */
	pkt_count = rte_read32(droq->pkts_sent_reg);

	/* Newly arrived packets */
	new_pkts = pkt_count - droq->last_pkt_count;
	otx2_sdp_dbg("Recvd [%d] new OQ pkts", new_pkts);

	min_pkts = (new_pkts > burst_size) ? burst_size : new_pkts;
	if (min_pkts) {
		rte_atomic64_add(&droq->pkts_pending, min_pkts);
		/* Back up the aggregated packet count so far */
		droq->last_pkt_count += min_pkts;
	}

	return min_pkts;
}

/* Check for response arrival from OCTEON TX2
 * returns number of requests completed
 */
int
sdp_rawdev_dequeue(struct rte_rawdev *rawdev,
		   struct rte_rawdev_buf **buffers, unsigned int count,
		   rte_rawdev_obj_t context __rte_unused)
{
	struct sdp_droq_pkt *oq_pkt;
	struct sdp_device *sdpvf;
	struct sdp_droq *droq;

	uint32_t q_no = 0, pkts;
	uint32_t new_pkts;
	uint32_t ret;

	sdpvf = (struct sdp_device *)rawdev->dev_private;

	droq = sdpvf->droq[q_no];
	if (!droq) {
		otx2_err("Invalid droq[%d]", q_no);
		goto droq_err;
	}

	/* Grab the lock */
	rte_spinlock_lock(&droq->lock);

	new_pkts = sdp_check_droq_pkts(droq, count);
	if (!new_pkts) {
		otx2_sdp_dbg("Zero new_pkts:%d", new_pkts);
		goto deq_fail; /* No pkts at this moment */
	}

	otx2_sdp_dbg("Received new_pkts = %d", new_pkts);

	for (pkts = 0; pkts < new_pkts; pkts++) {

		/* Push the received pkt to application */
		oq_pkt = (struct sdp_droq_pkt *)buffers[pkts];

		ret = sdp_droq_read_packet(sdpvf, droq, oq_pkt);
		if (ret) {
			otx2_err("DROQ read pakt failed.");
			goto deq_fail;
		}

		/* Stats */
		droq->stats.pkts_received++;
		droq->stats.bytes_received += oq_pkt->len;
	}

	/* Ack the h/w with no# of pkts read by Host */
	rte_write32(pkts, droq->pkts_sent_reg);
	rte_io_wmb();

	droq->last_pkt_count -= pkts;

	otx2_sdp_dbg("DROQ pkts[%d] pushed to application", pkts);

	/* Refill DROQ buffers */
	if (droq->refill_count >= 2 /* droq->refill_threshold */) {
		int desc_refilled = sdp_droq_refill(sdpvf, droq);

		/* Flush the droq descriptor data to memory to be sure
		 * that when we update the credits the data in memory is
		 * accurate.
		 */
		rte_write32(desc_refilled, droq->pkts_credit_reg);

		/* Ensure mmio write completes */
		rte_wmb();
		otx2_sdp_dbg("Refilled count = %d", desc_refilled);
	}

	/* Release the spin lock */
	rte_spinlock_unlock(&droq->lock);

	return pkts;

deq_fail:
	rte_spinlock_unlock(&droq->lock);

droq_err:
	return SDP_OQ_RECV_FAILED;
}
