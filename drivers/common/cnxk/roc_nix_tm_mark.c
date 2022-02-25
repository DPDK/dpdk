/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static const uint8_t y_mask_val[ROC_NIX_TM_MARK_MAX][2] = {
	[ROC_NIX_TM_MARK_VLAN_DEI] = {0x0, 0x8},
	[ROC_NIX_TM_MARK_IPV4_DSCP] = {0x1, 0x2},
	[ROC_NIX_TM_MARK_IPV4_ECN] = {0x0, 0xc},
	[ROC_NIX_TM_MARK_IPV6_DSCP] = {0x1, 0x2},
	[ROC_NIX_TM_MARK_IPV6_ECN] = {0x0, 0x3},
};

static const uint8_t r_mask_val[ROC_NIX_TM_MARK_MAX][2] = {
	[ROC_NIX_TM_MARK_VLAN_DEI] = {0x0, 0x8},
	[ROC_NIX_TM_MARK_IPV4_DSCP] = {0x0, 0x3},
	[ROC_NIX_TM_MARK_IPV4_ECN] = {0x0, 0xc},
	[ROC_NIX_TM_MARK_IPV6_DSCP] = {0x0, 0x3},
	[ROC_NIX_TM_MARK_IPV6_ECN] = {0x0, 0x3},
};

static const uint8_t mark_off[ROC_NIX_TM_MARK_MAX] = {
	[ROC_NIX_TM_MARK_VLAN_DEI] = 0x3,  /* Byte 14 Bit[4:1] */
	[ROC_NIX_TM_MARK_IPV4_DSCP] = 0x1, /* Byte 1 Bit[6:3] */
	[ROC_NIX_TM_MARK_IPV4_ECN] = 0x6, /* Byte 1 Bit[1:0], Byte 2 Bit[7:6] */
	[ROC_NIX_TM_MARK_IPV6_DSCP] = 0x5, /* Byte 0 Bit[2:0], Byte 1 Bit[7] */
	[ROC_NIX_TM_MARK_IPV6_ECN] = 0x0,  /* Byte 1 Bit[7:4] */
};

static const uint64_t mark_flag[ROC_NIX_TM_MARK_MAX] = {
	[ROC_NIX_TM_MARK_VLAN_DEI] = NIX_TM_MARK_VLAN_DEI_EN,
	[ROC_NIX_TM_MARK_IPV4_DSCP] = NIX_TM_MARK_IP_DSCP_EN,
	[ROC_NIX_TM_MARK_IPV4_ECN] = NIX_TM_MARK_IP_ECN_EN,
	[ROC_NIX_TM_MARK_IPV6_DSCP] = NIX_TM_MARK_IP_DSCP_EN,
	[ROC_NIX_TM_MARK_IPV6_ECN] = NIX_TM_MARK_IP_ECN_EN,
};

static uint8_t
prepare_tm_shaper_red_algo(struct nix_tm_node *tm_node, volatile uint64_t *reg,
			   volatile uint64_t *regval,
			   volatile uint64_t *regval_mask)
{
	uint32_t schq = tm_node->hw_id;
	uint8_t k = 0;

	plt_tm_dbg("Shaper read alg node %s(%u) lvl %u id %u, red_alg %x (%p)",
		   nix_tm_hwlvl2str(tm_node->hw_lvl), schq, tm_node->lvl,
		   tm_node->id, tm_node->red_algo, tm_node);

	/* Configure just RED algo */
	regval[k] = ((uint64_t)tm_node->red_algo << 9);
	regval_mask[k] = ~(BIT_ULL(10) | BIT_ULL(9));

	switch (tm_node->hw_lvl) {
	case NIX_TXSCH_LVL_SMQ:
		reg[k] = NIX_AF_MDQX_SHAPE(schq);
		k++;
		break;
	case NIX_TXSCH_LVL_TL4:
		reg[k] = NIX_AF_TL4X_SHAPE(schq);
		k++;
		break;
	case NIX_TXSCH_LVL_TL3:
		reg[k] = NIX_AF_TL3X_SHAPE(schq);
		k++;
		break;
	case NIX_TXSCH_LVL_TL2:
		reg[k] = NIX_AF_TL2X_SHAPE(schq);
		k++;
		break;
	default:
		break;
	}

	return k;
}

/* Only called while device is stopped */
static int
nix_tm_update_red_algo(struct nix *nix, bool red_send)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txschq_config *req;
	struct nix_tm_node_list *list;
	struct nix_tm_node *tm_node;
	uint8_t k;
	int rc;

	list = nix_tm_node_list(nix, nix->tm_tree);
	TAILQ_FOREACH(tm_node, list, node) {
		/* Skip leaf nodes */
		if (nix_tm_is_leaf(nix, tm_node->lvl))
			continue;

		if (tm_node->hw_lvl == NIX_TXSCH_LVL_TL1)
			continue;

		/* Skip if no update of red_algo is needed */
		if ((red_send && (tm_node->red_algo == NIX_REDALG_SEND)) ||
		    (!red_send && (tm_node->red_algo != NIX_REDALG_SEND)))
			continue;

		/* Update Red algo */
		if (red_send)
			tm_node->red_algo = NIX_REDALG_SEND;
		else
			tm_node->red_algo = NIX_REDALG_STD;

		/* Update txschq config  */
		req = mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = tm_node->hw_lvl;
		k = prepare_tm_shaper_red_algo(tm_node, req->reg, req->regval,
					       req->regval_mask);
		req->num_regs = k;

		rc = mbox_process(mbox);
		if (rc)
			return rc;
	}
	return 0;
}

/* Return's true if queue reconfig is needed */
static bool
nix_tm_update_markfmt(struct nix *nix, enum roc_nix_tm_mark type,
		      int mark_yellow, int mark_red)
{
	uint64_t new_markfmt, old_markfmt;
	uint8_t *tm_markfmt;
	uint8_t en_shift;
	uint64_t mask;

	if (type >= ROC_NIX_TM_MARK_MAX)
		return false;

	/* Pre-allocated mark formats for type:color combinations */
	tm_markfmt = nix->tm_markfmt[type];

	if (!mark_yellow && !mark_red) {
		/* Null format to disable */
		new_markfmt = nix->tm_markfmt_null;
	} else {
		/* Marking enabled with combination of yellow and red */
		if (mark_yellow && mark_red)
			new_markfmt = tm_markfmt[ROC_NIX_TM_MARK_COLOR_Y_R];
		else if (mark_yellow)
			new_markfmt = tm_markfmt[ROC_NIX_TM_MARK_COLOR_Y];
		else
			new_markfmt = tm_markfmt[ROC_NIX_TM_MARK_COLOR_R];
	}

	mask = 0xFFull;
	/* Format of fast path markfmt
	 * ipv6_ecn[8]:ipv4_ecn[8]:ipv6_dscp[8]:ipv4_dscp[8]:vlan_dei[16]
	 * fmt[7] = ptr offset for IPv4/IPv6 on l2_len.
	 * fmt[6:0] = markfmt idx.
	 */
	switch (type) {
	case ROC_NIX_TM_MARK_VLAN_DEI:
		en_shift = NIX_TM_MARK_VLAN_DEI_SHIFT;
		mask = 0xFFFFull;
		new_markfmt |= new_markfmt << 8;
		break;
	case ROC_NIX_TM_MARK_IPV4_DSCP:
		new_markfmt |= BIT_ULL(7);
		en_shift = NIX_TM_MARK_IPV4_DSCP_SHIFT;
		break;
	case ROC_NIX_TM_MARK_IPV4_ECN:
		new_markfmt |= BIT_ULL(7);
		en_shift = NIX_TM_MARK_IPV4_ECN_SHIFT;
		break;
	case ROC_NIX_TM_MARK_IPV6_DSCP:
		en_shift = NIX_TM_MARK_IPV6_DSCP_SHIFT;
		break;
	case ROC_NIX_TM_MARK_IPV6_ECN:
		new_markfmt |= BIT_ULL(7);
		en_shift = NIX_TM_MARK_IPV6_ECN_SHIFT;
		break;
	default:
		return false;
	}

	/* Skip if same as old config */
	old_markfmt = (nix->tm_markfmt_en >> en_shift) & mask;
	if (old_markfmt == new_markfmt)
		return false;

	/* Need queue reconfig */
	nix->tm_markfmt_en &= ~(mask << en_shift);
	nix->tm_markfmt_en |= (new_markfmt << en_shift);

	return true;
}

int
nix_tm_mark_init(struct nix *nix)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_mark_format_cfg_rsp *rsp;
	struct nix_mark_format_cfg *req;
	int rc, i, j;

	/* Check for supported revisions */
	if (roc_model_is_cn96_ax() || roc_model_is_cn95_a0())
		return 0;

	/* Null mark format */
	req = mbox_alloc_msg_nix_mark_format_cfg(mbox);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		plt_err("TM failed to alloc null mark format, rc=%d", rc);
		goto exit;
	}

	nix->tm_markfmt_null = rsp->mark_format_idx;

	/* Alloc vlan, dscp, ecn mark formats */
	for (i = 0; i < ROC_NIX_TM_MARK_MAX; i++) {
		for (j = 0; j < ROC_NIX_TM_MARK_COLOR_MAX; j++) {
			req = mbox_alloc_msg_nix_mark_format_cfg(mbox);
			req->offset = mark_off[i];

			switch (j) {
			case ROC_NIX_TM_MARK_COLOR_Y:
				req->y_mask = y_mask_val[i][0];
				req->y_val = y_mask_val[i][1];
				break;
			case ROC_NIX_TM_MARK_COLOR_R:
				req->r_mask = r_mask_val[i][0];
				req->r_val = r_mask_val[i][1];
				break;
			case ROC_NIX_TM_MARK_COLOR_Y_R:
				req->y_mask = y_mask_val[i][0];
				req->y_val = y_mask_val[i][1];
				req->r_mask = r_mask_val[i][0];
				req->r_val = r_mask_val[i][1];
				break;
			}

			rc = mbox_process_msg(mbox, (void *)&rsp);
			if (rc) {
				plt_err("TM failed to alloc mark fmt "
					"type %u color %u, rc=%d",
					i, j, rc);
				goto exit;
			}

			nix->tm_markfmt[i][j] = rsp->mark_format_idx;
			plt_tm_dbg("Mark type: %u, Mark Color:%u, id:%u\n", i,
				   j, nix->tm_markfmt[i][j]);
		}
	}
	/* Update null mark format as default */
	nix_tm_update_markfmt(nix, ROC_NIX_TM_MARK_VLAN_DEI, 0, 0);
	nix_tm_update_markfmt(nix, ROC_NIX_TM_MARK_IPV4_DSCP, 0, 0);
	nix_tm_update_markfmt(nix, ROC_NIX_TM_MARK_IPV4_ECN, 0, 0);
	nix_tm_update_markfmt(nix, ROC_NIX_TM_MARK_IPV6_DSCP, 0, 0);
	nix_tm_update_markfmt(nix, ROC_NIX_TM_MARK_IPV6_ECN, 0, 0);
exit:
	return rc;
}

int
roc_nix_tm_mark_config(struct roc_nix *roc_nix, enum roc_nix_tm_mark type,
		       int mark_yellow, int mark_red)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int rc;

	if (!(nix->tm_flags & NIX_TM_HIERARCHY_ENA))
		return -EINVAL;

	rc = nix_tm_update_markfmt(nix, type, mark_yellow, mark_red);
	if (!rc)
		return 0;

	if (!mark_yellow && !mark_red)
		nix->tm_flags &= ~mark_flag[type];
	else
		nix->tm_flags |= mark_flag[type];

	/* Update red algo for change in mark_red */
	return nix_tm_update_red_algo(nix, !!mark_red);
}

uint64_t
roc_nix_tm_mark_format_get(struct roc_nix *roc_nix, uint64_t *flags)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	*flags = ((nix->tm_flags & NIX_TM_MARK_EN_MASK) >> 3);
	return nix->tm_markfmt_en;
}
