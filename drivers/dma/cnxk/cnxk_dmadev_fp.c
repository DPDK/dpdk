/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2021 Marvell International Ltd.
 */

#include <eal_export.h>
#include <rte_vect.h>

#include "cnxk_dmadev.h"
#include <rte_event_dma_adapter.h>

#include <cn10k_eventdev.h>
#include <cnxk_eventdev.h>

static __plt_always_inline void
__dpi_cpy_scalar(uint64_t *src, uint64_t *dst, uint8_t n)
{
	uint8_t i;

	for (i = 0; i < n; i++)
		dst[i] = src[i];
}

#if defined(RTE_ARCH_ARM64)
static __plt_always_inline void
__dpi_cpy_vector(uint64_t *src, uint64_t *dst, uint8_t n)
{
	uint64x2_t vec;
	uint8_t i;

	for (i = 0; i < n; i += 2) {
		vec = vld1q_u64((const uint64_t *)&src[i]);
		vst1q_u64(&dst[i], vec);
	}
}

static __plt_always_inline void
__dpi_cpy_vector_sg(const struct rte_dma_sge *src, uint64_t *dst, uint16_t n)
{
	uint64x2_t mask = {0xFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL};
	uint64x2_t vec;
	uint8_t i;

	for (i = 0; i < n; i++) {
		vec = vld1q_u64((const uint64_t *)&src[i]);
		vec = vextq_u64(vec, vec, 1);
		vec = vandq_u64(vec, mask);
		vst1q_u64(dst, vec);
		dst += 2;
	}
}

static __plt_always_inline uint8_t
__dpi_cpy_vector_sg_lmt(const struct rte_dma_sge *src, uint64_t *dst, uint16_t n, uint16_t lmt)
{
	uint64x2_t mask = {0xFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL};
	uint64x2_t vec;
	uint8_t i;

	for (i = 0; i < n && lmt; i++) {
		vec = vld1q_u64((const uint64_t *)&src[i]);
		vec = vextq_u64(vec, vec, 1);
		vec = vandq_u64(vec, mask);
		vst1q_u64(dst, vec);
		dst += 2;
		lmt -= 2;
	}

	return i;
}
#else
static __plt_always_inline void
__dpi_cpy_scalar_sg(const struct rte_dma_sge *src, uint64_t *dst, uint16_t n)
{
	uint8_t i;

	for (i = 0; i < n; i++) {
		*dst++ = src[i].length;
		*dst++ = src[i].addr;
	}
}

static __plt_always_inline uint8_t
__dpi_cpy_scalar_sg_lmt(const struct rte_dma_sge *src, uint64_t *dst, uint16_t n, uint16_t lmt)
{
	uint8_t i;

	for (i = 0; i < n && lmt; i++) {
		*dst++ = src[i].length;
		*dst++ = src[i].addr;
		lmt -= 2;
	}

	return i;
}
#endif

static __plt_always_inline void
__dpi_cpy(uint64_t *src, uint64_t *dst, uint8_t n)
{
#if defined(RTE_ARCH_ARM64)
	__dpi_cpy_vector(src, dst, n);
#else
	__dpi_cpy_scalar(src, dst, n);
#endif
}

static __plt_always_inline void
__dpi_cpy_sg(const struct rte_dma_sge *src, uint64_t *dst, uint16_t n)
{
#if defined(RTE_ARCH_ARM64)
	__dpi_cpy_vector_sg(src, dst, n);
#else
	__dpi_cpy_scalar_sg(src, dst, n);
#endif
}

static __plt_always_inline uint8_t
__dpi_cpy_sg_lmt(const struct rte_dma_sge *src, uint64_t *dst, uint16_t n, uint16_t lmt)
{
#if defined(RTE_ARCH_ARM64)
	return __dpi_cpy_vector_sg_lmt(src, dst, n, lmt);
#else
	return __dpi_cpy_scalar_sg_lmt(src, dst, n, lmt);
#endif
}

static __plt_always_inline int
__dpi_queue_write_single(struct cnxk_dpi_vf_s *dpi, uint64_t *cmd)
{
	uint64_t *ptr = dpi->chunk_base;

	/* Check if command fits in the current chunk. */
	if (dpi->chunk_head + CNXK_DPI_DW_PER_SINGLE_CMD < dpi->chunk_size_m1) {
		ptr += dpi->chunk_head;

		__dpi_cpy_scalar(cmd, ptr, CNXK_DPI_DW_PER_SINGLE_CMD);
		dpi->chunk_head += CNXK_DPI_DW_PER_SINGLE_CMD;
	} else {
		uint64_t *new_buff = NULL;
		int count;

		if (rte_mempool_get(dpi->chunk_pool, (void **)&new_buff) < 0) {
			plt_dpi_dbg("Failed to alloc next buffer from NPA");
			return -ENOSPC;
		}

		/*
		 * Figure out how many cmd words will fit in the current chunk
		 * and copy them.
		 */
		count = dpi->chunk_size_m1 - dpi->chunk_head;
		ptr += dpi->chunk_head;

		__dpi_cpy_scalar(cmd, ptr, count);

		ptr += count;
		*ptr = (uint64_t)new_buff;
		ptr = new_buff;

		/* Copy the remaining cmd words to new chunk. */
		__dpi_cpy_scalar(cmd + count, ptr, CNXK_DPI_DW_PER_SINGLE_CMD - count);

		dpi->chunk_base = new_buff;
		dpi->chunk_head = CNXK_DPI_DW_PER_SINGLE_CMD - count;
	}

	return 0;
}

static __plt_always_inline int
__dpi_queue_write_sg(struct cnxk_dpi_vf_s *dpi, uint64_t *hdr, const struct rte_dma_sge *src,
		     const struct rte_dma_sge *dst, uint16_t nb_src, uint16_t nb_dst)
{
	uint8_t cmd_len = CNXK_DPI_CMD_LEN(nb_src, nb_dst);
	uint64_t *ptr = dpi->chunk_base;

	/* Check if command fits in the current chunk. */
	if (dpi->chunk_head + cmd_len < dpi->chunk_size_m1) {
		ptr += dpi->chunk_head;

		__dpi_cpy(hdr, ptr, CNXK_DPI_HDR_LEN);
		ptr += CNXK_DPI_HDR_LEN;
		__dpi_cpy_sg(src, ptr, nb_src);
		ptr += (nb_src << 1);
		__dpi_cpy_sg(dst, ptr, nb_dst);

		dpi->chunk_head += cmd_len;
	} else {
		uint64_t *new_buff = NULL, *buf;
		uint16_t count;

		if (rte_mempool_get(dpi->chunk_pool, (void **)&new_buff) < 0) {
			plt_dpi_dbg("Failed to alloc next buffer from NPA");
			return -ENOSPC;
		}

		/*
		 * Figure out how many cmd words will fit in the current chunk
		 * and copy them, copy the rest to the new buffer.
		 */
		count = dpi->chunk_size_m1 - dpi->chunk_head;
		ptr += dpi->chunk_head;
		buf = new_buff;
		if (count <= 4) {
			__dpi_cpy(hdr, ptr, count);
			ptr += count;
			__dpi_cpy(&hdr[count], buf, 4);
			buf += (4 - count);
		} else {
			uint8_t i;

			__dpi_cpy(hdr, ptr, 4);
			ptr += 4;
			count -= 4;

			i = __dpi_cpy_sg_lmt(src, ptr, nb_src, count);
			src += i;
			nb_src -= i;
			count -= (i << 1);
			ptr += (i << 1);

			i = __dpi_cpy_sg_lmt(dst, ptr, nb_dst, count);
			dst += i;
			nb_dst -= i;
			ptr += (i << 1);
		}
		*ptr = (uint64_t)new_buff;

		__dpi_cpy_sg(src, buf, nb_src);
		buf += (nb_src << 1);

		__dpi_cpy_sg(dst, buf, nb_dst);
		buf += (nb_dst << 1);

		dpi->chunk_base = new_buff;
		dpi->chunk_head = buf - new_buff;
	}

	return 0;
}

int
cnxk_dmadev_copy(void *dev_private, uint16_t vchan, rte_iova_t src, rte_iova_t dst, uint32_t length,
		 uint64_t flags)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	uint64_t cmd[CNXK_DPI_DW_PER_SINGLE_CMD];
	const uint16_t max_cnt = dpi_conf->c_desc.max_cnt;
	uint8_t *comp_ptr;
	int rc;

	if (unlikely(((dpi_conf->c_desc.tail + 1) & max_cnt) == (dpi_conf->c_desc.head & max_cnt)))
		return -ENOSPC;

	comp_ptr = &dpi_conf->c_desc
			    .compl_ptr[(dpi_conf->c_desc.tail & max_cnt) * CNXK_DPI_COMPL_OFFSET];
	dpi_conf->c_desc.tail++;

	cmd[0] = (1UL << 54) | (1UL << 48);
	cmd[1] = dpi_conf->cmd.u | ((flags & RTE_DMA_OP_FLAG_AUTO_FREE) << 37);
	cmd[2] = (uint64_t)comp_ptr;
	cmd[4] = length;
	cmd[6] = length;

	/*
	 * For inbound case, src pointers are last pointers.
	 * For all other cases, src pointers are first pointers.
	 */
	if (((dpi_conf->cmd.u >> 48) & DPI_HDR_XTYPE_MASK) == DPI_XTYPE_INBOUND) {
		cmd[5] = dst;
		cmd[7] = src;
	} else {
		cmd[5] = src;
		cmd[7] = dst;
	}

	rc = __dpi_queue_write_single(dpivf, cmd);
	if (unlikely(rc)) {
		dpi_conf->c_desc.tail--;
		return rc;
	}

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		plt_write64(dpivf->total_pnum_words + CNXK_DPI_DW_PER_SINGLE_CMD,
			    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
		dpivf->total_pnum_words = 0;
	} else {
		dpivf->total_pnum_words += CNXK_DPI_DW_PER_SINGLE_CMD;
	}

	dpi_conf->stats.submitted += 1;

	return dpi_conf->desc_idx++;
}

int
cnxk_dmadev_copy_sg(void *dev_private, uint16_t vchan, const struct rte_dma_sge *src,
		    const struct rte_dma_sge *dst, uint16_t nb_src, uint16_t nb_dst, uint64_t flags)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	const uint16_t max_cnt = dpi_conf->c_desc.max_cnt;
	const struct rte_dma_sge *fptr, *lptr;
	uint8_t *comp_ptr;
	uint64_t hdr[4];
	int rc;

	if (unlikely(((dpi_conf->c_desc.tail + 1) & max_cnt) == (dpi_conf->c_desc.head & max_cnt)))
		return -ENOSPC;

	comp_ptr = &dpi_conf->c_desc
			    .compl_ptr[(dpi_conf->c_desc.tail & max_cnt) * CNXK_DPI_COMPL_OFFSET];
	dpi_conf->c_desc.tail++;

	hdr[1] = dpi_conf->cmd.u | ((flags & RTE_DMA_OP_FLAG_AUTO_FREE) << 37);
	hdr[2] = (uint64_t)comp_ptr;

	/*
	 * For inbound case, src pointers are last pointers.
	 * For all other cases, src pointers are first pointers.
	 */
	if (((dpi_conf->cmd.u >> 48) & DPI_HDR_XTYPE_MASK) == DPI_XTYPE_INBOUND) {
		fptr = dst;
		lptr = src;
		RTE_SWAP(nb_src, nb_dst);
	} else {
		fptr = src;
		lptr = dst;
	}
	hdr[0] = ((uint64_t)nb_dst << 54) | (uint64_t)nb_src << 48;

	rc = __dpi_queue_write_sg(dpivf, hdr, fptr, lptr, nb_src, nb_dst);
	if (unlikely(rc)) {
		dpi_conf->c_desc.tail--;
		return rc;
	}

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		plt_write64(dpivf->total_pnum_words + CNXK_DPI_CMD_LEN(nb_src, nb_dst),
			    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
		dpivf->total_pnum_words = 0;
	} else {
		dpivf->total_pnum_words += CNXK_DPI_CMD_LEN(nb_src, nb_dst);
	}

	dpi_conf->stats.submitted += 1;

	return dpi_conf->desc_idx++;
}

int
cn10k_dmadev_copy(void *dev_private, uint16_t vchan, rte_iova_t src, rte_iova_t dst,
		  uint32_t length, uint64_t flags)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	const uint16_t max_cnt = dpi_conf->c_desc.max_cnt;
	uint64_t cmd[CNXK_DPI_DW_PER_SINGLE_CMD];
	uint8_t *comp_ptr;
	int rc;

	if (unlikely(((dpi_conf->c_desc.tail + 1) & max_cnt) == (dpi_conf->c_desc.head & max_cnt)))
		return -ENOSPC;

	comp_ptr = &dpi_conf->c_desc
			    .compl_ptr[(dpi_conf->c_desc.tail & max_cnt) * CNXK_DPI_COMPL_OFFSET];
	dpi_conf->c_desc.tail++;

	cmd[0] = dpi_conf->cmd.u | (1U << 6) | 1U;
	cmd[1] = (uint64_t)comp_ptr;
	cmd[2] = (1UL << 47) | ((flags & RTE_DMA_OP_FLAG_AUTO_FREE) << 43);
	cmd[4] = length;
	cmd[5] = src;
	cmd[6] = length;
	cmd[7] = dst;

	rc = __dpi_queue_write_single(dpivf, cmd);
	if (unlikely(rc)) {
		dpi_conf->c_desc.tail--;
		return rc;
	}

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		plt_write64(dpivf->total_pnum_words + CNXK_DPI_DW_PER_SINGLE_CMD,
			    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
		dpivf->total_pnum_words = 0;
	} else {
		dpivf->total_pnum_words += CNXK_DPI_DW_PER_SINGLE_CMD;
	}

	dpi_conf->stats.submitted += 1;

	return dpi_conf->desc_idx++;
}

int
cn10k_dmadev_copy_sg(void *dev_private, uint16_t vchan, const struct rte_dma_sge *src,
		     const struct rte_dma_sge *dst, uint16_t nb_src, uint16_t nb_dst,
		     uint64_t flags)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	const uint16_t max_cnt = dpi_conf->c_desc.max_cnt;
	uint8_t *comp_ptr;
	uint64_t hdr[4];
	int rc;

	if (unlikely(((dpi_conf->c_desc.tail + 1) & max_cnt) == (dpi_conf->c_desc.head & max_cnt)))
		return -ENOSPC;

	comp_ptr = &dpi_conf->c_desc
			    .compl_ptr[(dpi_conf->c_desc.tail & max_cnt) * CNXK_DPI_COMPL_OFFSET];
	dpi_conf->c_desc.tail++;

	hdr[0] = dpi_conf->cmd.u | (nb_dst << 6) | nb_src;
	hdr[1] = (uint64_t)comp_ptr;
	hdr[2] = (1UL << 47) | ((flags & RTE_DMA_OP_FLAG_AUTO_FREE) << 43);

	rc = __dpi_queue_write_sg(dpivf, hdr, src, dst, nb_src, nb_dst);
	if (unlikely(rc)) {
		dpi_conf->c_desc.tail--;
		return rc;
	}

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		plt_write64(dpivf->total_pnum_words + CNXK_DPI_CMD_LEN(nb_src, nb_dst),
			    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
		dpivf->total_pnum_words = 0;
	} else {
		dpivf->total_pnum_words += CNXK_DPI_CMD_LEN(nb_src, nb_dst);
	}

	dpi_conf->stats.submitted += 1;

	return dpi_conf->desc_idx++;
}

static inline uint64_t
cnxk_dma_adapter_format_event(uint64_t event)
{
	uint64_t w0;
	w0 = (event & 0xFFC000000000) >> 6 |
	     (event & 0xFFFFFFF) | RTE_EVENT_TYPE_DMADEV << 28;

	return w0;
}

RTE_EXPORT_INTERNAL_SYMBOL(cn10k_dma_adapter_enqueue)
uint16_t
cn10k_dma_adapter_enqueue(void *ws, struct rte_event ev[], uint16_t nb_events)
{
	const struct rte_dma_sge *src, *dst;
	struct rte_dma_op *op;
	struct cnxk_dpi_conf *dpi_conf;
	struct cnxk_dpi_vf_s *dpivf;
	struct cn10k_sso_hws *work;
	uint16_t nb_src, nb_dst;
	rte_mcslock_t mcs_lock_me;
	uint64_t hdr[4];
	uint16_t count;
	int rc;

	work = (struct cn10k_sso_hws *)ws;

	for (count = 0; count < nb_events; count++) {
		op = ev[count].event_ptr;
		dpivf = rte_dma_fp_objs[op->dma_dev_id].dev_private;
		dpi_conf = &dpivf->conf[op->vchan];

		nb_src = op->nb_src & CNXK_DPI_MAX_POINTER;
		nb_dst = op->nb_dst & CNXK_DPI_MAX_POINTER;

		hdr[0] = dpi_conf->cmd.u | ((uint64_t)DPI_HDR_PT_WQP << 54);
		hdr[0] |= (nb_dst << 6) | nb_src;
		hdr[1] = (uint64_t)op;
		hdr[2] = cnxk_dma_adapter_format_event(ev[count].event);

		src = &op->src_dst_seg[0];
		dst = &op->src_dst_seg[op->nb_src];

		if (CNXK_TAG_IS_HEAD(work->gw_rdata) ||
		    ((CNXK_TT_FROM_TAG(work->gw_rdata) == SSO_TT_ORDERED) &&
		     (ev[count].sched_type & DPI_HDR_TT_MASK) == RTE_SCHED_TYPE_ORDERED))
			roc_sso_hws_head_wait(work->base);

		rte_mcslock_lock(&dpivf->mcs_lock, &mcs_lock_me);
		rc = __dpi_queue_write_sg(dpivf, hdr, src, dst, nb_src, nb_dst);
		if (unlikely(rc)) {
			rte_mcslock_unlock(&dpivf->mcs_lock, &mcs_lock_me);
			return rc;
		}

		if (op->flags & RTE_DMA_OP_FLAG_SUBMIT) {
			rte_wmb();
			plt_write64(dpivf->total_pnum_words + CNXK_DPI_CMD_LEN(nb_src, nb_dst),
				    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
			dpivf->total_pnum_words = 0;
		} else {
			dpivf->total_pnum_words += CNXK_DPI_CMD_LEN(nb_src, nb_dst);
		}
		dpi_conf->stats.submitted += 1;
		rte_mcslock_unlock(&dpivf->mcs_lock, &mcs_lock_me);
	}

	return count;
}

RTE_EXPORT_INTERNAL_SYMBOL(cn9k_dma_adapter_dual_enqueue)
uint16_t
cn9k_dma_adapter_dual_enqueue(void *ws, struct rte_event ev[], uint16_t nb_events)
{
	const struct rte_dma_sge *fptr, *lptr;
	struct rte_dma_op *op;
	struct cn9k_sso_hws_dual *work;
	struct cnxk_dpi_conf *dpi_conf;
	struct cnxk_dpi_vf_s *dpivf;
	struct rte_event *rsp_info;
	uint16_t nb_src, nb_dst;
	rte_mcslock_t mcs_lock_me;
	uint64_t hdr[4];
	uint16_t count;
	int rc;

	work = (struct cn9k_sso_hws_dual *)ws;

	for (count = 0; count < nb_events; count++) {
		op = ev[count].event_ptr;
		rsp_info = (struct rte_event *)((uint8_t *)op +
						sizeof(struct rte_dma_op));
		dpivf = rte_dma_fp_objs[op->dma_dev_id].dev_private;
		dpi_conf = &dpivf->conf[op->vchan];

		hdr[1] = dpi_conf->cmd.u | ((uint64_t)DPI_HDR_PT_WQP << 36);
		hdr[2] = (uint64_t)op;

		nb_src = op->nb_src & CNXK_DPI_MAX_POINTER;
		nb_dst = op->nb_dst & CNXK_DPI_MAX_POINTER;
		/*
		 * For inbound case, src pointers are last pointers.
		 * For all other cases, src pointers are first pointers.
		 */
		if (((dpi_conf->cmd.u >> 48) & DPI_HDR_XTYPE_MASK) == DPI_XTYPE_INBOUND) {
			fptr = &op->src_dst_seg[nb_src];
			lptr = &op->src_dst_seg[0];
			RTE_SWAP(nb_src, nb_dst);
		} else {
			fptr = &op->src_dst_seg[0];
			lptr = &op->src_dst_seg[nb_src];
		}

		hdr[0] = ((uint64_t)nb_dst << 54) | (uint64_t)nb_src << 48;
		hdr[0] |= cnxk_dma_adapter_format_event(rsp_info->event);

		if ((rsp_info->sched_type & DPI_HDR_TT_MASK) == RTE_SCHED_TYPE_ORDERED)
			roc_sso_hws_head_wait(work->base[!work->vws]);

		rte_mcslock_lock(&dpivf->mcs_lock, &mcs_lock_me);
		rc = __dpi_queue_write_sg(dpivf, hdr, fptr, lptr, nb_src, nb_dst);
		if (unlikely(rc)) {
			rte_mcslock_unlock(&dpivf->mcs_lock, &mcs_lock_me);
			return rc;
		}

		if (op->flags & RTE_DMA_OP_FLAG_SUBMIT) {
			rte_wmb();
			plt_write64(dpivf->total_pnum_words + CNXK_DPI_CMD_LEN(nb_src, nb_dst),
				    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
			dpivf->total_pnum_words = 0;
		} else {
			dpivf->total_pnum_words += CNXK_DPI_CMD_LEN(nb_src, nb_dst);
		}
		dpi_conf->stats.submitted += 1;
		rte_mcslock_unlock(&dpivf->mcs_lock, &mcs_lock_me);
	}

	return count;
}

RTE_EXPORT_INTERNAL_SYMBOL(cn9k_dma_adapter_enqueue)
uint16_t
cn9k_dma_adapter_enqueue(void *ws, struct rte_event ev[], uint16_t nb_events)
{
	const struct rte_dma_sge *fptr, *lptr;
	struct rte_dma_op *op;
	struct cnxk_dpi_conf *dpi_conf;
	struct cnxk_dpi_vf_s *dpivf;
	struct cn9k_sso_hws *work;
	uint16_t nb_src, nb_dst;
	rte_mcslock_t mcs_lock_me;
	uint64_t hdr[4];
	uint16_t count;
	int rc;

	work = (struct cn9k_sso_hws *)ws;

	for (count = 0; count < nb_events; count++) {
		op = ev[count].event_ptr;
		dpivf = rte_dma_fp_objs[op->dma_dev_id].dev_private;
		dpi_conf = &dpivf->conf[op->vchan];

		hdr[1] = dpi_conf->cmd.u | ((uint64_t)DPI_HDR_PT_WQP << 36);
		hdr[2] = (uint64_t)op;

		nb_src = op->nb_src & CNXK_DPI_MAX_POINTER;
		nb_dst = op->nb_dst & CNXK_DPI_MAX_POINTER;
		/*
		 * For inbound case, src pointers are last pointers.
		 * For all other cases, src pointers are first pointers.
		 */
		if (((dpi_conf->cmd.u >> 48) & DPI_HDR_XTYPE_MASK) == DPI_XTYPE_INBOUND) {
			fptr = &op->src_dst_seg[nb_src];
			lptr = &op->src_dst_seg[0];
			RTE_SWAP(nb_src, nb_dst);
		} else {
			fptr = &op->src_dst_seg[0];
			lptr = &op->src_dst_seg[nb_src];
		}

		hdr[0] = ((uint64_t)nb_dst << 54) | (uint64_t)nb_src << 48;
		hdr[0] |= cnxk_dma_adapter_format_event(ev[count].event);

		if ((ev[count].sched_type & DPI_HDR_TT_MASK) == RTE_SCHED_TYPE_ORDERED)
			roc_sso_hws_head_wait(work->base);

		rte_mcslock_lock(&dpivf->mcs_lock, &mcs_lock_me);
		rc = __dpi_queue_write_sg(dpivf, hdr, fptr, lptr, nb_src, nb_dst);
		if (unlikely(rc)) {
			rte_mcslock_unlock(&dpivf->mcs_lock, &mcs_lock_me);
			return rc;
		}

		if (op->flags & RTE_DMA_OP_FLAG_SUBMIT) {
			rte_wmb();
			plt_write64(dpivf->total_pnum_words + CNXK_DPI_CMD_LEN(nb_src, nb_dst),
				    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
			dpivf->total_pnum_words = 0;
		} else {
			dpivf->total_pnum_words += CNXK_DPI_CMD_LEN(nb_src, nb_dst);
		}
		dpi_conf->stats.submitted += 1;
		rte_mcslock_unlock(&dpivf->mcs_lock, &mcs_lock_me);
	}

	return count;
}

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_dma_adapter_dequeue)
uintptr_t
cnxk_dma_adapter_dequeue(uintptr_t get_work1)
{
	struct rte_dma_op *op;
	struct cnxk_dpi_conf *dpi_conf;
	struct cnxk_dpi_vf_s *dpivf;

	op = (struct rte_dma_op *)get_work1;
	dpivf = rte_dma_fp_objs[op->dma_dev_id].dev_private;
	dpi_conf = &dpivf->conf[op->vchan];

	if (rte_atomic_load_explicit((RTE_ATOMIC(uint64_t) *)&op->impl_opaque[0],
				     rte_memory_order_relaxed) != 0)
		rte_atomic_fetch_add_explicit((RTE_ATOMIC(uint64_t) *)&dpi_conf->stats.errors, 1,
					      rte_memory_order_relaxed);

	/* Take into account errors also. This is similar to
	 * cnxk_dmadev_completed_status().
	 */
	rte_atomic_fetch_add_explicit((RTE_ATOMIC(uint64_t) *)&dpi_conf->stats.completed, 1,
				      rte_memory_order_relaxed);

	return (uintptr_t)op;
}

uint16_t
cnxk_dma_ops_enqueue(void *dev_private, uint16_t vchan, struct rte_dma_op **ops, uint16_t nb_ops)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	const uint16_t max_cnt = dpi_conf->c_desc.max_cnt;
	const struct rte_dma_sge *fptr, *lptr;
	uint16_t src, dst, nwords = 0;
	struct rte_dma_op *op;
	uint16_t space, i;
	uint8_t *comp_ptr;
	uint64_t hdr[4];
	int rc;

	space = ((dpi_conf->c_desc.max_cnt + dpi_conf->c_desc.head - dpi_conf->c_desc.tail) &
		 max_cnt);
	space = RTE_MIN(space, nb_ops);

	for (i = 0; i < space; i++) {
		op = ops[i];
		comp_ptr = &dpi_conf->c_desc.compl_ptr[(dpi_conf->c_desc.tail & max_cnt) *
						       CNXK_DPI_COMPL_OFFSET];
		dpi_conf->c_desc.ops[dpi_conf->c_desc.tail & max_cnt] = op;
		dpi_conf->c_desc.tail++;

		hdr[1] = dpi_conf->cmd.u | ((op->flags & RTE_DMA_OP_FLAG_AUTO_FREE) << 37);
		hdr[2] = (uint64_t)comp_ptr;

		src = op->nb_src;
		dst = op->nb_dst;
		/*
		 * For inbound case, src pointers are last pointers.
		 * For all other cases, src pointers are first pointers.
		 */
		if (((dpi_conf->cmd.u >> 48) & DPI_HDR_XTYPE_MASK) == DPI_XTYPE_INBOUND) {
			fptr = &op->src_dst_seg[src];
			lptr = &op->src_dst_seg[0];
			RTE_SWAP(src, dst);
		} else {
			fptr = &op->src_dst_seg[0];
			lptr = &op->src_dst_seg[src];
		}
		hdr[0] = ((uint64_t)dst << 54) | (uint64_t)src << 48;

		rc = __dpi_queue_write_sg(dpivf, hdr, fptr, lptr, src, dst);
		if (rc) {
			dpi_conf->c_desc.tail--;
			goto done;
		}
		nwords += CNXK_DPI_CMD_LEN(src, dst);
	}

done:
	if (nwords) {
		rte_wmb();
		plt_write64(nwords, dpivf->rdpi.rbase + DPI_VDMA_DBELL);
		dpi_conf->stats.submitted += i;
	}

	return i;
}

uint16_t
cn10k_dma_ops_enqueue(void *dev_private, uint16_t vchan, struct rte_dma_op **ops, uint16_t nb_ops)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	const uint16_t max_cnt = dpi_conf->c_desc.max_cnt;
	uint16_t space, i, nwords = 0;
	struct rte_dma_op *op;
	uint16_t src, dst;
	uint8_t *comp_ptr;
	uint64_t hdr[4];
	int rc;

	space = (max_cnt + dpi_conf->c_desc.head - dpi_conf->c_desc.tail) & max_cnt;
	space = RTE_MIN(space, nb_ops);

	for (i = 0; i < space; i++) {
		op = ops[i];
		src = op->nb_src;
		dst = op->nb_dst;

		comp_ptr = &dpi_conf->c_desc.compl_ptr[(dpi_conf->c_desc.tail & max_cnt) *
						       CNXK_DPI_COMPL_OFFSET];
		dpi_conf->c_desc.ops[dpi_conf->c_desc.tail & max_cnt] = op;
		dpi_conf->c_desc.tail++;

		hdr[0] = dpi_conf->cmd.u | (dst << 6) | src;
		hdr[1] = (uint64_t)comp_ptr;
		hdr[2] = (1UL << 47) | ((op->flags & RTE_DMA_OP_FLAG_AUTO_FREE) << 43);

		rc = __dpi_queue_write_sg(dpivf, hdr, &op->src_dst_seg[0], &op->src_dst_seg[src],
					  src, dst);
		if (rc) {
			dpi_conf->c_desc.tail--;
			goto done;
		}
		nwords += CNXK_DPI_CMD_LEN(src, dst);
	}

done:
	if (nwords) {
		rte_wmb();
		plt_write64(nwords, dpivf->rdpi.rbase + DPI_VDMA_DBELL);
		dpi_conf->stats.submitted += i;
	}

	return i;
}

uint16_t
cnxk_dma_ops_dequeue(void *dev_private, uint16_t vchan, struct rte_dma_op **ops, uint16_t nb_ops)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	struct cnxk_dpi_cdesc_data_s *c_desc = &dpi_conf->c_desc;
	const uint16_t max_cnt = c_desc->max_cnt;
	struct rte_dma_op *op;
	uint16_t space, cnt;
	uint8_t status;

	space = (c_desc->tail - c_desc->head) & max_cnt;
	space = RTE_MIN(nb_ops, space);

	for (cnt = 0; cnt < space; cnt++) {
		status = c_desc->compl_ptr[(c_desc->head & max_cnt) * CNXK_DPI_COMPL_OFFSET];
		op = c_desc->ops[c_desc->head & max_cnt];
		if (status) {
			if (status == CNXK_DPI_REQ_CDATA)
				break;
			dpi_conf->stats.errors++;
		}
		op->status = status;
		ops[cnt] = op;
		c_desc->compl_ptr[(c_desc->head & max_cnt) * CNXK_DPI_COMPL_OFFSET] =
			CNXK_DPI_REQ_CDATA;
		c_desc->head++;
	}

	dpi_conf->stats.completed += cnt;

	return cnt;
}
