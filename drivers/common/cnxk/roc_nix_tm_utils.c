/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static inline uint64_t
nix_tm_shaper2regval(struct nix_tm_shaper_data *shaper)
{
	uint64_t regval;

	if (roc_model_is_cn9k()) {
		regval = (shaper->burst_exponent << 37);
		regval |= (shaper->burst_mantissa << 29);
		regval |= (shaper->div_exp << 13);
		regval |= (shaper->exponent << 9);
		regval |= (shaper->mantissa << 1);
		return regval;
	}

	regval = (shaper->burst_exponent << 44);
	regval |= (shaper->burst_mantissa << 29);
	regval |= (shaper->div_exp << 13);
	regval |= (shaper->exponent << 9);
	regval |= (shaper->mantissa << 1);
	return regval;
}

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

static uint8_t
nix_tm_relchan_get(struct nix *nix)
{
	return nix->tx_chan_base & 0xff;
}

static int
nix_tm_find_prio_anchor(struct nix *nix, uint32_t node_id,
			enum roc_nix_tm_tree tree)
{
	struct nix_tm_node *child_node;
	struct nix_tm_node_list *list;

	list = nix_tm_node_list(nix, tree);

	TAILQ_FOREACH(child_node, list, node) {
		if (!child_node->parent)
			continue;
		if (!(child_node->parent->id == node_id))
			continue;
		if (child_node->priority == child_node->parent->rr_prio)
			continue;
		return child_node->hw_id - child_node->priority;
	}
	return 0;
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

static uint64_t
nix_tm_shaper_rate_conv_floor(uint64_t value, uint64_t *exponent_p,
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

		while (value <= (NIX_TM_SHAPER_RATE_CONST / (1 << div_exp)))
			div_exp += 1;

		while (value <= ((NIX_TM_SHAPER_RATE_CONST * (256 + mantissa)) /
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

		while (value <= (NIX_TM_SHAPER_RATE_CONST * (1 << exponent)))
			exponent -= 1;

		while (value <= ((NIX_TM_SHAPER_RATE_CONST *
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

static uint64_t
nix_tm_shaper_rate_conv_exact(uint64_t value, uint64_t *exponent_p,
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

/* With zero accuracy we will tune parameters as defined by HW,
 * non zero accuracy will keep the parameters close to lower values
 * and make sure long-term shaper rate will not exceed the requested rate.
 */
uint64_t
nix_tm_shaper_rate_conv(uint64_t value, uint64_t *exponent_p,
			uint64_t *mantissa_p, uint64_t *div_exp_p,
			int8_t accuracy)
{
	if (!accuracy)
		return nix_tm_shaper_rate_conv_exact(value, exponent_p,
						     mantissa_p, div_exp_p);

	return nix_tm_shaper_rate_conv_floor(value, exponent_p, mantissa_p,
					     div_exp_p);
}

uint64_t
nix_tm_shaper_burst_conv(uint64_t value, uint64_t *exponent_p,
			 uint64_t *mantissa_p)
{
	uint64_t min_burst, max_burst;
	uint64_t exponent, mantissa;
	uint32_t max_mantissa;

	min_burst = NIX_TM_MIN_SHAPER_BURST;
	max_burst = roc_nix_tm_max_shaper_burst_get();

	if (value < min_burst || value > max_burst)
		return 0;

	max_mantissa = (roc_model_is_cn9k() ? NIX_CN9K_TM_MAX_BURST_MANTISSA :
					      NIX_TM_MAX_BURST_MANTISSA);
	/* Calculate burst exponent and mantissa using
	 * the following formula:
	 *
	 * value = (((256 + mantissa) << (exponent + 1) / 256)
	 *
	 */
	exponent = NIX_TM_MAX_BURST_EXPONENT;
	mantissa = max_mantissa;

	while (value < (1ull << (exponent + 1)))
		exponent -= 1;

	while (value < ((256 + mantissa) << (exponent + 1)) / 256)
		mantissa -= 1;

	if (exponent > NIX_TM_MAX_BURST_EXPONENT || mantissa > max_mantissa)
		return 0;

	if (exponent_p)
		*exponent_p = exponent;
	if (mantissa_p)
		*mantissa_p = mantissa;

	return NIX_TM_SHAPER_BURST(exponent, mantissa);
}

static void
nix_tm_shaper_conf_get(struct nix_tm_shaper_profile *profile,
		       struct nix_tm_shaper_data *cir,
		       struct nix_tm_shaper_data *pir)
{
	memset(cir, 0, sizeof(*cir));
	memset(pir, 0, sizeof(*pir));

	if (!profile)
		return;

	/* Calculate CIR exponent and mantissa */
	if (profile->commit.rate)
		cir->rate = nix_tm_shaper_rate_conv(
			profile->commit.rate, &cir->exponent, &cir->mantissa,
			&cir->div_exp, profile->accuracy);

	/* Calculate PIR exponent and mantissa */
	if (profile->peak.rate)
		pir->rate = nix_tm_shaper_rate_conv(
			profile->peak.rate, &pir->exponent, &pir->mantissa,
			&pir->div_exp, profile->accuracy);

	/* Calculate CIR burst exponent and mantissa */
	if (profile->commit.size)
		cir->burst = nix_tm_shaper_burst_conv(profile->commit.size,
						      &cir->burst_exponent,
						      &cir->burst_mantissa);

	/* Calculate PIR burst exponent and mantissa */
	if (profile->peak.size)
		pir->burst = nix_tm_shaper_burst_conv(profile->peak.size,
						      &pir->burst_exponent,
						      &pir->burst_mantissa);
}

uint32_t
nix_tm_check_rr(struct nix *nix, uint32_t parent_id, enum roc_nix_tm_tree tree,
		uint32_t *rr_prio, uint32_t *max_prio)
{
	uint32_t node_cnt[NIX_TM_TLX_SP_PRIO_MAX];
	struct nix_tm_node_list *list;
	struct nix_tm_node *node;
	uint32_t rr_num = 0, i;
	uint32_t children = 0;
	uint32_t priority;

	memset(node_cnt, 0, sizeof(node_cnt));
	*rr_prio = 0xF;
	*max_prio = UINT32_MAX;

	list = nix_tm_node_list(nix, tree);
	TAILQ_FOREACH(node, list, node) {
		if (!node->parent)
			continue;

		if (!(node->parent->id == parent_id))
			continue;

		priority = node->priority;
		node_cnt[priority]++;
		children++;
	}

	for (i = 0; i < NIX_TM_TLX_SP_PRIO_MAX; i++) {
		if (!node_cnt[i])
			break;

		if (node_cnt[i] > rr_num) {
			*rr_prio = i;
			rr_num = node_cnt[i];
		}
	}

	/* RR group of single RR child is considered as SP */
	if (rr_num == 1) {
		*rr_prio = 0xF;
		rr_num = 0;
	}

	/* Max prio will be returned only when we have non zero prio
	 * or if a parent has single child.
	 */
	if (i > 1 || (children == 1))
		*max_prio = i - 1;
	return rr_num;
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

bool
nix_tm_child_res_valid(struct nix_tm_node_list *list,
		       struct nix_tm_node *parent)
{
	struct nix_tm_node *child;

	TAILQ_FOREACH(child, list, node) {
		if (child->parent != parent)
			continue;
		if (!(child->flags & NIX_TM_NODE_HWRES))
			return false;
	}
	return true;
}

uint8_t
nix_tm_tl1_default_prep(uint32_t schq, volatile uint64_t *reg,
			volatile uint64_t *regval)
{
	uint8_t k = 0;

	/*
	 * Default config for TL1.
	 * For VF this is always ignored.
	 */
	plt_tm_dbg("Default config for main root %s(%u)",
		   nix_tm_hwlvl2str(NIX_TXSCH_LVL_TL1), schq);

	/* Set DWRR quantum */
	reg[k] = NIX_AF_TL1X_SCHEDULE(schq);
	regval[k] = NIX_TM_TL1_DFLT_RR_QTM;
	k++;

	reg[k] = NIX_AF_TL1X_TOPOLOGY(schq);
	regval[k] = (NIX_TM_TL1_DFLT_RR_PRIO << 1);
	k++;

	reg[k] = NIX_AF_TL1X_CIR(schq);
	regval[k] = 0;
	k++;

	return k;
}

uint8_t
nix_tm_topology_reg_prep(struct nix *nix, struct nix_tm_node *node,
			 volatile uint64_t *reg, volatile uint64_t *regval,
			 volatile uint64_t *regval_mask)
{
	struct roc_nix *roc_nix = nix_priv_to_roc_nix(nix);
	uint8_t k = 0, hw_lvl, parent_lvl;
	uint64_t parent = 0, child = 0;
	enum roc_nix_tm_tree tree;
	uint32_t rr_prio, schq;
	uint16_t link, relchan;

	tree = node->tree;
	schq = node->hw_id;
	hw_lvl = node->hw_lvl;
	parent_lvl = hw_lvl + 1;
	rr_prio = node->rr_prio;

	/* Root node will not have a parent node */
	if (hw_lvl == nix->tm_root_lvl)
		parent = node->parent_hw_id;
	else
		parent = node->parent->hw_id;

	link = nix->tx_link;
	relchan = nix_tm_relchan_get(nix);

	if (hw_lvl != NIX_TXSCH_LVL_SMQ)
		child = nix_tm_find_prio_anchor(nix, node->id, tree);

	/* Override default rr_prio when TL1
	 * Static Priority is disabled
	 */
	if (hw_lvl == NIX_TXSCH_LVL_TL1 && nix->tm_flags & NIX_TM_TL1_NO_SP) {
		rr_prio = NIX_TM_TL1_DFLT_RR_PRIO;
		child = 0;
	}

	plt_tm_dbg("Topology config node %s(%u)->%s(%" PRIu64 ") lvl %u, id %u"
		   " prio_anchor %" PRIu64 " rr_prio %u (%p)",
		   nix_tm_hwlvl2str(hw_lvl), schq, nix_tm_hwlvl2str(parent_lvl),
		   parent, node->lvl, node->id, child, rr_prio, node);

	/* Prepare Topology and Link config */
	switch (hw_lvl) {
	case NIX_TXSCH_LVL_SMQ:

		/* Set xoff which will be cleared later */
		reg[k] = NIX_AF_SMQX_CFG(schq);
		regval[k] = (BIT_ULL(50) | NIX_MIN_HW_FRS |
			     ((nix->mtu & 0xFFFF) << 8));
		/* Maximum Vtag insertion size as a multiple of four bytes */
		if (roc_nix->hw_vlan_ins)
			regval[k] |= (0x2ULL << 36);
		regval_mask[k] = ~(BIT_ULL(50) | GENMASK_ULL(6, 0) |
				   GENMASK_ULL(23, 8) | GENMASK_ULL(38, 36));
		k++;

		/* Parent and schedule conf */
		reg[k] = NIX_AF_MDQX_PARENT(schq);
		regval[k] = parent << 16;
		k++;

		break;
	case NIX_TXSCH_LVL_TL4:
		/* Parent and schedule conf */
		reg[k] = NIX_AF_TL4X_PARENT(schq);
		regval[k] = parent << 16;
		k++;

		reg[k] = NIX_AF_TL4X_TOPOLOGY(schq);
		regval[k] = (child << 32) | (rr_prio << 1);
		k++;

		/* Configure TL4 to send to SDP channel instead of CGX/LBK */
		if (nix->sdp_link) {
			reg[k] = NIX_AF_TL4X_SDP_LINK_CFG(schq);
			regval[k] = BIT_ULL(12);
			k++;
		}
		break;
	case NIX_TXSCH_LVL_TL3:
		/* Parent and schedule conf */
		reg[k] = NIX_AF_TL3X_PARENT(schq);
		regval[k] = parent << 16;
		k++;

		reg[k] = NIX_AF_TL3X_TOPOLOGY(schq);
		regval[k] = (child << 32) | (rr_prio << 1);
		k++;

		/* Link configuration */
		if (!nix->sdp_link &&
		    nix->tm_link_cfg_lvl == NIX_TXSCH_LVL_TL3) {
			reg[k] = NIX_AF_TL3_TL2X_LINKX_CFG(schq, link);
			regval[k] = BIT_ULL(12) | relchan;
			/* Enable BP if node is BP capable and rx_pause is set
			 */
			if (nix->rx_pause && node->bp_capa)
				regval[k] |= BIT_ULL(13);
			k++;
		}

		break;
	case NIX_TXSCH_LVL_TL2:
		/* Parent and schedule conf */
		reg[k] = NIX_AF_TL2X_PARENT(schq);
		regval[k] = parent << 16;
		k++;

		reg[k] = NIX_AF_TL2X_TOPOLOGY(schq);
		regval[k] = (child << 32) | (rr_prio << 1);
		k++;

		/* Link configuration */
		if (!nix->sdp_link &&
		    nix->tm_link_cfg_lvl == NIX_TXSCH_LVL_TL2) {
			reg[k] = NIX_AF_TL3_TL2X_LINKX_CFG(schq, link);
			regval[k] = BIT_ULL(12) | relchan;
			/* Enable BP if node is BP capable and rx_pause is set
			 */
			if (nix->rx_pause && node->bp_capa)
				regval[k] |= BIT_ULL(13);
			k++;
		}

		break;
	case NIX_TXSCH_LVL_TL1:
		reg[k] = NIX_AF_TL1X_TOPOLOGY(schq);
		regval[k] = (child << 32) | (rr_prio << 1 /*RR_PRIO*/);
		k++;

		break;
	}

	return k;
}

uint8_t
nix_tm_sched_reg_prep(struct nix *nix, struct nix_tm_node *node,
		      volatile uint64_t *reg, volatile uint64_t *regval)
{
	uint64_t strict_prio = node->priority;
	uint32_t hw_lvl = node->hw_lvl;
	uint32_t schq = node->hw_id;
	uint64_t rr_quantum;
	uint8_t k = 0;

	/* For CN9K, weight needs to be converted to quantum */
	rr_quantum = nix_tm_weight_to_rr_quantum(node->weight);

	/* For children to root, strict prio is default if either
	 * device root is TL2 or TL1 Static Priority is disabled.
	 */
	if (hw_lvl == NIX_TXSCH_LVL_TL2 &&
	    (!nix_tm_have_tl1_access(nix) || nix->tm_flags & NIX_TM_TL1_NO_SP))
		strict_prio = NIX_TM_TL1_DFLT_RR_PRIO;

	plt_tm_dbg("Schedule config node %s(%u) lvl %u id %u, "
		   "prio 0x%" PRIx64 ", rr_quantum/rr_wt 0x%" PRIx64 " (%p)",
		   nix_tm_hwlvl2str(node->hw_lvl), schq, node->lvl, node->id,
		   strict_prio, rr_quantum, node);

	switch (hw_lvl) {
	case NIX_TXSCH_LVL_SMQ:
		reg[k] = NIX_AF_MDQX_SCHEDULE(schq);
		regval[k] = (strict_prio << 24) | rr_quantum;
		k++;

		break;
	case NIX_TXSCH_LVL_TL4:
		reg[k] = NIX_AF_TL4X_SCHEDULE(schq);
		regval[k] = (strict_prio << 24) | rr_quantum;
		k++;

		break;
	case NIX_TXSCH_LVL_TL3:
		reg[k] = NIX_AF_TL3X_SCHEDULE(schq);
		regval[k] = (strict_prio << 24) | rr_quantum;
		k++;

		break;
	case NIX_TXSCH_LVL_TL2:
		reg[k] = NIX_AF_TL2X_SCHEDULE(schq);
		regval[k] = (strict_prio << 24) | rr_quantum;
		k++;

		break;
	case NIX_TXSCH_LVL_TL1:
		reg[k] = NIX_AF_TL1X_SCHEDULE(schq);
		regval[k] = rr_quantum;
		k++;

		break;
	}

	return k;
}

uint8_t
nix_tm_shaper_reg_prep(struct nix_tm_node *node,
		       struct nix_tm_shaper_profile *profile,
		       volatile uint64_t *reg, volatile uint64_t *regval)
{
	struct nix_tm_shaper_data cir, pir;
	uint32_t schq = node->hw_id;
	uint64_t adjust = 0;
	uint8_t k = 0;

	nix_tm_shaper_conf_get(profile, &cir, &pir);

	if (profile && node->pkt_mode)
		adjust = profile->pkt_mode_adj;
	else if (profile)
		adjust = profile->pkt_len_adj;

	adjust &= 0x1FF;
	plt_tm_dbg("Shaper config node %s(%u) lvl %u id %u, "
		   "pir %" PRIu64 "(%" PRIu64 "B),"
		   " cir %" PRIu64 "(%" PRIu64 "B)"
		   "adjust 0x%" PRIx64 "(pktmode %u) (%p)",
		   nix_tm_hwlvl2str(node->hw_lvl), schq, node->lvl, node->id,
		   pir.rate, pir.burst, cir.rate, cir.burst, adjust,
		   node->pkt_mode, node);

	switch (node->hw_lvl) {
	case NIX_TXSCH_LVL_SMQ:
		/* Configure PIR, CIR */
		reg[k] = NIX_AF_MDQX_PIR(schq);
		regval[k] = (pir.rate && pir.burst) ?
					  (nix_tm_shaper2regval(&pir) | 1) :
					  0;
		k++;

		reg[k] = NIX_AF_MDQX_CIR(schq);
		regval[k] = (cir.rate && cir.burst) ?
					  (nix_tm_shaper2regval(&cir) | 1) :
					  0;
		k++;

		/* Configure RED ALG */
		reg[k] = NIX_AF_MDQX_SHAPE(schq);
		regval[k] = (adjust | (uint64_t)node->red_algo << 9 |
			     (uint64_t)node->pkt_mode << 24);
		k++;
		break;
	case NIX_TXSCH_LVL_TL4:
		/* Configure PIR, CIR */
		reg[k] = NIX_AF_TL4X_PIR(schq);
		regval[k] = (pir.rate && pir.burst) ?
					  (nix_tm_shaper2regval(&pir) | 1) :
					  0;
		k++;

		reg[k] = NIX_AF_TL4X_CIR(schq);
		regval[k] = (cir.rate && cir.burst) ?
					  (nix_tm_shaper2regval(&cir) | 1) :
					  0;
		k++;

		/* Configure RED algo */
		reg[k] = NIX_AF_TL4X_SHAPE(schq);
		regval[k] = (adjust | (uint64_t)node->red_algo << 9 |
			     (uint64_t)node->pkt_mode << 24);
		k++;
		break;
	case NIX_TXSCH_LVL_TL3:
		/* Configure PIR, CIR */
		reg[k] = NIX_AF_TL3X_PIR(schq);
		regval[k] = (pir.rate && pir.burst) ?
					  (nix_tm_shaper2regval(&pir) | 1) :
					  0;
		k++;

		reg[k] = NIX_AF_TL3X_CIR(schq);
		regval[k] = (cir.rate && cir.burst) ?
					  (nix_tm_shaper2regval(&cir) | 1) :
					  0;
		k++;

		/* Configure RED algo */
		reg[k] = NIX_AF_TL3X_SHAPE(schq);
		regval[k] = (adjust | (uint64_t)node->red_algo << 9 |
			     (uint64_t)node->pkt_mode << 24);
		k++;

		break;
	case NIX_TXSCH_LVL_TL2:
		/* Configure PIR, CIR */
		reg[k] = NIX_AF_TL2X_PIR(schq);
		regval[k] = (pir.rate && pir.burst) ?
					  (nix_tm_shaper2regval(&pir) | 1) :
					  0;
		k++;

		reg[k] = NIX_AF_TL2X_CIR(schq);
		regval[k] = (cir.rate && cir.burst) ?
					  (nix_tm_shaper2regval(&cir) | 1) :
					  0;
		k++;

		/* Configure RED algo */
		reg[k] = NIX_AF_TL2X_SHAPE(schq);
		regval[k] = (adjust | (uint64_t)node->red_algo << 9 |
			     (uint64_t)node->pkt_mode << 24);
		k++;

		break;
	case NIX_TXSCH_LVL_TL1:
		/* Configure CIR */
		reg[k] = NIX_AF_TL1X_CIR(schq);
		regval[k] = (cir.rate && cir.burst) ?
					  (nix_tm_shaper2regval(&cir) | 1) :
					  0;
		k++;

		/* Configure length disable and adjust */
		reg[k] = NIX_AF_TL1X_SHAPE(schq);
		regval[k] = (adjust | (uint64_t)node->pkt_mode << 24);
		k++;
		break;
	}

	return k;
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

uint16_t
nix_tm_resource_estimate(struct nix *nix, uint16_t *schq_contig, uint16_t *schq,
			 enum roc_nix_tm_tree tree)
{
	struct nix_tm_node_list *list;
	uint8_t contig_cnt, hw_lvl;
	struct nix_tm_node *parent;
	uint16_t cnt = 0, avail;

	list = nix_tm_node_list(nix, tree);
	/* Walk through parents from TL1..TL4 */
	for (hw_lvl = NIX_TXSCH_LVL_TL1; hw_lvl > 0; hw_lvl--) {
		TAILQ_FOREACH(parent, list, node) {
			if (hw_lvl != parent->hw_lvl)
				continue;

			/* Skip accounting for children whose
			 * parent does not indicate so.
			 */
			if (!parent->child_realloc)
				continue;

			/* Count children needed */
			schq[hw_lvl - 1] += parent->rr_num;
			if (parent->max_prio != UINT32_MAX) {
				contig_cnt = parent->max_prio + 1;
				schq_contig[hw_lvl - 1] += contig_cnt;
				/* When we have SP + DWRR at a parent,
				 * we will always have a spare schq at rr prio
				 * location in contiguous queues. Hence reduce
				 * discontiguous count by 1.
				 */
				if (parent->max_prio > 0 && parent->rr_num)
					schq[hw_lvl - 1] -= 1;
			}
		}
	}

	schq[nix->tm_root_lvl] = 1;
	if (!nix_tm_have_tl1_access(nix))
		schq[NIX_TXSCH_LVL_TL1] = 1;

	/* Now check for existing resources */
	for (hw_lvl = 0; hw_lvl < NIX_TXSCH_LVL_CNT; hw_lvl++) {
		avail = nix_tm_resource_avail(nix, hw_lvl, false);
		if (schq[hw_lvl] <= avail)
			schq[hw_lvl] = 0;
		else
			schq[hw_lvl] -= avail;

		/* For contiguous queues, realloc everything */
		avail = nix_tm_resource_avail(nix, hw_lvl, true);
		if (schq_contig[hw_lvl] <= avail)
			schq_contig[hw_lvl] = 0;

		cnt += schq[hw_lvl];
		cnt += schq_contig[hw_lvl];

		plt_tm_dbg("Estimate resources needed for %s: dis %u cont %u",
			   nix_tm_hwlvl2str(hw_lvl), schq[hw_lvl],
			   schq_contig[hw_lvl]);
	}

	return cnt;
}

uint16_t
roc_nix_tm_leaf_cnt(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_node_list *list;
	struct nix_tm_node *node;
	uint16_t leaf_cnt = 0;

	/* Count leafs only in user list */
	list = nix_tm_node_list(nix, ROC_NIX_TM_USER);
	TAILQ_FOREACH(node, list, node) {
		if (node->id < nix->nb_tx_queues)
			leaf_cnt++;
	}

	return leaf_cnt;
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

int
roc_nix_tm_node_stats_get(struct roc_nix *roc_nix, uint32_t node_id, bool clear,
			  struct roc_nix_tm_node_stats *n_stats)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txschq_config *req, *rsp;
	struct nix_tm_node *node;
	uint32_t schq;
	int rc, i;

	node = nix_tm_node_search(nix, node_id, ROC_NIX_TM_USER);
	if (!node)
		return NIX_ERR_TM_INVALID_NODE;

	if (node->hw_lvl != NIX_TXSCH_LVL_TL1)
		return NIX_ERR_OP_NOTSUP;

	/* Check if node has HW resource */
	if (!(node->flags & NIX_TM_NODE_HWRES))
		return 0;

	schq = node->hw_id;
	/* Skip fetch if not requested */
	if (!n_stats)
		goto clear_stats;

	memset(n_stats, 0, sizeof(struct roc_nix_tm_node_stats));

	req = mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->read = 1;
	req->lvl = NIX_TXSCH_LVL_TL1;

	i = 0;
	req->reg[i++] = NIX_AF_TL1X_DROPPED_PACKETS(schq);
	req->reg[i++] = NIX_AF_TL1X_DROPPED_BYTES(schq);
	req->reg[i++] = NIX_AF_TL1X_GREEN_PACKETS(schq);
	req->reg[i++] = NIX_AF_TL1X_GREEN_BYTES(schq);
	req->reg[i++] = NIX_AF_TL1X_YELLOW_PACKETS(schq);
	req->reg[i++] = NIX_AF_TL1X_YELLOW_BYTES(schq);
	req->reg[i++] = NIX_AF_TL1X_RED_PACKETS(schq);
	req->reg[i++] = NIX_AF_TL1X_RED_BYTES(schq);
	req->num_regs = i;

	rc = mbox_process_msg(mbox, (void **)&rsp);
	if (rc)
		return rc;

	/* Return stats */
	n_stats->stats[ROC_NIX_TM_NODE_PKTS_DROPPED] = rsp->regval[0];
	n_stats->stats[ROC_NIX_TM_NODE_BYTES_DROPPED] = rsp->regval[1];
	n_stats->stats[ROC_NIX_TM_NODE_GREEN_PKTS] = rsp->regval[2];
	n_stats->stats[ROC_NIX_TM_NODE_GREEN_BYTES] = rsp->regval[3];
	n_stats->stats[ROC_NIX_TM_NODE_YELLOW_PKTS] = rsp->regval[4];
	n_stats->stats[ROC_NIX_TM_NODE_YELLOW_BYTES] = rsp->regval[5];
	n_stats->stats[ROC_NIX_TM_NODE_RED_PKTS] = rsp->regval[6];
	n_stats->stats[ROC_NIX_TM_NODE_RED_BYTES] = rsp->regval[7];

clear_stats:
	if (!clear)
		return 0;

	/* Clear all the stats */
	req = mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = NIX_TXSCH_LVL_TL1;
	i = 0;
	req->reg[i++] = NIX_AF_TL1X_DROPPED_PACKETS(schq);
	req->reg[i++] = NIX_AF_TL1X_DROPPED_BYTES(schq);
	req->reg[i++] = NIX_AF_TL1X_GREEN_PACKETS(schq);
	req->reg[i++] = NIX_AF_TL1X_GREEN_BYTES(schq);
	req->reg[i++] = NIX_AF_TL1X_YELLOW_PACKETS(schq);
	req->reg[i++] = NIX_AF_TL1X_YELLOW_BYTES(schq);
	req->reg[i++] = NIX_AF_TL1X_RED_PACKETS(schq);
	req->reg[i++] = NIX_AF_TL1X_RED_BYTES(schq);
	req->num_regs = i;

	return mbox_process_msg(mbox, (void **)&rsp);
}

bool
roc_nix_tm_is_user_hierarchy_enabled(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	if ((nix->tm_flags & NIX_TM_HIERARCHY_ENA) &&
	    (nix->tm_tree == ROC_NIX_TM_USER))
		return true;
	return false;
}

int
roc_nix_tm_tree_type_get(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->tm_tree;
}

int
roc_nix_tm_max_prio(struct roc_nix *roc_nix, int lvl)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int hw_lvl = nix_tm_lvl2nix(nix, lvl);

	return nix_tm_max_prio(nix, hw_lvl);
}

int
roc_nix_tm_lvl_is_leaf(struct roc_nix *roc_nix, int lvl)
{
	return nix_tm_is_leaf(roc_nix_to_nix_priv(roc_nix), lvl);
}

void
roc_nix_tm_shaper_default_red_algo(struct roc_nix_tm_node *node,
				   struct roc_nix_tm_shaper_profile *roc_prof)
{
	struct nix_tm_node *tm_node = (struct nix_tm_node *)node;
	struct nix_tm_shaper_profile *profile;
	struct nix_tm_shaper_data cir, pir;

	profile = (struct nix_tm_shaper_profile *)roc_prof->reserved;
	tm_node->red_algo = NIX_REDALG_STD;

	/* C0 doesn't support STALL when both PIR & CIR are enabled */
	if (profile && roc_model_is_cn96_cx()) {
		nix_tm_shaper_conf_get(profile, &cir, &pir);

		if (pir.rate && cir.rate)
			tm_node->red_algo = NIX_REDALG_DISCARD;
	}
}

int
roc_nix_tm_lvl_cnt_get(struct roc_nix *roc_nix)
{
	if (nix_tm_have_tl1_access(roc_nix_to_nix_priv(roc_nix)))
		return NIX_TXSCH_LVL_CNT;

	return (NIX_TXSCH_LVL_CNT - 1);
}

int
roc_nix_tm_lvl_have_link_access(struct roc_nix *roc_nix, int lvl)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	if (nix_tm_lvl2nix(nix, lvl) == NIX_TXSCH_LVL_TL1)
		return 1;

	return 0;
}
