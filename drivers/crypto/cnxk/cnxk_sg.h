/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef _CNXK_SG_H_
#define _CNXK_SG_H_

#include "roc_cpt_sg.h"
#include "roc_se.h"

static __rte_always_inline uint32_t
fill_sg_comp(struct roc_sglist_comp *list, uint32_t i, phys_addr_t dma_addr, uint32_t size)
{
	struct roc_sglist_comp *to = &list[i >> 2];

	to->u.s.len[i % 4] = rte_cpu_to_be_16(size);
	to->ptr[i % 4] = rte_cpu_to_be_64(dma_addr);
	return ++i;
}

static __rte_always_inline uint32_t
fill_sg_comp_from_buf(struct roc_sglist_comp *list, uint32_t i, struct roc_se_buf_ptr *from)
{
	struct roc_sglist_comp *to = &list[i >> 2];

	to->u.s.len[i % 4] = rte_cpu_to_be_16(from->size);
	to->ptr[i % 4] = rte_cpu_to_be_64((uint64_t)from->vaddr);
	return ++i;
}

static __rte_always_inline uint32_t
fill_sg_comp_from_buf_min(struct roc_sglist_comp *list, uint32_t i, struct roc_se_buf_ptr *from,
			  uint32_t *psize)
{
	struct roc_sglist_comp *to = &list[i >> 2];
	uint32_t size = *psize;
	uint32_t e_len;

	e_len = RTE_MIN(from->size, size);
	to->u.s.len[i % 4] = rte_cpu_to_be_16(e_len);
	to->ptr[i % 4] = rte_cpu_to_be_64((uint64_t)from->vaddr);
	*psize -= e_len;
	return ++i;
}

/*
 * This fills the MC expected SGIO list
 * from IOV given by user.
 */
static __rte_always_inline uint32_t
fill_sg_comp_from_iov(struct roc_sglist_comp *list, uint32_t i, struct roc_se_iov_ptr *from,
		      uint32_t from_offset, uint32_t *psize, struct roc_se_buf_ptr *extra_buf,
		      uint32_t extra_offset)
{
	uint32_t extra_len = extra_buf ? extra_buf->size : 0;
	uint32_t size = *psize;
	int32_t j;

	for (j = 0; j < from->buf_cnt; j++) {
		struct roc_sglist_comp *to = &list[i >> 2];
		uint32_t buf_sz = from->bufs[j].size;
		void *vaddr = from->bufs[j].vaddr;
		uint64_t e_vaddr;
		uint32_t e_len;

		if (unlikely(from_offset)) {
			if (from_offset >= buf_sz) {
				from_offset -= buf_sz;
				continue;
			}
			e_vaddr = (uint64_t)vaddr + from_offset;
			e_len = RTE_MIN((buf_sz - from_offset), size);
			from_offset = 0;
		} else {
			e_vaddr = (uint64_t)vaddr;
			e_len = RTE_MIN(buf_sz, size);
		}

		to->u.s.len[i % 4] = rte_cpu_to_be_16(e_len);
		to->ptr[i % 4] = rte_cpu_to_be_64(e_vaddr);

		if (extra_len && (e_len >= extra_offset)) {
			/* Break the data at given offset */
			uint32_t next_len = e_len - extra_offset;
			uint64_t next_vaddr = e_vaddr + extra_offset;

			if (!extra_offset) {
				i--;
			} else {
				e_len = extra_offset;
				size -= e_len;
				to->u.s.len[i % 4] = rte_cpu_to_be_16(e_len);
			}

			extra_len = RTE_MIN(extra_len, size);
			/* Insert extra data ptr */
			if (extra_len) {
				i++;
				to = &list[i >> 2];
				to->u.s.len[i % 4] = rte_cpu_to_be_16(extra_len);
				to->ptr[i % 4] = rte_cpu_to_be_64((uint64_t)extra_buf->vaddr);
				size -= extra_len;
			}

			next_len = RTE_MIN(next_len, size);
			/* insert the rest of the data */
			if (next_len) {
				i++;
				to = &list[i >> 2];
				to->u.s.len[i % 4] = rte_cpu_to_be_16(next_len);
				to->ptr[i % 4] = rte_cpu_to_be_64(next_vaddr);
				size -= next_len;
			}
			extra_len = 0;

		} else {
			size -= e_len;
		}
		if (extra_offset)
			extra_offset -= size;
		i++;

		if (unlikely(!size))
			break;
	}

	*psize = size;
	return (uint32_t)i;
}

static __rte_always_inline uint32_t
fill_ipsec_sg_comp_from_pkt(struct roc_sglist_comp *list, uint32_t i, struct rte_mbuf *pkt)
{
	uint32_t buf_sz;
	void *vaddr;

	while (unlikely(pkt != NULL)) {
		struct roc_sglist_comp *to = &list[i >> 2];
		buf_sz = pkt->data_len;
		vaddr = rte_pktmbuf_mtod(pkt, void *);

		to->u.s.len[i % 4] = rte_cpu_to_be_16(buf_sz);
		to->ptr[i % 4] = rte_cpu_to_be_64((uint64_t)vaddr);

		pkt = pkt->next;
		i++;
	}

	return i;
}

static __rte_always_inline uint32_t
fill_ipsec_sg2_comp_from_pkt(struct roc_sg2list_comp *list, uint32_t i, struct rte_mbuf *pkt)
{
	uint32_t buf_sz;
	void *vaddr;

	while (unlikely(pkt != NULL)) {
		struct roc_sg2list_comp *to = &list[i / 3];
		buf_sz = pkt->data_len;
		vaddr = rte_pktmbuf_mtod(pkt, void *);

		to->u.s.len[i % 3] = buf_sz;
		to->ptr[i % 3] = (uint64_t)vaddr;
		to->u.s.valid_segs = (i % 3) + 1;

		pkt = pkt->next;
		i++;
	}

	return i;
}

static __rte_always_inline uint32_t
fill_sg2_comp(struct roc_sg2list_comp *list, uint32_t i, phys_addr_t dma_addr, uint32_t size)
{
	struct roc_sg2list_comp *to = &list[i / 3];

	to->u.s.len[i % 3] = (size);
	to->ptr[i % 3] = (dma_addr);
	to->u.s.valid_segs = (i % 3) + 1;
	return ++i;
}

static __rte_always_inline uint32_t
fill_sg2_comp_from_buf(struct roc_sg2list_comp *list, uint32_t i, struct roc_se_buf_ptr *from)
{
	struct roc_sg2list_comp *to = &list[i / 3];

	to->u.s.len[i % 3] = (from->size);
	to->ptr[i % 3] = ((uint64_t)from->vaddr);
	to->u.s.valid_segs = (i % 3) + 1;
	return ++i;
}

static __rte_always_inline uint32_t
fill_sg2_comp_from_buf_min(struct roc_sg2list_comp *list, uint32_t i, struct roc_se_buf_ptr *from,
			   uint32_t *psize)
{
	struct roc_sg2list_comp *to = &list[i / 3];
	uint32_t size = *psize;
	uint32_t e_len;

	e_len = RTE_MIN(from->size, size);
	to->u.s.len[i % 3] = (e_len);
	to->ptr[i % 3] = ((uint64_t)from->vaddr);
	to->u.s.valid_segs = (i % 3) + 1;
	*psize -= e_len;
	return ++i;
}

static __rte_always_inline uint32_t
fill_sg2_comp_from_iov(struct roc_sg2list_comp *list, uint32_t i, struct roc_se_iov_ptr *from,
		       uint32_t from_offset, uint32_t *psize, struct roc_se_buf_ptr *extra_buf,
		       uint32_t extra_offset)
{
	uint32_t extra_len = extra_buf ? extra_buf->size : 0;
	uint32_t size = *psize;
	int32_t j;

	rte_prefetch2(psize);

	for (j = 0; j < from->buf_cnt; j++) {
		struct roc_sg2list_comp *to = &list[i / 3];
		uint32_t buf_sz = from->bufs[j].size;
		void *vaddr = from->bufs[j].vaddr;
		uint64_t e_vaddr;
		uint32_t e_len;

		if (unlikely(from_offset)) {
			if (from_offset >= buf_sz) {
				from_offset -= buf_sz;
				continue;
			}
			e_vaddr = (uint64_t)vaddr + from_offset;
			e_len = RTE_MIN((buf_sz - from_offset), size);
			from_offset = 0;
		} else {
			e_vaddr = (uint64_t)vaddr;
			e_len = RTE_MIN(buf_sz, size);
		}

		to->u.s.len[i % 3] = (e_len);
		to->ptr[i % 3] = (e_vaddr);
		to->u.s.valid_segs = (i % 3) + 1;

		if (extra_len && (e_len >= extra_offset)) {
			/* Break the data at given offset */
			uint32_t next_len = e_len - extra_offset;
			uint64_t next_vaddr = e_vaddr + extra_offset;

			if (!extra_offset) {
				i--;
			} else {
				e_len = extra_offset;
				size -= e_len;
				to->u.s.len[i % 3] = (e_len);
			}

			extra_len = RTE_MIN(extra_len, size);
			/* Insert extra data ptr */
			if (extra_len) {
				i++;
				to = &list[i / 3];
				to->u.s.len[i % 3] = (extra_len);
				to->ptr[i % 3] = ((uint64_t)extra_buf->vaddr);
				to->u.s.valid_segs = (i % 3) + 1;
				size -= extra_len;
			}

			next_len = RTE_MIN(next_len, size);
			/* insert the rest of the data */
			if (next_len) {
				i++;
				to = &list[i / 3];
				to->u.s.len[i % 3] = (next_len);
				to->ptr[i % 3] = (next_vaddr);
				to->u.s.valid_segs = (i % 3) + 1;
				size -= next_len;
			}
			extra_len = 0;

		} else {
			size -= e_len;
		}
		if (extra_offset)
			extra_offset -= size;
		i++;

		if (unlikely(!size))
			break;
	}

	*psize = size;
	return (uint32_t)i;
}

#endif /*_CNXK_SG_H_ */
