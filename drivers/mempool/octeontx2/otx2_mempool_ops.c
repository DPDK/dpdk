/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_mempool.h>
#include <rte_vect.h>

#include "otx2_mempool.h"

static int __hot
otx2_npa_enq(struct rte_mempool *mp, void * const *obj_table, unsigned int n)
{
	unsigned int index; const uint64_t aura_handle = mp->pool_id;
	const uint64_t reg = npa_lf_aura_handle_to_aura(aura_handle);
	const uint64_t addr = npa_lf_aura_handle_to_base(aura_handle) +
				 NPA_LF_AURA_OP_FREE0;

	for (index = 0; index < n; index++)
		otx2_store_pair((uint64_t)obj_table[index], reg, addr);

	return 0;
}

static __rte_noinline int
npa_lf_aura_op_alloc_one(const int64_t wdata, int64_t * const addr,
			 void **obj_table, uint8_t i)
{
	uint8_t retry = 4;

	do {
		obj_table[i] = (void *)otx2_atomic64_add_nosync(wdata, addr);
		if (obj_table[i] != NULL)
			return 0;

	} while (retry--);

	return -ENOENT;
}

static inline int __hot
otx2_npa_deq(struct rte_mempool *mp, void **obj_table, unsigned int n)
{
	const int64_t wdata = npa_lf_aura_handle_to_aura(mp->pool_id);
	unsigned int index;
	uint64_t obj;

	int64_t * const addr = (int64_t * const)
			(npa_lf_aura_handle_to_base(mp->pool_id) +
				NPA_LF_AURA_OP_ALLOCX(0));
	for (index = 0; index < n; index++, obj_table++) {
		obj = npa_lf_aura_op_alloc_one(wdata, addr, obj_table, 0);
		if (obj == 0) {
			for (; index > 0; index--) {
				obj_table--;
				otx2_npa_enq(mp, obj_table, 1);
			}
			return -ENOENT;
		}
		*obj_table = (void *)obj;
	}

	return 0;
}

static unsigned int
otx2_npa_get_count(const struct rte_mempool *mp)
{
	return (unsigned int)npa_lf_aura_op_available(mp->pool_id);
}

static int
npa_lf_aura_pool_init(struct otx2_mbox *mbox, uint32_t aura_id,
		      struct npa_aura_s *aura, struct npa_pool_s *pool)
{
	struct npa_aq_enq_req *aura_init_req, *pool_init_req;
	struct npa_aq_enq_rsp *aura_init_rsp, *pool_init_rsp;
	struct otx2_mbox_dev *mdev = &mbox->dev[0];
	int rc, off;

	aura_init_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);

	aura_init_req->aura_id = aura_id;
	aura_init_req->ctype = NPA_AQ_CTYPE_AURA;
	aura_init_req->op = NPA_AQ_INSTOP_INIT;
	memcpy(&aura_init_req->aura, aura, sizeof(*aura));

	pool_init_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);

	pool_init_req->aura_id = aura_id;
	pool_init_req->ctype = NPA_AQ_CTYPE_POOL;
	pool_init_req->op = NPA_AQ_INSTOP_INIT;
	memcpy(&pool_init_req->pool, pool, sizeof(*pool));

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_wait_for_rsp(mbox, 0);
	if (rc < 0)
		return rc;

	off = mbox->rx_start +
			RTE_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	aura_init_rsp = (struct npa_aq_enq_rsp *)((uintptr_t)mdev->mbase + off);
	off = mbox->rx_start + aura_init_rsp->hdr.next_msgoff;
	pool_init_rsp = (struct npa_aq_enq_rsp *)((uintptr_t)mdev->mbase + off);

	if (rc == 2 && aura_init_rsp->hdr.rc == 0 && pool_init_rsp->hdr.rc == 0)
		return 0;
	else
		return NPA_LF_ERR_AURA_POOL_INIT;
}

static int
npa_lf_aura_pool_fini(struct otx2_mbox *mbox,
		      uint32_t aura_id,
		      uint64_t aura_handle)
{
	struct npa_aq_enq_req *aura_req, *pool_req;
	struct npa_aq_enq_rsp *aura_rsp, *pool_rsp;
	struct otx2_mbox_dev *mdev = &mbox->dev[0];
	struct ndc_sync_op *ndc_req;
	int rc, off;

	/* Procedure for disabling an aura/pool */
	rte_delay_us(10);
	npa_lf_aura_op_alloc(aura_handle, 0);

	pool_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);
	pool_req->aura_id = aura_id;
	pool_req->ctype = NPA_AQ_CTYPE_POOL;
	pool_req->op = NPA_AQ_INSTOP_WRITE;
	pool_req->pool.ena = 0;
	pool_req->pool_mask.ena = ~pool_req->pool_mask.ena;

	aura_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);
	aura_req->aura_id = aura_id;
	aura_req->ctype = NPA_AQ_CTYPE_AURA;
	aura_req->op = NPA_AQ_INSTOP_WRITE;
	aura_req->aura.ena = 0;
	aura_req->aura_mask.ena = ~aura_req->aura_mask.ena;

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_wait_for_rsp(mbox, 0);
	if (rc < 0)
		return rc;

	off = mbox->rx_start +
			RTE_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	pool_rsp = (struct npa_aq_enq_rsp *)((uintptr_t)mdev->mbase + off);

	off = mbox->rx_start + pool_rsp->hdr.next_msgoff;
	aura_rsp = (struct npa_aq_enq_rsp *)((uintptr_t)mdev->mbase + off);

	if (rc != 2 || aura_rsp->hdr.rc != 0 || pool_rsp->hdr.rc != 0)
		return NPA_LF_ERR_AURA_POOL_FINI;

	/* Sync NDC-NPA for LF */
	ndc_req = otx2_mbox_alloc_msg_ndc_sync_op(mbox);
	ndc_req->npa_lf_sync = 1;

	rc = otx2_mbox_process(mbox);
	if (rc) {
		otx2_err("Error on NDC-NPA LF sync, rc %d", rc);
		return NPA_LF_ERR_AURA_POOL_FINI;
	}
	return 0;
}

static inline char*
npa_lf_stack_memzone_name(struct otx2_npa_lf *lf, int pool_id, char *name)
{
	snprintf(name, RTE_MEMZONE_NAMESIZE, "otx2_npa_stack_%x_%d",
			lf->pf_func, pool_id);

	return name;
}

static inline const struct rte_memzone *
npa_lf_stack_dma_alloc(struct otx2_npa_lf *lf, char *name,
		       int pool_id, size_t size)
{
	return rte_memzone_reserve_aligned(
		npa_lf_stack_memzone_name(lf, pool_id, name), size, 0,
			RTE_MEMZONE_IOVA_CONTIG, OTX2_ALIGN);
}

static inline int
npa_lf_stack_dma_free(struct otx2_npa_lf *lf, char *name, int pool_id)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(npa_lf_stack_memzone_name(lf, pool_id, name));
	if (mz == NULL)
		return -EINVAL;

	return rte_memzone_free(mz);
}

static inline int
bitmap_ctzll(uint64_t slab)
{
	if (slab == 0)
		return 0;

	return __builtin_ctzll(slab);
}

static int
npa_lf_aura_pool_pair_alloc(struct otx2_npa_lf *lf, const uint32_t block_size,
			    const uint32_t block_count, struct npa_aura_s *aura,
			    struct npa_pool_s *pool, uint64_t *aura_handle)
{
	int rc, aura_id, pool_id, stack_size, alloc_size;
	char name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	uint64_t slab;
	uint32_t pos;

	/* Sanity check */
	if (!lf || !block_size || !block_count ||
	    !pool || !aura || !aura_handle)
		return NPA_LF_ERR_PARAM;

	/* Block size should be cache line aligned and in range of 128B-128KB */
	if (block_size % OTX2_ALIGN || block_size < 128 ||
	    block_size > 128 * 1024)
		return NPA_LF_ERR_INVALID_BLOCK_SZ;

	pos = slab = 0;
	/* Scan from the beginning */
	__rte_bitmap_scan_init(lf->npa_bmp);
	/* Scan bitmap to get the free pool */
	rc = rte_bitmap_scan(lf->npa_bmp, &pos, &slab);
	/* Empty bitmap */
	if (rc == 0) {
		otx2_err("Mempools exhausted, 'max_pools' devargs to increase");
		return -ERANGE;
	}

	/* Get aura_id from resource bitmap */
	aura_id = pos + bitmap_ctzll(slab);
	/* Mark pool as reserved */
	rte_bitmap_clear(lf->npa_bmp, aura_id);

	/* Configuration based on each aura has separate pool(aura-pool pair) */
	pool_id = aura_id;
	rc = (aura_id < 0 || pool_id >= (int)lf->nr_pools || aura_id >=
	      (int)BIT_ULL(6 + lf->aura_sz)) ? NPA_LF_ERR_AURA_ID_ALLOC : 0;
	if (rc)
		goto exit;

	/* Allocate stack memory */
	stack_size = (block_count + lf->stack_pg_ptrs - 1) / lf->stack_pg_ptrs;
	alloc_size = stack_size * lf->stack_pg_bytes;

	mz = npa_lf_stack_dma_alloc(lf, name, pool_id, alloc_size);
	if (mz == NULL) {
		rc = -ENOMEM;
		goto aura_res_put;
	}

	/* Update aura fields */
	aura->pool_addr = pool_id;/* AF will translate to associated poolctx */
	aura->ena = 1;
	aura->shift = __builtin_clz(block_count) - 8;
	aura->limit = block_count;
	aura->pool_caching = 1;
	aura->err_int_ena = BIT(NPA_AURA_ERR_INT_AURA_ADD_OVER);
	aura->err_int_ena |= BIT(NPA_AURA_ERR_INT_AURA_ADD_UNDER);
	aura->err_int_ena |= BIT(NPA_AURA_ERR_INT_AURA_FREE_UNDER);
	aura->err_int_ena |= BIT(NPA_AURA_ERR_INT_POOL_DIS);
	/* Many to one reduction */
	aura->err_qint_idx = aura_id % lf->qints;

	/* Update pool fields */
	pool->stack_base = mz->iova;
	pool->ena = 1;
	pool->buf_size = block_size / OTX2_ALIGN;
	pool->stack_max_pages = stack_size;
	pool->shift = __builtin_clz(block_count) - 8;
	pool->ptr_start = 0;
	pool->ptr_end = ~0;
	pool->stack_caching = 1;
	pool->err_int_ena = BIT(NPA_POOL_ERR_INT_OVFLS);
	pool->err_int_ena |= BIT(NPA_POOL_ERR_INT_RANGE);
	pool->err_int_ena |= BIT(NPA_POOL_ERR_INT_PERR);

	/* Many to one reduction */
	pool->err_qint_idx = pool_id % lf->qints;

	/* Issue AURA_INIT and POOL_INIT op */
	rc = npa_lf_aura_pool_init(lf->mbox, aura_id, aura, pool);
	if (rc)
		goto stack_mem_free;

	*aura_handle = npa_lf_aura_handle_gen(aura_id, lf->base);

	/* Update aura count */
	npa_lf_aura_op_cnt_set(*aura_handle, 0, block_count);
	/* Read it back to make sure aura count is updated */
	npa_lf_aura_op_cnt_get(*aura_handle);

	return 0;

stack_mem_free:
	rte_memzone_free(mz);
aura_res_put:
	rte_bitmap_set(lf->npa_bmp, aura_id);
exit:
	return rc;
}

static int
npa_lf_aura_pool_pair_free(struct otx2_npa_lf *lf, uint64_t aura_handle)
{
	char name[RTE_MEMZONE_NAMESIZE];
	int aura_id, pool_id, rc;

	if (!lf || !aura_handle)
		return NPA_LF_ERR_PARAM;

	aura_id = pool_id = npa_lf_aura_handle_to_aura(aura_handle);
	rc = npa_lf_aura_pool_fini(lf->mbox, aura_id, aura_handle);
	rc |= npa_lf_stack_dma_free(lf, name, pool_id);

	rte_bitmap_set(lf->npa_bmp, aura_id);

	return rc;
}

static int
otx2_npa_alloc(struct rte_mempool *mp)
{
	uint32_t block_size, block_count;
	struct otx2_npa_lf *lf;
	struct npa_aura_s aura;
	struct npa_pool_s pool;
	uint64_t aura_handle;
	int rc;

	lf = otx2_npa_lf_obj_get();
	if (lf == NULL) {
		rc = -EINVAL;
		goto error;
	}

	block_size = mp->elt_size + mp->header_size + mp->trailer_size;
	block_count = mp->size;

	if (block_size % OTX2_ALIGN != 0) {
		otx2_err("Block size should be multiple of 128B");
		rc = -ERANGE;
		goto error;
	}

	memset(&aura, 0, sizeof(struct npa_aura_s));
	memset(&pool, 0, sizeof(struct npa_pool_s));
	pool.nat_align = 1;
	pool.buf_offset = 1;

	if ((uint32_t)pool.buf_offset * OTX2_ALIGN != mp->header_size) {
		otx2_err("Unsupported mp->header_size=%d", mp->header_size);
		rc = -EINVAL;
		goto error;
	}

	/* Use driver specific mp->pool_config to override aura config */
	if (mp->pool_config != NULL)
		memcpy(&aura, mp->pool_config, sizeof(struct npa_aura_s));

	rc = npa_lf_aura_pool_pair_alloc(lf, block_size, block_count,
			 &aura, &pool, &aura_handle);
	if (rc) {
		otx2_err("Failed to alloc pool or aura rc=%d", rc);
		goto error;
	}

	/* Store aura_handle for future queue operations */
	mp->pool_id = aura_handle;
	otx2_npa_dbg("lf=%p block_sz=%d block_count=%d aura_handle=0x%"PRIx64,
		     lf, block_size, block_count, aura_handle);

	/* Just hold the reference of the object */
	otx2_npa_lf_obj_ref();
	return 0;
error:
	return rc;
}

static void
otx2_npa_free(struct rte_mempool *mp)
{
	struct otx2_npa_lf *lf = otx2_npa_lf_obj_get();
	int rc = 0;

	otx2_npa_dbg("lf=%p aura_handle=0x%"PRIx64, lf, mp->pool_id);
	if (lf != NULL)
		rc = npa_lf_aura_pool_pair_free(lf, mp->pool_id);

	if (rc)
		otx2_err("Failed to free pool or aura rc=%d", rc);

	/* Release the reference of npalf */
	otx2_npa_lf_fini();
}

static ssize_t
otx2_npa_calc_mem_size(const struct rte_mempool *mp, uint32_t obj_num,
		       uint32_t pg_shift, size_t *min_chunk_size, size_t *align)
{
	ssize_t mem_size;

	/*
	 * Simply need space for one more object to be able to
	 * fulfill alignment requirements.
	 */
	mem_size = rte_mempool_op_calc_mem_size_default(mp, obj_num + 1,
							pg_shift,
							min_chunk_size, align);
	if (mem_size >= 0) {
		/*
		 * Memory area which contains objects must be physically
		 * contiguous.
		 */
		*min_chunk_size = mem_size;
	}

	return mem_size;
}

static int
otx2_npa_populate(struct rte_mempool *mp, unsigned int max_objs, void *vaddr,
		  rte_iova_t iova, size_t len,
		  rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	size_t total_elt_sz;
	size_t off;

	if (iova == RTE_BAD_IOVA)
		return -EINVAL;

	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;

	/* Align object start address to a multiple of total_elt_sz */
	off = total_elt_sz - ((uintptr_t)vaddr % total_elt_sz);

	if (len < off)
		return -EINVAL;

	vaddr = (char *)vaddr + off;
	iova += off;
	len -= off;

	npa_lf_aura_op_range_set(mp->pool_id, iova, iova + len);

	return rte_mempool_op_populate_default(mp, max_objs, vaddr, iova, len,
					       obj_cb, obj_cb_arg);
}

static struct rte_mempool_ops otx2_npa_ops = {
	.name = "octeontx2_npa",
	.alloc = otx2_npa_alloc,
	.free = otx2_npa_free,
	.enqueue = otx2_npa_enq,
	.get_count = otx2_npa_get_count,
	.calc_mem_size = otx2_npa_calc_mem_size,
	.populate = otx2_npa_populate,
	.dequeue = otx2_npa_deq,
};

MEMPOOL_REGISTER_OPS(otx2_npa_ops);
