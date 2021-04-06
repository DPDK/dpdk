/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

uint16_t
nix_tm_lvl2nix_tl1_root(uint32_t lvl)
{
	switch (lvl) {
	case ROC_TM_LVL_ROOT:
		return NIX_TXSCH_LVL_TL1;
	case ROC_TM_LVL_SCH1:
		return NIX_TXSCH_LVL_TL2;
	case ROC_TM_LVL_SCH2:
		return NIX_TXSCH_LVL_TL3;
	case ROC_TM_LVL_SCH3:
		return NIX_TXSCH_LVL_TL4;
	case ROC_TM_LVL_SCH4:
		return NIX_TXSCH_LVL_SMQ;
	default:
		return NIX_TXSCH_LVL_CNT;
	}
}

uint16_t
nix_tm_lvl2nix_tl2_root(uint32_t lvl)
{
	switch (lvl) {
	case ROC_TM_LVL_ROOT:
		return NIX_TXSCH_LVL_TL2;
	case ROC_TM_LVL_SCH1:
		return NIX_TXSCH_LVL_TL3;
	case ROC_TM_LVL_SCH2:
		return NIX_TXSCH_LVL_TL4;
	case ROC_TM_LVL_SCH3:
		return NIX_TXSCH_LVL_SMQ;
	default:
		return NIX_TXSCH_LVL_CNT;
	}
}

uint16_t
nix_tm_lvl2nix(struct nix *nix, uint32_t lvl)
{
	if (nix_tm_have_tl1_access(nix))
		return nix_tm_lvl2nix_tl1_root(lvl);
	else
		return nix_tm_lvl2nix_tl2_root(lvl);
}


struct nix_tm_shaper_profile *
nix_tm_shaper_profile_search(struct nix *nix, uint32_t id)
{
	struct nix_tm_shaper_profile *profile;

	TAILQ_FOREACH(profile, &nix->shaper_profile_list, shaper) {
		if (profile->id == id)
			return profile;
	}
	return NULL;
}

struct nix_tm_node *
nix_tm_node_search(struct nix *nix, uint32_t node_id, enum roc_nix_tm_tree tree)
{
	struct nix_tm_node_list *list;
	struct nix_tm_node *node;

	list = nix_tm_node_list(nix, tree);
	TAILQ_FOREACH(node, list, node) {
		if (node->id == node_id)
			return node;
	}
	return NULL;
}

uint64_t
nix_tm_shaper_rate_conv(uint64_t value, uint64_t *exponent_p,
			uint64_t *mantissa_p, uint64_t *div_exp_p)
{
	uint64_t div_exp, exponent, mantissa;

	/* Boundary checks */
	if (value < NIX_TM_MIN_SHAPER_RATE || value > NIX_TM_MAX_SHAPER_RATE)
		return 0;

	if (value <= NIX_TM_SHAPER_RATE(0, 0, 0)) {
		/* Calculate rate div_exp and mantissa using
		 * the following formula:
		 *
		 * value = (2E6 * (256 + mantissa)
		 *              / ((1 << div_exp) * 256))
		 */
		div_exp = 0;
		exponent = 0;
		mantissa = NIX_TM_MAX_RATE_MANTISSA;

		while (value < (NIX_TM_SHAPER_RATE_CONST / (1 << div_exp)))
			div_exp += 1;

		while (value < ((NIX_TM_SHAPER_RATE_CONST * (256 + mantissa)) /
				((1 << div_exp) * 256)))
			mantissa -= 1;
	} else {
		/* Calculate rate exponent and mantissa using
		 * the following formula:
		 *
		 * value = (2E6 * ((256 + mantissa) << exponent)) / 256
		 *
		 */
		div_exp = 0;
		exponent = NIX_TM_MAX_RATE_EXPONENT;
		mantissa = NIX_TM_MAX_RATE_MANTISSA;

		while (value < (NIX_TM_SHAPER_RATE_CONST * (1 << exponent)))
			exponent -= 1;

		while (value < ((NIX_TM_SHAPER_RATE_CONST *
				 ((256 + mantissa) << exponent)) /
				256))
			mantissa -= 1;
	}

	if (div_exp > NIX_TM_MAX_RATE_DIV_EXP ||
	    exponent > NIX_TM_MAX_RATE_EXPONENT ||
	    mantissa > NIX_TM_MAX_RATE_MANTISSA)
		return 0;

	if (div_exp_p)
		*div_exp_p = div_exp;
	if (exponent_p)
		*exponent_p = exponent;
	if (mantissa_p)
		*mantissa_p = mantissa;

	/* Calculate real rate value */
	return NIX_TM_SHAPER_RATE(exponent, mantissa, div_exp);
}

uint64_t
nix_tm_shaper_burst_conv(uint64_t value, uint64_t *exponent_p,
			 uint64_t *mantissa_p)
{
	uint64_t exponent, mantissa;

	if (value < NIX_TM_MIN_SHAPER_BURST || value > NIX_TM_MAX_SHAPER_BURST)
		return 0;

	/* Calculate burst exponent and mantissa using
	 * the following formula:
	 *
	 * value = (((256 + mantissa) << (exponent + 1)
	 / 256)
	 *
	 */
	exponent = NIX_TM_MAX_BURST_EXPONENT;
	mantissa = NIX_TM_MAX_BURST_MANTISSA;

	while (value < (1ull << (exponent + 1)))
		exponent -= 1;

	while (value < ((256 + mantissa) << (exponent + 1)) / 256)
		mantissa -= 1;

	if (exponent > NIX_TM_MAX_BURST_EXPONENT ||
	    mantissa > NIX_TM_MAX_BURST_MANTISSA)
		return 0;

	if (exponent_p)
		*exponent_p = exponent;
	if (mantissa_p)
		*mantissa_p = mantissa;

	return NIX_TM_SHAPER_BURST(exponent, mantissa);
}

static uint16_t
nix_tm_max_prio(struct nix *nix, uint16_t hw_lvl)
{
	if (hw_lvl >= NIX_TXSCH_LVL_CNT)
		return 0;

	/* MDQ does not support SP */
	if (hw_lvl == NIX_TXSCH_LVL_MDQ)
		return 0;

	/* PF's TL1 with VF's enabled does not support SP */
	if (hw_lvl == NIX_TXSCH_LVL_TL1 && (!nix_tm_have_tl1_access(nix) ||
					    (nix->tm_flags & NIX_TM_TL1_NO_SP)))
		return 0;

	return NIX_TM_TLX_SP_PRIO_MAX - 1;
}

int
nix_tm_validate_prio(struct nix *nix, uint32_t lvl, uint32_t parent_id,
		     uint32_t priority, enum roc_nix_tm_tree tree)
{
	uint8_t priorities[NIX_TM_TLX_SP_PRIO_MAX];
	struct nix_tm_node_list *list;
	struct nix_tm_node *node;
	uint32_t rr_num = 0;
	int i;

	list = nix_tm_node_list(nix, tree);
	/* Validate priority against max */
	if (priority > nix_tm_max_prio(nix, nix_tm_lvl2nix(nix, lvl - 1)))
		return NIX_ERR_TM_PRIO_EXCEEDED;

	if (parent_id == ROC_NIX_TM_NODE_ID_INVALID)
		return 0;

	memset(priorities, 0, sizeof(priorities));
	priorities[priority] = 1;

	TAILQ_FOREACH(node, list, node) {
		if (!node->parent)
			continue;

		if (node->parent->id != parent_id)
			continue;

		priorities[node->priority]++;
	}

	for (i = 0; i < NIX_TM_TLX_SP_PRIO_MAX; i++)
		if (priorities[i] > 1)
			rr_num++;

	/* At max, one rr groups per parent */
	if (rr_num > 1)
		return NIX_ERR_TM_MULTIPLE_RR_GROUPS;

	/* Check for previous priority to avoid holes in priorities */
	if (priority && !priorities[priority - 1])
		return NIX_ERR_TM_PRIO_ORDER;

	return 0;
}

uint8_t
nix_tm_sw_xoff_prep(struct nix_tm_node *node, bool enable,
		    volatile uint64_t *reg, volatile uint64_t *regval)
{
	uint32_t hw_lvl = node->hw_lvl;
	uint32_t schq = node->hw_id;
	uint8_t k = 0;

	plt_tm_dbg("sw xoff config node %s(%u) lvl %u id %u, enable %u (%p)",
		   nix_tm_hwlvl2str(hw_lvl), schq, node->lvl, node->id, enable,
		   node);

	regval[k] = enable;

	switch (hw_lvl) {
	case NIX_TXSCH_LVL_MDQ:
		reg[k] = NIX_AF_MDQX_SW_XOFF(schq);
		k++;
		break;
	case NIX_TXSCH_LVL_TL4:
		reg[k] = NIX_AF_TL4X_SW_XOFF(schq);
		k++;
		break;
	case NIX_TXSCH_LVL_TL3:
		reg[k] = NIX_AF_TL3X_SW_XOFF(schq);
		k++;
		break;
	case NIX_TXSCH_LVL_TL2:
		reg[k] = NIX_AF_TL2X_SW_XOFF(schq);
		k++;
		break;
	case NIX_TXSCH_LVL_TL1:
		reg[k] = NIX_AF_TL1X_SW_XOFF(schq);
		k++;
		break;
	default:
		break;
	}

	return k;
}

/* Search for min rate in topology */
uint64_t
nix_tm_shaper_profile_rate_min(struct nix *nix)
{
	struct nix_tm_shaper_profile *profile;
	uint64_t rate_min = 1E9; /* 1 Gbps */

	TAILQ_FOREACH(profile, &nix->shaper_profile_list, shaper) {
		if (profile->peak.rate && profile->peak.rate < rate_min)
			rate_min = profile->peak.rate;

		if (profile->commit.rate && profile->commit.rate < rate_min)
			rate_min = profile->commit.rate;
	}
	return rate_min;
}

uint16_t
nix_tm_resource_avail(struct nix *nix, uint8_t hw_lvl, bool contig)
{
	uint32_t pos = 0, start_pos = 0;
	struct plt_bitmap *bmp;
	uint16_t count = 0;
	uint64_t slab = 0;

	bmp = contig ? nix->schq_contig_bmp[hw_lvl] : nix->schq_bmp[hw_lvl];
	plt_bitmap_scan_init(bmp);

	if (!plt_bitmap_scan(bmp, &pos, &slab))
		return count;

	/* Count bit set */
	start_pos = pos;
	do {
		count += __builtin_popcountll(slab);
		if (!plt_bitmap_scan(bmp, &pos, &slab))
			break;
	} while (pos != start_pos);

	return count;
}

int
roc_nix_tm_node_lvl(struct roc_nix *roc_nix, uint32_t node_id)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_node *node;

	node = nix_tm_node_search(nix, node_id, ROC_NIX_TM_USER);
	if (!node)
		return NIX_ERR_TM_INVALID_NODE;

	return node->lvl;
}

struct roc_nix_tm_node *
roc_nix_tm_node_get(struct roc_nix *roc_nix, uint32_t node_id)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_node *node;

	node = nix_tm_node_search(nix, node_id, ROC_NIX_TM_USER);
	return (struct roc_nix_tm_node *)node;
}

struct roc_nix_tm_node *
roc_nix_tm_node_next(struct roc_nix *roc_nix, struct roc_nix_tm_node *__prev)
{
	struct nix_tm_node *prev = (struct nix_tm_node *)__prev;
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_node_list *list;

	list = nix_tm_node_list(nix, ROC_NIX_TM_USER);

	/* HEAD of the list */
	if (!prev)
		return (struct roc_nix_tm_node *)TAILQ_FIRST(list);

	/* Next entry */
	if (prev->tree != ROC_NIX_TM_USER)
		return NULL;

	return (struct roc_nix_tm_node *)TAILQ_NEXT(prev, node);
}

struct roc_nix_tm_shaper_profile *
roc_nix_tm_shaper_profile_get(struct roc_nix *roc_nix, uint32_t profile_id)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_shaper_profile *profile;

	profile = nix_tm_shaper_profile_search(nix, profile_id);
	return (struct roc_nix_tm_shaper_profile *)profile;
}

struct roc_nix_tm_shaper_profile *
roc_nix_tm_shaper_profile_next(struct roc_nix *roc_nix,
			       struct roc_nix_tm_shaper_profile *__prev)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_shaper_profile_list *list;
	struct nix_tm_shaper_profile *prev;

	prev = (struct nix_tm_shaper_profile *)__prev;
	list = &nix->shaper_profile_list;

	/* HEAD of the list */
	if (!prev)
		return (struct roc_nix_tm_shaper_profile *)TAILQ_FIRST(list);

	return (struct roc_nix_tm_shaper_profile *)TAILQ_NEXT(prev, shaper);
}

struct nix_tm_node *
nix_tm_node_alloc(void)
{
	struct nix_tm_node *node;

	node = plt_zmalloc(sizeof(struct nix_tm_node), 0);
	if (!node)
		return NULL;

	node->free_fn = plt_free;
	return node;
}

void
nix_tm_node_free(struct nix_tm_node *node)
{
	if (!node || node->free_fn == NULL)
		return;

	(node->free_fn)(node);
}

struct nix_tm_shaper_profile *
nix_tm_shaper_profile_alloc(void)
{
	struct nix_tm_shaper_profile *profile;

	profile = plt_zmalloc(sizeof(struct nix_tm_shaper_profile), 0);
	if (!profile)
		return NULL;

	profile->free_fn = plt_free;
	return profile;
}

void
nix_tm_shaper_profile_free(struct nix_tm_shaper_profile *profile)
{
	if (!profile || !profile->free_fn)
		return;

	(profile->free_fn)(profile);
}
