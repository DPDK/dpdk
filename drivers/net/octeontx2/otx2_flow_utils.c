/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_ethdev.h"
#include "otx2_flow.h"

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

void
otx2_flow_get_hw_supp_mask(struct otx2_parse_state *pst,
			   struct otx2_flow_item_info *info, int lid, int lt)
{
	struct npc_xtract_info *xinfo;
	char *hw_mask = info->hw_mask;
	int max_off, offset;
	int i, j;
	int intf;

	intf = pst->flow->nix_intf;
	xinfo = pst->npc->prx_dxcfg[intf][lid][lt].xtract;
	memset(hw_mask, 0, info->len);

	for (i = 0; i < NPC_MAX_LD; i++) {
		if (xinfo[i].hdr_off < info->hw_hdr_len)
			continue;

		max_off = xinfo[i].hdr_off + xinfo[i].len - info->hw_hdr_len;

		if (xinfo[i].enable == 0)
			continue;

		if (max_off > info->len)
			max_off = info->len;

		offset = xinfo[i].hdr_off - info->hw_hdr_len;
		for (j = offset; j < max_off; j++)
			hw_mask[j] = 0xff;
	}
}

int
otx2_flow_update_parse_state(struct otx2_parse_state *pst,
			     struct otx2_flow_item_info *info, int lid, int lt,
			     uint8_t flags)
{
	uint8_t int_info_mask[NPC_MAX_EXTRACT_DATA_LEN];
	uint8_t int_info[NPC_MAX_EXTRACT_DATA_LEN];
	struct npc_lid_lt_xtract_info *xinfo;
	int len = 0;
	int intf;
	int i;

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

	/* Need to check if flags are supported but in latest
	 * KPU profile, flags are used as enumeration! No way,
	 * it can be validated unless MBOX is changed to return
	 * set of valid values out of 2**8 possible values.
	 */
	if (info->spec == NULL) {	/* Nothing to match */
		otx2_npc_dbg("Info spec NULL");
		goto done;
	}

	/* Copy spec and mask into mcam match string, mask.
	 * Since both RTE FLOW and OTX2 MCAM use network-endianness
	 * for data, we are saved from nasty conversions.
	 */
	for (i = 0; i < NPC_MAX_LD; i++) {
		struct npc_xtract_info *x;
		int k, idx, hdr_off;

		x = &xinfo->xtract[i];
		len = x->len;
		hdr_off = x->hdr_off;

		if (hdr_off < info->hw_hdr_len)
			continue;

		if (x->enable == 0)
			continue;

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

