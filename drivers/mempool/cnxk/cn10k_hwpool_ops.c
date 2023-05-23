/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_mempool.h>
#include <rte_pmd_cnxk_mempool.h>

#include "roc_api.h"
#include "cnxk_mempool.h"

#define CN10K_HWPOOL_MEM_SIZE	 128
#define CN10K_NPA_IOVA_RANGE_MIN 0x0
#define CN10K_NPA_IOVA_RANGE_MAX 0x1fffffffffff80

static int __rte_hot
cn10k_hwpool_enq(struct rte_mempool *hp, void *const *obj_table, unsigned int n)
{
	struct rte_mempool *mp;
	unsigned int index;

	mp = CNXK_MEMPOOL_CONFIG(hp);
	/* Ensure mbuf init changes are written before the free pointers
	 * are enqueued to the stack.
	 */
	rte_io_wmb();
	for (index = 0; index < n; index++) {
		struct rte_mempool_objhdr *hdr;
		struct rte_mbuf *m;

		m = PLT_PTR_CAST(obj_table[index]);
		/* Update mempool information in the mbuf */
		hdr = rte_mempool_get_header(obj_table[index]);
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
		if (hdr->mp != m->pool || hdr->mp != hp)
			plt_err("Pool Header Mismatch");
#endif
		m->pool = mp;
		hdr->mp = mp;
		roc_npa_aura_op_free(hp->pool_id, 0,
				     (uint64_t)obj_table[index]);
	}

	return 0;
}

static int __rte_hot
cn10k_hwpool_deq(struct rte_mempool *hp, void **obj_table, unsigned int n)
{
	unsigned int index;
	uint64_t obj;
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	struct rte_mempool *mp;

	mp = CNXK_MEMPOOL_CONFIG(hp);
#endif

	for (index = 0; index < n; index++, obj_table++) {
		struct rte_mempool_objhdr *hdr;
		struct rte_mbuf *m;
		int retry = 4;

		/* Retry few times before failing */
		do {
			obj = roc_npa_aura_op_alloc(hp->pool_id, 0);
		} while (retry-- && (obj == 0));

		if (obj == 0) {
			cn10k_hwpool_enq(hp, obj_table - index, index);
			return -ENOENT;
		}
		/* Update mempool information in the mbuf */
		hdr = rte_mempool_get_header(PLT_PTR_CAST(obj));
		m = PLT_PTR_CAST(obj);
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
		if (hdr->mp != m->pool || hdr->mp != mp)
			plt_err("Pool Header Mismatch");
#endif
		m->pool = hp;
		hdr->mp = hp;
		*obj_table = (void *)obj;
	}

	return 0;
}

static unsigned int
cn10k_hwpool_get_count(const struct rte_mempool *hp)
{
	return (unsigned int)roc_npa_aura_op_available(hp->pool_id);
}

static int
cn10k_hwpool_alloc(struct rte_mempool *hp)
{
	uint64_t aura_handle = 0;
	struct rte_mempool *mp;
	uint32_t pool_id;
	int rc;

	if (hp->cache_size) {
		plt_err("Hwpool does not support cache");
		return -EINVAL;
	}

	if (CNXK_MEMPOOL_FLAGS(hp)) {
		plt_err("Flags must not be passed to hwpool ops");
		return -EINVAL;
	}

	mp = CNXK_MEMPOOL_CONFIG(hp);
	if (!mp) {
		plt_err("Invalid rte_mempool passed as pool_config");
		return -EINVAL;
	}
	if (mp->cache_size) {
		plt_err("Hwpool does not support attaching to pool with cache");
		return -EINVAL;
	}

	if (hp->elt_size != mp->elt_size ||
	    hp->header_size != mp->header_size ||
	    hp->trailer_size != mp->trailer_size || hp->size != mp->size) {
		plt_err("Hwpool parameters matching with master pool");
		return -EINVAL;
	}

	/* Create the NPA aura */
	pool_id = roc_npa_aura_handle_to_aura(mp->pool_id);
	rc = roc_npa_aura_create(&aura_handle, hp->size, NULL, (int)pool_id, 0);
	if (rc) {
		plt_err("Failed to create aura rc=%d", rc);
		return rc;
	}

	/* Set the flags for the hardware pool */
	CNXK_MEMPOOL_SET_FLAGS(hp, CNXK_MEMPOOL_F_IS_HWPOOL);
	hp->pool_id = aura_handle;
	plt_npa_dbg("aura_handle=0x%" PRIx64, aura_handle);

	return 0;
}

static void
cn10k_hwpool_free(struct rte_mempool *hp)
{
	int rc = 0;

	plt_npa_dbg("aura_handle=0x%" PRIx64, hp->pool_id);
	/* It can happen that rte_mempool_free() is called immediately after
	 * rte_mempool_create_empty(). In such cases the NPA pool will not be
	 * allocated.
	 */
	if (roc_npa_aura_handle_to_base(hp->pool_id) == 0)
		return;

	rc = roc_npa_aura_destroy(hp->pool_id);
	if (rc)
		plt_err("Failed to destroy aura rc=%d", rc);
}

static ssize_t
cn10k_hwpool_calc_mem_size(const struct rte_mempool *hp, uint32_t obj_num,
			   uint32_t pg_shift, size_t *min_chunk_size,
			   size_t *align)
{
	RTE_SET_USED(hp);
	RTE_SET_USED(obj_num);
	RTE_SET_USED(pg_shift);
	*min_chunk_size = CN10K_HWPOOL_MEM_SIZE;
	*align = CN10K_HWPOOL_MEM_SIZE;
	/* Return a minimum mem size so that hwpool can also be initialized just
	 * like a regular pool. This memzone will not be used anywhere.
	 */
	return CN10K_HWPOOL_MEM_SIZE;
}

static int
cn10k_hwpool_populate(struct rte_mempool *hp, unsigned int max_objs,
		      void *vaddr, rte_iova_t iova, size_t len,
		      rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	uint64_t start_iova, end_iova;
	struct rte_mempool *mp;

	RTE_SET_USED(max_objs);
	RTE_SET_USED(vaddr);
	RTE_SET_USED(iova);
	RTE_SET_USED(len);
	RTE_SET_USED(obj_cb);
	RTE_SET_USED(obj_cb_arg);
	/* HW pools does not require populating anything as these pools are
	 * only associated with NPA aura. The NPA pool being used is that of
	 * another rte_mempool. Only copy the iova range from the aura of
	 * the other rte_mempool to this pool's aura.
	 */
	mp = CNXK_MEMPOOL_CONFIG(hp);
	roc_npa_aura_op_range_get(mp->pool_id, &start_iova, &end_iova);
	roc_npa_aura_op_range_set(hp->pool_id, start_iova, end_iova);

	return hp->size;
}

int
rte_pmd_cnxk_mempool_mbuf_exchange(struct rte_mbuf *m1, struct rte_mbuf *m2)
{
	struct rte_mempool_objhdr *hdr;

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	if (!(CNXK_MEMPOOL_FLAGS(m1->pool) & CNXK_MEMPOOL_F_NO_RANGE_CHECK) ||
	    !(CNXK_MEMPOOL_FLAGS(m2->pool) & CNXK_MEMPOOL_F_NO_RANGE_CHECK)) {
		plt_err("Pools must have range check disabled");
		return -EINVAL;
	}
	if (m1->pool->elt_size != m2->pool->elt_size ||
	    m1->pool->header_size != m2->pool->header_size ||
	    m1->pool->trailer_size != m2->pool->trailer_size ||
	    m1->pool->size != m2->pool->size) {
		plt_err("Parameters of pools involved in exchange does not match");
		return -EINVAL;
	}
#endif
	RTE_SWAP(m1->pool, m2->pool);
	hdr = rte_mempool_get_header(m1);
	hdr->mp = m1->pool;
	hdr = rte_mempool_get_header(m2);
	hdr->mp = m2->pool;
	return 0;
}

int
rte_pmd_cnxk_mempool_is_hwpool(struct rte_mempool *mp)
{
	return !!(CNXK_MEMPOOL_FLAGS(mp) & CNXK_MEMPOOL_F_IS_HWPOOL);
}

int
rte_pmd_cnxk_mempool_range_check_disable(struct rte_mempool *mp)
{
	if (rte_pmd_cnxk_mempool_is_hwpool(mp)) {
		/* Disable only aura range check for hardware pools */
		roc_npa_aura_op_range_set(mp->pool_id, CN10K_NPA_IOVA_RANGE_MIN,
					  CN10K_NPA_IOVA_RANGE_MAX);
		CNXK_MEMPOOL_SET_FLAGS(mp, CNXK_MEMPOOL_F_NO_RANGE_CHECK);
		mp = CNXK_MEMPOOL_CONFIG(mp);
	}

	/* No need to disable again if already disabled */
	if (CNXK_MEMPOOL_FLAGS(mp) & CNXK_MEMPOOL_F_NO_RANGE_CHECK)
		return 0;

	/* Disable aura/pool range check */
	roc_npa_pool_op_range_set(mp->pool_id, CN10K_NPA_IOVA_RANGE_MIN,
				  CN10K_NPA_IOVA_RANGE_MAX);
	if (roc_npa_pool_range_update_check(mp->pool_id) < 0)
		return -EBUSY;

	CNXK_MEMPOOL_SET_FLAGS(mp, CNXK_MEMPOOL_F_NO_RANGE_CHECK);
	return 0;
}

static struct rte_mempool_ops cn10k_hwpool_ops = {
	.name = "cn10k_hwpool_ops",
	.alloc = cn10k_hwpool_alloc,
	.free = cn10k_hwpool_free,
	.enqueue = cn10k_hwpool_enq,
	.dequeue = cn10k_hwpool_deq,
	.get_count = cn10k_hwpool_get_count,
	.calc_mem_size = cn10k_hwpool_calc_mem_size,
	.populate = cn10k_hwpool_populate,
};

RTE_MEMPOOL_REGISTER_OPS(cn10k_hwpool_ops);
