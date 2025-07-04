/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <rte_thash.h>

#include <cnxk_eswitch.h>

const uint8_t eswitch_vlan_rss_key[ROC_NIX_RSS_KEY_LEN] = {
	0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
	0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
	0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
	0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
	0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
	0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE};

int
cnxk_eswitch_flow_rules_remove_list(struct cnxk_eswitch_dev *eswitch_dev, struct flow_list *list,
				    uint16_t hw_func)
{
	struct roc_npc_flow *flow, *tvar;
	int rc = 0;

	RTE_TAILQ_FOREACH_SAFE(flow, list, next, tvar) {
		plt_esw_dbg("Removing flow %d", flow->mcam_id);
		rc = roc_eswitch_npc_mcam_delete_rule(&eswitch_dev->npc, flow,
						      hw_func);
		if (rc)
			plt_err("Failed to delete rule %d", flow->mcam_id);
		rc = roc_npc_mcam_free(&eswitch_dev->npc, flow);
		if (rc)
			plt_err("Failed to free entry %d", flow->mcam_id);
		TAILQ_REMOVE(list, flow, next);
		rte_free(flow);
	}

	return rc;
}

static int
eswitch_npc_vlan_rss_configure(struct roc_npc *roc_npc, struct roc_npc_flow *flow)
{
	struct roc_nix *roc_nix = roc_npc->roc_nix;
	uint32_t qid, idx, hash, vlan_tci;
	uint16_t *reta, reta_sz, id;
	int rc = 0;

	id = flow->mcam_id;
	/* Setting up the key */
	roc_nix_rss_key_set(roc_nix, eswitch_vlan_rss_key);

	reta_sz = roc_nix->reta_sz;
	reta = plt_zmalloc(reta_sz * sizeof(uint16_t), 0);
	if (!reta) {
		plt_err("Failed to allocate mem for reta table");
		rc = -ENOMEM;
		goto fail;
	}
	for (qid = 0; qid < reta_sz; qid++) {
		vlan_tci = (1 << CNXK_ESWITCH_VFPF_SHIFT) | qid;
		hash = rte_softrss(&vlan_tci, 1, eswitch_vlan_rss_key);
		idx = hash & 0xFF;
		reta[idx] = qid;
	}
	flow->mcam_id = id;
	rc = roc_eswitch_npc_rss_action_configure(roc_npc, flow, FLOW_KEY_TYPE_VLAN, reta);
	if (rc) {
		plt_err("Failed to configure rss action, err %d", rc);
		goto done;
	}

done:
	plt_free(reta);
fail:
	return rc;
}

static int
eswitch_pfvf_mcam_install_rules(struct cnxk_eswitch_dev *eswitch_dev, struct roc_npc_flow *flow,
				bool is_vf)
{
	uint16_t vlan_tci = 0, hw_func;
	int rc;

	hw_func = eswitch_dev->npc.pf_func | is_vf;
	if (!is_vf) {
		/* Eswitch PF RX VLAN rule */
		vlan_tci = 1ULL << CNXK_ESWITCH_VFPF_SHIFT;
		rc = roc_eswitch_npc_mcam_rx_rule(&eswitch_dev->npc, flow, hw_func, vlan_tci,
						  0xFF00);
		if (rc) {
			plt_err("Failed to install RX rule for ESW PF to ESW VF, rc %d", rc);
			goto exit;
		}
		plt_esw_dbg("Installed eswitch PF RX rule %d", flow->mcam_id);
		rc = eswitch_npc_vlan_rss_configure(&eswitch_dev->npc, flow);
		if (rc)
			goto exit;
		flow->enable = true;
	} else {
		/* Eswitch VF RX VLAN rule */
		rc = roc_eswitch_npc_mcam_rx_rule(&eswitch_dev->npc, flow, hw_func, vlan_tci,
						  0xFF00);
		if (rc) {
			plt_err("Failed to install RX rule for ESW VF to ESW PF, rc %d", rc);
			goto exit;
		}
		flow->enable = true;
		plt_esw_dbg("Installed eswitch PF RX rule %d", flow->mcam_id);
	}

	return 0;
exit:
	return rc;
}

static int
eswitch_npc_get_counter(struct roc_npc *npc, struct roc_npc_flow *flow)
{
	uint16_t ctr_id;
	int rc;

	rc = roc_npc_mcam_alloc_counter(npc, &ctr_id);
	if (rc < 0) {
		plt_err("Failed to allocate counter, rc %d", rc);
		goto fail;
	}
	flow->ctr_id = ctr_id;
	flow->use_ctr = true;

	rc = roc_npc_mcam_clear_counter(npc, flow->ctr_id);
	if (rc < 0) {
		plt_err("Failed to clear counter idx %d, rc %d", flow->ctr_id, rc);
		goto free;
	}
	return 0;
free:
	roc_npc_mcam_free_counter(npc, ctr_id);
fail:
	return rc;
}

static int
eswitch_npc_get_counter_entry_ref(struct roc_npc *npc, struct roc_npc_flow *flow,
				  struct roc_npc_flow *ref_flow)
{
	int rc = 0, resp_count;

	rc = eswitch_npc_get_counter(npc, flow);
	if (rc)
		goto free;

	/* Allocate an entry viz higher priority than ref flow */
	rc = roc_npc_mcam_alloc_entry(npc, flow, ref_flow, NPC_MCAM_HIGHER_PRIO, &resp_count);
	if (rc) {
		plt_err("Failed to allocate entry, err %d", rc);
		goto free;
	}
	plt_esw_dbg("New entry %d ref entry %d resp_count %d", flow->mcam_id, ref_flow->mcam_id,
		    resp_count);

	return 0;
free:
	roc_npc_mcam_free_counter(npc, flow->ctr_id);
	return rc;
}

int
cnxk_eswitch_flow_rule_shift(uint16_t hw_func, uint16_t *entry)
{
	struct cnxk_esw_repr_hw_info *repr_info;
	struct cnxk_eswitch_dev *eswitch_dev;
	struct roc_npc_flow *ref_flow, *flow;
	uint16_t curr_entry, new_entry;
	int rc = 0, resp_count;

	eswitch_dev = cnxk_eswitch_pmd_priv();
	if (!eswitch_dev) {
		plt_err("Invalid eswitch_dev handle");
		rc = -EINVAL;
		goto fail;
	}

	repr_info = cnxk_eswitch_representor_hw_info(eswitch_dev, hw_func);
	if (!repr_info) {
		plt_warn("Failed to get representor group for %x", hw_func);
		rc = -ENOENT;
		goto fail;
	}

	ref_flow = TAILQ_FIRST(&repr_info->repr_flow_list);
	if (*entry > ref_flow->mcam_id) {
		flow = plt_zmalloc(sizeof(struct roc_npc_flow), 0);
		if (!flow) {
			plt_err("Failed to allocate memory");
			rc = -ENOMEM;
			goto fail;
		}

		/* Allocate a higher priority flow rule */
		rc = roc_npc_mcam_alloc_entry(&eswitch_dev->npc, flow, ref_flow,
					      NPC_MCAM_HIGHER_PRIO, &resp_count);
		if (rc < 0) {
			plt_err("Failed to allocate a newmcam entry, rc %d", rc);
			goto fail;
		}

		if (flow->mcam_id > ref_flow->mcam_id) {
			plt_err("New flow %d is still at higher priority than ref_flow %d",
				flow->mcam_id, ref_flow->mcam_id);
			rc = -EINVAL;
			goto free_entry;
		}

		plt_info("Before shift: HW_func %x curr_entry %d ref flow id %d new_entry %d",
			 hw_func, *entry, ref_flow->mcam_id, flow->mcam_id);

		curr_entry = *entry;
		new_entry = flow->mcam_id;

		rc = roc_npc_mcam_move(&eswitch_dev->npc, curr_entry, new_entry);
		if (rc) {
			plt_err("Failed to shift the new index %d to curr index %d, err	%d", *entry,
				curr_entry, rc);
			goto free_entry;
		}
		*entry = flow->mcam_id;

		/* Freeing the current entry */
		rc = roc_npc_mcam_free_entry(&eswitch_dev->npc, curr_entry);
		if (rc) {
			plt_err("Failed to free the old entry. err %d", rc);
			goto free_entry;
		}

		plt_free(flow);
		plt_info("After shift: HW_func %x old_entry %d new_entry %d", hw_func, curr_entry,
			 *entry);
	}

	return 0;
free_entry:

fail:
	return rc;
}

int
cnxk_eswitch_flow_rules_delete(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func)
{
	struct cnxk_esw_repr_hw_info *repr_info;
	struct flow_list *list;
	int rc = 0;

	repr_info = cnxk_eswitch_representor_hw_info(eswitch_dev, hw_func);
	if (!repr_info) {
		plt_warn("Failed to get representor group for %x", hw_func);
		rc = -ENOENT;
		goto fail;
	}
	list = &repr_info->repr_flow_list;

	plt_esw_dbg("Deleting flows for %x", hw_func);
	rc = cnxk_eswitch_flow_rules_remove_list(eswitch_dev, list, hw_func);
	if (rc)
		plt_err("Failed to delete rules for hw func %x", hw_func);

fail:
	return rc;
}

int
cnxk_eswitch_flow_rules_install(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func)
{
	struct roc_npc_flow *rx_flow, *tx_flow, *flow_iter, *esw_pf_flow = NULL;
	struct cnxk_esw_repr_hw_info *repr_info;
	struct flow_list *list;
	uint16_t vlan_tci;
	int rc = 0;

	repr_info = cnxk_eswitch_representor_hw_info(eswitch_dev, hw_func);
	if (!repr_info) {
		plt_err("Failed to get representor group for %x", hw_func);
		rc = -EINVAL;
		goto fail;
	}
	list = &repr_info->repr_flow_list;

	/* Taking ESW PF as reference entry for installing new rules */
	TAILQ_FOREACH(flow_iter, &eswitch_dev->esw_flow_list, next) {
		if (flow_iter->mcam_id == eswitch_dev->esw_pf_entry) {
			esw_pf_flow = flow_iter;
			break;
		}
	}

	if (!esw_pf_flow) {
		plt_err("Failed to get the ESW PF flow");
		rc = -EINVAL;
		goto fail;
	}

	/* Installing RX rule */
	rx_flow = plt_zmalloc(sizeof(struct roc_npc_flow), 0);
	if (!rx_flow) {
		plt_err("Failed to allocate memory");
		rc = -ENOMEM;
		goto fail;
	}

	rc = eswitch_npc_get_counter_entry_ref(&eswitch_dev->npc, rx_flow, esw_pf_flow);
	if (rc) {
		plt_err("Failed to get counter and mcam entry, rc %d", rc);
		goto free_rx_flow;
	}

	/* VLAN TCI value for this representee is the rep id from AF driver */
	vlan_tci = repr_info->rep_id;
	rc = roc_eswitch_npc_mcam_rx_rule(&eswitch_dev->npc, rx_flow, hw_func, vlan_tci, 0xFFFF);
	if (rc) {
		plt_err("Failed to install RX rule for ESW PF to ESW VF, rc %d", rc);
		goto free_rx_entry;
	}
	rx_flow->enable = true;
	/* List in ascending order of mcam entries */
	TAILQ_FOREACH(flow_iter, list, next) {
		if (flow_iter->mcam_id > rx_flow->mcam_id) {
			TAILQ_INSERT_BEFORE(flow_iter, rx_flow, next);
			goto done_rx;
		}
	}
	TAILQ_INSERT_TAIL(list, rx_flow, next);
done_rx:
	repr_info->num_flow_entries++;
	plt_esw_dbg("Installed RX flow rule %d for representee %x with vlan tci %x MCAM id %d",
		    eswitch_dev->num_entries, hw_func, vlan_tci, rx_flow->mcam_id);

	/* Installing TX rule */
	tx_flow = plt_zmalloc(sizeof(struct roc_npc_flow), 0);
	if (!tx_flow) {
		plt_err("Failed to allocate memory");
		rc = -ENOMEM;
		goto remove_rx_rule;
	}

	rc = eswitch_npc_get_counter_entry_ref(&eswitch_dev->npc, tx_flow, esw_pf_flow);
	if (rc) {
		plt_err("Failed to get counter and mcam entry, rc %d", rc);
		goto free_tx_flow;
	}

	vlan_tci = (1ULL << CNXK_ESWITCH_VFPF_SHIFT) | repr_info->rep_id;
	rc = roc_eswitch_npc_mcam_tx_rule(&eswitch_dev->npc, tx_flow, hw_func, vlan_tci);
	if (rc) {
		plt_err("Failed to install RX rule for ESW PF to ESW VF, rc %d", rc);
		goto free_tx_entry;
	}
	tx_flow->enable = true;
	/* List in ascending order of mcam entries */
	TAILQ_FOREACH(flow_iter, list, next) {
		if (flow_iter->mcam_id > tx_flow->mcam_id) {
			TAILQ_INSERT_BEFORE(flow_iter, tx_flow, next);
			goto done_tx;
		}
	}
	TAILQ_INSERT_TAIL(list, tx_flow, next);
done_tx:
	repr_info->num_flow_entries++;
	plt_esw_dbg("Installed TX flow rule %d for representee %x with vlan tci %x MCAM id %d",
		    repr_info->num_flow_entries, hw_func, vlan_tci, tx_flow->mcam_id);

	return 0;
free_tx_entry:
	roc_npc_mcam_free(&eswitch_dev->npc, tx_flow);
free_tx_flow:
	rte_free(tx_flow);
remove_rx_rule:
	TAILQ_REMOVE(list, rx_flow, next);
free_rx_entry:
	roc_npc_mcam_free(&eswitch_dev->npc, rx_flow);
free_rx_flow:
	rte_free(rx_flow);
fail:
	return rc;
}

int
cnxk_eswitch_pfvf_flow_rules_install(struct cnxk_eswitch_dev *eswitch_dev, bool is_vf)
{
	struct roc_npc_flow *flow, *flow_iter;
	struct flow_list *list;
	int rc = 0;

	list = &eswitch_dev->esw_flow_list;
	flow = plt_zmalloc(sizeof(struct roc_npc_flow), 0);
	if (!flow) {
		plt_err("Failed to allocate memory");
		rc = -ENOMEM;
		goto fail;
	}

	rc = eswitch_npc_get_counter(&eswitch_dev->npc, flow);
	if (rc) {
		plt_err("Failed to get counter and mcam entry, rc %d", rc);
		goto free_flow;
	}
	if (!is_vf) {
		/* Reserving an entry for esw VF but will not be installed */
		rc = roc_npc_get_free_mcam_entry(&eswitch_dev->npc, flow);
		if (rc < 0) {
			plt_err("Failed to allocate entry for vf, err %d", rc);
			goto free_flow;
		}
		eswitch_dev->esw_vf_entry = flow->mcam_id;
		/* Allocate an entry for esw PF */
		rc = eswitch_npc_get_counter_entry_ref(&eswitch_dev->npc, flow, flow);
		if (rc) {
			plt_err("Failed to allocate entry for pf, err %d", rc);
			goto free_flow;
		}
		eswitch_dev->esw_pf_entry = flow->mcam_id;
		plt_esw_dbg("Allocated entries for esw: PF %d and VF %d", eswitch_dev->esw_pf_entry,
			    eswitch_dev->esw_vf_entry);
	} else {
		flow->mcam_id = eswitch_dev->esw_vf_entry;
	}

	rc = eswitch_pfvf_mcam_install_rules(eswitch_dev, flow, is_vf);
	if (rc) {
		plt_err("Failed to install entries, rc %d", rc);
		goto free_flow;
	}

	/* List in ascending order of mcam entries */
	TAILQ_FOREACH(flow_iter, list, next) {
		if (flow_iter->mcam_id > flow->mcam_id) {
			TAILQ_INSERT_BEFORE(flow_iter, flow, next);
			goto done;
		}
	}
	TAILQ_INSERT_TAIL(list, flow, next);
done:
	eswitch_dev->num_entries++;
	plt_esw_dbg("Installed new eswitch flow rule %d with MCAM id %d", eswitch_dev->num_entries,
		    flow->mcam_id);

	return 0;

free_flow:
	cnxk_eswitch_flow_rules_remove_list(eswitch_dev, &eswitch_dev->esw_flow_list,
					    eswitch_dev->npc.pf_func);
fail:
	return rc;
}
