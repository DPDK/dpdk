/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include "roc_api.h"
#include "roc_priv.h"

static void
npc_prep_mcam_ldata(uint8_t *ptr, const uint8_t *data, int len)
{
	int idx;

	for (idx = 0; idx < len; idx++)
		ptr[idx] = data[len - 1 - idx];
}

static int
npc_check_copysz(size_t size, size_t len)
{
	if (len <= size)
		return len;
	return NPC_ERR_PARAM;
}

static inline int
npc_mem_is_zero(const void *mem, int len)
{
	const char *m = mem;
	int i;

	for (i = 0; i < len; i++) {
		if (m[i] != 0)
			return 0;
	}
	return 1;
}

static void
npc_set_hw_mask(struct npc_parse_item_info *info, struct npc_xtract_info *xinfo,
		char *hw_mask)
{
	int max_off, offset;
	int j;

	if (xinfo->enable == 0)
		return;

	if (xinfo->hdr_off < info->hw_hdr_len)
		return;

	max_off = xinfo->hdr_off + xinfo->len - info->hw_hdr_len;

	if (max_off > info->len)
		max_off = info->len;

	offset = xinfo->hdr_off - info->hw_hdr_len;
	for (j = offset; j < max_off; j++)
		hw_mask[j] = 0xff;
}

void
npc_get_hw_supp_mask(struct npc_parse_state *pst,
		     struct npc_parse_item_info *info, int lid, int lt)
{
	struct npc_xtract_info *xinfo, *lfinfo;
	char *hw_mask = info->hw_mask;
	int lf_cfg = 0;
	int i, j;
	int intf;

	intf = pst->nix_intf;
	xinfo = pst->npc->prx_dxcfg[intf][lid][lt].xtract;
	memset(hw_mask, 0, info->len);

	for (i = 0; i < NPC_MAX_LD; i++)
		npc_set_hw_mask(info, &xinfo[i], hw_mask);

	for (i = 0; i < NPC_MAX_LD; i++) {
		if (xinfo[i].flags_enable == 0)
			continue;

		lf_cfg = pst->npc->prx_lfcfg[i].i;
		if (lf_cfg == lid) {
			for (j = 0; j < NPC_MAX_LFL; j++) {
				lfinfo = pst->npc->prx_fxcfg[intf][i][j].xtract;
				npc_set_hw_mask(info, &lfinfo[0], hw_mask);
			}
		}
	}
}

static inline int
npc_mask_is_supported(const char *mask, const char *hw_mask, int len)
{
	/*
	 * If no hw_mask, assume nothing is supported.
	 * mask is never NULL
	 */
	if (hw_mask == NULL)
		return npc_mem_is_zero(mask, len);

	while (len--) {
		if ((mask[len] | hw_mask[len]) != hw_mask[len])
			return 0; /* False */
	}
	return 1;
}

int
npc_parse_item_basic(const struct roc_npc_item_info *item,
		     struct npc_parse_item_info *info)
{
	/* Item must not be NULL */
	if (item == NULL)
		return NPC_ERR_PARAM;

	/* Don't support ranges */
	if (item->last != NULL)
		return NPC_ERR_INVALID_RANGE;

	/* If spec is NULL, both mask and last must be NULL, this
	 * makes it to match ANY value (eq to mask = 0).
	 * Setting either mask or last without spec is an error
	 */
	if (item->spec == NULL) {
		if (item->last == NULL && item->mask == NULL) {
			info->spec = NULL;
			return 0;
		}
		return NPC_ERR_INVALID_SPEC;
	}

	/* We have valid spec */
	if (item->type != ROC_NPC_ITEM_TYPE_RAW)
		info->spec = item->spec;

	/* If mask is not set, use default mask, err if default mask is
	 * also NULL.
	 */
	if (item->mask == NULL) {
		if (info->def_mask == NULL)
			return NPC_ERR_PARAM;
		info->mask = info->def_mask;
	} else {
		if (item->type != ROC_NPC_ITEM_TYPE_RAW)
			info->mask = item->mask;
	}

	/* mask specified must be subset of hw supported mask
	 * mask | hw_mask == hw_mask
	 */
	if (!npc_mask_is_supported(info->mask, info->hw_mask, info->len))
		return NPC_ERR_INVALID_MASK;

	return 0;
}

static int
npc_update_extraction_data(struct npc_parse_state *pst,
			   struct npc_parse_item_info *info,
			   struct npc_xtract_info *xinfo)
{
	uint8_t int_info_mask[NPC_MAX_EXTRACT_DATA_LEN];
	uint8_t int_info[NPC_MAX_EXTRACT_DATA_LEN];
	struct npc_xtract_info *x;
	int hdr_off;
	int len = 0;

	x = xinfo;
	len = x->len;
	hdr_off = x->hdr_off;

	if (hdr_off < info->hw_hdr_len)
		return 0;

	if (x->enable == 0)
		return 0;

	hdr_off -= info->hw_hdr_len;

	if (hdr_off >= info->len)
		return 0;

	if (hdr_off + len > info->len)
		len = info->len - hdr_off;

	len = npc_check_copysz((ROC_NPC_MAX_MCAM_WIDTH_DWORDS * 8) - x->key_off,
			       len);
	if (len < 0)
		return NPC_ERR_INVALID_SIZE;

	/* Need to reverse complete structure so that dest addr is at
	 * MSB so as to program the MCAM using mcam_data & mcam_mask
	 * arrays
	 */
	npc_prep_mcam_ldata(int_info, (const uint8_t *)info->spec + hdr_off,
			    x->len);
	npc_prep_mcam_ldata(int_info_mask,
			    (const uint8_t *)info->mask + hdr_off, x->len);

	memcpy(pst->mcam_mask + x->key_off, int_info_mask, len);
	memcpy(pst->mcam_data + x->key_off, int_info, len);
	return 0;
}

int
npc_update_parse_state(struct npc_parse_state *pst,
		       struct npc_parse_item_info *info, int lid, int lt,
		       uint8_t flags)
{
	struct npc_lid_lt_xtract_info *xinfo;
	struct roc_npc_flow_dump_data *dump;
	struct npc_xtract_info *lfinfo;
	int intf, lf_cfg;
	int i, j, rc = 0;

	pst->layer_mask |= lid;
	pst->lt[lid] = lt;
	pst->flags[lid] = flags;

	intf = pst->nix_intf;
	xinfo = &pst->npc->prx_dxcfg[intf][lid][lt];
	if (xinfo->is_terminating)
		pst->terminate = 1;

	if (info->spec == NULL)
		goto done;

	for (i = 0; i < NPC_MAX_LD; i++) {
		rc = npc_update_extraction_data(pst, info, &xinfo->xtract[i]);
		if (rc != 0)
			return rc;
	}

	for (i = 0; i < NPC_MAX_LD; i++) {
		if (xinfo->xtract[i].flags_enable == 0)
			continue;

		lf_cfg = pst->npc->prx_lfcfg[i].i;
		if (lf_cfg == lid) {
			for (j = 0; j < NPC_MAX_LFL; j++) {
				lfinfo = pst->npc->prx_fxcfg[intf][i][j].xtract;
				rc = npc_update_extraction_data(pst, info,
								&lfinfo[0]);
				if (rc != 0)
					return rc;

				if (lfinfo[0].enable)
					pst->flags[lid] = j;
			}
		}
	}

done:
	dump = &pst->flow->dump_data[pst->flow->num_patterns++];
	dump->lid = lid;
	dump->ltype = lt;
	pst->pattern++;
	return 0;
}

static int
npc_first_set_bit(uint64_t slab)
{
	int num = 0;

	if ((slab & 0xffffffff) == 0) {
		num += 32;
		slab >>= 32;
	}
	if ((slab & 0xffff) == 0) {
		num += 16;
		slab >>= 16;
	}
	if ((slab & 0xff) == 0) {
		num += 8;
		slab >>= 8;
	}
	if ((slab & 0xf) == 0) {
		num += 4;
		slab >>= 4;
	}
	if ((slab & 0x3) == 0) {
		num += 2;
		slab >>= 2;
	}
	if ((slab & 0x1) == 0)
		num += 1;

	return num;
}

static int
npc_shift_lv_ent(struct mbox *mbox, struct roc_npc_flow *flow, struct npc *npc,
		 uint32_t old_ent, uint32_t new_ent)
{
	struct npc_mcam_shift_entry_req *req;
	struct npc_mcam_shift_entry_rsp *rsp;
	struct npc_flow_list *list;
	struct roc_npc_flow *flow_iter;
	int rc = -ENOSPC;

	list = &npc->flow_list[flow->priority];

	/* Old entry is disabled & it's contents are moved to new_entry,
	 * new entry is enabled finally.
	 */
	req = mbox_alloc_msg_npc_mcam_shift_entry(mbox);
	if (req == NULL)
		return rc;
	req->curr_entry[0] = old_ent;
	req->new_entry[0] = new_ent;
	req->shift_count = 1;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	/* Remove old node from list */
	TAILQ_FOREACH(flow_iter, list, next) {
		if (flow_iter->mcam_id == old_ent)
			TAILQ_REMOVE(list, flow_iter, next);
	}

	/* Insert node with new mcam id at right place */
	TAILQ_FOREACH(flow_iter, list, next) {
		if (flow_iter->mcam_id > new_ent)
			TAILQ_INSERT_BEFORE(flow_iter, flow, next);
	}
	return rc;
}

/* Exchange all required entries with a given priority level */
static int
npc_shift_ent(struct mbox *mbox, struct roc_npc_flow *flow, struct npc *npc,
	      struct npc_mcam_alloc_entry_rsp *rsp, int dir, int prio_lvl)
{
	struct plt_bitmap *fr_bmp, *fr_bmp_rev, *lv_bmp, *lv_bmp_rev, *bmp;
	uint32_t e_fr = 0, e_lv = 0, e, e_id = 0, mcam_entries;
	uint64_t fr_bit_pos = 0, lv_bit_pos = 0, bit_pos = 0;
	/* Bit position within the slab */
	uint32_t sl_fr_bit_off = 0, sl_lv_bit_off = 0;
	/* Overall bit position of the start of slab */
	/* free & live entry index */
	int rc_fr = 0, rc_lv = 0, rc = 0, idx = 0;
	struct npc_mcam_ents_info *ent_info;
	/* free & live bitmap slab */
	uint64_t sl_fr = 0, sl_lv = 0, *sl;

	fr_bmp = npc->free_entries[prio_lvl];
	fr_bmp_rev = npc->free_entries_rev[prio_lvl];
	lv_bmp = npc->live_entries[prio_lvl];
	lv_bmp_rev = npc->live_entries_rev[prio_lvl];
	ent_info = &npc->flow_entry_info[prio_lvl];
	mcam_entries = npc->mcam_entries;

	/* New entries allocated are always contiguous, but older entries
	 * already in free/live bitmap can be non-contiguous: so return
	 * shifted entries should be in non-contiguous format.
	 */
	while (idx <= rsp->count) {
		if (!sl_fr && !sl_lv) {
			/* Lower index elements to be exchanged */
			if (dir < 0) {
				rc_fr = plt_bitmap_scan(fr_bmp, &e_fr, &sl_fr);
				rc_lv = plt_bitmap_scan(lv_bmp, &e_lv, &sl_lv);
			} else {
				rc_fr = plt_bitmap_scan(fr_bmp_rev,
							&sl_fr_bit_off, &sl_fr);
				rc_lv = plt_bitmap_scan(lv_bmp_rev,
							&sl_lv_bit_off, &sl_lv);
			}
		}

		if (rc_fr) {
			fr_bit_pos = npc_first_set_bit(sl_fr);
			e_fr = sl_fr_bit_off + fr_bit_pos;
		} else {
			e_fr = ~(0);
		}

		if (rc_lv) {
			lv_bit_pos = npc_first_set_bit(sl_lv);
			e_lv = sl_lv_bit_off + lv_bit_pos;
		} else {
			e_lv = ~(0);
		}

		/* First entry is from free_bmap */
		if (e_fr < e_lv) {
			bmp = fr_bmp;
			e = e_fr;
			sl = &sl_fr;
			bit_pos = fr_bit_pos;
			if (dir > 0)
				e_id = mcam_entries - e - 1;
			else
				e_id = e;
		} else {
			bmp = lv_bmp;
			e = e_lv;
			sl = &sl_lv;
			bit_pos = lv_bit_pos;
			if (dir > 0)
				e_id = mcam_entries - e - 1;
			else
				e_id = e;

			if (idx < rsp->count)
				rc = npc_shift_lv_ent(mbox, flow, npc, e_id,
						      rsp->entry + idx);
		}

		plt_bitmap_clear(bmp, e);
		plt_bitmap_set(bmp, rsp->entry + idx);
		/* Update entry list, use non-contiguous
		 * list now.
		 */
		rsp->entry_list[idx] = e_id;
		*sl &= ~(1UL << bit_pos);

		/* Update min & max entry identifiers in current
		 * priority level.
		 */
		if (dir < 0) {
			ent_info->max_id = rsp->entry + idx;
			ent_info->min_id = e_id;
		} else {
			ent_info->max_id = e_id;
			ent_info->min_id = rsp->entry;
		}

		idx++;
	}
	return rc;
}

/* Validate if newly allocated entries lie in the correct priority zone
 * since NPC_MCAM_LOWER_PRIO & NPC_MCAM_HIGHER_PRIO don't ensure zone accuracy.
 * If not properly aligned, shift entries to do so
 */
static int
npc_validate_and_shift_prio_ent(struct mbox *mbox, struct roc_npc_flow *flow,
				struct npc *npc,
				struct npc_mcam_alloc_entry_rsp *rsp,
				int req_prio)
{
	int prio_idx = 0, rc = 0, needs_shift = 0, idx, prio = flow->priority;
	struct npc_mcam_ents_info *info = npc->flow_entry_info;
	int dir = (req_prio == NPC_MCAM_HIGHER_PRIO) ? 1 : -1;
	uint32_t tot_ent = 0;

	if (dir < 0)
		prio_idx = npc->flow_max_priority - 1;

	/* Only live entries needs to be shifted, free entries can just be
	 * moved by bits manipulation.
	 */

	/* For dir = -1(NPC_MCAM_LOWER_PRIO), when shifting,
	 * NPC_MAX_PREALLOC_ENT are exchanged with adjoining higher priority
	 * level entries(lower indexes).
	 *
	 * For dir = +1(NPC_MCAM_HIGHER_PRIO), during shift,
	 * NPC_MAX_PREALLOC_ENT are exchanged with adjoining lower priority
	 * level entries(higher indexes) with highest indexes.
	 */
	do {
		tot_ent = info[prio_idx].free_ent + info[prio_idx].live_ent;

		if (dir < 0 && prio_idx != prio &&
		    rsp->entry > info[prio_idx].max_id && tot_ent) {
			needs_shift = 1;
		} else if ((dir > 0) && (prio_idx != prio) &&
			   (rsp->entry < info[prio_idx].min_id) && tot_ent) {
			needs_shift = 1;
		}

		if (needs_shift) {
			needs_shift = 0;
			rc = npc_shift_ent(mbox, flow, npc, rsp, dir, prio_idx);
		} else {
			for (idx = 0; idx < rsp->count; idx++)
				rsp->entry_list[idx] = rsp->entry + idx;
		}
	} while ((prio_idx != prio) && (prio_idx += dir));

	return rc;
}

static int
npc_find_ref_entry(struct npc *npc, int *prio, int prio_lvl)
{
	struct npc_mcam_ents_info *info = npc->flow_entry_info;
	int step = 1;

	while (step < npc->flow_max_priority) {
		if (((prio_lvl + step) < npc->flow_max_priority) &&
		    info[prio_lvl + step].live_ent) {
			*prio = NPC_MCAM_HIGHER_PRIO;
			return info[prio_lvl + step].min_id;
		}

		if (((prio_lvl - step) >= 0) &&
		    info[prio_lvl - step].live_ent) {
			*prio = NPC_MCAM_LOWER_PRIO;
			return info[prio_lvl - step].max_id;
		}
		step++;
	}
	*prio = NPC_MCAM_ANY_PRIO;
	return 0;
}

static int
npc_fill_entry_cache(struct mbox *mbox, struct roc_npc_flow *flow,
		     struct npc *npc, uint32_t *free_ent)
{
	struct plt_bitmap *free_bmp, *free_bmp_rev, *live_bmp, *live_bmp_rev;
	struct npc_mcam_alloc_entry_rsp rsp_local;
	struct npc_mcam_alloc_entry_rsp *rsp_cmd;
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	struct npc_mcam_ents_info *info;
	int rc = -ENOSPC, prio;
	uint16_t ref_ent, idx;

	info = &npc->flow_entry_info[flow->priority];
	free_bmp = npc->free_entries[flow->priority];
	free_bmp_rev = npc->free_entries_rev[flow->priority];
	live_bmp = npc->live_entries[flow->priority];
	live_bmp_rev = npc->live_entries_rev[flow->priority];

	ref_ent = npc_find_ref_entry(npc, &prio, flow->priority);

	req = mbox_alloc_msg_npc_mcam_alloc_entry(mbox);
	if (req == NULL)
		return rc;
	req->contig = 1;
	req->count = npc->flow_prealloc_size;
	req->priority = prio;
	req->ref_entry = ref_ent;

	rc = mbox_process_msg(mbox, (void *)&rsp_cmd);
	if (rc)
		return rc;

	rsp = &rsp_local;
	memcpy(rsp, rsp_cmd, sizeof(*rsp));

	/* Non-first ent cache fill */
	if (prio != NPC_MCAM_ANY_PRIO) {
		npc_validate_and_shift_prio_ent(mbox, flow, npc, rsp, prio);
	} else {
		/* Copy into response entry list */
		for (idx = 0; idx < rsp->count; idx++)
			rsp->entry_list[idx] = rsp->entry + idx;
	}

	/* Update free entries, reverse free entries list,
	 * min & max entry ids.
	 */
	for (idx = 0; idx < rsp->count; idx++) {
		if (unlikely(rsp->entry_list[idx] < info->min_id))
			info->min_id = rsp->entry_list[idx];

		if (unlikely(rsp->entry_list[idx] > info->max_id))
			info->max_id = rsp->entry_list[idx];

		/* Skip entry to be returned, not to be part of free
		 * list.
		 */
		if (prio == NPC_MCAM_HIGHER_PRIO) {
			if (unlikely(idx == (rsp->count - 1))) {
				*free_ent = rsp->entry_list[idx];
				continue;
			}
		} else {
			if (unlikely(!idx)) {
				*free_ent = rsp->entry_list[idx];
				continue;
			}
		}
		info->free_ent++;
		plt_bitmap_set(free_bmp, rsp->entry_list[idx]);
		plt_bitmap_set(free_bmp_rev,
			       npc->mcam_entries - rsp->entry_list[idx] - 1);
	}

	info->live_ent++;
	plt_bitmap_set(live_bmp, *free_ent);
	plt_bitmap_set(live_bmp_rev, npc->mcam_entries - *free_ent - 1);

	return 0;
}

int
npc_check_preallocated_entry_cache(struct mbox *mbox, struct roc_npc_flow *flow,
				   struct npc *npc)
{
	struct plt_bitmap *free, *free_rev, *live, *live_rev;
	uint32_t pos = 0, free_ent = 0, mcam_entries;
	struct npc_mcam_ents_info *info;
	uint64_t slab = 0;
	int rc;

	info = &npc->flow_entry_info[flow->priority];

	free_rev = npc->free_entries_rev[flow->priority];
	free = npc->free_entries[flow->priority];
	live_rev = npc->live_entries_rev[flow->priority];
	live = npc->live_entries[flow->priority];
	mcam_entries = npc->mcam_entries;

	if (info->free_ent) {
		rc = plt_bitmap_scan(free, &pos, &slab);
		if (rc) {
			/* Get free_ent from free entry bitmap */
			free_ent = pos + __builtin_ctzll(slab);
			/* Remove from free bitmaps and add to live ones */
			plt_bitmap_clear(free, free_ent);
			plt_bitmap_set(live, free_ent);
			plt_bitmap_clear(free_rev, mcam_entries - free_ent - 1);
			plt_bitmap_set(live_rev, mcam_entries - free_ent - 1);

			info->free_ent--;
			info->live_ent++;
			return free_ent;
		}
		return NPC_ERR_INTERNAL;
	}

	rc = npc_fill_entry_cache(mbox, flow, npc, &free_ent);
	if (rc)
		return rc;

	return free_ent;
}
