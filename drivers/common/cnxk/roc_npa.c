/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static roc_npa_lf_init_cb_t lf_init_cb;

int
roc_npa_lf_init_cb_register(roc_npa_lf_init_cb_t cb)
{
	if (lf_init_cb != NULL)
		return -EEXIST;

	lf_init_cb = cb;
	return 0;
}

void
roc_npa_aura_op_range_set(uint64_t aura_handle, uint64_t start_iova,
			  uint64_t end_iova)
{
	const uint64_t start = roc_npa_aura_handle_to_base(aura_handle) +
			       NPA_LF_POOL_OP_PTR_START0;
	const uint64_t end = roc_npa_aura_handle_to_base(aura_handle) +
			     NPA_LF_POOL_OP_PTR_END0;
	uint64_t reg = roc_npa_aura_handle_to_aura(aura_handle);
	struct npa_lf *lf = idev_npa_obj_get();
	struct npa_aura_lim *lim;

	PLT_ASSERT(lf);
	lim = lf->aura_lim;

	lim[reg].ptr_start = PLT_MIN(lim[reg].ptr_start, start_iova);
	lim[reg].ptr_end = PLT_MAX(lim[reg].ptr_end, end_iova);

	roc_store_pair(lim[reg].ptr_start, reg, start);
	roc_store_pair(lim[reg].ptr_end, reg, end);
}

static int
npa_aura_pool_init(struct mbox *mbox, uint32_t aura_id, struct npa_aura_s *aura,
		   struct npa_pool_s *pool)
{
	struct npa_aq_enq_req *aura_init_req, *pool_init_req;
	struct npa_aq_enq_rsp *aura_init_rsp, *pool_init_rsp;
	struct mbox_dev *mdev = &mbox->dev[0];
	int rc = -ENOSPC, off;

	aura_init_req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (aura_init_req == NULL)
		return rc;
	aura_init_req->aura_id = aura_id;
	aura_init_req->ctype = NPA_AQ_CTYPE_AURA;
	aura_init_req->op = NPA_AQ_INSTOP_INIT;
	mbox_memcpy(&aura_init_req->aura, aura, sizeof(*aura));

	pool_init_req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (pool_init_req == NULL)
		return rc;
	pool_init_req->aura_id = aura_id;
	pool_init_req->ctype = NPA_AQ_CTYPE_POOL;
	pool_init_req->op = NPA_AQ_INSTOP_INIT;
	mbox_memcpy(&pool_init_req->pool, pool, sizeof(*pool));

	rc = mbox_process(mbox);
	if (rc < 0)
		return rc;

	off = mbox->rx_start +
	      PLT_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	aura_init_rsp = (struct npa_aq_enq_rsp *)((uintptr_t)mdev->mbase + off);
	off = mbox->rx_start + aura_init_rsp->hdr.next_msgoff;
	pool_init_rsp = (struct npa_aq_enq_rsp *)((uintptr_t)mdev->mbase + off);

	if (aura_init_rsp->hdr.rc == 0 && pool_init_rsp->hdr.rc == 0)
		return 0;
	else
		return NPA_ERR_AURA_POOL_INIT;
}

static int
npa_aura_pool_fini(struct mbox *mbox, uint32_t aura_id, uint64_t aura_handle)
{
	struct npa_aq_enq_req *aura_req, *pool_req;
	struct npa_aq_enq_rsp *aura_rsp, *pool_rsp;
	struct mbox_dev *mdev = &mbox->dev[0];
	struct ndc_sync_op *ndc_req;
	int rc = -ENOSPC, off;
	uint64_t ptr;

	/* Procedure for disabling an aura/pool */
	plt_delay_us(10);

	/* Clear all the pointers from the aura */
	do {
		ptr = roc_npa_aura_op_alloc(aura_handle, 0);
	} while (ptr);

	pool_req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (pool_req == NULL)
		return rc;
	pool_req->aura_id = aura_id;
	pool_req->ctype = NPA_AQ_CTYPE_POOL;
	pool_req->op = NPA_AQ_INSTOP_WRITE;
	pool_req->pool.ena = 0;
	pool_req->pool_mask.ena = ~pool_req->pool_mask.ena;

	aura_req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (aura_req == NULL)
		return rc;
	aura_req->aura_id = aura_id;
	aura_req->ctype = NPA_AQ_CTYPE_AURA;
	aura_req->op = NPA_AQ_INSTOP_WRITE;
	aura_req->aura.ena = 0;
	aura_req->aura_mask.ena = ~aura_req->aura_mask.ena;

	rc = mbox_process(mbox);
	if (rc < 0)
		return rc;

	off = mbox->rx_start +
	      PLT_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	pool_rsp = (struct npa_aq_enq_rsp *)((uintptr_t)mdev->mbase + off);

	off = mbox->rx_start + pool_rsp->hdr.next_msgoff;
	aura_rsp = (struct npa_aq_enq_rsp *)((uintptr_t)mdev->mbase + off);

	if (aura_rsp->hdr.rc != 0 || pool_rsp->hdr.rc != 0)
		return NPA_ERR_AURA_POOL_FINI;

	/* Sync NDC-NPA for LF */
	ndc_req = mbox_alloc_msg_ndc_sync_op(mbox);
	if (ndc_req == NULL)
		return -ENOSPC;
	ndc_req->npa_lf_sync = 1;
	rc = mbox_process(mbox);
	if (rc) {
		plt_err("Error on NDC-NPA LF sync, rc %d", rc);
		return NPA_ERR_AURA_POOL_FINI;
	}
	return 0;
}

int
roc_npa_pool_op_pc_reset(uint64_t aura_handle)
{
	struct npa_lf *lf = idev_npa_obj_get();
	struct npa_aq_enq_req *pool_req;
	struct npa_aq_enq_rsp *pool_rsp;
	struct ndc_sync_op *ndc_req;
	struct mbox_dev *mdev;
	int rc = -ENOSPC, off;
	struct mbox *mbox;

	if (lf == NULL)
		return NPA_ERR_PARAM;

	mbox = lf->mbox;
	mdev = &mbox->dev[0];
	plt_npa_dbg("lf=%p aura_handle=0x%" PRIx64, lf, aura_handle);

	pool_req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (pool_req == NULL)
		return rc;
	pool_req->aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	pool_req->ctype = NPA_AQ_CTYPE_POOL;
	pool_req->op = NPA_AQ_INSTOP_WRITE;
	pool_req->pool.op_pc = 0;
	pool_req->pool_mask.op_pc = ~pool_req->pool_mask.op_pc;

	rc = mbox_process(mbox);
	if (rc < 0)
		return rc;

	off = mbox->rx_start +
	      PLT_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	pool_rsp = (struct npa_aq_enq_rsp *)((uintptr_t)mdev->mbase + off);

	if (pool_rsp->hdr.rc != 0)
		return NPA_ERR_AURA_POOL_FINI;

	/* Sync NDC-NPA for LF */
	ndc_req = mbox_alloc_msg_ndc_sync_op(mbox);
	if (ndc_req == NULL)
		return -ENOSPC;
	ndc_req->npa_lf_sync = 1;
	rc = mbox_process(mbox);
	if (rc) {
		plt_err("Error on NDC-NPA LF sync, rc %d", rc);
		return NPA_ERR_AURA_POOL_FINI;
	}
	return 0;
}

int
roc_npa_aura_drop_set(uint64_t aura_handle, uint64_t limit, bool ena)
{
	struct npa_aq_enq_req *aura_req;
	struct npa_lf *lf;
	int rc;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	aura_req = mbox_alloc_msg_npa_aq_enq(lf->mbox);
	if (aura_req == NULL)
		return -ENOMEM;
	aura_req->aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	aura_req->ctype = NPA_AQ_CTYPE_AURA;
	aura_req->op = NPA_AQ_INSTOP_WRITE;

	aura_req->aura.aura_drop_ena = ena;
	aura_req->aura.aura_drop = limit;
	aura_req->aura_mask.aura_drop_ena =
		~(aura_req->aura_mask.aura_drop_ena);
	aura_req->aura_mask.aura_drop = ~(aura_req->aura_mask.aura_drop);
	rc = mbox_process(lf->mbox);

	return rc;
}

static inline char *
npa_stack_memzone_name(struct npa_lf *lf, int pool_id, char *name)
{
	snprintf(name, PLT_MEMZONE_NAMESIZE, "roc_npa_stack_%x_%d", lf->pf_func,
		 pool_id);
	return name;
}

static inline const struct plt_memzone *
npa_stack_dma_alloc(struct npa_lf *lf, char *name, int pool_id, size_t size)
{
	const char *mz_name = npa_stack_memzone_name(lf, pool_id, name);
	size = PLT_ALIGN_CEIL(size, ROC_ALIGN);

	return plt_memzone_reserve_aligned(mz_name, size, 0, ROC_ALIGN);
}

static inline int
npa_stack_dma_free(struct npa_lf *lf, char *name, int pool_id)
{
	const struct plt_memzone *mz;

	mz = plt_memzone_lookup(npa_stack_memzone_name(lf, pool_id, name));
	if (mz == NULL)
		return NPA_ERR_PARAM;

	return plt_memzone_free(mz);
}

static inline int
bitmap_ctzll(uint64_t slab)
{
	if (slab == 0)
		return 0;

	return __builtin_ctzll(slab);
}

static int
npa_aura_pool_pair_alloc(struct npa_lf *lf, const uint32_t block_size,
			 const uint32_t block_count, struct npa_aura_s *aura,
			 struct npa_pool_s *pool, uint64_t *aura_handle)
{
	int rc, aura_id, pool_id, stack_size, alloc_size;
	char name[PLT_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	uint64_t slab;
	uint32_t pos;

	/* Sanity check */
	if (!lf || !block_size || !block_count || !pool || !aura ||
	    !aura_handle)
		return NPA_ERR_PARAM;

	/* Block size should be cache line aligned and in range of 128B-128KB */
	if (block_size % ROC_ALIGN || block_size < 128 ||
	    block_size > ROC_NPA_MAX_BLOCK_SZ)
		return NPA_ERR_INVALID_BLOCK_SZ;

	pos = 0;
	slab = 0;
	/* Scan from the beginning */
	plt_bitmap_scan_init(lf->npa_bmp);
	/* Scan bitmap to get the free pool */
	rc = plt_bitmap_scan(lf->npa_bmp, &pos, &slab);
	/* Empty bitmap */
	if (rc == 0) {
		plt_err("Mempools exhausted");
		return NPA_ERR_AURA_ID_ALLOC;
	}

	/* Get aura_id from resource bitmap */
	aura_id = pos + bitmap_ctzll(slab);
	/* Mark pool as reserved */
	plt_bitmap_clear(lf->npa_bmp, aura_id);

	/* Configuration based on each aura has separate pool(aura-pool pair) */
	pool_id = aura_id;
	rc = (aura_id < 0 || pool_id >= (int)lf->nr_pools ||
	      aura_id >= (int)BIT_ULL(6 + lf->aura_sz)) ?
			   NPA_ERR_AURA_ID_ALLOC :
			   0;
	if (rc)
		goto exit;

	/* Allocate stack memory */
	stack_size = (block_count + lf->stack_pg_ptrs - 1) / lf->stack_pg_ptrs;
	alloc_size = stack_size * lf->stack_pg_bytes;

	mz = npa_stack_dma_alloc(lf, name, pool_id, alloc_size);
	if (mz == NULL) {
		rc = NPA_ERR_ALLOC;
		goto aura_res_put;
	}

	/* Update aura fields */
	aura->pool_addr = pool_id; /* AF will translate to associated poolctx */
	aura->ena = 1;
	aura->shift = plt_log2_u32(block_count);
	aura->shift = aura->shift < 8 ? 0 : aura->shift - 8;
	aura->limit = block_count;
	aura->pool_caching = 1;
	aura->err_int_ena = BIT(NPA_AURA_ERR_INT_AURA_ADD_OVER);
	aura->err_int_ena |= BIT(NPA_AURA_ERR_INT_AURA_ADD_UNDER);
	aura->err_int_ena |= BIT(NPA_AURA_ERR_INT_AURA_FREE_UNDER);
	aura->err_int_ena |= BIT(NPA_AURA_ERR_INT_POOL_DIS);
	aura->avg_con = 0;
	/* Many to one reduction */
	aura->err_qint_idx = aura_id % lf->qints;

	/* Update pool fields */
	pool->stack_base = mz->iova;
	pool->ena = 1;
	pool->buf_size = block_size / ROC_ALIGN;
	pool->stack_max_pages = stack_size;
	pool->shift = plt_log2_u32(block_count);
	pool->shift = pool->shift < 8 ? 0 : pool->shift - 8;
	pool->ptr_start = 0;
	pool->ptr_end = ~0;
	pool->stack_caching = 1;
	pool->err_int_ena = BIT(NPA_POOL_ERR_INT_OVFLS);
	pool->err_int_ena |= BIT(NPA_POOL_ERR_INT_RANGE);
	pool->err_int_ena |= BIT(NPA_POOL_ERR_INT_PERR);
	pool->avg_con = 0;

	/* Many to one reduction */
	pool->err_qint_idx = pool_id % lf->qints;

	/* Issue AURA_INIT and POOL_INIT op */
	rc = npa_aura_pool_init(lf->mbox, aura_id, aura, pool);
	if (rc)
		goto stack_mem_free;

	*aura_handle = roc_npa_aura_handle_gen(aura_id, lf->base);
	/* Update aura count */
	roc_npa_aura_op_cnt_set(*aura_handle, 0, block_count);
	/* Read it back to make sure aura count is updated */
	roc_npa_aura_op_cnt_get(*aura_handle);

	return 0;

stack_mem_free:
	plt_memzone_free(mz);
aura_res_put:
	plt_bitmap_set(lf->npa_bmp, aura_id);
exit:
	return rc;
}

int
roc_npa_pool_create(uint64_t *aura_handle, uint32_t block_size,
		    uint32_t block_count, struct npa_aura_s *aura,
		    struct npa_pool_s *pool)
{
	struct npa_aura_s defaura;
	struct npa_pool_s defpool;
	struct idev_cfg *idev;
	struct npa_lf *lf;
	int rc;

	lf = idev_npa_obj_get();
	if (lf == NULL) {
		rc = NPA_ERR_DEVICE_NOT_BOUNDED;
		goto error;
	}

	idev = idev_get_cfg();
	if (idev == NULL) {
		rc = NPA_ERR_ALLOC;
		goto error;
	}

	if (aura == NULL) {
		memset(&defaura, 0, sizeof(struct npa_aura_s));
		aura = &defaura;
	}
	if (pool == NULL) {
		memset(&defpool, 0, sizeof(struct npa_pool_s));
		defpool.nat_align = 1;
		defpool.buf_offset = 1;
		pool = &defpool;
	}

	rc = npa_aura_pool_pair_alloc(lf, block_size, block_count, aura, pool,
				      aura_handle);
	if (rc) {
		plt_err("Failed to alloc pool or aura rc=%d", rc);
		goto error;
	}

	plt_npa_dbg("lf=%p block_sz=%d block_count=%d aura_handle=0x%" PRIx64,
		    lf, block_size, block_count, *aura_handle);

	/* Just hold the reference of the object */
	__atomic_fetch_add(&idev->npa_refcnt, 1, __ATOMIC_SEQ_CST);
error:
	return rc;
}

int
roc_npa_aura_limit_modify(uint64_t aura_handle, uint16_t aura_limit)
{
	struct npa_aq_enq_req *aura_req;
	struct npa_lf *lf;
	int rc;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	aura_req = mbox_alloc_msg_npa_aq_enq(lf->mbox);
	if (aura_req == NULL)
		return -ENOMEM;
	aura_req->aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	aura_req->ctype = NPA_AQ_CTYPE_AURA;
	aura_req->op = NPA_AQ_INSTOP_WRITE;

	aura_req->aura.limit = aura_limit;
	aura_req->aura_mask.limit = ~(aura_req->aura_mask.limit);
	rc = mbox_process(lf->mbox);

	return rc;
}

static int
npa_aura_pool_pair_free(struct npa_lf *lf, uint64_t aura_handle)
{
	char name[PLT_MEMZONE_NAMESIZE];
	int aura_id, pool_id, rc;

	if (!lf || !aura_handle)
		return NPA_ERR_PARAM;

	aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	pool_id = aura_id;
	rc = npa_aura_pool_fini(lf->mbox, aura_id, aura_handle);
	rc |= npa_stack_dma_free(lf, name, pool_id);

	plt_bitmap_set(lf->npa_bmp, aura_id);

	return rc;
}

int
roc_npa_pool_destroy(uint64_t aura_handle)
{
	struct npa_lf *lf = idev_npa_obj_get();
	int rc = 0;

	plt_npa_dbg("lf=%p aura_handle=0x%" PRIx64, lf, aura_handle);
	rc = npa_aura_pool_pair_free(lf, aura_handle);
	if (rc)
		plt_err("Failed to destroy pool or aura rc=%d", rc);

	/* Release the reference of npa */
	rc |= npa_lf_fini();
	return rc;
}

int
roc_npa_pool_range_update_check(uint64_t aura_handle)
{
	uint64_t aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	struct npa_lf *lf;
	struct npa_aura_lim *lim;
	__io struct npa_pool_s *pool;
	struct npa_aq_enq_req *req;
	struct npa_aq_enq_rsp *rsp;
	int rc;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_PARAM;

	lim = lf->aura_lim;

	req = mbox_alloc_msg_npa_aq_enq(lf->mbox);
	if (req == NULL)
		return -ENOSPC;

	req->aura_id = aura_id;
	req->ctype = NPA_AQ_CTYPE_POOL;
	req->op = NPA_AQ_INSTOP_READ;

	rc = mbox_process_msg(lf->mbox, (void *)&rsp);
	if (rc) {
		plt_err("Failed to get pool(0x%" PRIx64 ") context", aura_id);
		return rc;
	}

	pool = &rsp->pool;
	if (lim[aura_id].ptr_start != pool->ptr_start ||
	    lim[aura_id].ptr_end != pool->ptr_end) {
		plt_err("Range update failed on pool(0x%" PRIx64 ")", aura_id);
		return NPA_ERR_PARAM;
	}

	return 0;
}

static inline int
npa_attach(struct mbox *mbox)
{
	struct rsrc_attach_req *req;

	req = mbox_alloc_msg_attach_resources(mbox);
	if (req == NULL)
		return -ENOSPC;
	req->modify = true;
	req->npalf = true;

	return mbox_process(mbox);
}

static inline int
npa_detach(struct mbox *mbox)
{
	struct rsrc_detach_req *req;

	req = mbox_alloc_msg_detach_resources(mbox);
	if (req == NULL)
		return -ENOSPC;
	req->partial = true;
	req->npalf = true;

	return mbox_process(mbox);
}

static inline int
npa_get_msix_offset(struct mbox *mbox, uint16_t *npa_msixoff)
{
	struct msix_offset_rsp *msix_rsp;
	int rc;

	/* Get NPA MSIX vector offsets */
	mbox_alloc_msg_msix_offset(mbox);
	rc = mbox_process_msg(mbox, (void *)&msix_rsp);
	if (rc == 0)
		*npa_msixoff = msix_rsp->npa_msixoff;

	return rc;
}

static inline int
npa_lf_alloc(struct npa_lf *lf)
{
	struct mbox *mbox = lf->mbox;
	struct npa_lf_alloc_req *req;
	struct npa_lf_alloc_rsp *rsp;
	int rc;

	req = mbox_alloc_msg_npa_lf_alloc(mbox);
	if (req == NULL)
		return -ENOSPC;
	req->aura_sz = lf->aura_sz;
	req->nr_pools = lf->nr_pools;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return NPA_ERR_ALLOC;

	lf->stack_pg_ptrs = rsp->stack_pg_ptrs;
	lf->stack_pg_bytes = rsp->stack_pg_bytes;
	lf->qints = rsp->qints;

	return 0;
}

static int
npa_lf_free(struct mbox *mbox)
{
	mbox_alloc_msg_npa_lf_free(mbox);
	return mbox_process(mbox);
}

static inline uint32_t
aura_size_to_u32(uint8_t val)
{
	if (val == NPA_AURA_SZ_0)
		return 128;
	if (val >= NPA_AURA_SZ_MAX)
		return BIT_ULL(20);

	return 1 << (val + 6);
}

static inline void
pool_count_aura_sz_get(uint32_t *nr_pools, uint8_t *aura_sz)
{
	uint32_t val;

	val = roc_idev_npa_maxpools_get();
	if (val < aura_size_to_u32(NPA_AURA_SZ_128))
		val = 128;
	if (val > aura_size_to_u32(NPA_AURA_SZ_1M))
		val = BIT_ULL(20);

	roc_idev_npa_maxpools_set(val);
	*nr_pools = val;
	*aura_sz = plt_log2_u32(val) - 6;
}

static int
npa_dev_init(struct npa_lf *lf, uintptr_t base, struct mbox *mbox)
{
	uint32_t i, bmp_sz, nr_pools;
	uint8_t aura_sz;
	int rc;

	/* Sanity checks */
	if (!lf || !base || !mbox)
		return NPA_ERR_PARAM;

	if (base & ROC_AURA_ID_MASK)
		return NPA_ERR_BASE_INVALID;

	pool_count_aura_sz_get(&nr_pools, &aura_sz);
	if (aura_sz == NPA_AURA_SZ_0 || aura_sz >= NPA_AURA_SZ_MAX)
		return NPA_ERR_PARAM;

	memset(lf, 0x0, sizeof(*lf));
	lf->base = base;
	lf->aura_sz = aura_sz;
	lf->nr_pools = nr_pools;
	lf->mbox = mbox;

	rc = npa_lf_alloc(lf);
	if (rc)
		goto exit;

	bmp_sz = plt_bitmap_get_memory_footprint(nr_pools);

	/* Allocate memory for bitmap */
	lf->npa_bmp_mem = plt_zmalloc(bmp_sz, ROC_ALIGN);
	if (lf->npa_bmp_mem == NULL) {
		rc = NPA_ERR_ALLOC;
		goto lf_free;
	}

	/* Initialize pool resource bitmap array */
	lf->npa_bmp = plt_bitmap_init(nr_pools, lf->npa_bmp_mem, bmp_sz);
	if (lf->npa_bmp == NULL) {
		rc = NPA_ERR_PARAM;
		goto bmap_mem_free;
	}

	/* Mark all pools available */
	for (i = 0; i < nr_pools; i++)
		plt_bitmap_set(lf->npa_bmp, i);

	/* Allocate memory for qint context */
	lf->npa_qint_mem = plt_zmalloc(sizeof(struct npa_qint) * nr_pools, 0);
	if (lf->npa_qint_mem == NULL) {
		rc = NPA_ERR_ALLOC;
		goto bmap_free;
	}

	/* Allocate memory for nap_aura_lim memory */
	lf->aura_lim = plt_zmalloc(sizeof(struct npa_aura_lim) * nr_pools, 0);
	if (lf->aura_lim == NULL) {
		rc = NPA_ERR_ALLOC;
		goto qint_free;
	}

	/* Init aura start & end limits */
	for (i = 0; i < nr_pools; i++) {
		lf->aura_lim[i].ptr_start = UINT64_MAX;
		lf->aura_lim[i].ptr_end = 0x0ull;
	}

	return 0;

qint_free:
	plt_free(lf->npa_qint_mem);
bmap_free:
	plt_bitmap_free(lf->npa_bmp);
bmap_mem_free:
	plt_free(lf->npa_bmp_mem);
lf_free:
	npa_lf_free(lf->mbox);
exit:
	return rc;
}

static int
npa_dev_fini(struct npa_lf *lf)
{
	if (!lf)
		return NPA_ERR_PARAM;

	plt_free(lf->aura_lim);
	plt_free(lf->npa_qint_mem);
	plt_bitmap_free(lf->npa_bmp);
	plt_free(lf->npa_bmp_mem);

	return npa_lf_free(lf->mbox);
}

int
npa_lf_init(struct dev *dev, struct plt_pci_device *pci_dev)
{
	struct idev_cfg *idev;
	uint16_t npa_msixoff;
	struct npa_lf *lf;
	int rc;

	idev = idev_get_cfg();
	if (idev == NULL)
		return NPA_ERR_ALLOC;

	/* Not the first PCI device */
	if (__atomic_fetch_add(&idev->npa_refcnt, 1, __ATOMIC_SEQ_CST) != 0)
		return 0;

	if (lf_init_cb) {
		rc = (*lf_init_cb)(pci_dev);
		if (rc)
			goto fail;
	}

	rc = npa_attach(dev->mbox);
	if (rc)
		goto fail;

	rc = npa_get_msix_offset(dev->mbox, &npa_msixoff);
	if (rc)
		goto npa_detach;

	lf = &dev->npa;
	rc = npa_dev_init(lf, dev->bar2 + (RVU_BLOCK_ADDR_NPA << 20),
			  dev->mbox);
	if (rc)
		goto npa_detach;

	lf->pf_func = dev->pf_func;
	lf->npa_msixoff = npa_msixoff;
	lf->intr_handle = pci_dev->intr_handle;
	lf->pci_dev = pci_dev;

	idev->npa_pf_func = dev->pf_func;
	idev->npa = lf;
	plt_wmb();

	rc = npa_register_irqs(lf);
	if (rc)
		goto npa_fini;

	plt_npa_dbg("npa=%p max_pools=%d pf_func=0x%x msix=0x%x", lf,
		    roc_idev_npa_maxpools_get(), lf->pf_func, npa_msixoff);

	return 0;

npa_fini:
	npa_dev_fini(idev->npa);
npa_detach:
	npa_detach(dev->mbox);
fail:
	__atomic_fetch_sub(&idev->npa_refcnt, 1, __ATOMIC_SEQ_CST);
	return rc;
}

int
npa_lf_fini(void)
{
	struct idev_cfg *idev;
	int rc = 0;

	idev = idev_get_cfg();
	if (idev == NULL)
		return NPA_ERR_ALLOC;

	/* Not the last PCI device */
	if (__atomic_sub_fetch(&idev->npa_refcnt, 1, __ATOMIC_SEQ_CST) != 0)
		return 0;

	npa_unregister_irqs(idev->npa);
	rc |= npa_dev_fini(idev->npa);
	rc |= npa_detach(idev->npa->mbox);
	idev_set_defaults(idev);

	return rc;
}

int
roc_npa_dev_init(struct roc_npa *roc_npa)
{
	struct plt_pci_device *pci_dev;
	struct npa *npa;
	struct dev *dev;
	int rc;

	if (roc_npa == NULL || roc_npa->pci_dev == NULL)
		return NPA_ERR_PARAM;

	PLT_STATIC_ASSERT(sizeof(struct npa) <= ROC_NPA_MEM_SZ);
	npa = roc_npa_to_npa_priv(roc_npa);
	memset(npa, 0, sizeof(*npa));
	pci_dev = roc_npa->pci_dev;
	dev = &npa->dev;

	/* Initialize device  */
	rc = dev_init(dev, pci_dev);
	if (rc) {
		plt_err("Failed to init roc device");
		goto fail;
	}

	npa->pci_dev = pci_dev;
	dev->drv_inited = true;
fail:
	return rc;
}

int
roc_npa_dev_fini(struct roc_npa *roc_npa)
{
	struct npa *npa = roc_npa_to_npa_priv(roc_npa);

	if (npa == NULL)
		return NPA_ERR_PARAM;

	npa->dev.drv_inited = false;
	return dev_fini(&npa->dev, npa->pci_dev);
}
