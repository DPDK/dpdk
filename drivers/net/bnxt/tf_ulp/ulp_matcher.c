/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "ulp_matcher.h"
#include "ulp_utils.h"

/* Utility function to check if bitmap is zero */
static inline
int ulp_field_mask_is_zero(uint8_t *bitmap, uint32_t size)
{
	while (size-- > 0) {
		if (*bitmap != 0)
			return 0;
		bitmap++;
	}
	return 1;
}

/* Utility function to check if bitmap is all ones */
static inline int
ulp_field_mask_is_ones(uint8_t *bitmap, uint32_t size)
{
	while (size-- > 0) {
		if (*bitmap != 0xFF)
			return 0;
		bitmap++;
	}
	return 1;
}

/* Utility function to check if bitmap is non zero */
static inline int
ulp_field_mask_notzero(uint8_t *bitmap, uint32_t size)
{
	while (size-- > 0) {
		if (*bitmap != 0)
			return 1;
		bitmap++;
	}
	return 0;
}

/* Utility function to mask the computed and internal proto headers. */
static void
ulp_matcher_hdr_fields_normalize(struct ulp_rte_hdr_bitmap *hdr1,
				 struct ulp_rte_hdr_bitmap *hdr2)
{
	/* copy the contents first */
	rte_memcpy(hdr2, hdr1, sizeof(struct ulp_rte_hdr_bitmap));

	/* reset the computed fields */
	ULP_BITMAP_RESET(hdr2->bits, BNXT_ULP_HDR_BIT_SVIF);
	ULP_BITMAP_RESET(hdr2->bits, BNXT_ULP_HDR_BIT_OO_VLAN);
	ULP_BITMAP_RESET(hdr2->bits, BNXT_ULP_HDR_BIT_OI_VLAN);
	ULP_BITMAP_RESET(hdr2->bits, BNXT_ULP_HDR_BIT_IO_VLAN);
	ULP_BITMAP_RESET(hdr2->bits, BNXT_ULP_HDR_BIT_II_VLAN);
	ULP_BITMAP_RESET(hdr2->bits, BNXT_ULP_HDR_BIT_O_L3);
	ULP_BITMAP_RESET(hdr2->bits, BNXT_ULP_HDR_BIT_O_L4);
	ULP_BITMAP_RESET(hdr2->bits, BNXT_ULP_HDR_BIT_I_L3);
	ULP_BITMAP_RESET(hdr2->bits, BNXT_ULP_HDR_BIT_I_L4);
}

/*
 * Function to handle the matching of RTE Flows and validating
 * the pattern masks against the flow templates.
 */
int32_t
ulp_matcher_pattern_match(enum ulp_direction_type   dir,
			  struct ulp_rte_hdr_bitmap *hdr_bitmap,
			  struct ulp_rte_hdr_field  *hdr_field,
			  struct ulp_rte_act_bitmap *act_bitmap,
			  uint32_t		    *class_id)
{
	struct bnxt_ulp_header_match_info	*sel_hdr_match;
	uint32_t				hdr_num, idx, jdx;
	uint32_t				match = 0;
	struct ulp_rte_hdr_bitmap		hdr_bitmap_masked;
	uint32_t				start_idx;
	struct ulp_rte_hdr_field		*m_field;
	struct bnxt_ulp_matcher_field_info	*sf;

	/* Select the ingress or egress template to match against */
	if (dir == ULP_DIR_INGRESS) {
		sel_hdr_match = ulp_ingress_hdr_match_list;
		hdr_num = BNXT_ULP_INGRESS_HDR_MATCH_SZ;
	} else {
		sel_hdr_match = ulp_egress_hdr_match_list;
		hdr_num = BNXT_ULP_EGRESS_HDR_MATCH_SZ;
	}

	/* Remove the hdr bit maps that are internal or computed */
	ulp_matcher_hdr_fields_normalize(hdr_bitmap, &hdr_bitmap_masked);

	/* Loop through the list of class templates to find the match */
	for (idx = 0; idx < hdr_num; idx++, sel_hdr_match++) {
		if (ULP_BITSET_CMP(&sel_hdr_match->hdr_bitmap,
				   &hdr_bitmap_masked)) {
			/* no match found */
			BNXT_TF_DBG(DEBUG, "Pattern Match failed template=%d\n",
				    idx);
			continue;
		}
		match = ULP_BITMAP_ISSET(act_bitmap->bits,
					 BNXT_ULP_ACTION_BIT_VNIC);
		if (match != sel_hdr_match->act_vnic) {
			/* no match found */
			BNXT_TF_DBG(DEBUG, "Vnic Match failed template=%d\n",
				    idx);
			continue;
		} else {
			match = 1;
		}

		/* Found a matching hdr bitmap, match the fields next */
		start_idx = sel_hdr_match->start_idx;
		for (jdx = 0; jdx < sel_hdr_match->num_entries; jdx++) {
			m_field = &hdr_field[jdx + BNXT_ULP_HDR_FIELD_LAST - 1];
			sf = &ulp_field_match[start_idx + jdx];
			switch (sf->mask_opcode) {
			case BNXT_ULP_FMF_MASK_ANY:
				match &= ulp_field_mask_is_zero(m_field->mask,
								m_field->size);
				break;
			case BNXT_ULP_FMF_MASK_EXACT:
				match &= ulp_field_mask_is_ones(m_field->mask,
								m_field->size);
				break;
			case BNXT_ULP_FMF_MASK_WILDCARD:
				match &= ulp_field_mask_notzero(m_field->mask,
								m_field->size);
				break;
			case BNXT_ULP_FMF_MASK_IGNORE:
			default:
				break;
			}
			if (!match)
				break;
		}
		if (match) {
			BNXT_TF_DBG(DEBUG,
				    "Found matching pattern template %d\n",
				    sel_hdr_match->class_tmpl_id);
			*class_id = sel_hdr_match->class_tmpl_id;
			return BNXT_TF_RC_SUCCESS;
		}
	}
	BNXT_TF_DBG(DEBUG, "Did not find any matching template\n");
	*class_id = 0;
	return BNXT_TF_RC_ERROR;
}
