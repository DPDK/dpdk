/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_mbuf_pool_ops.h>
#include <rte_mempool.h>

#include "roc_api.h"
#include "cnxk_mempool.h"

int __rte_hot
cnxk_mempool_enq(struct rte_mempool *mp, void *const *obj_table, unsigned int n)
{
	unsigned int index;

	/* Ensure mbuf init changes are written before the free pointers
	 * are enqueued to the stack.
	 */
	rte_io_wmb();
	for (index = 0; index < n; index++)
		roc_npa_aura_op_free(mp->pool_id, 0,
				     (uint64_t)obj_table[index]);

	return 0;
}

int __rte_hot
cnxk_mempool_deq(struct rte_mempool *mp, void **obj_table, unsigned int n)
{
	unsigned int index;
	uint64_t obj;

	for (index = 0; index < n; index++, obj_table++) {
		int retry = 4;

		/* Retry few times before failing */
		do {
			obj = roc_npa_aura_op_alloc(mp->pool_id, 0);
		} while (retry-- && (obj == 0));

		if (obj == 0) {
			cnxk_mempool_enq(mp, obj_table - index, index);
			return -ENOENT;
		}
		*obj_table = (void *)obj;
	}

	return 0;
}

unsigned int
cnxk_mempool_get_count(const struct rte_mempool *mp)
{
	return (unsigned int)roc_npa_aura_op_available(mp->pool_id);
}

ssize_t
cnxk_mempool_calc_mem_size(const struct rte_mempool *mp, uint32_t obj_num,
			   uint32_t pg_shift, size_t *min_chunk_size,
			   size_t *align)
{
	size_t total_elt_sz;

	/* Need space for one more obj on each chunk to fulfill
	 * alignment requirements.
	 */
	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;
	return rte_mempool_op_calc_mem_size_helper(
		mp, obj_num, pg_shift, total_elt_sz, min_chunk_size, align);
}

int
cnxk_mempool_alloc(struct rte_mempool *mp)
{
	uint32_t block_count, flags, roc_flags = 0;
	uint64_t aura_handle = 0;
	struct npa_aura_s aura;
	struct npa_pool_s pool;
	size_t block_size;
	int rc = -ERANGE;

	block_size = mp->elt_size + mp->header_size + mp->trailer_size;
	block_count = mp->size;
	if (mp->header_size % ROC_ALIGN != 0) {
		plt_err("Header size should be multiple of %dB", ROC_ALIGN);
		goto error;
	}

	if (block_size % ROC_ALIGN != 0) {
		plt_err("Block size should be multiple of %dB", ROC_ALIGN);
		goto error;
	}

	memset(&aura, 0, sizeof(struct npa_aura_s));
	memset(&pool, 0, sizeof(struct npa_pool_s));
	pool.nat_align = 1;
	pool.buf_offset = mp->header_size / ROC_ALIGN;

	flags = CNXK_MEMPOOL_FLAGS(mp);
	if (flags & CNXK_MEMPOOL_F_ZERO_AURA) {
		roc_flags = ROC_NPA_ZERO_AURA_F;
	} else if (flags & CNXK_MEMPOOL_F_CUSTOM_AURA) {
		struct npa_aura_s *paura;

		paura = CNXK_MEMPOOL_CONFIG(mp);
		memcpy(&aura, paura, sizeof(struct npa_aura_s));
	}

	rc = roc_npa_pool_create(&aura_handle, block_size, block_count, &aura,
				 &pool, roc_flags);
	if (rc) {
		plt_err("Failed to alloc pool or aura rc=%d", rc);
		goto error;
	}

	/* Store aura_handle for future queue operations */
	mp->pool_id = aura_handle;
	plt_npa_dbg("block_sz=%lu block_count=%d aura_handle=0x%" PRIx64,
		    block_size, block_count, aura_handle);

	return 0;
error:
	return rc;
}

void
cnxk_mempool_free(struct rte_mempool *mp)
{
	int rc = 0;

	plt_npa_dbg("aura_handle=0x%" PRIx64, mp->pool_id);

	/* It can happen that rte_mempool_free() is called immediately after
	 * rte_mempool_create_empty(). In such cases the NPA pool will not be
	 * allocated.
	 */
	if (roc_npa_aura_handle_to_base(mp->pool_id) == 0)
		return;

	rc = roc_npa_pool_destroy(mp->pool_id);
	if (rc)
		plt_err("Failed to free pool or aura rc=%d", rc);
}

int
cnxk_mempool_populate(struct rte_mempool *mp, unsigned int max_objs,
		      void *vaddr, rte_iova_t iova, size_t len,
		      rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	size_t total_elt_sz, off;
	int num_elts;

	if (iova == RTE_BAD_IOVA)
		return -EINVAL;

	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;

	/* Align object start address to a multiple of total_elt_sz */
	off = total_elt_sz - ((((uintptr_t)vaddr - 1) % total_elt_sz) + 1);

	if (len < off)
		return -EINVAL;

	vaddr = (char *)vaddr + off;
	iova += off;
	len -= off;
	num_elts = len / total_elt_sz;

	plt_npa_dbg("iova %" PRIx64 ", aligned iova %" PRIx64 "", iova - off,
		    iova);
	plt_npa_dbg("length %" PRIu64 ", aligned length %" PRIu64 "",
		    (uint64_t)(len + off), (uint64_t)len);
	plt_npa_dbg("element size %" PRIu64 "", (uint64_t)total_elt_sz);
	plt_npa_dbg("requested objects %" PRIu64 ", possible objects %" PRIu64
		    "", (uint64_t)max_objs, (uint64_t)num_elts);

	roc_npa_pool_op_range_set(mp->pool_id, iova,
				  iova + num_elts * total_elt_sz);

	if (roc_npa_pool_range_update_check(mp->pool_id) < 0)
		return -EBUSY;

	return rte_mempool_op_populate_helper(
		mp, RTE_MEMPOOL_POPULATE_F_ALIGN_OBJ, max_objs, vaddr, iova,
		len, obj_cb, obj_cb_arg);
}

static int
cnxk_mempool_plt_init(void)
{
	int rc = 0;

	if (roc_model_is_cn9k()) {
		rte_mbuf_set_platform_mempool_ops("cn9k_mempool_ops");
	} else if (roc_model_is_cn10k()) {
		rte_mbuf_set_platform_mempool_ops("cn10k_mempool_ops");
		rc = cn10k_mempool_plt_init();
	}
	return rc;
}

RTE_INIT(cnxk_mempool_ops_init)
{
	roc_plt_init_cb_register(cnxk_mempool_plt_init);
}
