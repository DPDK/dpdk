/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define npa_dump plt_dump

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

static inline void
npa_cn20k_pool_dump(__io struct npa_cn20k_pool_s *pool)
{
	npa_dump("W0: Stack base\t\t0x%" PRIx64 "\n", pool->stack_base);
	npa_dump("W1: ena \t\t%d\nW1: nat_align \t\t%d\nW1: stack_caching \t%d",
		 pool->ena, pool->nat_align, pool->stack_caching);
	npa_dump("W1: buf_offset \t\t%d", pool->buf_offset);
	npa_dump("W1: buf_size \t\t%d\nW1: ref_cnt_prof \t%d",
		 pool->buf_size, pool->ref_cnt_prof);

	npa_dump("W2: stack_max_pages \t%d\nW2: stack_pages\t\t%d\n",
		 pool->stack_max_pages, pool->stack_pages);

	npa_dump("W3: bp_0 \t\t%d\nW3: bp_1 \t\t%d\nW3: bp_2 \t\t%d",
		 pool->bp_0, pool->bp_1, pool->bp_2);
	npa_dump("W3: bp_3 \t\t%d\nW3: bp_4 \t\t%d\nW3: bp_5 \t\t%d",
		 pool->bp_3, pool->bp_4, pool->bp_5);
	npa_dump("W3: bp_6 \t\t%d\nW3: bp_7 \t\t%d", pool->bp_6, pool->bp_7);
	npa_dump("W3: bp_ena_0 \t\t%d\nW3: bp_ena_1 \t\t%d", pool->bp_ena_0,
		 pool->bp_ena_1);
	npa_dump("W3: bp_ena_2 \t\t%d\nW3: bp_ena_3 \t\t%d", pool->bp_ena_2,
		 pool->bp_ena_3);
	npa_dump("W3: bp_ena_4 \t\t%d\nW3: bp_ena_5 \t\t%d", pool->bp_ena_4,
		 pool->bp_ena_5);
	npa_dump("W3: bp_ena_6 \t\t%d\nW3: bp_ena_7 \t\t%d\n", pool->bp_6,
		 pool->bp_7);

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

	npa_dump("W8: bpid_0 \t\t%d", pool->bpid_0);
	npa_dump("W8: err_int\t\t%d\nW8: err_int_ena\t\t%d", pool->err_int,
		 pool->err_int_ena);
	npa_dump("W8: thresh_int\t\t%d", pool->thresh_int);
	npa_dump("W8: thresh_int_ena\t%d\nW8: thresh_up\t\t%d",
		 pool->thresh_int_ena, pool->thresh_up);
	npa_dump("W8: thresh_qint_idx\t%d\nW8: err_qint_idx\t%d\n",
		 pool->thresh_qint_idx, pool->err_qint_idx);
	npa_dump("W9: thresh\t\t%" PRIx64 "", (uint64_t)pool->thresh);
	npa_dump("W9: fc_msh_dst \t\t%d", pool->fc_msh_dst);
	npa_dump("W9: op_dpc_ena \t\t%d\nW9: op_dpc_set \t\t%d",
		 pool->op_dpc_ena, pool->op_dpc_set);
	npa_dump("W9: stream_ctx \t\t%d", pool->stream_ctx);
}

static inline void
npa_halo_dump(__io struct npa_cn20k_halo_s *halo)
{
	npa_dump("W0: Stack base\t\t0x%" PRIx64 "\n", halo->stack_base);
	npa_dump("W1: ena \t\t%d\nW1: nat_align \t\t%d\nW1: stack_caching \t%d",
		 halo->ena, halo->nat_align, halo->stack_caching);
	npa_dump("W1: buf_offset\t\t%d\nW1: buf_size\t\t%d",
		 halo->buf_offset, halo->buf_size);
	npa_dump("W1: ref_cnt_prof \t%d\n", halo->ref_cnt_prof);

	npa_dump("W2: stack_max_pages \t%d\nW2: stack_pages\t\t%d\n",
		 halo->stack_max_pages, halo->stack_pages);

	npa_dump("W3: bp_0 \t\t%d\nW3: bp_1 \t\t%d\nW3: bp_2 \t\t%d",
		 halo->bp_0, halo->bp_1, halo->bp_2);
	npa_dump("W3: bp_3 \t\t%d\nW3: bp_4 \t\t%d\nW3: bp_5 \t\t%d",
		 halo->bp_3, halo->bp_4, halo->bp_5);
	npa_dump("W3: bp_6 \t\t%d\nW3: bp_7 \t\t%d", halo->bp_6, halo->bp_7);

	npa_dump("W3: bp_ena_0 \t\t%d\nW3: bp_ena_1 \t\t%d", halo->bp_ena_0,
		 halo->bp_ena_1);
	npa_dump("W3: bp_ena_2 \t\t%d\nW3: bp_ena_3 \t\t%d", halo->bp_ena_2,
		 halo->bp_ena_3);
	npa_dump("W3: bp_ena_4 \t\t%d\nW3: bp_ena_5 \t\t%d", halo->bp_ena_4,
		 halo->bp_ena_5);
	npa_dump("W3: bp_ena_6 \t\t%d\nW3: bp_ena_7 \t\t%d\n", halo->bp_6,
		 halo->bp_7);

	npa_dump("W4: stack_offset\t%d\nW4: shift\t\t%d\nW4: avg_level\t\t%d",
		 halo->stack_offset, halo->shift, halo->avg_level);
	npa_dump("W4: avg_con \t\t%d\nW4: fc_ena\t\t%d\nW4: fc_stype\t\t%d",
		 halo->avg_con, halo->fc_ena, halo->fc_stype);
	npa_dump("W4: fc_hyst_bits\t%d\nW4: fc_up_crossing\t%d",
		 halo->fc_hyst_bits, halo->fc_up_crossing);
	npa_dump("W4: update_time\t\t%d\n", halo->update_time);

	npa_dump("W5: fc_addr\t\t0x%" PRIx64 "\n", halo->fc_addr);

	npa_dump("W6: ptr_start\t\t0x%" PRIx64 "\n", halo->ptr_start);

	npa_dump("W7: ptr_end\t\t0x%" PRIx64 "\n", halo->ptr_end);
	npa_dump("W8: bpid_0 \t\t%d", halo->bpid_0);
	npa_dump("W8: err_int\t\t%d\nW8: err_int_ena\t\t%d", halo->err_int,
		 halo->err_int_ena);
	npa_dump("W8: thresh_int\t\t%d", halo->thresh_int);

	npa_dump("W8: thresh_int_ena\t%d\nW8: thresh_up\t\t%d",
		 halo->thresh_int_ena, halo->thresh_up);
	npa_dump("W8: thresh_qint_idx\t%d\nW8: err_qint_idx\t%d\n",
		 halo->thresh_qint_idx, halo->err_qint_idx);
	npa_dump("W9: thresh \t\t%" PRIx64 "\nW9: fc_msh_dst \t\t%d",
		 (unsigned long)halo->thresh, halo->fc_msh_dst);
	npa_dump("W9: op_dpc_ena \t\t%d\nW9: op_dpc_set \t\t%d",
		 halo->op_dpc_ena, halo->op_dpc_set);
	npa_dump("W9: stream_ctx \t\t%d\nW9: unified_ctx \t%d",
		 halo->stream_ctx, halo->unified_ctx);
}

static inline void
npa_cn20k_aura_dump(__io struct npa_cn20k_aura_s *aura)
{
	npa_dump("W0: Pool addr\t\t0x%" PRIx64 "\n", aura->pool_addr);

	npa_dump("W1: ena\t\t\t%d\nW1: pool caching\t%d", aura->ena,
		 aura->pool_caching);
	npa_dump("W1: avg con\t\t%d\nW1: pool drop ena\t%d", aura->avg_con,
		 aura->pool_drop_ena);
	npa_dump("W1: aura drop ena\t%d", aura->aura_drop_ena);
	npa_dump("W1: bp_ena\t\t%d", aura->bp_ena);
	npa_dump("W1: aura drop\t\t%d\nW1: aura shift\t\t%d", aura->aura_drop,
		 aura->shift);
	npa_dump("W1: avg_level\t\t%d\n", aura->avg_level);

	npa_dump("W2: count\t\t%" PRIx64 "", (uint64_t)aura->count);
	npa_dump("W2: bpid\t\t%d\n", aura->bpid);

	npa_dump("W3: limit\t\t%" PRIx64 "", (uint64_t)aura->limit);
	npa_dump("W3: bp\t\t\t%d", aura->bp);
	npa_dump("W3: fc_ena\t\t%d", aura->fc_ena);
	npa_dump("W3: fc_up_crossing\t%d\nW3: fc_stype\t\t%d",
		 aura->fc_up_crossing, aura->fc_stype);

	npa_dump("W3: fc_hyst_bits\t%d\n", aura->fc_hyst_bits);

	npa_dump("W4: fc_addr\t\t0x%" PRIx64 "\n", aura->fc_addr);

	npa_dump("W5: pool_drop\t\t%d\nW5: update_time\t\t%d", aura->pool_drop,
		 aura->update_time);
	npa_dump("W5: err_int\t\t%d", aura->err_int);
	npa_dump("W5: err_int_ena\t\t%d\nW5: thresh_int\t\t%d",
		 aura->err_int_ena, aura->thresh_int);
	npa_dump("W5: thresh_int_ena\t%d", aura->thresh_int_ena);

	npa_dump("W5: thresh_up\t\t%d\nW5: thresh_qint_idx\t%d",
		 aura->thresh_up, aura->thresh_qint_idx);
	npa_dump("W5: err_qint_idx\t%d\n", aura->err_qint_idx);

	npa_dump("W6: thresh\t\t%" PRIx64 "", (uint64_t)aura->thresh);
	npa_dump("W6: fc_msh_dst \t\t%d", aura->fc_msh_dst);
	npa_dump("W6: op_dpc_ena \t\t%d\nW6: op_dpc_set \t\t%d",
		 aura->op_dpc_ena, aura->op_dpc_set);
	npa_dump("W6: stream_ctx \t\t%d", aura->stream_ctx);
}

int
roc_npa_ctx_dump(void)
{
	struct npa_cn20k_aq_enq_req *aq_cn20k;
	struct npa_cn20k_aq_enq_rsp *rsp_cn20k;
	struct npa_aq_enq_rsp *rsp;
	struct npa_aq_enq_req *aq;
	struct mbox *mbox;
	struct npa_lf *lf;
	uint32_t q;
	int rc = 0;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;
	mbox = mbox_get(lf->mbox);

	for (q = 0; q < lf->nr_pools; q++) {
		/* Skip disabled AURA */
		if (plt_bitmap_get(lf->npa_bmp, q))
			continue;

		if (roc_model_is_cn20k()) {
			aq_cn20k = mbox_alloc_msg_npa_cn20k_aq_enq(mbox);
			aq = (struct npa_aq_enq_req *)aq_cn20k;
		} else {
			aq = mbox_alloc_msg_npa_aq_enq(mbox);
		}
		if (aq == NULL) {
			rc = -ENOSPC;
			goto exit;
		}
		aq->aura_id = q;
		aq->op = NPA_AQ_INSTOP_READ;
		if (lf->aura_attr[q].halo) {
			aq->ctype = NPA_AQ_CTYPE_HALO;
			rc = mbox_process_msg(mbox, (void *)&rsp_cn20k);
		} else {
			aq->ctype = NPA_AQ_CTYPE_AURA;
			rc = mbox_process_msg(mbox, (void *)&rsp);
		}

		if (rc) {
			plt_err("Failed to get aura/halo(%d) context", q);
			goto exit;
		}
		if (lf->aura_attr[q].halo) {
			npa_dump("============== halo=%d ===============\n", q);
			npa_halo_dump(&rsp_cn20k->halo);
		} else {
			npa_dump("============== aura=%d ===============\n", q);
			if (roc_model_is_cn20k())
				npa_cn20k_aura_dump(&rsp_cn20k->aura);
			else
				npa_aura_dump(&rsp->aura);
		}
	}

	for (q = 0; q < lf->nr_pools; q++) {
		/* Skip halo and disabled POOL */
		if (plt_bitmap_get(lf->npa_bmp, q) || lf->aura_attr[q].halo)
			continue;

		if (roc_model_is_cn20k()) {
			aq_cn20k = mbox_alloc_msg_npa_cn20k_aq_enq(mbox);
			aq = (struct npa_aq_enq_req *)aq_cn20k;
		} else {
			aq = mbox_alloc_msg_npa_aq_enq(mbox);
		}
		if (aq == NULL) {
			rc = -ENOSPC;
			goto exit;
		}
		aq->aura_id = q;
		aq->ctype = NPA_AQ_CTYPE_POOL;
		aq->op = NPA_AQ_INSTOP_READ;

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc) {
			plt_err("Failed to get pool(%d) context", q);
			goto exit;
		}
		npa_dump("============== pool=%d ===============\n", q);
		if (roc_model_is_cn20k())
			npa_cn20k_pool_dump(&rsp_cn20k->pool);
		else
			npa_pool_dump(&rsp->pool);
	}

exit:
	mbox_put(mbox);
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
