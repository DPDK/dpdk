/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_ethdev.h"
#include "otx2_flow.h"

static int
flow_mcam_alloc_counter(struct otx2_mbox *mbox, uint16_t *ctr)
{
	struct npc_mcam_alloc_counter_req *req;
	struct npc_mcam_alloc_counter_rsp *rsp;
	int rc;

	req = otx2_mbox_alloc_msg_npc_mcam_alloc_counter(mbox);
	req->count = 1;
	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, (void *)&rsp);

	*ctr = rsp->cntr_list[0];
	return rc;
}

int
otx2_flow_mcam_free_counter(struct otx2_mbox *mbox, uint16_t ctr_id)
{
	struct npc_mcam_oper_counter_req *req;
	int rc;

	req = otx2_mbox_alloc_msg_npc_mcam_free_counter(mbox);
	req->cntr = ctr_id;
	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, NULL);

	return rc;
}

int
otx2_flow_mcam_read_counter(struct otx2_mbox *mbox, uint32_t ctr_id,
			    uint64_t *count)
{
	struct npc_mcam_oper_counter_req *req;
	struct npc_mcam_oper_counter_rsp *rsp;
	int rc;

	req = otx2_mbox_alloc_msg_npc_mcam_counter_stats(mbox);
	req->cntr = ctr_id;
	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, (void *)&rsp);

	*count = rsp->stat;
	return rc;
}

int
otx2_flow_mcam_clear_counter(struct otx2_mbox *mbox, uint32_t ctr_id)
{
	struct npc_mcam_oper_counter_req *req;
	int rc;

	req = otx2_mbox_alloc_msg_npc_mcam_clear_counter(mbox);
	req->cntr = ctr_id;
	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, NULL);

	return rc;
}

int
otx2_flow_mcam_free_entry(struct otx2_mbox *mbox, uint32_t entry)
{
	struct npc_mcam_free_entry_req *req;
	int rc;

	req = otx2_mbox_alloc_msg_npc_mcam_free_entry(mbox);
	req->entry = entry;
	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, NULL);

	return rc;
}

int
otx2_flow_mcam_free_all_entries(struct otx2_mbox *mbox)
{
	struct npc_mcam_free_entry_req *req;
	int rc;

	req = otx2_mbox_alloc_msg_npc_mcam_free_entry(mbox);
	req->all = 1;
	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, NULL);

	return rc;
}

static void
flow_prep_mcam_ldata(uint8_t *ptr, const uint8_t *data, int len)
{
	int idx;

	for (idx = 0; idx < len; idx++)
		ptr[idx] = data[len - 1 - idx];
}

static int
flow_check_copysz(size_t size, size_t len)
{
	if (len <= size)
		return len;
	return -1;
}

static inline int
flow_mem_is_zero(const void *mem, int len)
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
flow_set_hw_mask(struct otx2_flow_item_info *info,
		 struct npc_xtract_info *xinfo,
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
otx2_flow_get_hw_supp_mask(struct otx2_parse_state *pst,
			   struct otx2_flow_item_info *info, int lid, int lt)
{
	struct npc_xtract_info *xinfo, *lfinfo;
	char *hw_mask = info->hw_mask;
	int lf_cfg;
	int i, j;
	int intf;

	intf = pst->flow->nix_intf;
	xinfo = pst->npc->prx_dxcfg[intf][lid][lt].xtract;
	memset(hw_mask, 0, info->len);

	for (i = 0; i < NPC_MAX_LD; i++) {
		flow_set_hw_mask(info, &xinfo[i], hw_mask);
	}

	for (i = 0; i < NPC_MAX_LD; i++) {

		if (xinfo[i].flags_enable == 0)
			continue;

		lf_cfg = pst->npc->prx_lfcfg[i].i;
		if (lf_cfg == lid) {
			for (j = 0; j < NPC_MAX_LFL; j++) {
				lfinfo = pst->npc->prx_fxcfg[intf]
					[i][j].xtract;
				flow_set_hw_mask(info, &lfinfo[0], hw_mask);
			}
		}
	}
}

static int
flow_update_extraction_data(struct otx2_parse_state *pst,
			    struct otx2_flow_item_info *info,
			    struct npc_xtract_info *xinfo)
{
	uint8_t int_info_mask[NPC_MAX_EXTRACT_DATA_LEN];
	uint8_t int_info[NPC_MAX_EXTRACT_DATA_LEN];
	struct npc_xtract_info *x;
	int k, idx, hdr_off;
	int len = 0;

	x = xinfo;
	len = x->len;
	hdr_off = x->hdr_off;

	if (hdr_off < info->hw_hdr_len)
		return 0;

	if (x->enable == 0)
		return 0;

	otx2_npc_dbg("x->hdr_off = %d, len = %d, info->len = %d,"
		     "x->key_off = %d", x->hdr_off, len, info->len,
		     x->key_off);

	hdr_off -= info->hw_hdr_len;

	if (hdr_off + len > info->len)
		len = info->len - hdr_off;

	/* Check for over-write of previous layer */
	if (!flow_mem_is_zero(pst->mcam_mask + x->key_off,
			      len)) {
		/* Cannot support this data match */
		rte_flow_error_set(pst->error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   pst->pattern,
				   "Extraction unsupported");
		return -rte_errno;
	}

	len = flow_check_copysz((OTX2_MAX_MCAM_WIDTH_DWORDS * 8)
				- x->key_off,
				len);
	if (len < 0) {
		rte_flow_error_set(pst->error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   pst->pattern,
				   "Internal Error");
		return -rte_errno;
	}

	/* Need to reverse complete structure so that dest addr is at
	 * MSB so as to program the MCAM using mcam_data & mcam_mask
	 * arrays
	 */
	flow_prep_mcam_ldata(int_info,
			     (const uint8_t *)info->spec + hdr_off,
			     x->len);
	flow_prep_mcam_ldata(int_info_mask,
			     (const uint8_t *)info->mask + hdr_off,
			     x->len);

	otx2_npc_dbg("Spec: ");
	for (k = 0; k < info->len; k++)
		otx2_npc_dbg("0x%.2x ",
			     ((const uint8_t *)info->spec)[k]);

	otx2_npc_dbg("Int_info: ");
	for (k = 0; k < info->len; k++)
		otx2_npc_dbg("0x%.2x ", int_info[k]);

	memcpy(pst->mcam_mask + x->key_off, int_info_mask, len);
	memcpy(pst->mcam_data + x->key_off, int_info, len);

	otx2_npc_dbg("Parse state mcam data & mask");
	for (idx = 0; idx < len ; idx++)
		otx2_npc_dbg("data[%d]: 0x%x, mask[%d]: 0x%x", idx,
			     *(pst->mcam_data + idx + x->key_off), idx,
			     *(pst->mcam_mask + idx + x->key_off));
	return 0;
}

int
otx2_flow_update_parse_state(struct otx2_parse_state *pst,
			     struct otx2_flow_item_info *info, int lid, int lt,
			     uint8_t flags)
{
	struct npc_lid_lt_xtract_info *xinfo;
	struct otx2_flow_dump_data *dump;
	struct npc_xtract_info *lfinfo;
	int intf, lf_cfg;
	int i, j, rc = 0;

	otx2_npc_dbg("Parse state function info mask total %s",
		     (const uint8_t *)info->mask);

	pst->layer_mask |= lid;
	pst->lt[lid] = lt;
	pst->flags[lid] = flags;

	intf = pst->flow->nix_intf;
	xinfo = &pst->npc->prx_dxcfg[intf][lid][lt];
	otx2_npc_dbg("Is_terminating = %d", xinfo->is_terminating);
	if (xinfo->is_terminating)
		pst->terminate = 1;

	if (info->spec == NULL) {
		otx2_npc_dbg("Info spec NULL");
		goto done;
	}

	for (i = 0; i < NPC_MAX_LD; i++) {
		rc = flow_update_extraction_data(pst, info, &xinfo->xtract[i]);
		if (rc != 0)
			return rc;
	}

	for (i = 0; i < NPC_MAX_LD; i++) {
		if (xinfo->xtract[i].flags_enable == 0)
			continue;

		lf_cfg = pst->npc->prx_lfcfg[i].i;
		if (lf_cfg == lid) {
			for (j = 0; j < NPC_MAX_LFL; j++) {
				lfinfo = pst->npc->prx_fxcfg[intf]
					[i][j].xtract;
				rc = flow_update_extraction_data(pst, info,
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
	/* Next pattern to parse by subsequent layers */
	pst->pattern++;
	return 0;
}

static inline int
flow_range_is_valid(const char *spec, const char *last, const char *mask,
		    int len)
{
	/* Mask must be zero or equal to spec as we do not support
	 * non-contiguous ranges.
	 */
	while (len--) {
		if (last[len] &&
		    (spec[len] & mask[len]) != (last[len] & mask[len]))
			return 0; /* False */
	}
	return 1;
}


static inline int
flow_mask_is_supported(const char *mask, const char *hw_mask, int len)
{
	/*
	 * If no hw_mask, assume nothing is supported.
	 * mask is never NULL
	 */
	if (hw_mask == NULL)
		return flow_mem_is_zero(mask, len);

	while (len--) {
		if ((mask[len] | hw_mask[len]) != hw_mask[len])
			return 0; /* False */
	}
	return 1;
}

int
otx2_flow_parse_item_basic(const struct rte_flow_item *item,
			   struct otx2_flow_item_info *info,
			   struct rte_flow_error *error)
{
	/* Item must not be NULL */
	if (item == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				   "Item is NULL");
		return -rte_errno;
	}
	/* If spec is NULL, both mask and last must be NULL, this
	 * makes it to match ANY value (eq to mask = 0).
	 * Setting either mask or last without spec is an error
	 */
	if (item->spec == NULL) {
		if (item->last == NULL && item->mask == NULL) {
			info->spec = NULL;
			return 0;
		}
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "mask or last set without spec");
		return -rte_errno;
	}

	/* We have valid spec */
	if (item->type != RTE_FLOW_ITEM_TYPE_RAW)
		info->spec = item->spec;

	/* If mask is not set, use default mask, err if default mask is
	 * also NULL.
	 */
	if (item->mask == NULL) {
		otx2_npc_dbg("Item mask null, using default mask");
		if (info->def_mask == NULL) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, item,
					   "No mask or default mask given");
			return -rte_errno;
		}
		info->mask = info->def_mask;
	} else {
		if (item->type != RTE_FLOW_ITEM_TYPE_RAW)
			info->mask = item->mask;
	}

	/* mask specified must be subset of hw supported mask
	 * mask | hw_mask == hw_mask
	 */
	if (!flow_mask_is_supported(info->mask, info->hw_mask, info->len)) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				   item, "Unsupported field in the mask");
		return -rte_errno;
	}

	/* Now we have spec and mask. OTX2 does not support non-contiguous
	 * range. We should have either:
	 * - spec & mask == last & mask or,
	 * - last == 0 or,
	 * - last == NULL
	 */
	if (item->last != NULL && !flow_mem_is_zero(item->last, info->len)) {
		if (!flow_range_is_valid(item->spec, item->last, info->mask,
					 info->len)) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, item,
					   "Unsupported range for match");
			return -rte_errno;
		}
	}

	return 0;
}

void
otx2_flow_keyx_compress(uint64_t *data, uint32_t nibble_mask)
{
	uint64_t cdata[2] = {0ULL, 0ULL}, nibble;
	int i, j = 0;

	for (i = 0; i < NPC_MAX_KEY_NIBBLES; i++) {
		if (nibble_mask & (1 << i)) {
			nibble = (data[i / 16] >> ((i & 0xf) * 4)) & 0xf;
			cdata[j / 16] |= (nibble << ((j & 0xf) * 4));
			j += 1;
		}
	}

	data[0] = cdata[0];
	data[1] = cdata[1];
}

static int
flow_first_set_bit(uint64_t slab)
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
flow_shift_lv_ent(struct otx2_mbox *mbox, struct rte_flow *flow,
		  struct otx2_npc_flow_info *flow_info,
		  uint32_t old_ent, uint32_t new_ent)
{
	struct npc_mcam_shift_entry_req *req;
	struct npc_mcam_shift_entry_rsp *rsp;
	struct otx2_flow_list *list;
	struct rte_flow *flow_iter;
	int rc = 0;

	otx2_npc_dbg("Old ent:%u new ent:%u priority:%u", old_ent, new_ent,
		     flow->priority);

	list = &flow_info->flow_list[flow->priority];

	/* Old entry is disabled & it's contents are moved to new_entry,
	 * new entry is enabled finally.
	 */
	req = otx2_mbox_alloc_msg_npc_mcam_shift_entry(mbox);
	req->curr_entry[0] = old_ent;
	req->new_entry[0] = new_ent;
	req->shift_count = 1;

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, (void *)&rsp);
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
flow_shift_ent(struct otx2_mbox *mbox, struct rte_flow *flow,
	       struct otx2_npc_flow_info *flow_info,
	       struct npc_mcam_alloc_entry_rsp *rsp, int dir, int prio_lvl)
{
	struct rte_bitmap *fr_bmp, *fr_bmp_rev, *lv_bmp, *lv_bmp_rev, *bmp;
	uint32_t e_fr = 0, e_lv = 0, e, e_id = 0, mcam_entries;
	uint64_t fr_bit_pos = 0, lv_bit_pos = 0, bit_pos = 0;
	/* Bit position within the slab */
	uint32_t sl_fr_bit_off = 0, sl_lv_bit_off = 0;
	/* Overall bit position of the start of slab */
	/* free & live entry index */
	int rc_fr = 0, rc_lv = 0, rc = 0, idx = 0;
	struct otx2_mcam_ents_info *ent_info;
	/* free & live bitmap slab */
	uint64_t sl_fr = 0, sl_lv = 0, *sl;

	fr_bmp = flow_info->free_entries[prio_lvl];
	fr_bmp_rev = flow_info->free_entries_rev[prio_lvl];
	lv_bmp = flow_info->live_entries[prio_lvl];
	lv_bmp_rev = flow_info->live_entries_rev[prio_lvl];
	ent_info = &flow_info->flow_entry_info[prio_lvl];
	mcam_entries = flow_info->mcam_entries;


	/* New entries allocated are always contiguous, but older entries
	 * already in free/live bitmap can be non-contiguous: so return
	 * shifted entries should be in non-contiguous format.
	 */
	while (idx <= rsp->count) {
		if (!sl_fr && !sl_lv) {
			/* Lower index elements to be exchanged */
			if (dir < 0) {
				rc_fr = rte_bitmap_scan(fr_bmp, &e_fr, &sl_fr);
				rc_lv = rte_bitmap_scan(lv_bmp, &e_lv, &sl_lv);
				otx2_npc_dbg("Fwd slab rc fr %u rc lv %u "
					     "e_fr %u e_lv %u", rc_fr, rc_lv,
					      e_fr, e_lv);
			} else {
				rc_fr = rte_bitmap_scan(fr_bmp_rev,
							&sl_fr_bit_off,
							&sl_fr);
				rc_lv = rte_bitmap_scan(lv_bmp_rev,
							&sl_lv_bit_off,
							&sl_lv);

				otx2_npc_dbg("Rev slab rc fr %u rc lv %u "
					     "e_fr %u e_lv %u", rc_fr, rc_lv,
					      e_fr, e_lv);
			}
		}

		if (rc_fr) {
			fr_bit_pos = flow_first_set_bit(sl_fr);
			e_fr = sl_fr_bit_off + fr_bit_pos;
			otx2_npc_dbg("Fr_bit_pos 0x%" PRIx64, fr_bit_pos);
		} else {
			e_fr = ~(0);
		}

		if (rc_lv) {
			lv_bit_pos = flow_first_set_bit(sl_lv);
			e_lv = sl_lv_bit_off + lv_bit_pos;
			otx2_npc_dbg("Lv_bit_pos 0x%" PRIx64, lv_bit_pos);
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
			otx2_npc_dbg("Fr e %u e_id %u", e, e_id);
		} else {
			bmp = lv_bmp;
			e = e_lv;
			sl = &sl_lv;
			bit_pos = lv_bit_pos;
			if (dir > 0)
				e_id = mcam_entries - e - 1;
			else
				e_id = e;

			otx2_npc_dbg("Lv e %u e_id %u", e, e_id);
			if (idx < rsp->count)
				rc =
				  flow_shift_lv_ent(mbox, flow,
						    flow_info, e_id,
						    rsp->entry + idx);
		}

		rte_bitmap_clear(bmp, e);
		rte_bitmap_set(bmp, rsp->entry + idx);
		/* Update entry list, use non-contiguous
		 * list now.
		 */
		rsp->entry_list[idx] = e_id;
		*sl &= ~(1 << bit_pos);

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
flow_validate_and_shift_prio_ent(struct otx2_mbox *mbox, struct rte_flow *flow,
				 struct otx2_npc_flow_info *flow_info,
				 struct npc_mcam_alloc_entry_rsp *rsp,
				 int req_prio)
{
	int prio_idx = 0, rc = 0, needs_shift = 0, idx, prio = flow->priority;
	struct otx2_mcam_ents_info *info = flow_info->flow_entry_info;
	int dir = (req_prio == NPC_MCAM_HIGHER_PRIO) ? 1 : -1;
	uint32_t tot_ent = 0;

	otx2_npc_dbg("Dir %d, priority = %d", dir, prio);

	if (dir < 0)
		prio_idx = flow_info->flow_max_priority - 1;

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
			otx2_npc_dbg("Rsp entry %u prio idx %u "
				     "max id %u", rsp->entry, prio_idx,
				      info[prio_idx].max_id);

			needs_shift = 1;
		} else if ((dir > 0) && (prio_idx != prio) &&
		     (rsp->entry < info[prio_idx].min_id) && tot_ent) {
			otx2_npc_dbg("Rsp entry %u prio idx %u "
				     "min id %u", rsp->entry, prio_idx,
				      info[prio_idx].min_id);
			needs_shift = 1;
		}

		otx2_npc_dbg("Needs_shift = %d", needs_shift);
		if (needs_shift) {
			needs_shift = 0;
			rc = flow_shift_ent(mbox, flow, flow_info, rsp, dir,
					    prio_idx);
		} else {
			for (idx = 0; idx < rsp->count; idx++)
				rsp->entry_list[idx] = rsp->entry + idx;
		}
	} while ((prio_idx != prio) && (prio_idx += dir));

	return rc;
}

static int
flow_find_ref_entry(struct otx2_npc_flow_info *flow_info, int *prio,
		    int prio_lvl)
{
	struct otx2_mcam_ents_info *info = flow_info->flow_entry_info;
	int step = 1;

	while (step < flow_info->flow_max_priority) {
		if (((prio_lvl + step) < flow_info->flow_max_priority) &&
		    info[prio_lvl + step].live_ent) {
			*prio = NPC_MCAM_HIGHER_PRIO;
			return info[prio_lvl + step].min_id;
		}

		if (((prio_lvl - step) >= 0) &&
		    info[prio_lvl - step].live_ent) {
			otx2_npc_dbg("Prio_lvl %u live %u", prio_lvl - step,
				     info[prio_lvl - step].live_ent);
			*prio = NPC_MCAM_LOWER_PRIO;
			return info[prio_lvl - step].max_id;
		}
		step++;
	}
	*prio = NPC_MCAM_ANY_PRIO;
	return 0;
}

static int
flow_fill_entry_cache(struct otx2_mbox *mbox, struct rte_flow *flow,
		      struct otx2_npc_flow_info *flow_info, uint32_t *free_ent)
{
	struct rte_bitmap *free_bmp, *free_bmp_rev, *live_bmp, *live_bmp_rev;
	struct npc_mcam_alloc_entry_rsp rsp_local;
	struct npc_mcam_alloc_entry_rsp *rsp_cmd;
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	struct otx2_mcam_ents_info *info;
	uint16_t ref_ent, idx;
	int rc, prio;

	info = &flow_info->flow_entry_info[flow->priority];
	free_bmp = flow_info->free_entries[flow->priority];
	free_bmp_rev = flow_info->free_entries_rev[flow->priority];
	live_bmp = flow_info->live_entries[flow->priority];
	live_bmp_rev = flow_info->live_entries_rev[flow->priority];

	ref_ent = flow_find_ref_entry(flow_info, &prio, flow->priority);

	req = otx2_mbox_alloc_msg_npc_mcam_alloc_entry(mbox);
	req->contig = 1;
	req->count = flow_info->flow_prealloc_size;
	req->priority = prio;
	req->ref_entry = ref_ent;

	otx2_npc_dbg("Fill cache ref entry %u prio %u", ref_ent, prio);

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, (void *)&rsp_cmd);
	if (rc)
		return rc;

	rsp = &rsp_local;
	memcpy(rsp, rsp_cmd, sizeof(*rsp));

	otx2_npc_dbg("Alloc entry %u count %u , prio = %d", rsp->entry,
		     rsp->count, prio);

	/* Non-first ent cache fill */
	if (prio != NPC_MCAM_ANY_PRIO) {
		flow_validate_and_shift_prio_ent(mbox, flow, flow_info, rsp,
						 prio);
	} else {
		/* Copy into response entry list */
		for (idx = 0; idx < rsp->count; idx++)
			rsp->entry_list[idx] = rsp->entry + idx;
	}

	otx2_npc_dbg("Fill entry cache rsp count %u", rsp->count);
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
		rte_bitmap_set(free_bmp, rsp->entry_list[idx]);
		rte_bitmap_set(free_bmp_rev, flow_info->mcam_entries -
			       rsp->entry_list[idx] - 1);

		otx2_npc_dbg("Final rsp entry %u rsp entry rev %u",
			     rsp->entry_list[idx],
		flow_info->mcam_entries - rsp->entry_list[idx] - 1);
	}

	otx2_npc_dbg("Cache free entry %u, rev = %u", *free_ent,
		     flow_info->mcam_entries - *free_ent - 1);
	info->live_ent++;
	rte_bitmap_set(live_bmp, *free_ent);
	rte_bitmap_set(live_bmp_rev, flow_info->mcam_entries - *free_ent - 1);

	return 0;
}

static int
flow_check_preallocated_entry_cache(struct otx2_mbox *mbox,
				    struct rte_flow *flow,
				    struct otx2_npc_flow_info *flow_info)
{
	struct rte_bitmap *free, *free_rev, *live, *live_rev;
	uint32_t pos = 0, free_ent = 0, mcam_entries;
	struct otx2_mcam_ents_info *info;
	uint64_t slab = 0;
	int rc;

	otx2_npc_dbg("Flow priority %u", flow->priority);

	info = &flow_info->flow_entry_info[flow->priority];

	free_rev = flow_info->free_entries_rev[flow->priority];
	free = flow_info->free_entries[flow->priority];
	live_rev = flow_info->live_entries_rev[flow->priority];
	live = flow_info->live_entries[flow->priority];
	mcam_entries = flow_info->mcam_entries;

	if (info->free_ent) {
		rc = rte_bitmap_scan(free, &pos, &slab);
		if (rc) {
			/* Get free_ent from free entry bitmap */
			free_ent = pos + __builtin_ctzll(slab);
			otx2_npc_dbg("Allocated from cache entry %u", free_ent);
			/* Remove from free bitmaps and add to live ones */
			rte_bitmap_clear(free, free_ent);
			rte_bitmap_set(live, free_ent);
			rte_bitmap_clear(free_rev,
					 mcam_entries - free_ent - 1);
			rte_bitmap_set(live_rev,
				       mcam_entries - free_ent - 1);

			info->free_ent--;
			info->live_ent++;
			return free_ent;
		}

		otx2_npc_dbg("No free entry:its a mess");
		return -1;
	}

	rc = flow_fill_entry_cache(mbox, flow, flow_info, &free_ent);
	if (rc)
		return rc;

	return free_ent;
}

int
otx2_flow_mcam_alloc_and_write(struct rte_flow *flow, struct otx2_mbox *mbox,
			       struct otx2_parse_state *pst,
			       struct otx2_npc_flow_info *flow_info)
{
	int use_ctr = (flow->ctr_id == NPC_COUNTER_NONE ? 0 : 1);
	struct npc_mcam_read_base_rule_rsp *base_rule_rsp;
	struct npc_mcam_write_entry_req *req;
	struct mcam_entry *base_entry;
	struct mbox_msghdr *rsp;
	uint16_t ctr = ~(0);
	int rc, idx;
	int entry;

	if (use_ctr) {
		rc = flow_mcam_alloc_counter(mbox, &ctr);
		if (rc)
			return rc;
	}

	entry = flow_check_preallocated_entry_cache(mbox, flow, flow_info);
	if (entry < 0) {
		otx2_err("Prealloc failed");
		otx2_flow_mcam_free_counter(mbox, ctr);
		return NPC_MCAM_ALLOC_FAILED;
	}

	if (pst->is_vf) {
		(void)otx2_mbox_alloc_msg_npc_read_base_steer_rule(mbox);
		rc = otx2_mbox_process_msg(mbox, (void *)&base_rule_rsp);
		if (rc) {
			otx2_err("Failed to fetch VF's base MCAM entry");
			return rc;
		}
		base_entry = &base_rule_rsp->entry_data;
		for (idx = 0; idx < OTX2_MAX_MCAM_WIDTH_DWORDS; idx++) {
			flow->mcam_data[idx] |= base_entry->kw[idx];
			flow->mcam_mask[idx] |= base_entry->kw_mask[idx];
		}
	}

	req = otx2_mbox_alloc_msg_npc_mcam_write_entry(mbox);
	req->set_cntr = use_ctr;
	req->cntr = ctr;
	req->entry = entry;
	otx2_npc_dbg("Alloc & write entry %u", entry);

	req->intf =
		(flow->nix_intf == OTX2_INTF_RX) ? NPC_MCAM_RX : NPC_MCAM_TX;
	req->enable_entry = 1;
	req->entry_data.action = flow->npc_action;
	req->entry_data.vtag_action = flow->vtag_action;

	for (idx = 0; idx < OTX2_MAX_MCAM_WIDTH_DWORDS; idx++) {
		req->entry_data.kw[idx] = flow->mcam_data[idx];
		req->entry_data.kw_mask[idx] = flow->mcam_mask[idx];
	}

	if (flow->nix_intf == OTX2_INTF_RX) {
		req->entry_data.kw[0] |= flow_info->channel;
		req->entry_data.kw_mask[0] |=  (BIT_ULL(12) - 1);
	} else {
		uint16_t pf_func = (flow->npc_action >> 48) & 0xffff;

		pf_func = htons(pf_func);
		req->entry_data.kw[0] |= ((uint64_t)pf_func << 32);
		req->entry_data.kw_mask[0] |= ((uint64_t)0xffff << 32);
	}

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, (void *)&rsp);
	if (rc != 0)
		return rc;

	flow->mcam_id = entry;
	if (use_ctr)
		flow->ctr_id = ctr;
	return 0;
}
