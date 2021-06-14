/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_mempool.h>
#include <rte_vect.h>

#include "otx2_mempool.h"

static int __rte_hot
otx2_npa_enq(struct rte_mempool *mp, void * const *obj_table, unsigned int n)
{
	unsigned int index; const uint64_t aura_handle = mp->pool_id;
	const uint64_t reg = npa_lf_aura_handle_to_aura(aura_handle);
	const uint64_t addr = npa_lf_aura_handle_to_base(aura_handle) +
				 NPA_LF_AURA_OP_FREE0;

	/* Ensure mbuf init changes are written before the free pointers
	 * are enqueued to the stack.
	 */
	rte_io_wmb();
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

#if defined(RTE_ARCH_ARM64)
static __rte_noinline int
npa_lf_aura_op_search_alloc(const int64_t wdata, int64_t * const addr,
		void **obj_table, unsigned int n)
{
	uint8_t i;

	for (i = 0; i < n; i++) {
		if (obj_table[i] != NULL)
			continue;
		if (npa_lf_aura_op_alloc_one(wdata, addr, obj_table, i))
			return -ENOENT;
	}

	return 0;
}

static __rte_noinline int
npa_lf_aura_op_alloc_bulk(const int64_t wdata, int64_t * const addr,
			  unsigned int n, void **obj_table)
{
	register const uint64_t wdata64 __asm("x26") = wdata;
	register const uint64_t wdata128 __asm("x27") = wdata;
	uint64x2_t failed = vdupq_n_u64(~0);

	switch (n) {
	case 32:
	{
		asm volatile (
		".cpu  generic+lse\n"
		"casp x0, x1, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x2, x3, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x4, x5, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x6, x7, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x8, x9, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x10, x11, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x12, x13, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x14, x15, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x16, x17, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x18, x19, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x20, x21, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x22, x23, %[wdata64], %[wdata128], [%[loc]]\n"
		"fmov d16, x0\n"
		"fmov v16.D[1], x1\n"
		"casp x0, x1, %[wdata64], %[wdata128], [%[loc]]\n"
		"fmov d17, x2\n"
		"fmov v17.D[1], x3\n"
		"casp x2, x3, %[wdata64], %[wdata128], [%[loc]]\n"
		"fmov d18, x4\n"
		"fmov v18.D[1], x5\n"
		"casp x4, x5, %[wdata64], %[wdata128], [%[loc]]\n"
		"fmov d19, x6\n"
		"fmov v19.D[1], x7\n"
		"casp x6, x7, %[wdata64], %[wdata128], [%[loc]]\n"
		"and %[failed].16B, %[failed].16B, v16.16B\n"
		"and %[failed].16B, %[failed].16B, v17.16B\n"
		"and %[failed].16B, %[failed].16B, v18.16B\n"
		"and %[failed].16B, %[failed].16B, v19.16B\n"
		"fmov d20, x8\n"
		"fmov v20.D[1], x9\n"
		"fmov d21, x10\n"
		"fmov v21.D[1], x11\n"
		"fmov d22, x12\n"
		"fmov v22.D[1], x13\n"
		"fmov d23, x14\n"
		"fmov v23.D[1], x15\n"
		"and %[failed].16B, %[failed].16B, v20.16B\n"
		"and %[failed].16B, %[failed].16B, v21.16B\n"
		"and %[failed].16B, %[failed].16B, v22.16B\n"
		"and %[failed].16B, %[failed].16B, v23.16B\n"
		"st1 { v16.2d, v17.2d, v18.2d, v19.2d}, [%[dst]], 64\n"
		"st1 { v20.2d, v21.2d, v22.2d, v23.2d}, [%[dst]], 64\n"
		"fmov d16, x16\n"
		"fmov v16.D[1], x17\n"
		"fmov d17, x18\n"
		"fmov v17.D[1], x19\n"
		"fmov d18, x20\n"
		"fmov v18.D[1], x21\n"
		"fmov d19, x22\n"
		"fmov v19.D[1], x23\n"
		"and %[failed].16B, %[failed].16B, v16.16B\n"
		"and %[failed].16B, %[failed].16B, v17.16B\n"
		"and %[failed].16B, %[failed].16B, v18.16B\n"
		"and %[failed].16B, %[failed].16B, v19.16B\n"
		"fmov d20, x0\n"
		"fmov v20.D[1], x1\n"
		"fmov d21, x2\n"
		"fmov v21.D[1], x3\n"
		"fmov d22, x4\n"
		"fmov v22.D[1], x5\n"
		"fmov d23, x6\n"
		"fmov v23.D[1], x7\n"
		"and %[failed].16B, %[failed].16B, v20.16B\n"
		"and %[failed].16B, %[failed].16B, v21.16B\n"
		"and %[failed].16B, %[failed].16B, v22.16B\n"
		"and %[failed].16B, %[failed].16B, v23.16B\n"
		"st1 { v16.2d, v17.2d, v18.2d, v19.2d}, [%[dst]], 64\n"
		"st1 { v20.2d, v21.2d, v22.2d, v23.2d}, [%[dst]], 64\n"
		: "+Q" (*addr), [failed] "=&w" (failed)
		: [wdata64] "r" (wdata64), [wdata128] "r" (wdata128),
		[dst] "r" (obj_table), [loc] "r" (addr)
		: "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
		"x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16",
		"x17", "x18", "x19", "x20", "x21", "x22", "x23", "v16", "v17",
		"v18", "v19", "v20", "v21", "v22", "v23"
		);
		break;
	}
	case 16:
	{
		asm volatile (
		".cpu  generic+lse\n"
		"casp x0, x1, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x2, x3, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x4, x5, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x6, x7, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x8, x9, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x10, x11, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x12, x13, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x14, x15, %[wdata64], %[wdata128], [%[loc]]\n"
		"fmov d16, x0\n"
		"fmov v16.D[1], x1\n"
		"fmov d17, x2\n"
		"fmov v17.D[1], x3\n"
		"fmov d18, x4\n"
		"fmov v18.D[1], x5\n"
		"fmov d19, x6\n"
		"fmov v19.D[1], x7\n"
		"and %[failed].16B, %[failed].16B, v16.16B\n"
		"and %[failed].16B, %[failed].16B, v17.16B\n"
		"and %[failed].16B, %[failed].16B, v18.16B\n"
		"and %[failed].16B, %[failed].16B, v19.16B\n"
		"fmov d20, x8\n"
		"fmov v20.D[1], x9\n"
		"fmov d21, x10\n"
		"fmov v21.D[1], x11\n"
		"fmov d22, x12\n"
		"fmov v22.D[1], x13\n"
		"fmov d23, x14\n"
		"fmov v23.D[1], x15\n"
		"and %[failed].16B, %[failed].16B, v20.16B\n"
		"and %[failed].16B, %[failed].16B, v21.16B\n"
		"and %[failed].16B, %[failed].16B, v22.16B\n"
		"and %[failed].16B, %[failed].16B, v23.16B\n"
		"st1 { v16.2d, v17.2d, v18.2d, v19.2d}, [%[dst]], 64\n"
		"st1 { v20.2d, v21.2d, v22.2d, v23.2d}, [%[dst]], 64\n"
		: "+Q" (*addr), [failed] "=&w" (failed)
		: [wdata64] "r" (wdata64), [wdata128] "r" (wdata128),
		[dst] "r" (obj_table), [loc] "r" (addr)
		: "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
		"x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "v16",
		"v17", "v18", "v19", "v20", "v21", "v22", "v23"
		);
		break;
	}
	case 8:
	{
		asm volatile (
		".cpu  generic+lse\n"
		"casp x0, x1, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x2, x3, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x4, x5, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x6, x7, %[wdata64], %[wdata128], [%[loc]]\n"
		"fmov d16, x0\n"
		"fmov v16.D[1], x1\n"
		"fmov d17, x2\n"
		"fmov v17.D[1], x3\n"
		"fmov d18, x4\n"
		"fmov v18.D[1], x5\n"
		"fmov d19, x6\n"
		"fmov v19.D[1], x7\n"
		"and %[failed].16B, %[failed].16B, v16.16B\n"
		"and %[failed].16B, %[failed].16B, v17.16B\n"
		"and %[failed].16B, %[failed].16B, v18.16B\n"
		"and %[failed].16B, %[failed].16B, v19.16B\n"
		"st1 { v16.2d, v17.2d, v18.2d, v19.2d}, [%[dst]], 64\n"
		: "+Q" (*addr), [failed] "=&w" (failed)
		: [wdata64] "r" (wdata64), [wdata128] "r" (wdata128),
		[dst] "r" (obj_table), [loc] "r" (addr)
		: "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
		"v16", "v17", "v18", "v19"
		);
		break;
	}
	case 4:
	{
		asm volatile (
		".cpu  generic+lse\n"
		"casp x0, x1, %[wdata64], %[wdata128], [%[loc]]\n"
		"casp x2, x3, %[wdata64], %[wdata128], [%[loc]]\n"
		"fmov d16, x0\n"
		"fmov v16.D[1], x1\n"
		"fmov d17, x2\n"
		"fmov v17.D[1], x3\n"
		"and %[failed].16B, %[failed].16B, v16.16B\n"
		"and %[failed].16B, %[failed].16B, v17.16B\n"
		"st1 { v16.2d, v17.2d}, [%[dst]], 32\n"
		: "+Q" (*addr), [failed] "=&w" (failed)
		: [wdata64] "r" (wdata64), [wdata128] "r" (wdata128),
		[dst] "r" (obj_table), [loc] "r" (addr)
		: "memory", "x0", "x1", "x2", "x3", "v16", "v17"
		);
		break;
	}
	case 2:
	{
		asm volatile (
		".cpu  generic+lse\n"
		"casp x0, x1, %[wdata64], %[wdata128], [%[loc]]\n"
		"fmov d16, x0\n"
		"fmov v16.D[1], x1\n"
		"and %[failed].16B, %[failed].16B, v16.16B\n"
		"st1 { v16.2d}, [%[dst]], 16\n"
		: "+Q" (*addr), [failed] "=&w" (failed)
		: [wdata64] "r" (wdata64), [wdata128] "r" (wdata128),
		[dst] "r" (obj_table), [loc] "r" (addr)
		: "memory", "x0", "x1", "v16"
		);
		break;
	}
	case 1:
		return npa_lf_aura_op_alloc_one(wdata, addr, obj_table, 0);
	}

	if (unlikely(!(vgetq_lane_u64(failed, 0) & vgetq_lane_u64(failed, 1))))
		return npa_lf_aura_op_search_alloc(wdata, addr, (void **)
			((char *)obj_table - (sizeof(uint64_t) * n)), n);

	return 0;
}

static __rte_noinline void
otx2_npa_clear_alloc(struct rte_mempool *mp, void **obj_table, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		if (obj_table[i] != NULL) {
			otx2_npa_enq(mp, &obj_table[i], 1);
			obj_table[i] = NULL;
		}
	}
}

static __rte_noinline int __rte_hot
otx2_npa_deq_arm64(struct rte_mempool *mp, void **obj_table, unsigned int n)
{
	const int64_t wdata = npa_lf_aura_handle_to_aura(mp->pool_id);
	void **obj_table_bak = obj_table;
	const unsigned int nfree = n;
	unsigned int parts;

	int64_t * const addr = (int64_t * const)
			(npa_lf_aura_handle_to_base(mp->pool_id) +
				NPA_LF_AURA_OP_ALLOCX(0));
	while (n) {
		parts = n > 31 ? 32 : rte_align32prevpow2(n);
		n -= parts;
		if (unlikely(npa_lf_aura_op_alloc_bulk(wdata, addr,
				parts, obj_table))) {
			otx2_npa_clear_alloc(mp, obj_table_bak, nfree - n);
			return -ENOENT;
		}
		obj_table += parts;
	}

	return 0;
}

#else

static inline int __rte_hot
otx2_npa_deq(struct rte_mempool *mp, void **obj_table, unsigned int n)
{
	const int64_t wdata = npa_lf_aura_handle_to_aura(mp->pool_id);
	unsigned int index;
	uint64_t obj;

	int64_t * const addr = (int64_t *)
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

#endif

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
	struct otx2_idev_cfg *idev;
	int rc, off;

	idev = otx2_intra_dev_get_cfg();
	if (idev == NULL)
		return -ENOMEM;

	aura_init_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);

	aura_init_req->aura_id = aura_id;
	aura_init_req->ctype = NPA_AQ_CTYPE_AURA;
	aura_init_req->op = NPA_AQ_INSTOP_INIT;
	otx2_mbox_memcpy(&aura_init_req->aura, aura, sizeof(*aura));

	pool_init_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);

	pool_init_req->aura_id = aura_id;
	pool_init_req->ctype = NPA_AQ_CTYPE_POOL;
	pool_init_req->op = NPA_AQ_INSTOP_INIT;
	otx2_mbox_memcpy(&pool_init_req->pool, pool, sizeof(*pool));

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

	if (!(idev->npa_lock_mask & BIT_ULL(aura_id)))
		return 0;

	aura_init_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);
	aura_init_req->aura_id = aura_id;
	aura_init_req->ctype = NPA_AQ_CTYPE_AURA;
	aura_init_req->op = NPA_AQ_INSTOP_LOCK;

	pool_init_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);
	if (!pool_init_req) {
		/* The shared memory buffer can be full.
		 * Flush it and retry
		 */
		otx2_mbox_msg_send(mbox, 0);
		rc = otx2_mbox_wait_for_rsp(mbox, 0);
		if (rc < 0) {
			otx2_err("Failed to LOCK AURA context");
			return -ENOMEM;
		}

		pool_init_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);
		if (!pool_init_req) {
			otx2_err("Failed to LOCK POOL context");
			return -ENOMEM;
		}
	}
	pool_init_req->aura_id = aura_id;
	pool_init_req->ctype = NPA_AQ_CTYPE_POOL;
	pool_init_req->op = NPA_AQ_INSTOP_LOCK;

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to lock POOL ctx to NDC");
		return -ENOMEM;
	}

	return 0;
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
	struct otx2_idev_cfg *idev;
	int rc, off;

	idev = otx2_intra_dev_get_cfg();
	if (idev == NULL)
		return -EINVAL;

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

	if (!(idev->npa_lock_mask & BIT_ULL(aura_id)))
		return 0;

	aura_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);
	aura_req->aura_id = aura_id;
	aura_req->ctype = NPA_AQ_CTYPE_AURA;
	aura_req->op = NPA_AQ_INSTOP_UNLOCK;

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to unlock AURA ctx to NDC");
		return -EINVAL;
	}

	pool_req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);
	pool_req->aura_id = aura_id;
	pool_req->ctype = NPA_AQ_CTYPE_POOL;
	pool_req->op = NPA_AQ_INSTOP_UNLOCK;

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to unlock POOL ctx to NDC");
		return -EINVAL;
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
	aura->shift = rte_log2_u32(block_count);
	aura->shift = aura->shift < 8 ? 0 : aura->shift - 8;
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
	pool->shift = rte_log2_u32(block_count);
	pool->shift = pool->shift < 8 ? 0 : pool->shift - 8;
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
npa_lf_aura_range_update_check(uint64_t aura_handle)
{
	uint64_t aura_id = npa_lf_aura_handle_to_aura(aura_handle);
	struct otx2_npa_lf *lf = otx2_npa_lf_obj_get();
	struct npa_aura_lim *lim = lf->aura_lim;
	__otx2_io struct npa_pool_s *pool;
	struct npa_aq_enq_req *req;
	struct npa_aq_enq_rsp *rsp;
	int rc;

	req  = otx2_mbox_alloc_msg_npa_aq_enq(lf->mbox);

	req->aura_id = aura_id;
	req->ctype = NPA_AQ_CTYPE_POOL;
	req->op = NPA_AQ_INSTOP_READ;

	rc = otx2_mbox_process_msg(lf->mbox, (void *)&rsp);
	if (rc) {
		otx2_err("Failed to get pool(0x%"PRIx64") context", aura_id);
		return rc;
	}

	pool = &rsp->pool;

	if (lim[aura_id].ptr_start != pool->ptr_start ||
		lim[aura_id].ptr_end != pool->ptr_end) {
		otx2_err("Range update failed on pool(0x%"PRIx64")", aura_id);
		return -ERANGE;
	}

	return 0;
}

static int
otx2_npa_alloc(struct rte_mempool *mp)
{
	uint32_t block_size, block_count;
	uint64_t aura_handle = 0;
	struct otx2_npa_lf *lf;
	struct npa_aura_s aura;
	struct npa_pool_s pool;
	size_t padding;
	int rc;

	lf = otx2_npa_lf_obj_get();
	if (lf == NULL) {
		rc = -EINVAL;
		goto error;
	}

	block_size = mp->elt_size + mp->header_size + mp->trailer_size;
	/*
	 * OCTEON TX2 has 8 sets, 41 ways L1D cache, VA<9:7> bits dictate
	 * the set selection.
	 * Add additional padding to ensure that the element size always
	 * occupies odd number of cachelines to ensure even distribution
	 * of elements among L1D cache sets.
	 */
	padding = ((block_size / RTE_CACHE_LINE_SIZE) % 2) ? 0 :
				RTE_CACHE_LINE_SIZE;
	mp->trailer_size += padding;
	block_size += padding;

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
	size_t total_elt_sz;

	/* Need space for one more obj on each chunk to fulfill
	 * alignment requirements.
	 */
	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;
	return rte_mempool_op_calc_mem_size_helper(mp, obj_num, pg_shift,
						total_elt_sz, min_chunk_size,
						align);
}

static uint8_t
otx2_npa_l1d_way_set_get(uint64_t iova)
{
	return (iova >> rte_log2_u32(RTE_CACHE_LINE_SIZE)) & 0x7;
}

static int
otx2_npa_populate(struct rte_mempool *mp, unsigned int max_objs, void *vaddr,
		  rte_iova_t iova, size_t len,
		  rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
#define OTX2_L1D_NB_SETS	8
	uint64_t distribution[OTX2_L1D_NB_SETS];
	rte_iova_t start_iova;
	size_t total_elt_sz;
	uint8_t set;
	size_t off;
	int i;

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

	memset(distribution, 0, sizeof(uint64_t) * OTX2_L1D_NB_SETS);
	start_iova = iova;
	while (start_iova < iova + len) {
		set = otx2_npa_l1d_way_set_get(start_iova + mp->header_size);
		distribution[set]++;
		start_iova += total_elt_sz;
	}

	otx2_npa_dbg("iova %"PRIx64", aligned iova %"PRIx64"", iova - off,
		     iova);
	otx2_npa_dbg("length %"PRIu64", aligned length %"PRIu64"",
		     (uint64_t)(len + off), (uint64_t)len);
	otx2_npa_dbg("element size %"PRIu64"", (uint64_t)total_elt_sz);
	otx2_npa_dbg("requested objects %"PRIu64", possible objects %"PRIu64"",
		     (uint64_t)max_objs, (uint64_t)(len / total_elt_sz));
	otx2_npa_dbg("L1D set distribution :");
	for (i = 0; i < OTX2_L1D_NB_SETS; i++)
		otx2_npa_dbg("set[%d] : objects : %"PRIu64"", i,
			     distribution[i]);

	npa_lf_aura_op_range_set(mp->pool_id, iova, iova + len);

	if (npa_lf_aura_range_update_check(mp->pool_id) < 0)
		return -EBUSY;

	return rte_mempool_op_populate_helper(mp,
					RTE_MEMPOOL_POPULATE_F_ALIGN_OBJ,
					max_objs, vaddr, iova, len,
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
#if defined(RTE_ARCH_ARM64)
	.dequeue = otx2_npa_deq_arm64,
#else
	.dequeue = otx2_npa_deq,
#endif
};

MEMPOOL_REGISTER_OPS(otx2_npa_ops);
