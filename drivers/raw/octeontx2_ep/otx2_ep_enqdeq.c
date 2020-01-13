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

static int
sdp_droq_setup_ring_buffers(struct sdp_device *sdpvf,
		struct sdp_droq *droq)
{
	struct sdp_droq_desc *desc_ring = droq->desc_ring;
	uint32_t idx;
	void *buf;

	for (idx = 0; idx < droq->nb_desc; idx++) {
		rte_mempool_get(sdpvf->enqdeq_mpool, &buf);
		if (buf == NULL) {
			otx2_err("OQ buffer alloc failed");
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
	return -ENOMEM;
}
