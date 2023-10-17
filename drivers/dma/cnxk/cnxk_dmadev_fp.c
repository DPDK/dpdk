/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2021 Marvell International Ltd.
 */

#include <rte_vect.h>

#include "cnxk_dmadev.h"

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
	struct cnxk_dpi_compl_s *comp_ptr;
	int rc;

	if (unlikely(((dpi_conf->c_desc.tail + 1) & dpi_conf->c_desc.max_cnt) ==
		     dpi_conf->c_desc.head))
		return -ENOSPC;

	comp_ptr = dpi_conf->c_desc.compl_ptr[dpi_conf->c_desc.tail];
	CNXK_DPI_STRM_INC(dpi_conf->c_desc, tail);

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
		CNXK_DPI_STRM_DEC(dpi_conf->c_desc, tail);
		return rc;
	}

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		plt_write64(dpi_conf->pnum_words + CNXK_DPI_DW_PER_SINGLE_CMD,
			    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
		dpi_conf->stats.submitted += dpi_conf->pending + 1;
		dpi_conf->pnum_words = 0;
		dpi_conf->pending = 0;
	} else {
		dpi_conf->pnum_words += CNXK_DPI_DW_PER_SINGLE_CMD;
		dpi_conf->pending++;
	}

	return dpi_conf->desc_idx++;
}

int
cnxk_dmadev_copy_sg(void *dev_private, uint16_t vchan, const struct rte_dma_sge *src,
		    const struct rte_dma_sge *dst, uint16_t nb_src, uint16_t nb_dst, uint64_t flags)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	const struct rte_dma_sge *fptr, *lptr;
	struct cnxk_dpi_compl_s *comp_ptr;
	uint64_t hdr[4];
	int rc;

	if (unlikely(((dpi_conf->c_desc.tail + 1) & dpi_conf->c_desc.max_cnt) ==
		     dpi_conf->c_desc.head))
		return -ENOSPC;

	comp_ptr = dpi_conf->c_desc.compl_ptr[dpi_conf->c_desc.tail];
	CNXK_DPI_STRM_INC(dpi_conf->c_desc, tail);

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
		CNXK_DPI_STRM_DEC(dpi_conf->c_desc, tail);
		return rc;
	}

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		plt_write64(dpi_conf->pnum_words + CNXK_DPI_CMD_LEN(nb_src, nb_dst),
			    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
		dpi_conf->stats.submitted += dpi_conf->pending + 1;
		dpi_conf->pnum_words = 0;
		dpi_conf->pending = 0;
	} else {
		dpi_conf->pnum_words += CNXK_DPI_CMD_LEN(nb_src, nb_dst);
		dpi_conf->pending++;
	}

	return dpi_conf->desc_idx++;
}

int
cn10k_dmadev_copy(void *dev_private, uint16_t vchan, rte_iova_t src, rte_iova_t dst,
		  uint32_t length, uint64_t flags)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	uint64_t cmd[CNXK_DPI_DW_PER_SINGLE_CMD];
	struct cnxk_dpi_compl_s *comp_ptr;
	int rc;

	if (unlikely(((dpi_conf->c_desc.tail + 1) & dpi_conf->c_desc.max_cnt) ==
		     dpi_conf->c_desc.head))
		return -ENOSPC;

	comp_ptr = dpi_conf->c_desc.compl_ptr[dpi_conf->c_desc.tail];
	CNXK_DPI_STRM_INC(dpi_conf->c_desc, tail);

	cmd[0] = dpi_conf->cmd.u | (1U << 6) | 1U;
	cmd[1] = (uint64_t)comp_ptr;
	cmd[2] = (1UL << 47) | ((flags & RTE_DMA_OP_FLAG_AUTO_FREE) << 43);
	cmd[4] = length;
	cmd[5] = src;
	cmd[6] = length;
	cmd[7] = dst;

	rc = __dpi_queue_write_single(dpivf, cmd);
	if (unlikely(rc)) {
		CNXK_DPI_STRM_DEC(dpi_conf->c_desc, tail);
		return rc;
	}

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		plt_write64(dpi_conf->pnum_words + CNXK_DPI_DW_PER_SINGLE_CMD,
			    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
		dpi_conf->stats.submitted += dpi_conf->pending + 1;
		dpi_conf->pnum_words = 0;
		dpi_conf->pending = 0;
	} else {
		dpi_conf->pnum_words += 8;
		dpi_conf->pending++;
	}

	return dpi_conf->desc_idx++;
}

int
cn10k_dmadev_copy_sg(void *dev_private, uint16_t vchan, const struct rte_dma_sge *src,
		     const struct rte_dma_sge *dst, uint16_t nb_src, uint16_t nb_dst,
		     uint64_t flags)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	struct cnxk_dpi_compl_s *comp_ptr;
	uint64_t hdr[4];
	int rc;

	if (unlikely(((dpi_conf->c_desc.tail + 1) & dpi_conf->c_desc.max_cnt) ==
		     dpi_conf->c_desc.head))
		return -ENOSPC;

	comp_ptr = dpi_conf->c_desc.compl_ptr[dpi_conf->c_desc.tail];
	CNXK_DPI_STRM_INC(dpi_conf->c_desc, tail);

	hdr[0] = dpi_conf->cmd.u | (nb_dst << 6) | nb_src;
	hdr[1] = (uint64_t)comp_ptr;
	hdr[2] = (1UL << 47) | ((flags & RTE_DMA_OP_FLAG_AUTO_FREE) << 43);

	rc = __dpi_queue_write_sg(dpivf, hdr, src, dst, nb_src, nb_dst);
	if (unlikely(rc)) {
		CNXK_DPI_STRM_DEC(dpi_conf->c_desc, tail);
		return rc;
	}

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		plt_write64(dpi_conf->pnum_words + CNXK_DPI_CMD_LEN(nb_src, nb_dst),
			    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
		dpi_conf->stats.submitted += dpi_conf->pending + 1;
		dpi_conf->pnum_words = 0;
		dpi_conf->pending = 0;
	} else {
		dpi_conf->pnum_words += CNXK_DPI_CMD_LEN(nb_src, nb_dst);
		dpi_conf->pending++;
	}

	return dpi_conf->desc_idx++;
}
