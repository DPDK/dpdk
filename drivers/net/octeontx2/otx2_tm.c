/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_malloc.h>

#include "otx2_ethdev.h"
#include "otx2_tm.h"

/* Use last LVL_CNT nodes as default nodes */
#define NIX_DEFAULT_NODE_ID_START (RTE_TM_NODE_ID_NULL - NIX_TXSCH_LVL_CNT)

enum otx2_tm_node_level {
	OTX2_TM_LVL_ROOT = 0,
	OTX2_TM_LVL_SCH1,
	OTX2_TM_LVL_SCH2,
	OTX2_TM_LVL_SCH3,
	OTX2_TM_LVL_SCH4,
	OTX2_TM_LVL_QUEUE,
	OTX2_TM_LVL_MAX,
};

static inline
uint64_t shaper2regval(struct shaper_params *shaper)
{
	return (shaper->burst_exponent << 37) | (shaper->burst_mantissa << 29) |
		(shaper->div_exp << 13) | (shaper->exponent << 9) |
		(shaper->mantissa << 1);
}

int
otx2_nix_get_link(struct otx2_eth_dev *dev)
{
	int link = 13 /* SDP */;
	uint16_t lmac_chan;
	uint16_t map;

	lmac_chan = dev->tx_chan_base;

	/* CGX lmac link */
	if (lmac_chan >= 0x800) {
		map = lmac_chan & 0x7FF;
		link = 4 * ((map >> 8) & 0xF) + ((map >> 4) & 0xF);
	} else if (lmac_chan < 0x700) {
		/* LBK channel */
		link = 12;
	}

	return link;
}

static uint8_t
nix_get_relchan(struct otx2_eth_dev *dev)
{
	return dev->tx_chan_base & 0xff;
}

static bool
nix_tm_have_tl1_access(struct otx2_eth_dev *dev)
{
	bool is_lbk = otx2_dev_is_lbk(dev);
	return otx2_dev_is_pf(dev) && !otx2_dev_is_Ax(dev) && !is_lbk;
}

static bool
nix_tm_is_leaf(struct otx2_eth_dev *dev, int lvl)
{
	if (nix_tm_have_tl1_access(dev))
		return (lvl == OTX2_TM_LVL_QUEUE);

	return (lvl == OTX2_TM_LVL_SCH4);
}

static int
find_prio_anchor(struct otx2_eth_dev *dev, uint32_t node_id)
{
	struct otx2_nix_tm_node *child_node;

	TAILQ_FOREACH(child_node, &dev->node_list, node) {
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


static struct otx2_nix_tm_shaper_profile *
nix_tm_shaper_profile_search(struct otx2_eth_dev *dev, uint32_t shaper_id)
{
	struct otx2_nix_tm_shaper_profile *tm_shaper_profile;

	TAILQ_FOREACH(tm_shaper_profile, &dev->shaper_profile_list, shaper) {
		if (tm_shaper_profile->shaper_profile_id == shaper_id)
			return tm_shaper_profile;
	}
	return NULL;
}

static inline uint64_t
shaper_rate_to_nix(uint64_t value, uint64_t *exponent_p,
		   uint64_t *mantissa_p, uint64_t *div_exp_p)
{
	uint64_t div_exp, exponent, mantissa;

	/* Boundary checks */
	if (value < MIN_SHAPER_RATE ||
	    value > MAX_SHAPER_RATE)
		return 0;

	if (value <= SHAPER_RATE(0, 0, 0)) {
		/* Calculate rate div_exp and mantissa using
		 * the following formula:
		 *
		 * value = (2E6 * (256 + mantissa)
		 *              / ((1 << div_exp) * 256))
		 */
		div_exp = 0;
		exponent = 0;
		mantissa = MAX_RATE_MANTISSA;

		while (value < (NIX_SHAPER_RATE_CONST / (1 << div_exp)))
			div_exp += 1;

		while (value <
		       ((NIX_SHAPER_RATE_CONST * (256 + mantissa)) /
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
		exponent = MAX_RATE_EXPONENT;
		mantissa = MAX_RATE_MANTISSA;

		while (value < (NIX_SHAPER_RATE_CONST * (1 << exponent)))
			exponent -= 1;

		while (value < ((NIX_SHAPER_RATE_CONST *
				((256 + mantissa) << exponent)) / 256))
			mantissa -= 1;
	}

	if (div_exp > MAX_RATE_DIV_EXP ||
	    exponent > MAX_RATE_EXPONENT || mantissa > MAX_RATE_MANTISSA)
		return 0;

	if (div_exp_p)
		*div_exp_p = div_exp;
	if (exponent_p)
		*exponent_p = exponent;
	if (mantissa_p)
		*mantissa_p = mantissa;

	/* Calculate real rate value */
	return SHAPER_RATE(exponent, mantissa, div_exp);
}

static inline uint64_t
shaper_burst_to_nix(uint64_t value, uint64_t *exponent_p,
		    uint64_t *mantissa_p)
{
	uint64_t exponent, mantissa;

	if (value < MIN_SHAPER_BURST || value > MAX_SHAPER_BURST)
		return 0;

	/* Calculate burst exponent and mantissa using
	 * the following formula:
	 *
	 * value = (((256 + mantissa) << (exponent + 1)
	 / 256)
	 *
	 */
	exponent = MAX_BURST_EXPONENT;
	mantissa = MAX_BURST_MANTISSA;

	while (value < (1ull << (exponent + 1)))
		exponent -= 1;

	while (value < ((256 + mantissa) << (exponent + 1)) / 256)
		mantissa -= 1;

	if (exponent > MAX_BURST_EXPONENT || mantissa > MAX_BURST_MANTISSA)
		return 0;

	if (exponent_p)
		*exponent_p = exponent;
	if (mantissa_p)
		*mantissa_p = mantissa;

	return SHAPER_BURST(exponent, mantissa);
}

static void
shaper_config_to_nix(struct otx2_nix_tm_shaper_profile *profile,
		     struct shaper_params *cir,
		     struct shaper_params *pir)
{
	struct rte_tm_shaper_params *param = &profile->params;

	if (!profile)
		return;

	/* Calculate CIR exponent and mantissa */
	if (param->committed.rate)
		cir->rate = shaper_rate_to_nix(param->committed.rate,
					       &cir->exponent,
					       &cir->mantissa,
					       &cir->div_exp);

	/* Calculate PIR exponent and mantissa */
	if (param->peak.rate)
		pir->rate = shaper_rate_to_nix(param->peak.rate,
					       &pir->exponent,
					       &pir->mantissa,
					       &pir->div_exp);

	/* Calculate CIR burst exponent and mantissa */
	if (param->committed.size)
		cir->burst = shaper_burst_to_nix(param->committed.size,
						 &cir->burst_exponent,
						 &cir->burst_mantissa);

	/* Calculate PIR burst exponent and mantissa */
	if (param->peak.size)
		pir->burst = shaper_burst_to_nix(param->peak.size,
						 &pir->burst_exponent,
						 &pir->burst_mantissa);
}

static void
shaper_default_red_algo(struct otx2_eth_dev *dev,
			struct otx2_nix_tm_node *tm_node,
			struct otx2_nix_tm_shaper_profile *profile)
{
	struct shaper_params cir, pir;

	/* C0 doesn't support STALL when both PIR & CIR are enabled */
	if (profile && otx2_dev_is_96xx_Cx(dev)) {
		memset(&cir, 0, sizeof(cir));
		memset(&pir, 0, sizeof(pir));
		shaper_config_to_nix(profile, &cir, &pir);

		if (pir.rate && cir.rate) {
			tm_node->red_algo = NIX_REDALG_DISCARD;
			tm_node->flags |= NIX_TM_NODE_RED_DISCARD;
			return;
		}
	}

	tm_node->red_algo = NIX_REDALG_STD;
	tm_node->flags &= ~NIX_TM_NODE_RED_DISCARD;
}

static int
populate_tm_tl1_default(struct otx2_eth_dev *dev, uint32_t schq)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_txschq_config *req;

	/*
	 * Default config for TL1.
	 * For VF this is always ignored.
	 */

	req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = NIX_TXSCH_LVL_TL1;

	/* Set DWRR quantum */
	req->reg[0] = NIX_AF_TL1X_SCHEDULE(schq);
	req->regval[0] = TXSCH_TL1_DFLT_RR_QTM;
	req->num_regs++;

	req->reg[1] = NIX_AF_TL1X_TOPOLOGY(schq);
	req->regval[1] = (TXSCH_TL1_DFLT_RR_PRIO << 1);
	req->num_regs++;

	req->reg[2] = NIX_AF_TL1X_CIR(schq);
	req->regval[2] = 0;
	req->num_regs++;

	return otx2_mbox_process(mbox);
}

static uint8_t
prepare_tm_sched_reg(struct otx2_eth_dev *dev,
		     struct otx2_nix_tm_node *tm_node,
		     volatile uint64_t *reg, volatile uint64_t *regval)
{
	uint64_t strict_prio = tm_node->priority;
	uint32_t hw_lvl = tm_node->hw_lvl;
	uint32_t schq = tm_node->hw_id;
	uint64_t rr_quantum;
	uint8_t k = 0;

	rr_quantum = NIX_TM_WEIGHT_TO_RR_QUANTUM(tm_node->weight);

	/* For children to root, strict prio is default if either
	 * device root is TL2 or TL1 Static Priority is disabled.
	 */
	if (hw_lvl == NIX_TXSCH_LVL_TL2 &&
	    (dev->otx2_tm_root_lvl == NIX_TXSCH_LVL_TL2 ||
	     dev->tm_flags & NIX_TM_TL1_NO_SP))
		strict_prio = TXSCH_TL1_DFLT_RR_PRIO;

	otx2_tm_dbg("Schedule config node %s(%u) lvl %u id %u, "
		     "prio 0x%" PRIx64 ", rr_quantum 0x%" PRIx64 " (%p)",
		     nix_hwlvl2str(tm_node->hw_lvl), schq, tm_node->lvl,
		     tm_node->id, strict_prio, rr_quantum, tm_node);

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

static uint8_t
prepare_tm_shaper_reg(struct otx2_nix_tm_node *tm_node,
		      struct otx2_nix_tm_shaper_profile *profile,
		      volatile uint64_t *reg, volatile uint64_t *regval)
{
	struct shaper_params cir, pir;
	uint32_t schq = tm_node->hw_id;
	uint8_t k = 0;

	memset(&cir, 0, sizeof(cir));
	memset(&pir, 0, sizeof(pir));
	shaper_config_to_nix(profile, &cir, &pir);

	otx2_tm_dbg("Shaper config node %s(%u) lvl %u id %u, "
		    "pir %" PRIu64 "(%" PRIu64 "B),"
		     " cir %" PRIu64 "(%" PRIu64 "B) (%p)",
		     nix_hwlvl2str(tm_node->hw_lvl), schq, tm_node->lvl,
		     tm_node->id, pir.rate, pir.burst,
		     cir.rate, cir.burst, tm_node);

	switch (tm_node->hw_lvl) {
	case NIX_TXSCH_LVL_SMQ:
		/* Configure PIR, CIR */
		reg[k] = NIX_AF_MDQX_PIR(schq);
		regval[k] = (pir.rate && pir.burst) ?
				(shaper2regval(&pir) | 1) : 0;
		k++;

		reg[k] = NIX_AF_MDQX_CIR(schq);
		regval[k] = (cir.rate && cir.burst) ?
				(shaper2regval(&cir) | 1) : 0;
		k++;

		/* Configure RED ALG */
		reg[k] = NIX_AF_MDQX_SHAPE(schq);
		regval[k] = ((uint64_t)tm_node->red_algo << 9);
		k++;
		break;
	case NIX_TXSCH_LVL_TL4:
		/* Configure PIR, CIR */
		reg[k] = NIX_AF_TL4X_PIR(schq);
		regval[k] = (pir.rate && pir.burst) ?
				(shaper2regval(&pir) | 1) : 0;
		k++;

		reg[k] = NIX_AF_TL4X_CIR(schq);
		regval[k] = (cir.rate && cir.burst) ?
				(shaper2regval(&cir) | 1) : 0;
		k++;

		/* Configure RED algo */
		reg[k] = NIX_AF_TL4X_SHAPE(schq);
		regval[k] = ((uint64_t)tm_node->red_algo << 9);
		k++;
		break;
	case NIX_TXSCH_LVL_TL3:
		/* Configure PIR, CIR */
		reg[k] = NIX_AF_TL3X_PIR(schq);
		regval[k] = (pir.rate && pir.burst) ?
				(shaper2regval(&pir) | 1) : 0;
		k++;

		reg[k] = NIX_AF_TL3X_CIR(schq);
		regval[k] = (cir.rate && cir.burst) ?
				(shaper2regval(&cir) | 1) : 0;
		k++;

		/* Configure RED algo */
		reg[k] = NIX_AF_TL3X_SHAPE(schq);
		regval[k] = ((uint64_t)tm_node->red_algo << 9);
		k++;

		break;
	case NIX_TXSCH_LVL_TL2:
		/* Configure PIR, CIR */
		reg[k] = NIX_AF_TL2X_PIR(schq);
		regval[k] = (pir.rate && pir.burst) ?
				(shaper2regval(&pir) | 1) : 0;
		k++;

		reg[k] = NIX_AF_TL2X_CIR(schq);
		regval[k] = (cir.rate && cir.burst) ?
				(shaper2regval(&cir) | 1) : 0;
		k++;

		/* Configure RED algo */
		reg[k] = NIX_AF_TL2X_SHAPE(schq);
		regval[k] = ((uint64_t)tm_node->red_algo << 9);
		k++;

		break;
	case NIX_TXSCH_LVL_TL1:
		/* Configure CIR */
		reg[k] = NIX_AF_TL1X_CIR(schq);
		regval[k] = (cir.rate && cir.burst) ?
				(shaper2regval(&cir) | 1) : 0;
		k++;
		break;
	}

	return k;
}

static uint8_t
prepare_tm_sw_xoff(struct otx2_nix_tm_node *tm_node, bool enable,
		   volatile uint64_t *reg, volatile uint64_t *regval)
{
	uint32_t hw_lvl = tm_node->hw_lvl;
	uint32_t schq = tm_node->hw_id;
	uint8_t k = 0;

	otx2_tm_dbg("sw xoff config node %s(%u) lvl %u id %u, enable %u (%p)",
		    nix_hwlvl2str(hw_lvl), schq, tm_node->lvl,
		    tm_node->id, enable, tm_node);

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

static int
populate_tm_reg(struct otx2_eth_dev *dev,
		struct otx2_nix_tm_node *tm_node)
{
	struct otx2_nix_tm_shaper_profile *profile;
	uint64_t regval_mask[MAX_REGS_PER_MBOX_MSG];
	uint64_t regval[MAX_REGS_PER_MBOX_MSG];
	uint64_t reg[MAX_REGS_PER_MBOX_MSG];
	struct otx2_mbox *mbox = dev->mbox;
	uint64_t parent = 0, child = 0;
	uint32_t hw_lvl, rr_prio, schq;
	struct nix_txschq_config *req;
	int rc = -EFAULT;
	uint8_t k = 0;

	memset(regval_mask, 0, sizeof(regval_mask));
	profile = nix_tm_shaper_profile_search(dev,
					tm_node->params.shaper_profile_id);
	rr_prio = tm_node->rr_prio;
	hw_lvl = tm_node->hw_lvl;
	schq = tm_node->hw_id;

	/* Root node will not have a parent node */
	if (hw_lvl == dev->otx2_tm_root_lvl)
		parent = tm_node->parent_hw_id;
	else
		parent = tm_node->parent->hw_id;

	/* Do we need this trigger to configure TL1 */
	if (dev->otx2_tm_root_lvl == NIX_TXSCH_LVL_TL2 &&
	    hw_lvl == dev->otx2_tm_root_lvl) {
		rc = populate_tm_tl1_default(dev, parent);
		if (rc)
			goto error;
	}

	if (hw_lvl != NIX_TXSCH_LVL_SMQ)
		child = find_prio_anchor(dev, tm_node->id);

	/* Override default rr_prio when TL1
	 * Static Priority is disabled
	 */
	if (hw_lvl == NIX_TXSCH_LVL_TL1 &&
	    dev->tm_flags & NIX_TM_TL1_NO_SP) {
		rr_prio = TXSCH_TL1_DFLT_RR_PRIO;
		child = 0;
	}

	otx2_tm_dbg("Topology config node %s(%u)->%s(%"PRIu64") lvl %u, id %u"
		    " prio_anchor %"PRIu64" rr_prio %u (%p)",
		    nix_hwlvl2str(hw_lvl), schq, nix_hwlvl2str(hw_lvl + 1),
		    parent, tm_node->lvl, tm_node->id, child, rr_prio, tm_node);

	/* Prepare Topology and Link config */
	switch (hw_lvl) {
	case NIX_TXSCH_LVL_SMQ:

		/* Set xoff which will be cleared later and minimum length
		 * which will be used for zero padding if packet length is
		 * smaller
		 */
		reg[k] = NIX_AF_SMQX_CFG(schq);
		regval[k] = BIT_ULL(50) | NIX_MIN_HW_FRS;
		regval_mask[k] = ~(BIT_ULL(50) | 0x7f);
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
		if (otx2_dev_is_sdp(dev)) {
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
		if (!otx2_dev_is_sdp(dev) &&
		    dev->link_cfg_lvl == NIX_TXSCH_LVL_TL3) {
			reg[k] = NIX_AF_TL3_TL2X_LINKX_CFG(schq,
						otx2_nix_get_link(dev));
			regval[k] = BIT_ULL(12) | nix_get_relchan(dev);
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
		if (!otx2_dev_is_sdp(dev) &&
		    dev->link_cfg_lvl == NIX_TXSCH_LVL_TL2) {
			reg[k] = NIX_AF_TL3_TL2X_LINKX_CFG(schq,
						otx2_nix_get_link(dev));
			regval[k] = BIT_ULL(12) | nix_get_relchan(dev);
			k++;
		}

		break;
	case NIX_TXSCH_LVL_TL1:
		reg[k] = NIX_AF_TL1X_TOPOLOGY(schq);
		regval[k] = (child << 32) | (rr_prio << 1 /*RR_PRIO*/);
		k++;

		break;
	}

	/* Prepare schedule config */
	k += prepare_tm_sched_reg(dev, tm_node, &reg[k], &regval[k]);

	/* Prepare shaping config */
	k += prepare_tm_shaper_reg(tm_node, profile, &reg[k], &regval[k]);

	if (!k)
		return 0;

	/* Copy and send config mbox */
	req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = hw_lvl;
	req->num_regs = k;

	otx2_mbox_memcpy(req->reg, reg, sizeof(uint64_t) * k);
	otx2_mbox_memcpy(req->regval, regval, sizeof(uint64_t) * k);
	otx2_mbox_memcpy(req->regval_mask, regval_mask, sizeof(uint64_t) * k);

	rc = otx2_mbox_process(mbox);
	if (rc)
		goto error;

	return 0;
error:
	otx2_err("Txschq cfg request failed for node %p, rc=%d", tm_node, rc);
	return rc;
}


static int
nix_tm_txsch_reg_config(struct otx2_eth_dev *dev)
{
	struct otx2_nix_tm_node *tm_node;
	uint32_t hw_lvl;
	int rc = 0;

	for (hw_lvl = 0; hw_lvl <= dev->otx2_tm_root_lvl; hw_lvl++) {
		TAILQ_FOREACH(tm_node, &dev->node_list, node) {
			if (tm_node->hw_lvl == hw_lvl &&
			    tm_node->hw_lvl != NIX_TXSCH_LVL_CNT) {
				rc = populate_tm_reg(dev, tm_node);
				if (rc)
					goto exit;
			}
		}
	}
exit:
	return rc;
}

static struct otx2_nix_tm_node *
nix_tm_node_search(struct otx2_eth_dev *dev,
		   uint32_t node_id, bool user)
{
	struct otx2_nix_tm_node *tm_node;

	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (tm_node->id == node_id &&
		    (user == !!(tm_node->flags & NIX_TM_NODE_USER)))
			return tm_node;
	}
	return NULL;
}

static uint32_t
check_rr(struct otx2_eth_dev *dev, uint32_t priority, uint32_t parent_id)
{
	struct otx2_nix_tm_node *tm_node;
	uint32_t rr_num = 0;

	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (!tm_node->parent)
			continue;

		if (!(tm_node->parent->id == parent_id))
			continue;

		if (tm_node->priority == priority)
			rr_num++;
	}
	return rr_num;
}

static int
nix_tm_update_parent_info(struct otx2_eth_dev *dev)
{
	struct otx2_nix_tm_node *tm_node_child;
	struct otx2_nix_tm_node *tm_node;
	struct otx2_nix_tm_node *parent;
	uint32_t rr_num = 0;
	uint32_t priority;

	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (!tm_node->parent)
			continue;
		/* Count group of children of same priority i.e are RR */
		parent = tm_node->parent;
		priority = tm_node->priority;
		rr_num = check_rr(dev, priority, parent->id);

		/* Assuming that multiple RR groups are
		 * not configured based on capability.
		 */
		if (rr_num > 1) {
			parent->rr_prio = priority;
			parent->rr_num = rr_num;
		}

		/* Find out static priority children that are not in RR */
		TAILQ_FOREACH(tm_node_child, &dev->node_list, node) {
			if (!tm_node_child->parent)
				continue;
			if (parent->id != tm_node_child->parent->id)
				continue;
			if (parent->max_prio == UINT32_MAX &&
			    tm_node_child->priority != parent->rr_prio)
				parent->max_prio = 0;

			if (parent->max_prio < tm_node_child->priority &&
			    parent->rr_prio != tm_node_child->priority)
				parent->max_prio = tm_node_child->priority;
		}
	}

	return 0;
}

static int
nix_tm_node_add_to_list(struct otx2_eth_dev *dev, uint32_t node_id,
			uint32_t parent_node_id, uint32_t priority,
			uint32_t weight, uint16_t hw_lvl,
			uint16_t lvl, bool user,
			struct rte_tm_node_params *params)
{
	struct otx2_nix_tm_shaper_profile *profile;
	struct otx2_nix_tm_node *tm_node, *parent_node;
	uint32_t profile_id;

	profile_id = params->shaper_profile_id;
	profile = nix_tm_shaper_profile_search(dev, profile_id);

	parent_node = nix_tm_node_search(dev, parent_node_id, user);

	tm_node = rte_zmalloc("otx2_nix_tm_node",
			      sizeof(struct otx2_nix_tm_node), 0);
	if (!tm_node)
		return -ENOMEM;

	tm_node->lvl = lvl;
	tm_node->hw_lvl = hw_lvl;

	/* Maintain minimum weight */
	if (!weight)
		weight = 1;

	tm_node->id = node_id;
	tm_node->priority = priority;
	tm_node->weight = weight;
	tm_node->rr_prio = 0xf;
	tm_node->max_prio = UINT32_MAX;
	tm_node->hw_id = UINT32_MAX;
	tm_node->flags = 0;
	if (user)
		tm_node->flags = NIX_TM_NODE_USER;
	rte_memcpy(&tm_node->params, params, sizeof(struct rte_tm_node_params));

	if (profile)
		profile->reference_count++;

	tm_node->parent = parent_node;
	tm_node->parent_hw_id = UINT32_MAX;
	shaper_default_red_algo(dev, tm_node, profile);

	TAILQ_INSERT_TAIL(&dev->node_list, tm_node, node);

	return 0;
}

static int
nix_tm_clear_shaper_profiles(struct otx2_eth_dev *dev)
{
	struct otx2_nix_tm_shaper_profile *shaper_profile;

	while ((shaper_profile = TAILQ_FIRST(&dev->shaper_profile_list))) {
		if (shaper_profile->reference_count)
			otx2_tm_dbg("Shaper profile %u has non zero references",
				    shaper_profile->shaper_profile_id);
		TAILQ_REMOVE(&dev->shaper_profile_list, shaper_profile, shaper);
		rte_free(shaper_profile);
	}

	return 0;
}

static int
nix_clear_path_xoff(struct otx2_eth_dev *dev,
		    struct otx2_nix_tm_node *tm_node)
{
	struct nix_txschq_config *req;
	struct otx2_nix_tm_node *p;
	int rc;

	/* Manipulating SW_XOFF not supported on Ax */
	if (otx2_dev_is_Ax(dev))
		return 0;

	/* Enable nodes in path for flush to succeed */
	if (!nix_tm_is_leaf(dev, tm_node->lvl))
		p = tm_node;
	else
		p = tm_node->parent;
	while (p) {
		if (!(p->flags & NIX_TM_NODE_ENABLED) &&
		    (p->flags & NIX_TM_NODE_HWRES)) {
			req = otx2_mbox_alloc_msg_nix_txschq_cfg(dev->mbox);
			req->lvl = p->hw_lvl;
			req->num_regs = prepare_tm_sw_xoff(p, false, req->reg,
							   req->regval);
			rc = otx2_mbox_process(dev->mbox);
			if (rc)
				return rc;

			p->flags |= NIX_TM_NODE_ENABLED;
		}
		p = p->parent;
	}

	return 0;
}

static int
nix_smq_xoff(struct otx2_eth_dev *dev,
	     struct otx2_nix_tm_node *tm_node,
	     bool enable)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_txschq_config *req;
	uint16_t smq;
	int rc;

	smq = tm_node->hw_id;
	otx2_tm_dbg("Setting SMQ %u XOFF/FLUSH to %s", smq,
		    enable ? "enable" : "disable");

	rc = nix_clear_path_xoff(dev, tm_node);
	if (rc)
		return rc;

	req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = NIX_TXSCH_LVL_SMQ;
	req->num_regs = 1;

	req->reg[0] = NIX_AF_SMQX_CFG(smq);
	req->regval[0] = enable ? (BIT_ULL(50) | BIT_ULL(49)) : 0;
	req->regval_mask[0] = enable ?
				~(BIT_ULL(50) | BIT_ULL(49)) : ~BIT_ULL(50);

	return otx2_mbox_process(mbox);
}

int
otx2_nix_sq_sqb_aura_fc(void *__txq, bool enable)
{
	struct otx2_eth_txq *txq = __txq;
	struct npa_aq_enq_req *req;
	struct npa_aq_enq_rsp *rsp;
	struct otx2_npa_lf *lf;
	struct otx2_mbox *mbox;
	uint64_t aura_handle;
	int rc;

	otx2_tm_dbg("Setting SQ %u SQB aura FC to %s", txq->sq,
		    enable ? "enable" : "disable");

	lf = otx2_npa_lf_obj_get();
	if (!lf)
		return -EFAULT;
	mbox = lf->mbox;
	/* Set/clear sqb aura fc_ena */
	aura_handle = txq->sqb_pool->pool_id;
	req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);

	req->aura_id = npa_lf_aura_handle_to_aura(aura_handle);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_WRITE;
	/* Below is not needed for aura writes but AF driver needs it */
	/* AF will translate to associated poolctx */
	req->aura.pool_addr = req->aura_id;

	req->aura.fc_ena = enable;
	req->aura_mask.fc_ena = 1;

	rc = otx2_mbox_process(mbox);
	if (rc)
		return rc;

	/* Read back npa aura ctx */
	req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);

	req->aura_id = npa_lf_aura_handle_to_aura(aura_handle);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_READ;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	/* Init when enabled as there might be no triggers */
	if (enable)
		*(volatile uint64_t *)txq->fc_mem = rsp->aura.count;
	else
		*(volatile uint64_t *)txq->fc_mem = txq->nb_sqb_bufs;
	/* Sync write barrier */
	rte_wmb();

	return 0;
}

static int
nix_txq_flush_sq_spin(struct otx2_eth_txq *txq)
{
	uint16_t sqb_cnt, head_off, tail_off;
	struct otx2_eth_dev *dev = txq->dev;
	uint64_t wdata, val, prev;
	uint16_t sq = txq->sq;
	int64_t *regaddr;
	uint64_t timeout;/* 10's of usec */

	/* Wait for enough time based on shaper min rate */
	timeout = (txq->qconf.nb_desc * NIX_MAX_HW_FRS * 8 * 1E5);
	timeout = timeout / dev->tm_rate_min;
	if (!timeout)
		timeout = 10000;

	wdata = ((uint64_t)sq << 32);
	regaddr = (int64_t *)(dev->base + NIX_LF_SQ_OP_STATUS);
	val = otx2_atomic64_add_nosync(wdata, regaddr);

	/* Spin multiple iterations as "txq->fc_cache_pkts" can still
	 * have space to send pkts even though fc_mem is disabled
	 */

	while (true) {
		prev = val;
		rte_delay_us(10);
		val = otx2_atomic64_add_nosync(wdata, regaddr);
		/* Continue on error */
		if (val & BIT_ULL(63))
			continue;

		if (prev != val)
			continue;

		sqb_cnt = val & 0xFFFF;
		head_off = (val >> 20) & 0x3F;
		tail_off = (val >> 28) & 0x3F;

		/* SQ reached quiescent state */
		if (sqb_cnt <= 1 && head_off == tail_off &&
		    (*txq->fc_mem == txq->nb_sqb_bufs)) {
			break;
		}

		/* Timeout */
		if (!timeout)
			goto exit;
		timeout--;
	}

	return 0;
exit:
	otx2_nix_tm_dump(dev);
	return -EFAULT;
}

/* Flush and disable tx queue and its parent SMQ */
int otx2_nix_sq_flush_pre(void *_txq, bool dev_started)
{
	struct otx2_nix_tm_node *tm_node, *sibling;
	struct otx2_eth_txq *txq;
	struct otx2_eth_dev *dev;
	uint16_t sq;
	bool user;
	int rc;

	txq = _txq;
	dev = txq->dev;
	sq = txq->sq;

	user = !!(dev->tm_flags & NIX_TM_COMMITTED);

	/* Find the node for this SQ */
	tm_node = nix_tm_node_search(dev, sq, user);
	if (!tm_node || !(tm_node->flags & NIX_TM_NODE_ENABLED)) {
		otx2_err("Invalid node/state for sq %u", sq);
		return -EFAULT;
	}

	/* Enable CGX RXTX to drain pkts */
	if (!dev_started) {
		/* Though it enables both RX MCAM Entries and CGX Link
		 * we assume all the rx queues are stopped way back.
		 */
		otx2_mbox_alloc_msg_nix_lf_start_rx(dev->mbox);
		rc = otx2_mbox_process(dev->mbox);
		if (rc) {
			otx2_err("cgx start failed, rc=%d", rc);
			return rc;
		}
	}

	/* Disable smq xoff for case it was enabled earlier */
	rc = nix_smq_xoff(dev, tm_node->parent, false);
	if (rc) {
		otx2_err("Failed to enable smq %u, rc=%d",
			 tm_node->parent->hw_id, rc);
		return rc;
	}

	/* As per HRM, to disable an SQ, all other SQ's
	 * that feed to same SMQ must be paused before SMQ flush.
	 */
	TAILQ_FOREACH(sibling, &dev->node_list, node) {
		if (sibling->parent != tm_node->parent)
			continue;
		if (!(sibling->flags & NIX_TM_NODE_ENABLED))
			continue;

		sq = sibling->id;
		txq = dev->eth_dev->data->tx_queues[sq];
		if (!txq)
			continue;

		rc = otx2_nix_sq_sqb_aura_fc(txq, false);
		if (rc) {
			otx2_err("Failed to disable sqb aura fc, rc=%d", rc);
			goto cleanup;
		}

		/* Wait for sq entries to be flushed */
		rc = nix_txq_flush_sq_spin(txq);
		if (rc) {
			otx2_err("Failed to drain sq %u, rc=%d\n", txq->sq, rc);
			return rc;
		}
	}

	tm_node->flags &= ~NIX_TM_NODE_ENABLED;

	/* Disable and flush */
	rc = nix_smq_xoff(dev, tm_node->parent, true);
	if (rc) {
		otx2_err("Failed to disable smq %u, rc=%d",
			 tm_node->parent->hw_id, rc);
		goto cleanup;
	}
cleanup:
	/* Restore cgx state */
	if (!dev_started) {
		otx2_mbox_alloc_msg_nix_lf_stop_rx(dev->mbox);
		rc |= otx2_mbox_process(dev->mbox);
	}

	return rc;
}

int otx2_nix_sq_flush_post(void *_txq)
{
	struct otx2_nix_tm_node *tm_node, *sibling;
	struct otx2_eth_txq *txq = _txq;
	struct otx2_eth_txq *s_txq;
	struct otx2_eth_dev *dev;
	bool once = false;
	uint16_t sq, s_sq;
	bool user;
	int rc;

	dev = txq->dev;
	sq = txq->sq;
	user = !!(dev->tm_flags & NIX_TM_COMMITTED);

	/* Find the node for this SQ */
	tm_node = nix_tm_node_search(dev, sq, user);
	if (!tm_node) {
		otx2_err("Invalid node for sq %u", sq);
		return -EFAULT;
	}

	/* Enable all the siblings back */
	TAILQ_FOREACH(sibling, &dev->node_list, node) {
		if (sibling->parent != tm_node->parent)
			continue;

		if (sibling->id == sq)
			continue;

		if (!(sibling->flags & NIX_TM_NODE_ENABLED))
			continue;

		s_sq = sibling->id;
		s_txq = dev->eth_dev->data->tx_queues[s_sq];
		if (!s_txq)
			continue;

		if (!once) {
			/* Enable back if any SQ is still present */
			rc = nix_smq_xoff(dev, tm_node->parent, false);
			if (rc) {
				otx2_err("Failed to enable smq %u, rc=%d",
					 tm_node->parent->hw_id, rc);
				return rc;
			}
			once = true;
		}

		rc = otx2_nix_sq_sqb_aura_fc(s_txq, true);
		if (rc) {
			otx2_err("Failed to enable sqb aura fc, rc=%d", rc);
			return rc;
		}
	}

	return 0;
}

static int
nix_sq_sched_data(struct otx2_eth_dev *dev,
		  struct otx2_nix_tm_node *tm_node,
		  bool rr_quantum_only)
{
	struct rte_eth_dev *eth_dev = dev->eth_dev;
	struct otx2_mbox *mbox = dev->mbox;
	uint16_t sq = tm_node->id, smq;
	struct nix_aq_enq_req *req;
	uint64_t rr_quantum;
	int rc;

	smq = tm_node->parent->hw_id;
	rr_quantum = NIX_TM_WEIGHT_TO_RR_QUANTUM(tm_node->weight);

	if (rr_quantum_only)
		otx2_tm_dbg("Update sq(%u) rr_quantum 0x%"PRIx64, sq, rr_quantum);
	else
		otx2_tm_dbg("Enabling sq(%u)->smq(%u), rr_quantum 0x%"PRIx64,
			    sq, smq, rr_quantum);

	if (sq > eth_dev->data->nb_tx_queues)
		return -EFAULT;

	req = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	req->qidx = sq;
	req->ctype = NIX_AQ_CTYPE_SQ;
	req->op = NIX_AQ_INSTOP_WRITE;

	/* smq update only when needed */
	if (!rr_quantum_only) {
		req->sq.smq = smq;
		req->sq_mask.smq = ~req->sq_mask.smq;
	}
	req->sq.smq_rr_quantum = rr_quantum;
	req->sq_mask.smq_rr_quantum = ~req->sq_mask.smq_rr_quantum;

	rc = otx2_mbox_process(mbox);
	if (rc)
		otx2_err("Failed to set smq, rc=%d", rc);
	return rc;
}

int otx2_nix_sq_enable(void *_txq)
{
	struct otx2_eth_txq *txq = _txq;
	int rc;

	/* Enable sqb_aura fc */
	rc = otx2_nix_sq_sqb_aura_fc(txq, true);
	if (rc) {
		otx2_err("Failed to enable sqb aura fc, rc=%d", rc);
		return rc;
	}

	return 0;
}

static int
nix_tm_free_resources(struct otx2_eth_dev *dev, uint32_t flags_mask,
		      uint32_t flags, bool hw_only)
{
	struct otx2_nix_tm_shaper_profile *profile;
	struct otx2_nix_tm_node *tm_node, *next_node;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_txsch_free_req *req;
	uint32_t profile_id;
	int rc = 0;

	next_node = TAILQ_FIRST(&dev->node_list);
	while (next_node) {
		tm_node = next_node;
		next_node = TAILQ_NEXT(tm_node, node);

		/* Check for only requested nodes */
		if ((tm_node->flags & flags_mask) != flags)
			continue;

		if (!nix_tm_is_leaf(dev, tm_node->lvl) &&
		    tm_node->hw_lvl != NIX_TXSCH_LVL_TL1 &&
		    tm_node->flags & NIX_TM_NODE_HWRES) {
			/* Free specific HW resource */
			otx2_tm_dbg("Free hwres %s(%u) lvl %u id %u (%p)",
				    nix_hwlvl2str(tm_node->hw_lvl),
				    tm_node->hw_id, tm_node->lvl,
				    tm_node->id, tm_node);

			rc = nix_clear_path_xoff(dev, tm_node);
			if (rc)
				return rc;

			req = otx2_mbox_alloc_msg_nix_txsch_free(mbox);
			req->flags = 0;
			req->schq_lvl = tm_node->hw_lvl;
			req->schq = tm_node->hw_id;
			rc = otx2_mbox_process(mbox);
			if (rc)
				return rc;
			tm_node->flags &= ~NIX_TM_NODE_HWRES;
		}

		/* Leave software elements if needed */
		if (hw_only)
			continue;

		otx2_tm_dbg("Free node lvl %u id %u (%p)",
			    tm_node->lvl, tm_node->id, tm_node);

		profile_id = tm_node->params.shaper_profile_id;
		profile = nix_tm_shaper_profile_search(dev, profile_id);
		if (profile)
			profile->reference_count--;

		TAILQ_REMOVE(&dev->node_list, tm_node, node);
		rte_free(tm_node);
	}

	if (!flags_mask) {
		/* Free all hw resources */
		req = otx2_mbox_alloc_msg_nix_txsch_free(mbox);
		req->flags = TXSCHQ_FREE_ALL;

		return otx2_mbox_process(mbox);
	}

	return rc;
}

static uint8_t
nix_tm_copy_rsp_to_dev(struct otx2_eth_dev *dev,
		       struct nix_txsch_alloc_rsp *rsp)
{
	uint16_t schq;
	uint8_t lvl;

	for (lvl = 0; lvl < NIX_TXSCH_LVL_CNT; lvl++) {
		for (schq = 0; schq < MAX_TXSCHQ_PER_FUNC; schq++) {
			dev->txschq_list[lvl][schq] = rsp->schq_list[lvl][schq];
			dev->txschq_contig_list[lvl][schq] =
				rsp->schq_contig_list[lvl][schq];
		}

		dev->txschq[lvl] = rsp->schq[lvl];
		dev->txschq_contig[lvl] = rsp->schq_contig[lvl];
	}
	return 0;
}

static int
nix_tm_assign_id_to_node(struct otx2_eth_dev *dev,
			 struct otx2_nix_tm_node *child,
			 struct otx2_nix_tm_node *parent)
{
	uint32_t hw_id, schq_con_index, prio_offset;
	uint32_t l_id, schq_index;

	otx2_tm_dbg("Assign hw id for child node %s lvl %u id %u (%p)",
		    nix_hwlvl2str(child->hw_lvl), child->lvl, child->id, child);

	child->flags |= NIX_TM_NODE_HWRES;

	/* Process root nodes */
	if (dev->otx2_tm_root_lvl == NIX_TXSCH_LVL_TL2 &&
	    child->hw_lvl == dev->otx2_tm_root_lvl && !parent) {
		int idx = 0;
		uint32_t tschq_con_index;

		l_id = child->hw_lvl;
		tschq_con_index = dev->txschq_contig_index[l_id];
		hw_id = dev->txschq_contig_list[l_id][tschq_con_index];
		child->hw_id = hw_id;
		dev->txschq_contig_index[l_id]++;
		/* Update TL1 hw_id for its parent for config purpose */
		idx = dev->txschq_index[NIX_TXSCH_LVL_TL1]++;
		hw_id = dev->txschq_list[NIX_TXSCH_LVL_TL1][idx];
		child->parent_hw_id = hw_id;
		return 0;
	}
	if (dev->otx2_tm_root_lvl == NIX_TXSCH_LVL_TL1 &&
	    child->hw_lvl == dev->otx2_tm_root_lvl && !parent) {
		uint32_t tschq_con_index;

		l_id = child->hw_lvl;
		tschq_con_index = dev->txschq_index[l_id];
		hw_id = dev->txschq_list[l_id][tschq_con_index];
		child->hw_id = hw_id;
		dev->txschq_index[l_id]++;
		return 0;
	}

	/* Process children with parents */
	l_id = child->hw_lvl;
	schq_index = dev->txschq_index[l_id];
	schq_con_index = dev->txschq_contig_index[l_id];

	if (child->priority == parent->rr_prio) {
		hw_id = dev->txschq_list[l_id][schq_index];
		child->hw_id = hw_id;
		child->parent_hw_id = parent->hw_id;
		dev->txschq_index[l_id]++;
	} else {
		prio_offset = schq_con_index + child->priority;
		hw_id = dev->txschq_contig_list[l_id][prio_offset];
		child->hw_id = hw_id;
	}
	return 0;
}

static int
nix_tm_assign_hw_id(struct otx2_eth_dev *dev)
{
	struct otx2_nix_tm_node *parent, *child;
	uint32_t child_hw_lvl, con_index_inc, i;

	for (i = NIX_TXSCH_LVL_TL1; i > 0; i--) {
		TAILQ_FOREACH(parent, &dev->node_list, node) {
			child_hw_lvl = parent->hw_lvl - 1;
			if (parent->hw_lvl != i)
				continue;
			TAILQ_FOREACH(child, &dev->node_list, node) {
				if (!child->parent)
					continue;
				if (child->parent->id != parent->id)
					continue;
				nix_tm_assign_id_to_node(dev, child, parent);
			}

			con_index_inc = parent->max_prio + 1;
			dev->txschq_contig_index[child_hw_lvl] += con_index_inc;

			/*
			 * Explicitly assign id to parent node if it
			 * doesn't have a parent
			 */
			if (parent->hw_lvl == dev->otx2_tm_root_lvl)
				nix_tm_assign_id_to_node(dev, parent, NULL);
		}
	}
	return 0;
}

static uint8_t
nix_tm_count_req_schq(struct otx2_eth_dev *dev,
		      struct nix_txsch_alloc_req *req, uint8_t lvl)
{
	struct otx2_nix_tm_node *tm_node;
	uint8_t contig_count;

	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (lvl == tm_node->hw_lvl) {
			req->schq[lvl - 1] += tm_node->rr_num;
			if (tm_node->max_prio != UINT32_MAX) {
				contig_count = tm_node->max_prio + 1;
				req->schq_contig[lvl - 1] += contig_count;
			}
		}
		if (lvl == dev->otx2_tm_root_lvl &&
		    dev->otx2_tm_root_lvl && lvl == NIX_TXSCH_LVL_TL2 &&
		    tm_node->hw_lvl == dev->otx2_tm_root_lvl) {
			req->schq_contig[dev->otx2_tm_root_lvl]++;
		}
	}

	req->schq[NIX_TXSCH_LVL_TL1] = 1;
	req->schq_contig[NIX_TXSCH_LVL_TL1] = 0;

	return 0;
}

static int
nix_tm_prepare_txschq_req(struct otx2_eth_dev *dev,
			  struct nix_txsch_alloc_req *req)
{
	uint8_t i;

	for (i = NIX_TXSCH_LVL_TL1; i > 0; i--)
		nix_tm_count_req_schq(dev, req, i);

	for (i = 0; i < NIX_TXSCH_LVL_CNT; i++) {
		dev->txschq_index[i] = 0;
		dev->txschq_contig_index[i] = 0;
	}
	return 0;
}

static int
nix_tm_send_txsch_alloc_msg(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_txsch_alloc_req *req;
	struct nix_txsch_alloc_rsp *rsp;
	int rc;

	req = otx2_mbox_alloc_msg_nix_txsch_alloc(mbox);

	rc = nix_tm_prepare_txschq_req(dev, req);
	if (rc)
		return rc;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	nix_tm_copy_rsp_to_dev(dev, rsp);
	dev->link_cfg_lvl = rsp->link_cfg_lvl;

	nix_tm_assign_hw_id(dev);
	return 0;
}

static int
nix_tm_alloc_resources(struct rte_eth_dev *eth_dev, bool xmit_enable)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_node *tm_node;
	struct otx2_eth_txq *txq;
	uint16_t sq;
	int rc;

	nix_tm_update_parent_info(dev);

	rc = nix_tm_send_txsch_alloc_msg(dev);
	if (rc) {
		otx2_err("TM failed to alloc tm resources=%d", rc);
		return rc;
	}

	rc = nix_tm_txsch_reg_config(dev);
	if (rc) {
		otx2_err("TM failed to configure sched registers=%d", rc);
		return rc;
	}

	/* Trigger MTU recalculate as SMQ needs MTU conf */
	if (eth_dev->data->dev_started && eth_dev->data->nb_rx_queues) {
		rc = otx2_nix_recalc_mtu(eth_dev);
		if (rc) {
			otx2_err("TM MTU update failed, rc=%d", rc);
			return rc;
		}
	}

	/* Mark all non-leaf's as enabled */
	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (!nix_tm_is_leaf(dev, tm_node->lvl))
			tm_node->flags |= NIX_TM_NODE_ENABLED;
	}

	if (!xmit_enable)
		return 0;

	/* Update SQ Sched Data while SQ is idle */
	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (!nix_tm_is_leaf(dev, tm_node->lvl))
			continue;

		rc = nix_sq_sched_data(dev, tm_node, false);
		if (rc) {
			otx2_err("SQ %u sched update failed, rc=%d",
				 tm_node->id, rc);
			return rc;
		}
	}

	/* Finally XON all SMQ's */
	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (tm_node->hw_lvl != NIX_TXSCH_LVL_SMQ)
			continue;

		rc = nix_smq_xoff(dev, tm_node, false);
		if (rc) {
			otx2_err("Failed to enable smq %u, rc=%d",
				 tm_node->hw_id, rc);
			return rc;
		}
	}

	/* Enable xmit as all the topology is ready */
	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (!nix_tm_is_leaf(dev, tm_node->lvl))
			continue;

		sq = tm_node->id;
		txq = eth_dev->data->tx_queues[sq];

		rc = otx2_nix_sq_enable(txq);
		if (rc) {
			otx2_err("TM sw xon failed on SQ %u, rc=%d",
				 tm_node->id, rc);
			return rc;
		}
		tm_node->flags |= NIX_TM_NODE_ENABLED;
	}

	return 0;
}

static int
send_tm_reqval(struct otx2_mbox *mbox,
	       struct nix_txschq_config *req,
	       struct rte_tm_error *error)
{
	int rc;

	if (!req->num_regs ||
	    req->num_regs > MAX_REGS_PER_MBOX_MSG) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "invalid config";
		return -EIO;
	}

	rc = otx2_mbox_process(mbox);
	if (rc) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "unexpected fatal error";
	}
	return rc;
}

static uint16_t
nix_tm_lvl2nix(struct otx2_eth_dev *dev, uint32_t lvl)
{
	if (nix_tm_have_tl1_access(dev)) {
		switch (lvl) {
		case OTX2_TM_LVL_ROOT:
			return NIX_TXSCH_LVL_TL1;
		case OTX2_TM_LVL_SCH1:
			return NIX_TXSCH_LVL_TL2;
		case OTX2_TM_LVL_SCH2:
			return NIX_TXSCH_LVL_TL3;
		case OTX2_TM_LVL_SCH3:
			return NIX_TXSCH_LVL_TL4;
		case OTX2_TM_LVL_SCH4:
			return NIX_TXSCH_LVL_SMQ;
		default:
			return NIX_TXSCH_LVL_CNT;
		}
	} else {
		switch (lvl) {
		case OTX2_TM_LVL_ROOT:
			return NIX_TXSCH_LVL_TL2;
		case OTX2_TM_LVL_SCH1:
			return NIX_TXSCH_LVL_TL3;
		case OTX2_TM_LVL_SCH2:
			return NIX_TXSCH_LVL_TL4;
		case OTX2_TM_LVL_SCH3:
			return NIX_TXSCH_LVL_SMQ;
		default:
			return NIX_TXSCH_LVL_CNT;
		}
	}
}

static uint16_t
nix_max_prio(struct otx2_eth_dev *dev, uint16_t hw_lvl)
{
	if (hw_lvl >= NIX_TXSCH_LVL_CNT)
		return 0;

	/* MDQ doesn't support SP */
	if (hw_lvl == NIX_TXSCH_LVL_MDQ)
		return 0;

	/* PF's TL1 with VF's enabled doesn't support SP */
	if (hw_lvl == NIX_TXSCH_LVL_TL1 &&
	    (dev->otx2_tm_root_lvl == NIX_TXSCH_LVL_TL2 ||
	     (dev->tm_flags & NIX_TM_TL1_NO_SP)))
		return 0;

	return TXSCH_TLX_SP_PRIO_MAX - 1;
}


static int
validate_prio(struct otx2_eth_dev *dev, uint32_t lvl,
	      uint32_t parent_id, uint32_t priority,
	      struct rte_tm_error *error)
{
	uint8_t priorities[TXSCH_TLX_SP_PRIO_MAX];
	struct otx2_nix_tm_node *tm_node;
	uint32_t rr_num = 0;
	int i;

	/* Validate priority against max */
	if (priority > nix_max_prio(dev, nix_tm_lvl2nix(dev, lvl - 1))) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "unsupported priority value";
		return -EINVAL;
	}

	if (parent_id == RTE_TM_NODE_ID_NULL)
		return 0;

	memset(priorities, 0, TXSCH_TLX_SP_PRIO_MAX);
	priorities[priority] = 1;

	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (!tm_node->parent)
			continue;

		if (!(tm_node->flags & NIX_TM_NODE_USER))
			continue;

		if (tm_node->parent->id != parent_id)
			continue;

		priorities[tm_node->priority]++;
	}

	for (i = 0; i < TXSCH_TLX_SP_PRIO_MAX; i++)
		if (priorities[i] > 1)
			rr_num++;

	/* At max, one rr groups per parent */
	if (rr_num > 1) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PRIORITY;
		error->message = "multiple DWRR node priority";
		return -EINVAL;
	}

	/* Check for previous priority to avoid holes in priorities */
	if (priority && !priorities[priority - 1]) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PRIORITY;
		error->message = "priority not in order";
		return -EINVAL;
	}

	return 0;
}

static int
read_tm_reg(struct otx2_mbox *mbox, uint64_t reg,
	    uint64_t *regval, uint32_t hw_lvl)
{
	volatile struct nix_txschq_config *req;
	struct nix_txschq_config *rsp;
	int rc;

	req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->read = 1;
	req->lvl = hw_lvl;
	req->reg[0] = reg;
	req->num_regs = 1;

	rc = otx2_mbox_process_msg(mbox, (void **)&rsp);
	if (rc)
		return rc;
	*regval = rsp->regval[0];
	return 0;
}

/* Search for min rate in topology */
static void
nix_tm_shaper_profile_update_min(struct otx2_eth_dev *dev)
{
	struct otx2_nix_tm_shaper_profile *profile;
	uint64_t rate_min = 1E9; /* 1 Gbps */

	TAILQ_FOREACH(profile, &dev->shaper_profile_list, shaper) {
		if (profile->params.peak.rate &&
		    profile->params.peak.rate < rate_min)
			rate_min = profile->params.peak.rate;

		if (profile->params.committed.rate &&
		    profile->params.committed.rate < rate_min)
			rate_min = profile->params.committed.rate;
	}

	dev->tm_rate_min = rate_min;
}

static int
nix_xmit_disable(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint16_t sq_cnt = eth_dev->data->nb_tx_queues;
	uint16_t sqb_cnt, head_off, tail_off;
	struct otx2_nix_tm_node *tm_node;
	struct otx2_eth_txq *txq;
	uint64_t wdata, val;
	int i, rc;

	otx2_tm_dbg("Disabling xmit on %s", eth_dev->data->name);

	/* Enable CGX RXTX to drain pkts */
	if (!eth_dev->data->dev_started) {
		otx2_mbox_alloc_msg_nix_lf_start_rx(dev->mbox);
		rc = otx2_mbox_process(dev->mbox);
		if (rc)
			return rc;
	}

	/* XON all SMQ's */
	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (tm_node->hw_lvl != NIX_TXSCH_LVL_SMQ)
			continue;
		if (!(tm_node->flags & NIX_TM_NODE_HWRES))
			continue;

		rc = nix_smq_xoff(dev, tm_node, false);
		if (rc) {
			otx2_err("Failed to enable smq %u, rc=%d",
				 tm_node->hw_id, rc);
			goto cleanup;
		}
	}

	/* Flush all tx queues */
	for (i = 0; i < sq_cnt; i++) {
		txq = eth_dev->data->tx_queues[i];

		rc = otx2_nix_sq_sqb_aura_fc(txq, false);
		if (rc) {
			otx2_err("Failed to disable sqb aura fc, rc=%d", rc);
			goto cleanup;
		}

		/* Wait for sq entries to be flushed */
		rc = nix_txq_flush_sq_spin(txq);
		if (rc) {
			otx2_err("Failed to drain sq, rc=%d\n", rc);
			goto cleanup;
		}
	}

	/* XOFF & Flush all SMQ's. HRM mandates
	 * all SQ's empty before SMQ flush is issued.
	 */
	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (tm_node->hw_lvl != NIX_TXSCH_LVL_SMQ)
			continue;
		if (!(tm_node->flags & NIX_TM_NODE_HWRES))
			continue;

		rc = nix_smq_xoff(dev, tm_node, true);
		if (rc) {
			otx2_err("Failed to enable smq %u, rc=%d",
				 tm_node->hw_id, rc);
			goto cleanup;
		}
	}

	/* Verify sanity of all tx queues */
	for (i = 0; i < sq_cnt; i++) {
		txq = eth_dev->data->tx_queues[i];

		wdata = ((uint64_t)txq->sq << 32);
		val = otx2_atomic64_add_nosync(wdata,
			       (int64_t *)(dev->base + NIX_LF_SQ_OP_STATUS));

		sqb_cnt = val & 0xFFFF;
		head_off = (val >> 20) & 0x3F;
		tail_off = (val >> 28) & 0x3F;

		if (sqb_cnt > 1 || head_off != tail_off ||
		    (*txq->fc_mem != txq->nb_sqb_bufs))
			otx2_err("Failed to gracefully flush sq %u", txq->sq);
	}

cleanup:
	/* restore cgx state */
	if (!eth_dev->data->dev_started) {
		otx2_mbox_alloc_msg_nix_lf_stop_rx(dev->mbox);
		rc |= otx2_mbox_process(dev->mbox);
	}

	return rc;
}

static int
otx2_nix_tm_node_type_get(struct rte_eth_dev *eth_dev, uint32_t node_id,
			  int *is_leaf, struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_node *tm_node;

	if (is_leaf == NULL) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		return -EINVAL;
	}

	tm_node = nix_tm_node_search(dev, node_id, true);
	if (node_id == RTE_TM_NODE_ID_NULL || !tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		return -EINVAL;
	}
	if (nix_tm_is_leaf(dev, tm_node->lvl))
		*is_leaf = true;
	else
		*is_leaf = false;
	return 0;
}

static int
otx2_nix_tm_capa_get(struct rte_eth_dev *eth_dev,
		     struct rte_tm_capabilities *cap,
		     struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	int rc, max_nr_nodes = 0, i;
	struct free_rsrcs_rsp *rsp;

	memset(cap, 0, sizeof(*cap));

	otx2_mbox_alloc_msg_free_rsrc_cnt(mbox);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "unexpected fatal error";
		return rc;
	}

	for (i = 0; i < NIX_TXSCH_LVL_TL1; i++)
		max_nr_nodes += rsp->schq[i];

	cap->n_nodes_max = max_nr_nodes + dev->tm_leaf_cnt;
	/* TL1 level is reserved for PF */
	cap->n_levels_max = nix_tm_have_tl1_access(dev) ?
				OTX2_TM_LVL_MAX : OTX2_TM_LVL_MAX - 1;
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;

	/* Shaper Capabilities */
	cap->shaper_private_n_max = max_nr_nodes;
	cap->shaper_n_max = max_nr_nodes;
	cap->shaper_private_dual_rate_n_max = max_nr_nodes;
	cap->shaper_private_rate_min = MIN_SHAPER_RATE / 8;
	cap->shaper_private_rate_max = MAX_SHAPER_RATE / 8;
	cap->shaper_pkt_length_adjust_min = 0;
	cap->shaper_pkt_length_adjust_max = 0;

	/* Schedule Capabilities */
	cap->sched_n_children_max = rsp->schq[NIX_TXSCH_LVL_MDQ];
	cap->sched_sp_n_priorities_max = TXSCH_TLX_SP_PRIO_MAX;
	cap->sched_wfq_n_children_per_group_max = cap->sched_n_children_max;
	cap->sched_wfq_n_groups_max = 1;
	cap->sched_wfq_weight_max = MAX_SCHED_WEIGHT;

	cap->dynamic_update_mask =
		RTE_TM_UPDATE_NODE_PARENT_KEEP_LEVEL |
		RTE_TM_UPDATE_NODE_SUSPEND_RESUME;
	cap->stats_mask =
		RTE_TM_STATS_N_PKTS |
		RTE_TM_STATS_N_BYTES |
		RTE_TM_STATS_N_PKTS_RED_DROPPED |
		RTE_TM_STATS_N_BYTES_RED_DROPPED;

	for (i = 0; i < RTE_COLORS; i++) {
		cap->mark_vlan_dei_supported[i] = false;
		cap->mark_ip_ecn_tcp_supported[i] = false;
		cap->mark_ip_dscp_supported[i] = false;
	}

	return 0;
}

static int
otx2_nix_tm_level_capa_get(struct rte_eth_dev *eth_dev, uint32_t lvl,
				   struct rte_tm_level_capabilities *cap,
				   struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct free_rsrcs_rsp *rsp;
	uint16_t hw_lvl;
	int rc;

	memset(cap, 0, sizeof(*cap));

	otx2_mbox_alloc_msg_free_rsrc_cnt(mbox);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "unexpected fatal error";
		return rc;
	}

	hw_lvl = nix_tm_lvl2nix(dev, lvl);

	if (nix_tm_is_leaf(dev, lvl)) {
		/* Leaf */
		cap->n_nodes_max = dev->tm_leaf_cnt;
		cap->n_nodes_leaf_max = dev->tm_leaf_cnt;
		cap->leaf_nodes_identical = 1;
		cap->leaf.stats_mask =
			RTE_TM_STATS_N_PKTS |
			RTE_TM_STATS_N_BYTES;

	} else if (lvl == OTX2_TM_LVL_ROOT) {
		/* Root node, aka TL2(vf)/TL1(pf) */
		cap->n_nodes_max = 1;
		cap->n_nodes_nonleaf_max = 1;
		cap->non_leaf_nodes_identical = 1;

		cap->nonleaf.shaper_private_supported = true;
		cap->nonleaf.shaper_private_dual_rate_supported =
			nix_tm_have_tl1_access(dev) ? false : true;
		cap->nonleaf.shaper_private_rate_min = MIN_SHAPER_RATE / 8;
		cap->nonleaf.shaper_private_rate_max = MAX_SHAPER_RATE / 8;

		cap->nonleaf.sched_n_children_max = rsp->schq[hw_lvl - 1];
		cap->nonleaf.sched_sp_n_priorities_max =
					nix_max_prio(dev, hw_lvl) + 1;
		cap->nonleaf.sched_wfq_n_groups_max = 1;
		cap->nonleaf.sched_wfq_weight_max = MAX_SCHED_WEIGHT;

		if (nix_tm_have_tl1_access(dev))
			cap->nonleaf.stats_mask =
				RTE_TM_STATS_N_PKTS_RED_DROPPED |
				RTE_TM_STATS_N_BYTES_RED_DROPPED;
	} else if ((lvl < OTX2_TM_LVL_MAX) &&
		   (hw_lvl < NIX_TXSCH_LVL_CNT)) {
		/* TL2, TL3, TL4, MDQ */
		cap->n_nodes_max = rsp->schq[hw_lvl];
		cap->n_nodes_nonleaf_max = cap->n_nodes_max;
		cap->non_leaf_nodes_identical = 1;

		cap->nonleaf.shaper_private_supported = true;
		cap->nonleaf.shaper_private_dual_rate_supported = true;
		cap->nonleaf.shaper_private_rate_min = MIN_SHAPER_RATE / 8;
		cap->nonleaf.shaper_private_rate_max = MAX_SHAPER_RATE / 8;

		/* MDQ doesn't support Strict Priority */
		if (hw_lvl == NIX_TXSCH_LVL_MDQ)
			cap->nonleaf.sched_n_children_max = dev->tm_leaf_cnt;
		else
			cap->nonleaf.sched_n_children_max =
				rsp->schq[hw_lvl - 1];
		cap->nonleaf.sched_sp_n_priorities_max =
			nix_max_prio(dev, hw_lvl) + 1;
		cap->nonleaf.sched_wfq_n_groups_max = 1;
		cap->nonleaf.sched_wfq_weight_max = MAX_SCHED_WEIGHT;
	} else {
		/* unsupported level */
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		return rc;
	}
	return 0;
}

static int
otx2_nix_tm_node_capa_get(struct rte_eth_dev *eth_dev, uint32_t node_id,
			  struct rte_tm_node_capabilities *cap,
			  struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct otx2_nix_tm_node *tm_node;
	struct free_rsrcs_rsp *rsp;
	int rc, hw_lvl, lvl;

	memset(cap, 0, sizeof(*cap));

	tm_node = nix_tm_node_search(dev, node_id, true);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	hw_lvl = tm_node->hw_lvl;
	lvl = tm_node->lvl;

	/* Leaf node */
	if (nix_tm_is_leaf(dev, lvl)) {
		cap->stats_mask = RTE_TM_STATS_N_PKTS |
					RTE_TM_STATS_N_BYTES;
		return 0;
	}

	otx2_mbox_alloc_msg_free_rsrc_cnt(mbox);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "unexpected fatal error";
		return rc;
	}

	/* Non Leaf Shaper */
	cap->shaper_private_supported = true;
	cap->shaper_private_dual_rate_supported =
		(hw_lvl == NIX_TXSCH_LVL_TL1) ? false : true;
	cap->shaper_private_rate_min = MIN_SHAPER_RATE / 8;
	cap->shaper_private_rate_max = MAX_SHAPER_RATE / 8;

	/* Non Leaf Scheduler */
	if (hw_lvl == NIX_TXSCH_LVL_MDQ)
		cap->nonleaf.sched_n_children_max = dev->tm_leaf_cnt;
	else
		cap->nonleaf.sched_n_children_max = rsp->schq[hw_lvl - 1];

	cap->nonleaf.sched_sp_n_priorities_max = nix_max_prio(dev, hw_lvl) + 1;
	cap->nonleaf.sched_wfq_n_children_per_group_max =
		cap->nonleaf.sched_n_children_max;
	cap->nonleaf.sched_wfq_n_groups_max = 1;
	cap->nonleaf.sched_wfq_weight_max = MAX_SCHED_WEIGHT;

	if (hw_lvl == NIX_TXSCH_LVL_TL1)
		cap->stats_mask = RTE_TM_STATS_N_PKTS_RED_DROPPED |
			RTE_TM_STATS_N_BYTES_RED_DROPPED;
	return 0;
}

static int
otx2_nix_tm_shaper_profile_add(struct rte_eth_dev *eth_dev,
			       uint32_t profile_id,
			       struct rte_tm_shaper_params *params,
			       struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_shaper_profile *profile;

	profile = nix_tm_shaper_profile_search(dev, profile_id);
	if (profile) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "shaper profile ID exist";
		return -EINVAL;
	}

	/* Committed rate and burst size can be enabled/disabled */
	if (params->committed.size || params->committed.rate) {
		if (params->committed.size < MIN_SHAPER_BURST ||
		    params->committed.size > MAX_SHAPER_BURST) {
			error->type =
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_SIZE;
			return -EINVAL;
		} else if (!shaper_rate_to_nix(params->committed.rate * 8,
					       NULL, NULL, NULL)) {
			error->type =
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE;
			error->message = "shaper committed rate invalid";
			return -EINVAL;
		}
	}

	/* Peak rate and burst size can be enabled/disabled */
	if (params->peak.size || params->peak.rate) {
		if (params->peak.size < MIN_SHAPER_BURST ||
		    params->peak.size > MAX_SHAPER_BURST) {
			error->type =
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE;
			return -EINVAL;
		} else if (!shaper_rate_to_nix(params->peak.rate * 8,
					       NULL, NULL, NULL)) {
			error->type =
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE;
			error->message = "shaper peak rate invalid";
			return -EINVAL;
		}
	}

	profile = rte_zmalloc("otx2_nix_tm_shaper_profile",
			      sizeof(struct otx2_nix_tm_shaper_profile), 0);
	if (!profile)
		return -ENOMEM;

	profile->shaper_profile_id = profile_id;
	rte_memcpy(&profile->params, params,
		   sizeof(struct rte_tm_shaper_params));
	TAILQ_INSERT_TAIL(&dev->shaper_profile_list, profile, shaper);

	otx2_tm_dbg("Added TM shaper profile %u, "
		    " pir %" PRIu64 " , pbs %" PRIu64 ", cir %" PRIu64
		    ", cbs %" PRIu64 " , adj %u",
		    profile_id,
		    params->peak.rate * 8,
		    params->peak.size,
		    params->committed.rate * 8,
		    params->committed.size,
		    params->pkt_length_adjust);

	/* Translate rate as bits per second */
	profile->params.peak.rate = profile->params.peak.rate * 8;
	profile->params.committed.rate = profile->params.committed.rate * 8;
	/* Always use PIR for single rate shaping */
	if (!params->peak.rate && params->committed.rate) {
		profile->params.peak = profile->params.committed;
		memset(&profile->params.committed, 0,
		       sizeof(profile->params.committed));
	}

	/* update min rate */
	nix_tm_shaper_profile_update_min(dev);
	return 0;
}

static int
otx2_nix_tm_shaper_profile_delete(struct rte_eth_dev *eth_dev,
				  uint32_t profile_id,
				  struct rte_tm_error *error)
{
	struct otx2_nix_tm_shaper_profile *profile;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	profile = nix_tm_shaper_profile_search(dev, profile_id);

	if (!profile) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "shaper profile ID not exist";
		return -EINVAL;
	}

	if (profile->reference_count) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
		error->message = "shaper profile in use";
		return -EINVAL;
	}

	otx2_tm_dbg("Removing TM shaper profile %u", profile_id);
	TAILQ_REMOVE(&dev->shaper_profile_list, profile, shaper);
	rte_free(profile);

	/* update min rate */
	nix_tm_shaper_profile_update_min(dev);
	return 0;
}

static int
otx2_nix_tm_node_add(struct rte_eth_dev *eth_dev, uint32_t node_id,
		     uint32_t parent_node_id, uint32_t priority,
		     uint32_t weight, uint32_t lvl,
		     struct rte_tm_node_params *params,
		     struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_node *parent_node;
	int rc, clear_on_fail = 0;
	uint32_t exp_next_lvl;
	uint16_t hw_lvl;

	/* we don't support dynamic updates */
	if (dev->tm_flags & NIX_TM_COMMITTED) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "dynamic update not supported";
		return -EIO;
	}

	/* Leaf nodes have to be same priority */
	if (nix_tm_is_leaf(dev, lvl) && priority != 0) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "queue shapers must be priority 0";
		return -EIO;
	}

	parent_node = nix_tm_node_search(dev, parent_node_id, true);

	/* find the right level */
	if (lvl == RTE_TM_NODE_LEVEL_ID_ANY) {
		if (parent_node_id == RTE_TM_NODE_ID_NULL) {
			lvl = OTX2_TM_LVL_ROOT;
		} else if (parent_node) {
			lvl = parent_node->lvl + 1;
		} else {
			/* Neigher proper parent nor proper level id given */
			error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
			error->message = "invalid parent node id";
			return -ERANGE;
		}
	}

	/* Translate rte_tm level id's to nix hw level id's */
	hw_lvl = nix_tm_lvl2nix(dev, lvl);
	if (hw_lvl == NIX_TXSCH_LVL_CNT &&
	    !nix_tm_is_leaf(dev, lvl)) {
		error->type = RTE_TM_ERROR_TYPE_LEVEL_ID;
		error->message = "invalid level id";
		return -ERANGE;
	}

	if (node_id < dev->tm_leaf_cnt)
		exp_next_lvl = NIX_TXSCH_LVL_SMQ;
	else
		exp_next_lvl = hw_lvl + 1;

	/* Check if there is no parent node yet */
	if (hw_lvl != dev->otx2_tm_root_lvl &&
	    (!parent_node || parent_node->hw_lvl != exp_next_lvl)) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "invalid parent node id";
		return -EINVAL;
	}

	/* Check if a node already exists */
	if (nix_tm_node_search(dev, node_id, true)) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "node already exists";
		return -EINVAL;
	}

	/* Check if shaper profile exists for non leaf node */
	if (!nix_tm_is_leaf(dev, lvl) &&
	    params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE &&
	    !nix_tm_shaper_profile_search(dev, params->shaper_profile_id)) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "invalid shaper profile";
		return -EINVAL;
	}

	/* Check if there is second DWRR already in siblings or holes in prio */
	if (validate_prio(dev, lvl, parent_node_id, priority, error))
		return -EINVAL;

	if (weight > MAX_SCHED_WEIGHT) {
		error->type = RTE_TM_ERROR_TYPE_NODE_WEIGHT;
		error->message = "max weight exceeded";
		return -EINVAL;
	}

	rc = nix_tm_node_add_to_list(dev, node_id, parent_node_id,
				     priority, weight, hw_lvl,
				     lvl, true, params);
	if (rc) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		/* cleanup user added nodes */
		if (clear_on_fail)
			nix_tm_free_resources(dev, NIX_TM_NODE_USER,
					      NIX_TM_NODE_USER, false);
		error->message = "failed to add node";
		return rc;
	}
	error->type = RTE_TM_ERROR_TYPE_NONE;
	return 0;
}

static int
otx2_nix_tm_node_delete(struct rte_eth_dev *eth_dev, uint32_t node_id,
			struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_node *tm_node, *child_node;
	struct otx2_nix_tm_shaper_profile *profile;
	uint32_t profile_id;

	/* we don't support dynamic updates yet */
	if (dev->tm_flags & NIX_TM_COMMITTED) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "hierarchy exists";
		return -EIO;
	}

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	tm_node = nix_tm_node_search(dev, node_id, true);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	/* Check for any existing children */
	TAILQ_FOREACH(child_node, &dev->node_list, node) {
		if (child_node->parent == tm_node) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "children exist";
			return -EINVAL;
		}
	}

	/* Remove shaper profile reference */
	profile_id = tm_node->params.shaper_profile_id;
	profile = nix_tm_shaper_profile_search(dev, profile_id);
	profile->reference_count--;

	TAILQ_REMOVE(&dev->node_list, tm_node, node);
	rte_free(tm_node);
	return 0;
}

static int
nix_tm_node_suspend_resume(struct rte_eth_dev *eth_dev, uint32_t node_id,
			   struct rte_tm_error *error, bool suspend)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct otx2_nix_tm_node *tm_node;
	struct nix_txschq_config *req;
	uint16_t flags;
	int rc;

	tm_node = nix_tm_node_search(dev, node_id, true);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	if (!(dev->tm_flags & NIX_TM_COMMITTED)) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "hierarchy doesn't exist";
		return -EINVAL;
	}

	flags = tm_node->flags;
	flags = suspend ? (flags & ~NIX_TM_NODE_ENABLED) :
		(flags | NIX_TM_NODE_ENABLED);

	if (tm_node->flags == flags)
		return 0;

	/* send mbox for state change */
	req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);

	req->lvl = tm_node->hw_lvl;
	req->num_regs =	prepare_tm_sw_xoff(tm_node, suspend,
					   req->reg, req->regval);
	rc = send_tm_reqval(mbox, req, error);
	if (!rc)
		tm_node->flags = flags;
	return rc;
}

static int
otx2_nix_tm_node_suspend(struct rte_eth_dev *eth_dev, uint32_t node_id,
			 struct rte_tm_error *error)
{
	return nix_tm_node_suspend_resume(eth_dev, node_id, error, true);
}

static int
otx2_nix_tm_node_resume(struct rte_eth_dev *eth_dev, uint32_t node_id,
			struct rte_tm_error *error)
{
	return nix_tm_node_suspend_resume(eth_dev, node_id, error, false);
}

static int
otx2_nix_tm_hierarchy_commit(struct rte_eth_dev *eth_dev,
			     int clear_on_fail,
			     struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_node *tm_node;
	uint32_t leaf_cnt = 0;
	int rc;

	if (dev->tm_flags & NIX_TM_COMMITTED) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "hierarchy exists";
		return -EINVAL;
	}

	/* Check if we have all the leaf nodes */
	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (tm_node->flags & NIX_TM_NODE_USER &&
		    tm_node->id < dev->tm_leaf_cnt)
			leaf_cnt++;
	}

	if (leaf_cnt != dev->tm_leaf_cnt) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "incomplete hierarchy";
		return -EINVAL;
	}

	/*
	 * Disable xmit will be enabled when
	 * new topology is available.
	 */
	rc = nix_xmit_disable(eth_dev);
	if (rc) {
		otx2_err("failed to disable TX, rc=%d", rc);
		return -EIO;
	}

	/* Delete default/ratelimit tree */
	if (dev->tm_flags & (NIX_TM_DEFAULT_TREE | NIX_TM_RATE_LIMIT_TREE)) {
		rc = nix_tm_free_resources(dev, NIX_TM_NODE_USER, 0, false);
		if (rc) {
			error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
			error->message = "failed to free default resources";
			return rc;
		}
		dev->tm_flags &= ~(NIX_TM_DEFAULT_TREE |
				   NIX_TM_RATE_LIMIT_TREE);
	}

	/* Free up user alloc'ed resources */
	rc = nix_tm_free_resources(dev, NIX_TM_NODE_USER,
				   NIX_TM_NODE_USER, true);
	if (rc) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "failed to free user resources";
		return rc;
	}

	rc = nix_tm_alloc_resources(eth_dev, true);
	if (rc) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "alloc resources failed";
		/* TODO should we restore default config ? */
		if (clear_on_fail)
			nix_tm_free_resources(dev, 0, 0, false);
		return rc;
	}

	error->type = RTE_TM_ERROR_TYPE_NONE;
	dev->tm_flags |= NIX_TM_COMMITTED;
	return 0;
}

static int
otx2_nix_tm_node_shaper_update(struct rte_eth_dev *eth_dev,
			       uint32_t node_id,
			       uint32_t profile_id,
			       struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_shaper_profile *profile = NULL;
	struct otx2_mbox *mbox = dev->mbox;
	struct otx2_nix_tm_node *tm_node;
	struct nix_txschq_config *req;
	uint8_t k;
	int rc;

	tm_node = nix_tm_node_search(dev, node_id, true);
	if (!tm_node || nix_tm_is_leaf(dev, tm_node->lvl)) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node";
		return -EINVAL;
	}

	if (profile_id == tm_node->params.shaper_profile_id)
		return 0;

	if (profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE) {
		profile = nix_tm_shaper_profile_search(dev, profile_id);
		if (!profile) {
			error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
			error->message = "shaper profile ID not exist";
			return -EINVAL;
		}
	}

	tm_node->params.shaper_profile_id = profile_id;

	/* Nothing to do if not yet committed */
	if (!(dev->tm_flags & NIX_TM_COMMITTED))
		return 0;

	tm_node->flags &= ~NIX_TM_NODE_ENABLED;

	/* Flush the specific node with SW_XOFF */
	req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = tm_node->hw_lvl;
	k = prepare_tm_sw_xoff(tm_node, true, req->reg, req->regval);
	req->num_regs = k;

	rc = send_tm_reqval(mbox, req, error);
	if (rc)
		return rc;

	shaper_default_red_algo(dev, tm_node, profile);

	/* Update the PIR/CIR and clear SW XOFF */
	req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = tm_node->hw_lvl;

	k = prepare_tm_shaper_reg(tm_node, profile, req->reg, req->regval);

	k += prepare_tm_sw_xoff(tm_node, false, &req->reg[k], &req->regval[k]);

	req->num_regs = k;
	rc = send_tm_reqval(mbox, req, error);
	if (!rc)
		tm_node->flags |= NIX_TM_NODE_ENABLED;
	return rc;
}

static int
otx2_nix_tm_node_parent_update(struct rte_eth_dev *eth_dev,
			       uint32_t node_id, uint32_t new_parent_id,
			       uint32_t priority, uint32_t weight,
			       struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_node *tm_node, *sibling;
	struct otx2_nix_tm_node *new_parent;
	struct nix_txschq_config *req;
	uint8_t k;
	int rc;

	if (!(dev->tm_flags & NIX_TM_COMMITTED)) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "hierarchy doesn't exist";
		return -EINVAL;
	}

	tm_node = nix_tm_node_search(dev, node_id, true);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	/* Parent id valid only for non root nodes */
	if (tm_node->hw_lvl != dev->otx2_tm_root_lvl) {
		new_parent = nix_tm_node_search(dev, new_parent_id, true);
		if (!new_parent) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
			error->message = "no such parent node";
			return -EINVAL;
		}

		/* Current support is only for dynamic weight update */
		if (tm_node->parent != new_parent ||
		    tm_node->priority != priority) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
			error->message = "only weight update supported";
			return -EINVAL;
		}
	}

	/* Skip if no change */
	if (tm_node->weight == weight)
		return 0;

	tm_node->weight = weight;

	/* For leaf nodes, SQ CTX needs update */
	if (nix_tm_is_leaf(dev, tm_node->lvl)) {
		/* Update SQ quantum data on the fly */
		rc = nix_sq_sched_data(dev, tm_node, true);
		if (rc) {
			error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
			error->message = "sq sched data update failed";
			return rc;
		}
	} else {
		/* XOFF Parent node */
		req = otx2_mbox_alloc_msg_nix_txschq_cfg(dev->mbox);
		req->lvl = tm_node->parent->hw_lvl;
		req->num_regs = prepare_tm_sw_xoff(tm_node->parent, true,
						   req->reg, req->regval);
		rc = send_tm_reqval(dev->mbox, req, error);
		if (rc)
			return rc;

		/* XOFF this node and all other siblings */
		req = otx2_mbox_alloc_msg_nix_txschq_cfg(dev->mbox);
		req->lvl = tm_node->hw_lvl;

		k = 0;
		TAILQ_FOREACH(sibling, &dev->node_list, node) {
			if (sibling->parent != tm_node->parent)
				continue;
			k += prepare_tm_sw_xoff(sibling, true, &req->reg[k],
						&req->regval[k]);
		}
		req->num_regs = k;
		rc = send_tm_reqval(dev->mbox, req, error);
		if (rc)
			return rc;

		/* Update new weight for current node */
		req = otx2_mbox_alloc_msg_nix_txschq_cfg(dev->mbox);
		req->lvl = tm_node->hw_lvl;
		req->num_regs = prepare_tm_sched_reg(dev, tm_node,
						     req->reg, req->regval);
		rc = send_tm_reqval(dev->mbox, req, error);
		if (rc)
			return rc;

		/* XON this node and all other siblings */
		req = otx2_mbox_alloc_msg_nix_txschq_cfg(dev->mbox);
		req->lvl = tm_node->hw_lvl;

		k = 0;
		TAILQ_FOREACH(sibling, &dev->node_list, node) {
			if (sibling->parent != tm_node->parent)
				continue;
			k += prepare_tm_sw_xoff(sibling, false, &req->reg[k],
						&req->regval[k]);
		}
		req->num_regs = k;
		rc = send_tm_reqval(dev->mbox, req, error);
		if (rc)
			return rc;

		/* XON Parent node */
		req = otx2_mbox_alloc_msg_nix_txschq_cfg(dev->mbox);
		req->lvl = tm_node->parent->hw_lvl;
		req->num_regs = prepare_tm_sw_xoff(tm_node->parent, false,
						   req->reg, req->regval);
		rc = send_tm_reqval(dev->mbox, req, error);
		if (rc)
			return rc;
	}
	return 0;
}

static int
otx2_nix_tm_node_stats_read(struct rte_eth_dev *eth_dev, uint32_t node_id,
			    struct rte_tm_node_stats *stats,
			    uint64_t *stats_mask, int clear,
			    struct rte_tm_error *error)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_node *tm_node;
	uint64_t reg, val;
	int64_t *addr;
	int rc = 0;

	tm_node = nix_tm_node_search(dev, node_id, true);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	/* Stats support only for leaf node or TL1 root */
	if (nix_tm_is_leaf(dev, tm_node->lvl)) {
		reg = (((uint64_t)tm_node->id) << 32);

		/* Packets */
		addr = (int64_t *)(dev->base + NIX_LF_SQ_OP_PKTS);
		val = otx2_atomic64_add_nosync(reg, addr);
		if (val & OP_ERR)
			val = 0;
		stats->n_pkts = val - tm_node->last_pkts;

		/* Bytes */
		addr = (int64_t *)(dev->base + NIX_LF_SQ_OP_OCTS);
		val = otx2_atomic64_add_nosync(reg, addr);
		if (val & OP_ERR)
			val = 0;
		stats->n_bytes = val - tm_node->last_bytes;

		if (clear) {
			tm_node->last_pkts = stats->n_pkts;
			tm_node->last_bytes = stats->n_bytes;
		}

		*stats_mask = RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES;

	} else if (tm_node->hw_lvl == NIX_TXSCH_LVL_TL1) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "stats read error";

		/* RED Drop packets */
		reg = NIX_AF_TL1X_DROPPED_PACKETS(tm_node->hw_id);
		rc = read_tm_reg(dev->mbox, reg, &val, NIX_TXSCH_LVL_TL1);
		if (rc)
			goto exit;
		stats->leaf.n_pkts_dropped[RTE_COLOR_RED] =
						val - tm_node->last_pkts;

		/* RED Drop bytes */
		reg = NIX_AF_TL1X_DROPPED_BYTES(tm_node->hw_id);
		rc = read_tm_reg(dev->mbox, reg, &val, NIX_TXSCH_LVL_TL1);
		if (rc)
			goto exit;
		stats->leaf.n_bytes_dropped[RTE_COLOR_RED] =
						val - tm_node->last_bytes;

		/* Clear stats */
		if (clear) {
			tm_node->last_pkts =
				stats->leaf.n_pkts_dropped[RTE_COLOR_RED];
			tm_node->last_bytes =
				stats->leaf.n_bytes_dropped[RTE_COLOR_RED];
		}

		*stats_mask = RTE_TM_STATS_N_PKTS_RED_DROPPED |
			RTE_TM_STATS_N_BYTES_RED_DROPPED;

	} else {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "unsupported node";
		rc = -EINVAL;
	}

exit:
	return rc;
}

const struct rte_tm_ops otx2_tm_ops = {
	.node_type_get = otx2_nix_tm_node_type_get,

	.capabilities_get = otx2_nix_tm_capa_get,
	.level_capabilities_get = otx2_nix_tm_level_capa_get,
	.node_capabilities_get = otx2_nix_tm_node_capa_get,

	.shaper_profile_add = otx2_nix_tm_shaper_profile_add,
	.shaper_profile_delete = otx2_nix_tm_shaper_profile_delete,

	.node_add = otx2_nix_tm_node_add,
	.node_delete = otx2_nix_tm_node_delete,
	.node_suspend = otx2_nix_tm_node_suspend,
	.node_resume = otx2_nix_tm_node_resume,
	.hierarchy_commit = otx2_nix_tm_hierarchy_commit,

	.node_shaper_update = otx2_nix_tm_node_shaper_update,
	.node_parent_update = otx2_nix_tm_node_parent_update,
	.node_stats_read = otx2_nix_tm_node_stats_read,
};

static int
nix_tm_prepare_default_tree(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint32_t def = eth_dev->data->nb_tx_queues;
	struct rte_tm_node_params params;
	uint32_t leaf_parent, i;
	int rc = 0, leaf_level;

	/* Default params */
	memset(&params, 0, sizeof(params));
	params.shaper_profile_id = RTE_TM_SHAPER_PROFILE_ID_NONE;

	if (nix_tm_have_tl1_access(dev)) {
		dev->otx2_tm_root_lvl = NIX_TXSCH_LVL_TL1;
		rc = nix_tm_node_add_to_list(dev, def, RTE_TM_NODE_ID_NULL, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL1,
					     OTX2_TM_LVL_ROOT, false, &params);
		if (rc)
			goto exit;
		rc = nix_tm_node_add_to_list(dev, def + 1, def, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL2,
					     OTX2_TM_LVL_SCH1, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 2, def + 1, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL3,
					     OTX2_TM_LVL_SCH2, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 3, def + 2, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL4,
					     OTX2_TM_LVL_SCH3, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 4, def + 3, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_SMQ,
					     OTX2_TM_LVL_SCH4, false, &params);
		if (rc)
			goto exit;

		leaf_parent = def + 4;
		leaf_level = OTX2_TM_LVL_QUEUE;
	} else {
		dev->otx2_tm_root_lvl = NIX_TXSCH_LVL_TL2;
		rc = nix_tm_node_add_to_list(dev, def, RTE_TM_NODE_ID_NULL, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL2,
					     OTX2_TM_LVL_ROOT, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 1, def, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL3,
					     OTX2_TM_LVL_SCH1, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 2, def + 1, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL4,
					     OTX2_TM_LVL_SCH2, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 3, def + 2, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_SMQ,
					     OTX2_TM_LVL_SCH3, false, &params);
		if (rc)
			goto exit;

		leaf_parent = def + 3;
		leaf_level = OTX2_TM_LVL_SCH4;
	}

	/* Add leaf nodes */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		rc = nix_tm_node_add_to_list(dev, i, leaf_parent, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_CNT,
					     leaf_level, false, &params);
		if (rc)
			break;
	}

exit:
	return rc;
}

void otx2_nix_tm_conf_init(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	TAILQ_INIT(&dev->node_list);
	TAILQ_INIT(&dev->shaper_profile_list);
	dev->tm_rate_min = 1E9; /* 1Gbps */
}

int otx2_nix_tm_init_default(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct otx2_eth_dev  *dev = otx2_eth_pmd_priv(eth_dev);
	uint16_t sq_cnt = eth_dev->data->nb_tx_queues;
	int rc;

	/* Free up all resources already held */
	rc = nix_tm_free_resources(dev, 0, 0, false);
	if (rc) {
		otx2_err("Failed to freeup existing resources,rc=%d", rc);
		return rc;
	}

	/* Clear shaper profiles */
	nix_tm_clear_shaper_profiles(dev);
	dev->tm_flags = NIX_TM_DEFAULT_TREE;

	/* Disable TL1 Static Priority when VF's are enabled
	 * as otherwise VF's TL2 reallocation will be needed
	 * runtime to support a specific topology of PF.
	 */
	if (pci_dev->max_vfs)
		dev->tm_flags |= NIX_TM_TL1_NO_SP;

	rc = nix_tm_prepare_default_tree(eth_dev);
	if (rc != 0)
		return rc;

	rc = nix_tm_alloc_resources(eth_dev, false);
	if (rc != 0)
		return rc;
	dev->tm_leaf_cnt = sq_cnt;

	return 0;
}

static int
nix_tm_prepare_rate_limited_tree(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint32_t def = eth_dev->data->nb_tx_queues;
	struct rte_tm_node_params params;
	uint32_t leaf_parent, i, rc = 0;

	memset(&params, 0, sizeof(params));

	if (nix_tm_have_tl1_access(dev)) {
		dev->otx2_tm_root_lvl = NIX_TXSCH_LVL_TL1;
		rc = nix_tm_node_add_to_list(dev, def, RTE_TM_NODE_ID_NULL, 0,
					DEFAULT_RR_WEIGHT,
					NIX_TXSCH_LVL_TL1,
					OTX2_TM_LVL_ROOT, false, &params);
		if (rc)
			goto error;
		rc = nix_tm_node_add_to_list(dev, def + 1, def, 0,
					DEFAULT_RR_WEIGHT,
					NIX_TXSCH_LVL_TL2,
					OTX2_TM_LVL_SCH1, false, &params);
		if (rc)
			goto error;
		rc = nix_tm_node_add_to_list(dev, def + 2, def + 1, 0,
					DEFAULT_RR_WEIGHT,
					NIX_TXSCH_LVL_TL3,
					OTX2_TM_LVL_SCH2, false, &params);
		if (rc)
			goto error;
		rc = nix_tm_node_add_to_list(dev, def + 3, def + 2, 0,
					DEFAULT_RR_WEIGHT,
					NIX_TXSCH_LVL_TL4,
					OTX2_TM_LVL_SCH3, false, &params);
		if (rc)
			goto error;
		leaf_parent = def + 3;

		/* Add per queue SMQ nodes */
		for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
			rc = nix_tm_node_add_to_list(dev, leaf_parent + 1 + i,
						leaf_parent,
						0, DEFAULT_RR_WEIGHT,
						NIX_TXSCH_LVL_SMQ,
						OTX2_TM_LVL_SCH4,
						false, &params);
			if (rc)
				goto error;
		}

		/* Add leaf nodes */
		for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
			rc = nix_tm_node_add_to_list(dev, i,
						     leaf_parent + 1 + i, 0,
						     DEFAULT_RR_WEIGHT,
						     NIX_TXSCH_LVL_CNT,
						     OTX2_TM_LVL_QUEUE,
						     false, &params);
		if (rc)
			goto error;
		}

		return 0;
	}

	dev->otx2_tm_root_lvl = NIX_TXSCH_LVL_TL2;
	rc = nix_tm_node_add_to_list(dev, def, RTE_TM_NODE_ID_NULL, 0,
				DEFAULT_RR_WEIGHT, NIX_TXSCH_LVL_TL2,
				OTX2_TM_LVL_ROOT, false, &params);
	if (rc)
		goto error;
	rc = nix_tm_node_add_to_list(dev, def + 1, def, 0,
				DEFAULT_RR_WEIGHT, NIX_TXSCH_LVL_TL3,
				OTX2_TM_LVL_SCH1, false, &params);
	if (rc)
		goto error;
	rc = nix_tm_node_add_to_list(dev, def + 2, def + 1, 0,
				     DEFAULT_RR_WEIGHT, NIX_TXSCH_LVL_TL4,
				     OTX2_TM_LVL_SCH2, false, &params);
	if (rc)
		goto error;
	leaf_parent = def + 2;

	/* Add per queue SMQ nodes */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		rc = nix_tm_node_add_to_list(dev, leaf_parent + 1 + i,
					     leaf_parent,
					     0, DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_SMQ,
					     OTX2_TM_LVL_SCH3,
					     false, &params);
		if (rc)
			goto error;
	}

	/* Add leaf nodes */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		rc = nix_tm_node_add_to_list(dev, i, leaf_parent + 1 + i, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_CNT,
					     OTX2_TM_LVL_SCH4,
					     false, &params);
		if (rc)
			break;
	}
error:
	return rc;
}

static int
otx2_nix_tm_rate_limit_mdq(struct rte_eth_dev *eth_dev,
			   struct otx2_nix_tm_node *tm_node,
			   uint64_t tx_rate)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_shaper_profile profile;
	struct otx2_mbox *mbox = dev->mbox;
	volatile uint64_t *reg, *regval;
	struct nix_txschq_config *req;
	uint16_t flags;
	uint8_t k = 0;
	int rc;

	flags = tm_node->flags;

	req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = NIX_TXSCH_LVL_MDQ;
	reg = req->reg;
	regval = req->regval;

	if (tx_rate == 0) {
		k += prepare_tm_sw_xoff(tm_node, true, &reg[k], &regval[k]);
		flags &= ~NIX_TM_NODE_ENABLED;
		goto exit;
	}

	if (!(flags & NIX_TM_NODE_ENABLED)) {
		k += prepare_tm_sw_xoff(tm_node, false, &reg[k], &regval[k]);
		flags |= NIX_TM_NODE_ENABLED;
	}

	/* Use only PIR for rate limit */
	memset(&profile, 0, sizeof(profile));
	profile.params.peak.rate = tx_rate;
	/* Minimum burst of ~4us Bytes of Tx */
	profile.params.peak.size = RTE_MAX(NIX_MAX_HW_FRS,
					   (4ull * tx_rate) / (1E6 * 8));
	if (!dev->tm_rate_min || dev->tm_rate_min > tx_rate)
		dev->tm_rate_min = tx_rate;

	k += prepare_tm_shaper_reg(tm_node, &profile, &reg[k], &regval[k]);
exit:
	req->num_regs = k;
	rc = otx2_mbox_process(mbox);
	if (rc)
		return rc;

	tm_node->flags = flags;
	return 0;
}

int
otx2_nix_tm_set_queue_rate_limit(struct rte_eth_dev *eth_dev,
				uint16_t queue_idx, uint16_t tx_rate_mbps)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint64_t tx_rate = tx_rate_mbps * (uint64_t)1E6;
	struct otx2_nix_tm_node *tm_node;
	int rc;

	/* Check for supported revisions */
	if (otx2_dev_is_95xx_Ax(dev) ||
	    otx2_dev_is_96xx_Ax(dev))
		return -EINVAL;

	if (queue_idx >= eth_dev->data->nb_tx_queues)
		return -EINVAL;

	if (!(dev->tm_flags & NIX_TM_DEFAULT_TREE) &&
	    !(dev->tm_flags & NIX_TM_RATE_LIMIT_TREE))
		goto error;

	if ((dev->tm_flags & NIX_TM_DEFAULT_TREE) &&
	    eth_dev->data->nb_tx_queues > 1) {
		/* For TM topology change ethdev needs to be stopped */
		if (eth_dev->data->dev_started)
			return -EBUSY;

		/*
		 * Disable xmit will be enabled when
		 * new topology is available.
		 */
		rc = nix_xmit_disable(eth_dev);
		if (rc) {
			otx2_err("failed to disable TX, rc=%d", rc);
			return -EIO;
		}

		rc = nix_tm_free_resources(dev, 0, 0, false);
		if (rc < 0) {
			otx2_tm_dbg("failed to free default resources, rc %d",
				   rc);
			return -EIO;
		}

		rc = nix_tm_prepare_rate_limited_tree(eth_dev);
		if (rc < 0) {
			otx2_tm_dbg("failed to prepare tm tree, rc=%d", rc);
			return rc;
		}

		rc = nix_tm_alloc_resources(eth_dev, true);
		if (rc != 0) {
			otx2_tm_dbg("failed to allocate tm tree, rc=%d", rc);
			return rc;
		}

		dev->tm_flags &= ~NIX_TM_DEFAULT_TREE;
		dev->tm_flags |= NIX_TM_RATE_LIMIT_TREE;
	}

	tm_node = nix_tm_node_search(dev, queue_idx, false);

	/* check if we found a valid leaf node */
	if (!tm_node ||
	    !nix_tm_is_leaf(dev, tm_node->lvl) ||
	    !tm_node->parent ||
	    tm_node->parent->hw_id == UINT32_MAX)
		return -EIO;

	return otx2_nix_tm_rate_limit_mdq(eth_dev, tm_node->parent, tx_rate);
error:
	otx2_tm_dbg("Unsupported TM tree 0x%0x", dev->tm_flags);
	return -EINVAL;
}

int
otx2_nix_tm_ops_get(struct rte_eth_dev *eth_dev, void *arg)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	if (!arg)
		return -EINVAL;

	/* Check for supported revisions */
	if (otx2_dev_is_95xx_Ax(dev) ||
	    otx2_dev_is_96xx_Ax(dev))
		return -EINVAL;

	*(const void **)arg = &otx2_tm_ops;

	return 0;
}

int
otx2_nix_tm_fini(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc;

	/* Xmit is assumed to be disabled */
	/* Free up resources already held */
	rc = nix_tm_free_resources(dev, 0, 0, false);
	if (rc) {
		otx2_err("Failed to freeup existing resources,rc=%d", rc);
		return rc;
	}

	/* Clear shaper profiles */
	nix_tm_clear_shaper_profiles(dev);

	dev->tm_flags = 0;
	return 0;
}

int
otx2_nix_tm_get_leaf_data(struct otx2_eth_dev *dev, uint16_t sq,
			  uint32_t *rr_quantum, uint16_t *smq)
{
	struct otx2_nix_tm_node *tm_node;
	int rc;

	/* 0..sq_cnt-1 are leaf nodes */
	if (sq >= dev->tm_leaf_cnt)
		return -EINVAL;

	/* Search for internal node first */
	tm_node = nix_tm_node_search(dev, sq, false);
	if (!tm_node)
		tm_node = nix_tm_node_search(dev, sq, true);

	/* Check if we found a valid leaf node */
	if (!tm_node || !nix_tm_is_leaf(dev, tm_node->lvl) ||
	    !tm_node->parent || tm_node->parent->hw_id == UINT32_MAX) {
		return -EIO;
	}

	/* Get SMQ Id of leaf node's parent */
	*smq = tm_node->parent->hw_id;
	*rr_quantum = NIX_TM_WEIGHT_TO_RR_QUANTUM(tm_node->weight);

	rc = nix_smq_xoff(dev, tm_node->parent, false);
	if (rc)
		return rc;
	tm_node->flags |= NIX_TM_NODE_ENABLED;

	return 0;
}
