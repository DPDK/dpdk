/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

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
