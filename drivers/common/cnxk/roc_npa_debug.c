/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define npa_dump(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

static inline void
npa_pool_dump(__io struct npa_pool_s *pool)
{
	npa_dump("W0: Stack base\t\t0x%" PRIx64 "", pool->stack_base);
	npa_dump("W1: ena \t\t%d\nW1: nat_align \t\t%d\nW1: stack_caching \t%d",
		 pool->ena, pool->nat_align, pool->stack_caching);
	npa_dump("W1: stack_way_mask\t%d\nW1: buf_offset\t\t%d",
		 pool->stack_way_mask, pool->buf_offset);
	npa_dump("W1: buf_size \t\t%d", pool->buf_size);

	npa_dump("W2: stack_max_pages \t%d\nW2: stack_pages\t\t%d",
		 pool->stack_max_pages, pool->stack_pages);

	npa_dump("W3: op_pc \t\t0x%" PRIx64 "", (uint64_t)pool->op_pc);

	npa_dump("W4: stack_offset\t%d\nW4: shift\t\t%d\nW4: avg_level\t\t%d",
		 pool->stack_offset, pool->shift, pool->avg_level);
	npa_dump("W4: avg_con \t\t%d\nW4: fc_ena\t\t%d\nW4: fc_stype\t\t%d",
		 pool->avg_con, pool->fc_ena, pool->fc_stype);
	npa_dump("W4: fc_hyst_bits\t%d\nW4: fc_up_crossing\t%d",
		 pool->fc_hyst_bits, pool->fc_up_crossing);
	npa_dump("W4: update_time\t\t%d\n", pool->update_time);

	npa_dump("W5: fc_addr\t\t0x%" PRIx64 "\n", pool->fc_addr);

	npa_dump("W6: ptr_start\t\t0x%" PRIx64 "\n", pool->ptr_start);

	npa_dump("W7: ptr_end\t\t0x%" PRIx64 "\n", pool->ptr_end);
	npa_dump("W8: err_int\t\t%d\nW8: err_int_ena\t\t%d", pool->err_int,
		 pool->err_int_ena);
	npa_dump("W8: thresh_int\t\t%d", pool->thresh_int);

	npa_dump("W8: thresh_int_ena\t%d\nW8: thresh_up\t\t%d",
		 pool->thresh_int_ena, pool->thresh_up);
	npa_dump("W8: thresh_qint_idx\t%d\nW8: err_qint_idx\t%d",
		 pool->thresh_qint_idx, pool->err_qint_idx);
}

static inline void
npa_aura_dump(__io struct npa_aura_s *aura)
{
	npa_dump("W0: Pool addr\t\t0x%" PRIx64 "\n", aura->pool_addr);

	npa_dump("W1: ena\t\t\t%d\nW1: pool caching\t%d\nW1: pool way mask\t%d",
		 aura->ena, aura->pool_caching, aura->pool_way_mask);
	npa_dump("W1: avg con\t\t%d\nW1: pool drop ena\t%d", aura->avg_con,
		 aura->pool_drop_ena);
	npa_dump("W1: aura drop ena\t%d", aura->aura_drop_ena);
	npa_dump("W1: bp_ena\t\t%d\nW1: aura drop\t\t%d\nW1: aura shift\t\t%d",
		 aura->bp_ena, aura->aura_drop, aura->shift);
	npa_dump("W1: avg_level\t\t%d\n", aura->avg_level);

	npa_dump("W2: count\t\t%" PRIx64 "\nW2: nix0_bpid\t\t%d",
		 (uint64_t)aura->count, aura->nix0_bpid);
	npa_dump("W2: nix1_bpid\t\t%d", aura->nix1_bpid);

	npa_dump("W3: limit\t\t%" PRIx64 "\nW3: bp\t\t\t%d\nW3: fc_ena\t\t%d\n",
		 (uint64_t)aura->limit, aura->bp, aura->fc_ena);
	npa_dump("W3: fc_up_crossing\t%d\nW3: fc_stype\t\t%d",
		 aura->fc_up_crossing, aura->fc_stype);

	npa_dump("W3: fc_hyst_bits\t%d", aura->fc_hyst_bits);

	npa_dump("W4: fc_addr\t\t0x%" PRIx64 "\n", aura->fc_addr);

	npa_dump("W5: pool_drop\t\t%d\nW5: update_time\t\t%d", aura->pool_drop,
		 aura->update_time);
	npa_dump("W5: err_int\t\t%d", aura->err_int);
	npa_dump("W5: err_int_ena\t\t%d\nW5: thresh_int\t\t%d",
		 aura->err_int_ena, aura->thresh_int);
	npa_dump("W5: thresh_int_ena\t%d", aura->thresh_int_ena);

	npa_dump("W5: thresh_up\t\t%d\nW5: thresh_qint_idx\t%d",
		 aura->thresh_up, aura->thresh_qint_idx);
	npa_dump("W5: err_qint_idx\t%d", aura->err_qint_idx);

	npa_dump("W6: thresh\t\t%" PRIx64 "\n", (uint64_t)aura->thresh);
}

int
roc_npa_ctx_dump(void)
{
	struct npa_aq_enq_req *aq;
	struct npa_aq_enq_rsp *rsp;
	struct npa_lf *lf;
	uint32_t q;
	int rc = 0;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	for (q = 0; q < lf->nr_pools; q++) {
		/* Skip disabled POOL */
		if (plt_bitmap_get(lf->npa_bmp, q))
			continue;

		aq = mbox_alloc_msg_npa_aq_enq(lf->mbox);
		if (aq == NULL)
			return -ENOSPC;
		aq->aura_id = q;
		aq->ctype = NPA_AQ_CTYPE_POOL;
		aq->op = NPA_AQ_INSTOP_READ;

		rc = mbox_process_msg(lf->mbox, (void *)&rsp);
		if (rc) {
			plt_err("Failed to get pool(%d) context", q);
			return rc;
		}
		npa_dump("============== pool=%d ===============\n", q);
		npa_pool_dump(&rsp->pool);
	}

	for (q = 0; q < lf->nr_pools; q++) {
		/* Skip disabled AURA */
		if (plt_bitmap_get(lf->npa_bmp, q))
			continue;

		aq = mbox_alloc_msg_npa_aq_enq(lf->mbox);
		if (aq == NULL)
			return -ENOSPC;
		aq->aura_id = q;
		aq->ctype = NPA_AQ_CTYPE_AURA;
		aq->op = NPA_AQ_INSTOP_READ;

		rc = mbox_process_msg(lf->mbox, (void *)&rsp);
		if (rc) {
			plt_err("Failed to get aura(%d) context", q);
			return rc;
		}
		npa_dump("============== aura=%d ===============\n", q);
		npa_aura_dump(&rsp->aura);
	}

	return rc;
}

int
roc_npa_dump(void)
{
	struct npa_lf *lf;
	int aura_cnt = 0;
	uint32_t i;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	for (i = 0; i < lf->nr_pools; i++) {
		if (plt_bitmap_get(lf->npa_bmp, i))
			continue;
		aura_cnt++;
	}

	npa_dump("npa@%p", lf);
	npa_dump("  pf = %d", dev_get_pf(lf->pf_func));
	npa_dump("  vf = %d", dev_get_vf(lf->pf_func));
	npa_dump("  aura_cnt = %d", aura_cnt);
	npa_dump("  \tpci_dev = %p", lf->pci_dev);
	npa_dump("  \tnpa_bmp = %p", lf->npa_bmp);
	npa_dump("  \tnpa_bmp_mem = %p", lf->npa_bmp_mem);
	npa_dump("  \tnpa_qint_mem = %p", lf->npa_qint_mem);
	npa_dump("  \tintr_handle = %p", lf->intr_handle);
	npa_dump("  \tmbox = %p", lf->mbox);
	npa_dump("  \tbase = 0x%" PRIx64 "", lf->base);
	npa_dump("  \tstack_pg_ptrs = %d", lf->stack_pg_ptrs);
	npa_dump("  \tstack_pg_bytes = %d", lf->stack_pg_bytes);
	npa_dump("  \tnpa_msixoff = 0x%x", lf->npa_msixoff);
	npa_dump("  \tnr_pools = %d", lf->nr_pools);
	npa_dump("  \tpf_func = 0x%x", lf->pf_func);
	npa_dump("  \taura_sz = %d", lf->aura_sz);
	npa_dump("  \tqints = %d", lf->qints);

	return 0;
}
