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
otx2_initialise_mcam_entry(struct otx2_mbox *mbox,
			   struct otx2_npc_flow_info *flow_info,
			   struct rte_flow *flow, int mcam_id)
{
	struct npc_mcam_write_entry_req *req;
	struct npc_mcam_write_entry_rsq *rsp;
	int rc = 0, idx;

	req = otx2_mbox_alloc_msg_npc_mcam_write_entry(mbox);
	if (req == NULL)
		return -ENOSPC;
	req->set_cntr = 0;
	req->cntr = 0;
	req->entry = mcam_id;

	req->intf = (flow->nix_intf == NIX_INTF_RX) ? NPC_MCAM_RX : NPC_MCAM_TX;
	req->enable_entry = 1;
	req->entry_data.action = flow->npc_action;
	req->entry_data.vtag_action = flow->vtag_action;

	for (idx = 0; idx < OTX2_MAX_MCAM_WIDTH_DWORDS; idx++) {
		req->entry_data.kw[idx] = 0x0;
		req->entry_data.kw_mask[idx] = 0x0;
	}

	if (flow->nix_intf == NIX_INTF_RX) {
		req->entry_data.kw[0] |= (uint64_t)flow_info->channel;
		req->entry_data.kw_mask[0] |= (BIT_ULL(12) - 1);
	} else {
		uint16_t pf_func = (flow->npc_action >> 4) & 0xffff;

		pf_func = rte_cpu_to_be_16(pf_func);
		req->entry_data.kw[0] |= ((uint64_t)pf_func << 32);
		req->entry_data.kw_mask[0] |= ((uint64_t)0xffff << 32);
	}

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc != 0) {
		otx2_err("npc: mcam initialisation write failed");
		return rc;
	}
	return 0;
}

static int
otx2_shift_mcam_entry(struct otx2_mbox *mbox, uint16_t old_ent,
		      uint16_t new_ent)
{
	struct npc_mcam_shift_entry_req *req;
	struct npc_mcam_shift_entry_rsp *rsp;
	int rc = -ENOSPC;

	/* Old entry is disabled & it's contents are moved to new_entry,
	 * new entry is enabled finally.
	 */
	req = otx2_mbox_alloc_msg_npc_mcam_shift_entry(mbox);
	if (req == NULL)
		return rc;
	req->curr_entry[0] = old_ent;
	req->new_entry[0] = new_ent;
	req->shift_count = 1;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	return 0;
}

enum SHIFT_DIR {
	SLIDE_ENTRIES_TO_LOWER_INDEX,
	SLIDE_ENTRIES_TO_HIGHER_INDEX,
};

static int
otx2_slide_mcam_entries(struct otx2_mbox *mbox,
			struct otx2_npc_flow_info *flow_info, int prio,
			uint16_t *free_mcam_id, int dir)
{
	uint16_t to_mcam_id = 0, from_mcam_id = 0;
	struct otx2_prio_flow_list_head *list;
	struct otx2_prio_flow_entry *curr = 0;
	int rc = 0;

	list = &flow_info->prio_flow_list[prio];

	to_mcam_id = *free_mcam_id;
	if (dir == SLIDE_ENTRIES_TO_HIGHER_INDEX)
		curr = TAILQ_LAST(list, otx2_prio_flow_list_head);
	else if (dir == SLIDE_ENTRIES_TO_LOWER_INDEX)
		curr = TAILQ_FIRST(list);

	while (curr) {
		from_mcam_id = curr->flow->mcam_id;
		if ((dir == SLIDE_ENTRIES_TO_HIGHER_INDEX &&
		     from_mcam_id < to_mcam_id) ||
		    (dir == SLIDE_ENTRIES_TO_LOWER_INDEX &&
		     from_mcam_id > to_mcam_id)) {
			/* Newly allocated entry and the source entry given to
			 * npc_mcam_shift_entry_req will be in disabled state.
			 * Initialise and enable before moving an entry into
			 * this mcam.
			 */
			rc = otx2_initialise_mcam_entry(mbox, flow_info,
							curr->flow, to_mcam_id);
			if (rc)
				return rc;
			rc = otx2_shift_mcam_entry(mbox, from_mcam_id,
						   to_mcam_id);
			if (rc)
				return rc;

			curr->flow->mcam_id = to_mcam_id;
			to_mcam_id = from_mcam_id;
		}

		if (dir == SLIDE_ENTRIES_TO_HIGHER_INDEX)
			curr = TAILQ_PREV(curr, otx2_prio_flow_list_head, next);
		else if (dir == SLIDE_ENTRIES_TO_LOWER_INDEX)
			curr = TAILQ_NEXT(curr, next);
	}

	*free_mcam_id = from_mcam_id;

	return 0;
}

/*
 * The mcam_alloc request is first made with NPC_MCAM_LOWER_PRIO with the last
 * entry in the requested priority level as the reference entry. If it fails,
 * the alloc request is retried with NPC_MCAM_HIGHER_PRIO with the first entry
 * in the next lower priority level as the reference entry. After obtaining
 * the free MCAM from kernel, we check if it is at the right user requested
 * priority level. If not, the flow rules are moved across MCAM entries till
 * the user requested priority levels are met.
 * The MCAM sorting algorithm works as below.
 * For any given free MCAM obtained from the kernel, there are 3 possibilities.
 * Case 1:
 * There are entries belonging to higher user priority level(numerically
 * lesser) in higher mcam indices. In this case, the entries with higher user
 * priority are slided towards lower indices and a free entry is created in the
 * higher indices.
 * Example:
 * Assume free entry = 1610, user requested priority = 2 and
 * max user priority levels = 5 with below entries in respective priority
 * levels.
 * 0: 1630, 1635, 1641
 * 1: 1646, 1650, 1651
 * 2: 1652, 1655, 1660
 * 3: 1661, 1662, 1663, 1664
 * 4: 1665, 1667, 1670
 *
 * Entries (1630, 1635, 1641, 1646, 1650, 1651) have to be slided down towards
 * lower indices.
 * Shifting sequence will be as below:
 *     1610 <- 1630 <- 1635 <- 1641 <- 1646 <- 1650 <- 1651
 * Entry 1651 will be free-ed for writing the new flow. This entry will now
 * become the head of priority level 2.
 *
 * Case 2:
 * There are entries belonging to lower user priority level (numerically
 * bigger) in lower mcam indices. In this case, the entries with lower user
 * priority are slided towards higher indices and a free entry is created in the
 * lower indices.
 *
 * Example:
 * free entry = 1653, user requested priority = 0
 * 0: 1630, 1635, 1641
 * 1: 1646, 1650, 1651
 * 2: 1652, 1655, 1660
 * 3: 1661, 1662, 1663, 1664
 * 4: 1665, 1667, 1670
 *
 * Entries (1646, 1650, 1651, 1652) have to be slided up towards higher
 * indices.
 * Shifting sequence will be as below:
 *     1646 -> 1650 -> 1651 -> 1652 -> 1653
 * Entry 1646 will be free-ed for writing the new flow. This entry will now
 * become the last element in priority level 0.
 *
 * Case 3:
 * Free mcam is at the right place, ie, all higher user priority level
 * mcams lie in lower indices and all lower user priority level mcams lie in
 * higher mcam indices.
 *
 * The priority level lists are scanned first for case (1) and if the
 * condition is found true, case(2) is skipped because they are mutually
 * exclusive. For example, consider below state.
 * 0: 1630, 1635, 1641
 * 1: 1646, 1650, 1651
 * 2: 1652, 1655, 1660
 * 3: 1661, 1662, 1663, 1664
 * 4: 1665, 1667, 1670
 * free entry = 1610, user requested priority = 2
 *
 * Case 1: Here the condition is;
 * "if (requested_prio > prio_idx && free_mcam < tail->flow->mcam_id ){}"
 * If this condition is true, it means at some higher priority level than
 * requested priority level, there are entries at lower indices than the given
 * free mcam. That is, we have found in levels 0,1 there is an mcam X which is
 * greater than 1610.
 * If, for any free entry and user req prio, the above condition is true, then
 * the below case(2) condition will always be false since the lists are kept
 * sorted. The case(2) condition is;
 *  "if (requested_prio < prio_idx && free_mcam > head->flow->mcam_id){}"
 * There can't be entries at lower indices at priority level higher
 * than the requested priority level. That is, here, at levels 3 & 4 there
 * cannot be any entry greater than 1610. Because all entries in 3 & 4 must be
 * greater than X which was found to be greater than 1610 earlier.
 */

static int
otx2_sort_mcams_by_user_prio_level(struct otx2_mbox *mbox,
				   struct otx2_prio_flow_entry *flow_list_entry,
				   struct otx2_npc_flow_info *flow_info,
				   struct npc_mcam_alloc_entry_rsp *rsp)
{
	int requested_prio = flow_list_entry->flow->priority;
	struct otx2_prio_flow_entry *head, *tail;
	struct otx2_prio_flow_list_head *list;
	uint16_t free_mcam = rsp->entry;
	bool do_reverse_scan = true;
	int prio_idx = 0, rc = 0;

	while (prio_idx <= flow_info->flow_max_priority - 1) {
		list = &flow_info->prio_flow_list[prio_idx];
		tail = TAILQ_LAST(list, otx2_prio_flow_list_head);

		/* requested priority is lower than current level
		 * ie, numerically req prio is higher
		 */
		if (requested_prio > prio_idx && tail) {
			/* but there are some mcams in current level
			 * at higher indices, ie, at priority lower
			 * than free_mcam.
			 */
			if (free_mcam < tail->flow->mcam_id) {
				rc = otx2_slide_mcam_entries(mbox, flow_info,
						prio_idx, &free_mcam,
						SLIDE_ENTRIES_TO_LOWER_INDEX);
				if (rc)
					return rc;
				do_reverse_scan = false;
			}
		}
		prio_idx++;
	}

	prio_idx = flow_info->flow_max_priority - 1;
	while (prio_idx && do_reverse_scan) {
		list = &flow_info->prio_flow_list[prio_idx];
		head = TAILQ_FIRST(list);

		/* requested priority is higher than current level
		 * ie, numerically req prio is lower
		 */
		if (requested_prio < prio_idx && head) {
			/* but free mcam is higher than lowest priority
			 * mcam in current level
			 */
			if (free_mcam > head->flow->mcam_id) {
				rc = otx2_slide_mcam_entries(mbox, flow_info,
						prio_idx, &free_mcam,
						SLIDE_ENTRIES_TO_HIGHER_INDEX);
				if (rc)
					return rc;
			}
		}
		prio_idx--;
	}
	rsp->entry = free_mcam;
	return rc;
}

static void
otx2_insert_into_flow_list(struct otx2_npc_flow_info *flow_info,
			   struct otx2_prio_flow_entry *entry)
{
	struct otx2_prio_flow_list_head *list;
	struct otx2_prio_flow_entry *curr;

	list = &flow_info->prio_flow_list[entry->flow->priority];
	curr = TAILQ_FIRST(list);

	if (curr) {
		while (curr) {
			if (entry->flow->mcam_id > curr->flow->mcam_id)
				curr = TAILQ_NEXT(curr, next);
			else
				break;
		}
		if (curr)
			TAILQ_INSERT_BEFORE(curr, entry, next);
		else
			TAILQ_INSERT_TAIL(list, entry, next);
	} else {
		TAILQ_INSERT_HEAD(list, entry, next);
	}
}

static int
otx2_allocate_mcam_entry(struct otx2_mbox *mbox, int prio,
			 struct npc_mcam_alloc_entry_rsp *rsp_local,
			 int ref_entry)
{
	struct npc_mcam_alloc_entry_rsp *rsp_cmd;
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	int rc = -ENOSPC;

	req = otx2_mbox_alloc_msg_npc_mcam_alloc_entry(mbox);
	if (req == NULL)
		return rc;
	req->contig = 1;
	req->count = 1;
	req->priority = prio;
	req->ref_entry = ref_entry;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp_cmd);
	if (rc)
		return rc;

	if (!rsp_cmd->count)
		return -ENOSPC;

	memcpy(rsp_local, rsp_cmd, sizeof(*rsp));

	return 0;
}

static void
otx2_find_mcam_ref_entry(struct rte_flow *flow,
			 struct otx2_npc_flow_info *flow_info, int *prio,
			 int *ref_entry, int dir)
{
	struct otx2_prio_flow_entry *head, *tail;
	struct otx2_prio_flow_list_head *list;
	int prio_idx = flow->priority;

	if (dir == NPC_MCAM_LOWER_PRIO) {
		while (prio_idx >= 0) {
			list = &flow_info->prio_flow_list[prio_idx];
			head = TAILQ_FIRST(list);
			if (head) {
				*prio = NPC_MCAM_LOWER_PRIO;
				*ref_entry = head->flow->mcam_id;
				return;
			}
			prio_idx--;
		}
	} else if (dir == NPC_MCAM_HIGHER_PRIO) {
		prio_idx = flow->priority;
		while (prio_idx <= flow_info->flow_max_priority - 1) {
			list = &flow_info->prio_flow_list[prio_idx];
			tail = TAILQ_LAST(list, otx2_prio_flow_list_head);
			if (tail) {
				*prio = NPC_MCAM_HIGHER_PRIO;
				*ref_entry = tail->flow->mcam_id;
				return;
			}
			prio_idx++;
		}
	}
	*prio = NPC_MCAM_ANY_PRIO;
	*ref_entry = 0;
}

static int
otx2_alloc_mcam_by_ref_entry(struct otx2_mbox *mbox, struct rte_flow *flow,
			     struct otx2_npc_flow_info *flow_info,
			     struct npc_mcam_alloc_entry_rsp *rsp_local)
{
	int prio, ref_entry = 0, rc = 0, dir = NPC_MCAM_LOWER_PRIO;
	bool retry_done = false;

retry:
	otx2_find_mcam_ref_entry(flow, flow_info, &prio, &ref_entry, dir);
	rc = otx2_allocate_mcam_entry(mbox, prio, rsp_local, ref_entry);
	if (rc && !retry_done) {
		otx2_info("npc: Lower priority entry not available. "
			 "Retrying for higher priority");

		dir = NPC_MCAM_HIGHER_PRIO;
		retry_done = true;
		goto retry;
	} else if (rc && retry_done) {
		return rc;
	}

	return 0;
}

static int
otx2_get_free_mcam_entry(struct otx2_mbox *mbox, struct rte_flow *flow,
			 struct otx2_npc_flow_info *flow_info)
{
	struct npc_mcam_alloc_entry_rsp rsp_local;
	struct otx2_prio_flow_entry *new_entry;
	int rc = 0;

	rc = otx2_alloc_mcam_by_ref_entry(mbox, flow, flow_info, &rsp_local);

	if (rc)
		return rc;

	new_entry = rte_zmalloc("otx2_rte_flow", sizeof(*new_entry), 0);
	if (!new_entry)
		return -ENOSPC;

	new_entry->flow = flow;

	otx2_npc_dbg("kernel allocated MCAM entry %d", rsp_local.entry);

	rc = otx2_sort_mcams_by_user_prio_level(mbox, new_entry, flow_info,
						&rsp_local);
	if (rc)
		goto err;

	otx2_npc_dbg("allocated MCAM entry after sorting %d", rsp_local.entry);
	flow->mcam_id = rsp_local.entry;
	otx2_insert_into_flow_list(flow_info, new_entry);

	return rsp_local.entry;
err:
	rte_free(new_entry);
	return rc;
}

void
otx2_delete_prio_list_entry(struct otx2_npc_flow_info *flow_info,
			    struct rte_flow *flow)
{
	struct otx2_prio_flow_list_head *list;
	struct otx2_prio_flow_entry *curr;

	list = &flow_info->prio_flow_list[flow->priority];
	curr = TAILQ_FIRST(list);

	if (!curr)
		return;

	while (curr) {
		if (flow->mcam_id == curr->flow->mcam_id) {
			TAILQ_REMOVE(list, curr, next);
			rte_free(curr);
			break;
		}
		curr = TAILQ_NEXT(curr, next);
	}
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

	entry = otx2_get_free_mcam_entry(mbox, flow, flow_info);
	if (entry < 0) {
		otx2_err("MCAM allocation failed");
		if (use_ctr)
			otx2_flow_mcam_free_counter(mbox, ctr);
		return NPC_MCAM_ALLOC_FAILED;
	}

	if (pst->is_vf && flow->nix_intf == OTX2_INTF_RX) {
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
