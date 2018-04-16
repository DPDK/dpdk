/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation.
 * Copyright(c) 2016 6WIND S.A.
 * Copyright(c) 2018 Solarflare Communications Inc.
 */

#include <rte_mempool.h>

ssize_t
rte_mempool_op_calc_mem_size_default(const struct rte_mempool *mp,
				     uint32_t obj_num, uint32_t pg_shift,
				     size_t *min_chunk_size, size_t *align)
{
	unsigned int mp_flags;
	int ret;
	size_t total_elt_sz;
	size_t mem_size;

	/* Get mempool capabilities */
	mp_flags = 0;
	ret = rte_mempool_ops_get_capabilities(mp, &mp_flags);
	if ((ret < 0) && (ret != -ENOTSUP))
		return ret;

	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;

	mem_size = rte_mempool_xmem_size(obj_num, total_elt_sz, pg_shift,
					 mp->flags | mp_flags);

	if (mp_flags & MEMPOOL_F_CAPA_PHYS_CONTIG)
		*min_chunk_size = mem_size;
	else
		*min_chunk_size = RTE_MAX((size_t)1 << pg_shift, total_elt_sz);

	*align = RTE_MAX((size_t)RTE_CACHE_LINE_SIZE, (size_t)1 << pg_shift);

	return mem_size;
}

int
rte_mempool_op_populate_default(struct rte_mempool *mp, unsigned int max_objs,
		void *vaddr, rte_iova_t iova, size_t len,
		rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	size_t total_elt_sz;
	size_t off;
	unsigned int i;
	void *obj;

	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;

	for (off = 0, i = 0; off + total_elt_sz <= len && i < max_objs; i++) {
		off += mp->header_size;
		obj = (char *)vaddr + off;
		obj_cb(mp, obj_cb_arg, obj,
		       (iova == RTE_BAD_IOVA) ? RTE_BAD_IOVA : (iova + off));
		rte_mempool_ops_enqueue_bulk(mp, &obj, 1);
		off += mp->elt_size + mp->trailer_size;
	}

	return i;
}
