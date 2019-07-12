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

static int
nix_get_link(struct otx2_eth_dev *dev)
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
	return otx2_dev_is_pf(dev) && !otx2_dev_is_Ax(dev) &&
		!is_lbk && !dev->maxvf;
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
shaper_rate_to_nix(uint64_t cclk_hz, uint64_t cclk_ticks,
		   uint64_t value, uint64_t *exponent_p,
		   uint64_t *mantissa_p, uint64_t *div_exp_p)
{
	uint64_t div_exp, exponent, mantissa;

	/* Boundary checks */
	if (value < MIN_SHAPER_RATE(cclk_hz, cclk_ticks) ||
	    value > MAX_SHAPER_RATE(cclk_hz, cclk_ticks))
		return 0;

	if (value <= SHAPER_RATE(cclk_hz, cclk_ticks, 0, 0, 0)) {
		/* Calculate rate div_exp and mantissa using
		 * the following formula:
		 *
		 * value = (cclk_hz * (256 + mantissa)
		 *              / ((cclk_ticks << div_exp) * 256)
		 */
		div_exp = 0;
		exponent = 0;
		mantissa = MAX_RATE_MANTISSA;

		while (value < (cclk_hz / (cclk_ticks << div_exp)))
			div_exp += 1;

		while (value <
		       ((cclk_hz * (256 + mantissa)) /
			((cclk_ticks << div_exp) * 256)))
			mantissa -= 1;
	} else {
		/* Calculate rate exponent and mantissa using
		 * the following formula:
		 *
		 * value = (cclk_hz * ((256 + mantissa) << exponent)
		 *              / (cclk_ticks * 256)
		 *
		 */
		div_exp = 0;
		exponent = MAX_RATE_EXPONENT;
		mantissa = MAX_RATE_MANTISSA;

		while (value < (cclk_hz * (1 << exponent)) / cclk_ticks)
			exponent -= 1;

		while (value < (cclk_hz * ((256 + mantissa) << exponent)) /
		       (cclk_ticks * 256))
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
	return SHAPER_RATE(cclk_hz, cclk_ticks, exponent, mantissa, div_exp);
}

static inline uint64_t
lx_shaper_rate_to_nix(uint64_t cclk_hz, uint32_t hw_lvl,
		      uint64_t value, uint64_t *exponent,
		      uint64_t *mantissa, uint64_t *div_exp)
{
	if (hw_lvl == NIX_TXSCH_LVL_TL1)
		return shaper_rate_to_nix(cclk_hz, L1_TIME_WHEEL_CCLK_TICKS,
					  value, exponent, mantissa, div_exp);
	else
		return shaper_rate_to_nix(cclk_hz, LX_TIME_WHEEL_CCLK_TICKS,
					  value, exponent, mantissa, div_exp);
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

static int
configure_shaper_cir_pir_reg(struct otx2_eth_dev *dev,
			     struct otx2_nix_tm_node *tm_node,
			     struct shaper_params *cir,
			     struct shaper_params *pir)
{
	uint32_t shaper_profile_id = RTE_TM_SHAPER_PROFILE_ID_NONE;
	struct otx2_nix_tm_shaper_profile *shaper_profile = NULL;
	struct rte_tm_shaper_params *param;

	shaper_profile_id = tm_node->params.shaper_profile_id;

	shaper_profile = nix_tm_shaper_profile_search(dev, shaper_profile_id);
	if (shaper_profile) {
		param = &shaper_profile->profile;
		/* Calculate CIR exponent and mantissa */
		if (param->committed.rate)
			cir->rate = lx_shaper_rate_to_nix(CCLK_HZ,
							  tm_node->hw_lvl_id,
							  param->committed.rate,
							  &cir->exponent,
							  &cir->mantissa,
							  &cir->div_exp);

		/* Calculate PIR exponent and mantissa */
		if (param->peak.rate)
			pir->rate = lx_shaper_rate_to_nix(CCLK_HZ,
							  tm_node->hw_lvl_id,
							  param->peak.rate,
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

	return 0;
}

static int
send_tm_reqval(struct otx2_mbox *mbox, struct nix_txschq_config *req)
{
	int rc;

	if (req->num_regs > MAX_REGS_PER_MBOX_MSG)
		return -ERANGE;

	rc = otx2_mbox_process(mbox);
	if (rc)
		return rc;

	req->num_regs = 0;
	return 0;
}

static int
populate_tm_registers(struct otx2_eth_dev *dev,
		      struct otx2_nix_tm_node *tm_node)
{
	uint64_t strict_schedul_prio, rr_prio;
	struct otx2_mbox *mbox = dev->mbox;
	volatile uint64_t *reg, *regval;
	uint64_t parent = 0, child = 0;
	struct shaper_params cir, pir;
	struct nix_txschq_config *req;
	uint64_t rr_quantum;
	uint32_t hw_lvl;
	uint32_t schq;
	int rc;

	memset(&cir, 0, sizeof(cir));
	memset(&pir, 0, sizeof(pir));

	/* Skip leaf nodes */
	if (tm_node->hw_lvl_id == NIX_TXSCH_LVL_CNT)
		return 0;

	/* Root node will not have a parent node */
	if (tm_node->hw_lvl_id == dev->otx2_tm_root_lvl)
		parent = tm_node->parent_hw_id;
	else
		parent = tm_node->parent->hw_id;

	/* Do we need this trigger to configure TL1 */
	if (dev->otx2_tm_root_lvl == NIX_TXSCH_LVL_TL2 &&
	    tm_node->hw_lvl_id == dev->otx2_tm_root_lvl) {
		schq = parent;
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

		rc = send_tm_reqval(mbox, req);
		if (rc)
			goto error;
	}

	if (tm_node->hw_lvl_id != NIX_TXSCH_LVL_SMQ)
		child = find_prio_anchor(dev, tm_node->id);

	rr_prio = tm_node->rr_prio;
	hw_lvl = tm_node->hw_lvl_id;
	strict_schedul_prio = tm_node->priority;
	schq = tm_node->hw_id;
	rr_quantum = (tm_node->weight * NIX_TM_RR_QUANTUM_MAX) /
		MAX_SCHED_WEIGHT;

	configure_shaper_cir_pir_reg(dev, tm_node, &cir, &pir);

	otx2_tm_dbg("Configure node %p, lvl %u hw_lvl %u, id %u, hw_id %u,"
		     "parent_hw_id %" PRIx64 ", pir %" PRIx64 ", cir %" PRIx64,
		     tm_node, tm_node->level_id, hw_lvl,
		     tm_node->id, schq, parent, pir.rate, cir.rate);

	rc = -EFAULT;

	switch (hw_lvl) {
	case NIX_TXSCH_LVL_SMQ:
		req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = hw_lvl;
		reg = req->reg;
		regval = req->regval;
		req->num_regs = 0;

		/* Set xoff which will be cleared later */
		*reg++ = NIX_AF_SMQX_CFG(schq);
		*regval++ = BIT_ULL(50) | ((uint64_t)NIX_MAX_VTAG_INS << 36) |
				(NIX_MAX_HW_FRS << 8) | NIX_MIN_HW_FRS;
		req->num_regs++;
		*reg++ = NIX_AF_MDQX_PARENT(schq);
		*regval++ = parent << 16;
		req->num_regs++;
		*reg++ = NIX_AF_MDQX_SCHEDULE(schq);
		*regval++ = (strict_schedul_prio << 24) | rr_quantum;
		req->num_regs++;
		if (pir.rate && pir.burst) {
			*reg++ = NIX_AF_MDQX_PIR(schq);
			*regval++ = shaper2regval(&pir) | 1;
			req->num_regs++;
		}

		if (cir.rate && cir.burst) {
			*reg++ = NIX_AF_MDQX_CIR(schq);
			*regval++ = shaper2regval(&cir) | 1;
			req->num_regs++;
		}

		rc = send_tm_reqval(mbox, req);
		if (rc)
			goto error;
		break;
	case NIX_TXSCH_LVL_TL4:
		req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = hw_lvl;
		req->num_regs = 0;
		reg = req->reg;
		regval = req->regval;

		*reg++ = NIX_AF_TL4X_PARENT(schq);
		*regval++ = parent << 16;
		req->num_regs++;
		*reg++ = NIX_AF_TL4X_TOPOLOGY(schq);
		*regval++ = (child << 32) | (rr_prio << 1);
		req->num_regs++;
		*reg++ = NIX_AF_TL4X_SCHEDULE(schq);
		*regval++ = (strict_schedul_prio << 24) | rr_quantum;
		req->num_regs++;
		if (pir.rate && pir.burst) {
			*reg++ = NIX_AF_TL4X_PIR(schq);
			*regval++ = shaper2regval(&pir) | 1;
			req->num_regs++;
		}
		if (cir.rate && cir.burst) {
			*reg++ = NIX_AF_TL4X_CIR(schq);
			*regval++ = shaper2regval(&cir) | 1;
			req->num_regs++;
		}

		rc = send_tm_reqval(mbox, req);
		if (rc)
			goto error;
		break;
	case NIX_TXSCH_LVL_TL3:
		req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = hw_lvl;
		req->num_regs = 0;
		reg = req->reg;
		regval = req->regval;

		*reg++ = NIX_AF_TL3X_PARENT(schq);
		*regval++ = parent << 16;
		req->num_regs++;
		*reg++ = NIX_AF_TL3X_TOPOLOGY(schq);
		*regval++ = (child << 32) | (rr_prio << 1);
		req->num_regs++;
		*reg++ = NIX_AF_TL3X_SCHEDULE(schq);
		*regval++ = (strict_schedul_prio << 24) | rr_quantum;
		req->num_regs++;
		if (pir.rate && pir.burst) {
			*reg++ = NIX_AF_TL3X_PIR(schq);
			*regval++ = shaper2regval(&pir) | 1;
			req->num_regs++;
		}
		if (cir.rate && cir.burst) {
			*reg++ = NIX_AF_TL3X_CIR(schq);
			*regval++ = shaper2regval(&cir) | 1;
			req->num_regs++;
		}

		rc = send_tm_reqval(mbox, req);
		if (rc)
			goto error;
		break;
	case NIX_TXSCH_LVL_TL2:
		req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = hw_lvl;
		req->num_regs = 0;
		reg = req->reg;
		regval = req->regval;

		*reg++ = NIX_AF_TL2X_PARENT(schq);
		*regval++ = parent << 16;
		req->num_regs++;
		*reg++ = NIX_AF_TL2X_TOPOLOGY(schq);
		*regval++ = (child << 32) | (rr_prio << 1);
		req->num_regs++;
		*reg++ = NIX_AF_TL2X_SCHEDULE(schq);
		if (dev->otx2_tm_root_lvl == NIX_TXSCH_LVL_TL2)
			*regval++ = (1 << 24) | rr_quantum;
		else
			*regval++ = (strict_schedul_prio << 24) | rr_quantum;
		req->num_regs++;
		*reg++ = NIX_AF_TL3_TL2X_LINKX_CFG(schq, nix_get_link(dev));
		*regval++ = BIT_ULL(12) | nix_get_relchan(dev);
		req->num_regs++;
		if (pir.rate && pir.burst) {
			*reg++ = NIX_AF_TL2X_PIR(schq);
			*regval++ = shaper2regval(&pir) | 1;
			req->num_regs++;
		}
		if (cir.rate && cir.burst) {
			*reg++ = NIX_AF_TL2X_CIR(schq);
			*regval++ = shaper2regval(&cir) | 1;
			req->num_regs++;
		}

		rc = send_tm_reqval(mbox, req);
		if (rc)
			goto error;
		break;
	case NIX_TXSCH_LVL_TL1:
		req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = hw_lvl;
		req->num_regs = 0;
		reg = req->reg;
		regval = req->regval;

		*reg++ = NIX_AF_TL1X_SCHEDULE(schq);
		*regval++ = rr_quantum;
		req->num_regs++;
		*reg++ = NIX_AF_TL1X_TOPOLOGY(schq);
		*regval++ = (child << 32) | (rr_prio << 1 /*RR_PRIO*/);
		req->num_regs++;
		if (cir.rate && cir.burst) {
			*reg++ = NIX_AF_TL1X_CIR(schq);
			*regval++ = shaper2regval(&cir) | 1;
			req->num_regs++;
		}

		rc = send_tm_reqval(mbox, req);
		if (rc)
			goto error;
		break;
	}

	return 0;
error:
	otx2_err("Txschq cfg request failed for node %p, rc=%d", tm_node, rc);
	return rc;
}


static int
nix_tm_txsch_reg_config(struct otx2_eth_dev *dev)
{
	struct otx2_nix_tm_node *tm_node;
	uint32_t lvl;
	int rc = 0;

	if (nix_get_link(dev) == 13)
		return -EPERM;

	for (lvl = 0; lvl < (uint32_t)dev->otx2_tm_root_lvl + 1; lvl++) {
		TAILQ_FOREACH(tm_node, &dev->node_list, node) {
			if (tm_node->hw_lvl_id == lvl) {
				rc = populate_tm_registers(dev, tm_node);
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
			uint32_t weight, uint16_t hw_lvl_id,
			uint16_t level_id, bool user,
			struct rte_tm_node_params *params)
{
	struct otx2_nix_tm_shaper_profile *shaper_profile;
	struct otx2_nix_tm_node *tm_node, *parent_node;
	uint32_t shaper_profile_id;

	shaper_profile_id = params->shaper_profile_id;
	shaper_profile = nix_tm_shaper_profile_search(dev, shaper_profile_id);

	parent_node = nix_tm_node_search(dev, parent_node_id, user);

	tm_node = rte_zmalloc("otx2_nix_tm_node",
			      sizeof(struct otx2_nix_tm_node), 0);
	if (!tm_node)
		return -ENOMEM;

	tm_node->level_id = level_id;
	tm_node->hw_lvl_id = hw_lvl_id;

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

	if (shaper_profile)
		shaper_profile->reference_count++;
	tm_node->parent = parent_node;
	tm_node->parent_hw_id = UINT32_MAX;

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
nix_smq_xoff(struct otx2_eth_dev *dev, uint16_t smq, bool enable)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_txschq_config *req;

	req = otx2_mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = NIX_TXSCH_LVL_SMQ;
	req->num_regs = 1;

	req->reg[0] = NIX_AF_SMQX_CFG(smq);
	/* Unmodified fields */
	req->regval[0] = ((uint64_t)NIX_MAX_VTAG_INS << 36) |
				(NIX_MAX_HW_FRS << 8) | NIX_MIN_HW_FRS;

	if (enable)
		req->regval[0] |= BIT_ULL(50) | BIT_ULL(49);
	else
		req->regval[0] |= 0;

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

static void
nix_txq_flush_sq_spin(struct otx2_eth_txq *txq)
{
	uint16_t sqb_cnt, head_off, tail_off;
	struct otx2_eth_dev *dev = txq->dev;
	uint16_t sq = txq->sq;
	uint64_t reg, val;
	int64_t *regaddr;

	while (true) {
		reg = ((uint64_t)sq << 32);
		regaddr = (int64_t *)(dev->base + NIX_LF_SQ_OP_PKTS);
		val = otx2_atomic64_add_nosync(reg, regaddr);

		regaddr = (int64_t *)(dev->base + NIX_LF_SQ_OP_STATUS);
		val = otx2_atomic64_add_nosync(reg, regaddr);
		sqb_cnt = val & 0xFFFF;
		head_off = (val >> 20) & 0x3F;
		tail_off = (val >> 28) & 0x3F;

		/* SQ reached quiescent state */
		if (sqb_cnt <= 1 && head_off == tail_off &&
		    (*txq->fc_mem == txq->nb_sqb_bufs)) {
			break;
		}

		rte_pause();
	}
}

int
otx2_nix_tm_sw_xoff(void *__txq, bool dev_started)
{
	struct otx2_eth_txq *txq = __txq;
	struct otx2_eth_dev *dev = txq->dev;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_aq_enq_req *req;
	struct nix_aq_enq_rsp *rsp;
	uint16_t smq;
	int rc;

	/* Get smq from sq */
	req = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	req->qidx = txq->sq;
	req->ctype = NIX_AQ_CTYPE_SQ;
	req->op = NIX_AQ_INSTOP_READ;
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		otx2_err("Failed to get smq, rc=%d", rc);
		return -EIO;
	}

	/* Check if sq is enabled */
	if (!rsp->sq.ena)
		return 0;

	smq = rsp->sq.smq;

	/* Enable CGX RXTX to drain pkts */
	if (!dev_started) {
		rc = otx2_cgx_rxtx_start(dev);
		if (rc)
			return rc;
	}

	rc = otx2_nix_sq_sqb_aura_fc(txq, false);
	if (rc < 0) {
		otx2_err("Failed to disable sqb aura fc, rc=%d", rc);
		goto cleanup;
	}

	/* Disable smq xoff for case it was enabled earlier */
	rc = nix_smq_xoff(dev, smq, false);
	if (rc) {
		otx2_err("Failed to enable smq for sq %u, rc=%d", txq->sq, rc);
		goto cleanup;
	}

	/* Wait for sq entries to be flushed */
	nix_txq_flush_sq_spin(txq);

	/* Flush and enable smq xoff */
	rc = nix_smq_xoff(dev, smq, true);
	if (rc) {
		otx2_err("Failed to disable smq for sq %u, rc=%d", txq->sq, rc);
		return rc;
	}

cleanup:
	/* Restore cgx state */
	if (!dev_started)
		rc |= otx2_cgx_rxtx_stop(dev);

	return rc;
}

static int
nix_tm_sw_xon(struct otx2_eth_txq *txq,
	      uint16_t smq, uint32_t rr_quantum)
{
	struct otx2_eth_dev *dev = txq->dev;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_aq_enq_req *req;
	int rc;

	otx2_tm_dbg("Enabling sq(%u)->smq(%u), rr_quantum %u",
		    txq->sq, txq->sq, rr_quantum);
	/* Set smq from sq */
	req = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	req->qidx = txq->sq;
	req->ctype = NIX_AQ_CTYPE_SQ;
	req->op = NIX_AQ_INSTOP_WRITE;
	req->sq.smq = smq;
	req->sq.smq_rr_quantum = rr_quantum;
	req->sq_mask.smq = ~req->sq_mask.smq;
	req->sq_mask.smq_rr_quantum = ~req->sq_mask.smq_rr_quantum;

	rc = otx2_mbox_process(mbox);
	if (rc) {
		otx2_err("Failed to set smq, rc=%d", rc);
		return -EIO;
	}

	/* Enable sqb_aura fc */
	rc = otx2_nix_sq_sqb_aura_fc(txq, true);
	if (rc < 0) {
		otx2_err("Failed to enable sqb aura fc, rc=%d", rc);
		return rc;
	}

	/* Disable smq xoff */
	rc = nix_smq_xoff(dev, smq, false);
	if (rc) {
		otx2_err("Failed to enable smq for sq %u", txq->sq);
		return rc;
	}

	return 0;
}

static int
nix_tm_free_resources(struct otx2_eth_dev *dev, uint32_t flags_mask,
		      uint32_t flags, bool hw_only)
{
	struct otx2_nix_tm_shaper_profile *shaper_profile;
	struct otx2_nix_tm_node *tm_node, *next_node;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_txsch_free_req *req;
	uint32_t shaper_profile_id;
	bool skip_node = false;
	int rc = 0;

	next_node = TAILQ_FIRST(&dev->node_list);
	while (next_node) {
		tm_node = next_node;
		next_node = TAILQ_NEXT(tm_node, node);

		/* Check for only requested nodes */
		if ((tm_node->flags & flags_mask) != flags)
			continue;

		if (nix_tm_have_tl1_access(dev) &&
		    tm_node->hw_lvl_id ==  NIX_TXSCH_LVL_TL1)
			skip_node = true;

		otx2_tm_dbg("Free hwres for node %u, hwlvl %u, hw_id %u (%p)",
			    tm_node->id,  tm_node->hw_lvl_id,
			    tm_node->hw_id, tm_node);
		/* Free specific HW resource if requested */
		if (!skip_node && flags_mask &&
		    tm_node->flags & NIX_TM_NODE_HWRES) {
			req = otx2_mbox_alloc_msg_nix_txsch_free(mbox);
			req->flags = 0;
			req->schq_lvl = tm_node->hw_lvl_id;
			req->schq = tm_node->hw_id;
			rc = otx2_mbox_process(mbox);
			if (rc)
				break;
		} else {
			skip_node = false;
		}
		tm_node->flags &= ~NIX_TM_NODE_HWRES;

		/* Leave software elements if needed */
		if (hw_only)
			continue;

		shaper_profile_id = tm_node->params.shaper_profile_id;
		shaper_profile =
			nix_tm_shaper_profile_search(dev, shaper_profile_id);
		if (shaper_profile)
			shaper_profile->reference_count--;

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

	otx2_tm_dbg("Assign hw id for child node %u, lvl %u, hw_lvl %u (%p)",
		    child->id, child->level_id, child->hw_lvl_id, child);

	child->flags |= NIX_TM_NODE_HWRES;

	/* Process root nodes */
	if (dev->otx2_tm_root_lvl == NIX_TXSCH_LVL_TL2 &&
	    child->hw_lvl_id == dev->otx2_tm_root_lvl && !parent) {
		int idx = 0;
		uint32_t tschq_con_index;

		l_id = child->hw_lvl_id;
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
	    child->hw_lvl_id == dev->otx2_tm_root_lvl && !parent) {
		uint32_t tschq_con_index;

		l_id = child->hw_lvl_id;
		tschq_con_index = dev->txschq_index[l_id];
		hw_id = dev->txschq_list[l_id][tschq_con_index];
		child->hw_id = hw_id;
		dev->txschq_index[l_id]++;
		return 0;
	}

	/* Process children with parents */
	l_id = child->hw_lvl_id;
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
			child_hw_lvl = parent->hw_lvl_id - 1;
			if (parent->hw_lvl_id != i)
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
			if (parent->hw_lvl_id == dev->otx2_tm_root_lvl)
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
		if (lvl == tm_node->hw_lvl_id) {
			req->schq[lvl - 1] += tm_node->rr_num;
			if (tm_node->max_prio != UINT32_MAX) {
				contig_count = tm_node->max_prio + 1;
				req->schq_contig[lvl - 1] += contig_count;
			}
		}
		if (lvl == dev->otx2_tm_root_lvl &&
		    dev->otx2_tm_root_lvl && lvl == NIX_TXSCH_LVL_TL2 &&
		    tm_node->hw_lvl_id == dev->otx2_tm_root_lvl) {
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

	nix_tm_assign_hw_id(dev);
	return 0;
}

static int
nix_tm_alloc_resources(struct rte_eth_dev *eth_dev, bool xmit_enable)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_nix_tm_node *tm_node;
	uint16_t sq, smq, rr_quantum;
	struct otx2_eth_txq *txq;
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

	/* Enable xmit as all the topology is ready */
	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (tm_node->flags & NIX_TM_NODE_ENABLED)
			continue;

		/* Enable xmit on sq */
		if (tm_node->level_id != OTX2_TM_LVL_QUEUE) {
			tm_node->flags |= NIX_TM_NODE_ENABLED;
			continue;
		}

		/* Don't enable SMQ or mark as enable */
		if (!xmit_enable)
			continue;

		sq = tm_node->id;
		if (sq > eth_dev->data->nb_tx_queues) {
			rc = -EFAULT;
			break;
		}

		txq = eth_dev->data->tx_queues[sq];

		smq = tm_node->parent->hw_id;
		rr_quantum = (tm_node->weight *
			      NIX_TM_RR_QUANTUM_MAX) / MAX_SCHED_WEIGHT;

		rc = nix_tm_sw_xon(txq, smq, rr_quantum);
		if (rc)
			break;
		tm_node->flags |= NIX_TM_NODE_ENABLED;
	}

	if (rc)
		otx2_err("TM failed to enable xmit on sq %u, rc=%d", sq, rc);

	return rc;
}

static int
nix_tm_prepare_default_tree(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint32_t def = eth_dev->data->nb_tx_queues;
	struct rte_tm_node_params params;
	uint32_t leaf_parent, i;
	int rc = 0;

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
	}

	/* Add leaf nodes */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		rc = nix_tm_node_add_to_list(dev, i, leaf_parent, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_CNT,
					     OTX2_TM_LVL_QUEUE, false, &params);
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
}

int otx2_nix_tm_init_default(struct rte_eth_dev *eth_dev)
{
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

	rc = nix_tm_prepare_default_tree(eth_dev);
	if (rc != 0)
		return rc;

	rc = nix_tm_alloc_resources(eth_dev, false);
	if (rc != 0)
		return rc;
	dev->tm_leaf_cnt = sq_cnt;

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
	if (!tm_node || tm_node->level_id != OTX2_TM_LVL_QUEUE ||
	    !tm_node->parent || tm_node->parent->hw_id == UINT32_MAX) {
		return -EIO;
	}

	/* Get SMQ Id of leaf node's parent */
	*smq = tm_node->parent->hw_id;
	*rr_quantum = (tm_node->weight * NIX_TM_RR_QUANTUM_MAX)
		/ MAX_SCHED_WEIGHT;

	rc = nix_smq_xoff(dev, *smq, false);
	if (rc)
		return rc;
	tm_node->flags |= NIX_TM_NODE_ENABLED;

	return 0;
}
