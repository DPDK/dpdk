/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <stdbool.h>

#include <rte_common.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_log.h"

int
sfc_mae_attach(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	struct sfc_mae *mae = &sa->mae;
	efx_mae_limits_t limits;
	int rc;

	sfc_log_init(sa, "entry");

	if (!encp->enc_mae_supported) {
		mae->status = SFC_MAE_STATUS_UNSUPPORTED;
		return 0;
	}

	sfc_log_init(sa, "init MAE");
	rc = efx_mae_init(sa->nic);
	if (rc != 0)
		goto fail_mae_init;

	sfc_log_init(sa, "get MAE limits");
	rc = efx_mae_get_limits(sa->nic, &limits);
	if (rc != 0)
		goto fail_mae_get_limits;

	mae->status = SFC_MAE_STATUS_SUPPORTED;
	mae->nb_action_rule_prios_max = limits.eml_max_n_action_prios;
	TAILQ_INIT(&mae->action_sets);

	sfc_log_init(sa, "done");

	return 0;

fail_mae_get_limits:
	efx_mae_fini(sa->nic);

fail_mae_init:
	sfc_log_init(sa, "failed %d", rc);

	return rc;
}

void
sfc_mae_detach(struct sfc_adapter *sa)
{
	struct sfc_mae *mae = &sa->mae;
	enum sfc_mae_status status_prev = mae->status;

	sfc_log_init(sa, "entry");

	mae->nb_action_rule_prios_max = 0;
	mae->status = SFC_MAE_STATUS_UNKNOWN;

	if (status_prev != SFC_MAE_STATUS_SUPPORTED)
		return;

	efx_mae_fini(sa->nic);

	sfc_log_init(sa, "done");
}

static struct sfc_mae_action_set *
sfc_mae_action_set_attach(struct sfc_adapter *sa,
			  const efx_mae_actions_t *spec)
{
	struct sfc_mae_action_set *action_set;
	struct sfc_mae *mae = &sa->mae;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	TAILQ_FOREACH(action_set, &mae->action_sets, entries) {
		if (efx_mae_action_set_specs_equal(action_set->spec, spec)) {
			++(action_set->refcnt);
			return action_set;
		}
	}

	return NULL;
}

static int
sfc_mae_action_set_add(struct sfc_adapter *sa,
		       efx_mae_actions_t *spec,
		       struct sfc_mae_action_set **action_setp)
{
	struct sfc_mae_action_set *action_set;
	struct sfc_mae *mae = &sa->mae;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	action_set = rte_zmalloc("sfc_mae_action_set", sizeof(*action_set), 0);
	if (action_set == NULL)
		return ENOMEM;

	action_set->refcnt = 1;
	action_set->spec = spec;

	action_set->fw_rsrc.aset_id.id = EFX_MAE_RSRC_ID_INVALID;

	TAILQ_INSERT_TAIL(&mae->action_sets, action_set, entries);

	*action_setp = action_set;

	return 0;
}

static void
sfc_mae_action_set_del(struct sfc_adapter *sa,
		       struct sfc_mae_action_set *action_set)
{
	struct sfc_mae *mae = &sa->mae;

	SFC_ASSERT(sfc_adapter_is_locked(sa));
	SFC_ASSERT(action_set->refcnt != 0);

	--(action_set->refcnt);

	if (action_set->refcnt != 0)
		return;

	SFC_ASSERT(action_set->fw_rsrc.aset_id.id == EFX_MAE_RSRC_ID_INVALID);
	SFC_ASSERT(action_set->fw_rsrc.refcnt == 0);

	efx_mae_action_set_spec_fini(sa->nic, action_set->spec);
	TAILQ_REMOVE(&mae->action_sets, action_set, entries);
	rte_free(action_set);
}

static int
sfc_mae_action_set_enable(struct sfc_adapter *sa,
			  struct sfc_mae_action_set *action_set)
{
	struct sfc_mae_fw_rsrc *fw_rsrc = &action_set->fw_rsrc;
	int rc;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (fw_rsrc->refcnt == 0) {
		SFC_ASSERT(fw_rsrc->aset_id.id == EFX_MAE_RSRC_ID_INVALID);
		SFC_ASSERT(action_set->spec != NULL);

		rc = efx_mae_action_set_alloc(sa->nic, action_set->spec,
					      &fw_rsrc->aset_id);
		if (rc != 0)
			return rc;
	}

	++(fw_rsrc->refcnt);

	return 0;
}

static int
sfc_mae_action_set_disable(struct sfc_adapter *sa,
			   struct sfc_mae_action_set *action_set)
{
	struct sfc_mae_fw_rsrc *fw_rsrc = &action_set->fw_rsrc;
	int rc;

	SFC_ASSERT(sfc_adapter_is_locked(sa));
	SFC_ASSERT(fw_rsrc->aset_id.id != EFX_MAE_RSRC_ID_INVALID);
	SFC_ASSERT(fw_rsrc->refcnt != 0);

	if (fw_rsrc->refcnt == 1) {
		rc = efx_mae_action_set_free(sa->nic, &fw_rsrc->aset_id);
		if (rc != 0)
			return rc;

		fw_rsrc->aset_id.id = EFX_MAE_RSRC_ID_INVALID;
	}

	--(fw_rsrc->refcnt);

	return 0;
}

void
sfc_mae_flow_cleanup(struct sfc_adapter *sa,
		     struct rte_flow *flow)
{
	struct sfc_flow_spec *spec;
	struct sfc_flow_spec_mae *spec_mae;

	if (flow == NULL)
		return;

	spec = &flow->spec;

	if (spec == NULL)
		return;

	spec_mae = &spec->mae;

	SFC_ASSERT(spec_mae->rule_id.id == EFX_MAE_RSRC_ID_INVALID);

	if (spec_mae->action_set != NULL)
		sfc_mae_action_set_del(sa, spec_mae->action_set);

	if (spec_mae->match_spec != NULL)
		efx_mae_match_spec_fini(sa->nic, spec_mae->match_spec);
}

static int
sfc_mae_rule_parse_item_phy_port(const struct rte_flow_item *item,
				 struct sfc_flow_parse_ctx *ctx,
				 struct rte_flow_error *error)
{
	struct sfc_mae_parse_ctx *ctx_mae = ctx->mae;
	const struct rte_flow_item_phy_port supp_mask = {
		.index = 0xffffffff,
	};
	const void *def_mask = &rte_flow_item_phy_port_mask;
	const struct rte_flow_item_phy_port *spec = NULL;
	const struct rte_flow_item_phy_port *mask = NULL;
	efx_mport_sel_t mport_v;
	int rc;

	if (ctx_mae->match_mport_set) {
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Can't handle multiple traffic source items");
	}

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec, (const void **)&mask,
				 (const void *)&supp_mask, def_mask,
				 sizeof(struct rte_flow_item_phy_port), error);
	if (rc != 0)
		return rc;

	if (mask->index != supp_mask.index) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Bad mask in the PHY_PORT pattern item");
	}

	/* If "spec" is not set, could be any physical port */
	if (spec == NULL)
		return 0;

	rc = efx_mae_mport_by_phy_port(spec->index, &mport_v);
	if (rc != 0) {
		return rte_flow_error_set(error, rc,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Failed to convert the PHY_PORT index");
	}

	rc = efx_mae_match_spec_mport_set(ctx_mae->match_spec_action,
					  &mport_v, NULL);
	if (rc != 0) {
		return rte_flow_error_set(error, rc,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Failed to set MPORT for the PHY_PORT");
	}

	ctx_mae->match_mport_set = B_TRUE;

	return 0;
}

struct sfc_mae_field_locator {
	efx_mae_field_id_t		field_id;
	size_t				size;
	/* Field offset in the corresponding rte_flow_item_ struct */
	size_t				ofst;
};

static void
sfc_mae_item_build_supp_mask(const struct sfc_mae_field_locator *field_locators,
			     unsigned int nb_field_locators, void *mask_ptr,
			     size_t mask_size)
{
	unsigned int i;

	memset(mask_ptr, 0, mask_size);

	for (i = 0; i < nb_field_locators; ++i) {
		const struct sfc_mae_field_locator *fl = &field_locators[i];

		SFC_ASSERT(fl->ofst + fl->size <= mask_size);
		memset(RTE_PTR_ADD(mask_ptr, fl->ofst), 0xff, fl->size);
	}
}

static int
sfc_mae_parse_item(const struct sfc_mae_field_locator *field_locators,
		   unsigned int nb_field_locators, const uint8_t *spec,
		   const uint8_t *mask, efx_mae_match_spec_t *efx_spec,
		   struct rte_flow_error *error)
{
	unsigned int i;
	int rc = 0;

	for (i = 0; i < nb_field_locators; ++i) {
		const struct sfc_mae_field_locator *fl = &field_locators[i];

		rc = efx_mae_match_spec_field_set(efx_spec, fl->field_id,
						  fl->size, spec + fl->ofst,
						  fl->size, mask + fl->ofst);
		if (rc != 0)
			break;
	}

	if (rc != 0) {
		rc = rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "Failed to process item fields");
	}

	return rc;
}

static const struct sfc_mae_field_locator flocs_eth[] = {
	{
		EFX_MAE_FIELD_ETHER_TYPE_BE,
		RTE_SIZEOF_FIELD(struct rte_flow_item_eth, type),
		offsetof(struct rte_flow_item_eth, type),
	},
	{
		EFX_MAE_FIELD_ETH_DADDR_BE,
		RTE_SIZEOF_FIELD(struct rte_flow_item_eth, dst),
		offsetof(struct rte_flow_item_eth, dst),
	},
	{
		EFX_MAE_FIELD_ETH_SADDR_BE,
		RTE_SIZEOF_FIELD(struct rte_flow_item_eth, src),
		offsetof(struct rte_flow_item_eth, src),
	},
};

static int
sfc_mae_rule_parse_item_eth(const struct rte_flow_item *item,
			    struct sfc_flow_parse_ctx *ctx,
			    struct rte_flow_error *error)
{
	struct sfc_mae_parse_ctx *ctx_mae = ctx->mae;
	struct rte_flow_item_eth supp_mask;
	const uint8_t *spec = NULL;
	const uint8_t *mask = NULL;
	int rc;

	sfc_mae_item_build_supp_mask(flocs_eth, RTE_DIM(flocs_eth),
				     &supp_mask, sizeof(supp_mask));

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec, (const void **)&mask,
				 (const void *)&supp_mask,
				 &rte_flow_item_eth_mask,
				 sizeof(struct rte_flow_item_eth), error);
	if (rc != 0)
		return rc;

	/* If "spec" is not set, could be any Ethernet */
	if (spec == NULL)
		return 0;

	return sfc_mae_parse_item(flocs_eth, RTE_DIM(flocs_eth), spec, mask,
				  ctx_mae->match_spec_action, error);
}

static const struct sfc_flow_item sfc_flow_items[] = {
	{
		.type = RTE_FLOW_ITEM_TYPE_PHY_PORT,
		/*
		 * In terms of RTE flow, this item is a META one,
		 * and its position in the pattern is don't care.
		 */
		.prev_layer = SFC_FLOW_ITEM_ANY_LAYER,
		.layer = SFC_FLOW_ITEM_ANY_LAYER,
		.ctx_type = SFC_FLOW_PARSE_CTX_MAE,
		.parse = sfc_mae_rule_parse_item_phy_port,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.prev_layer = SFC_FLOW_ITEM_START_LAYER,
		.layer = SFC_FLOW_ITEM_L2,
		.ctx_type = SFC_FLOW_PARSE_CTX_MAE,
		.parse = sfc_mae_rule_parse_item_eth,
	},
};

int
sfc_mae_rule_parse_pattern(struct sfc_adapter *sa,
			   const struct rte_flow_item pattern[],
			   struct sfc_flow_spec_mae *spec,
			   struct rte_flow_error *error)
{
	struct sfc_mae_parse_ctx ctx_mae;
	struct sfc_flow_parse_ctx ctx;
	int rc;

	memset(&ctx_mae, 0, sizeof(ctx_mae));

	rc = efx_mae_match_spec_init(sa->nic, EFX_MAE_RULE_ACTION,
				     spec->priority,
				     &ctx_mae.match_spec_action);
	if (rc != 0) {
		rc = rte_flow_error_set(error, rc,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Failed to initialise action rule match specification");
		goto fail_init_match_spec_action;
	}

	ctx.type = SFC_FLOW_PARSE_CTX_MAE;
	ctx.mae = &ctx_mae;

	rc = sfc_flow_parse_pattern(sfc_flow_items, RTE_DIM(sfc_flow_items),
				    pattern, &ctx, error);
	if (rc != 0)
		goto fail_parse_pattern;

	if (!efx_mae_match_spec_is_valid(sa->nic, ctx_mae.match_spec_action)) {
		rc = rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					"Inconsistent pattern");
		goto fail_validate_match_spec_action;
	}

	spec->match_spec = ctx_mae.match_spec_action;

	return 0;

fail_validate_match_spec_action:
fail_parse_pattern:
	efx_mae_match_spec_fini(sa->nic, ctx_mae.match_spec_action);

fail_init_match_spec_action:
	return rc;
}

/*
 * An action supported by MAE may correspond to a bundle of RTE flow actions,
 * in example, VLAN_PUSH = OF_PUSH_VLAN + OF_VLAN_SET_VID + OF_VLAN_SET_PCP.
 * That is, related RTE flow actions need to be tracked as parts of a whole
 * so that they can be combined into a single action and submitted to MAE
 * representation of a given rule's action set.
 *
 * Each RTE flow action provided by an application gets classified as
 * one belonging to some bundle type. If an action is not supposed to
 * belong to any bundle, or if this action is END, it is described as
 * one belonging to a dummy bundle of type EMPTY.
 *
 * A currently tracked bundle will be submitted if a repeating
 * action or an action of different bundle type follows.
 */

enum sfc_mae_actions_bundle_type {
	SFC_MAE_ACTIONS_BUNDLE_EMPTY = 0,
	SFC_MAE_ACTIONS_BUNDLE_VLAN_PUSH,
};

struct sfc_mae_actions_bundle {
	enum sfc_mae_actions_bundle_type	type;

	/* Indicates actions already tracked by the current bundle */
	uint64_t				actions_mask;

	/* Parameters used by SFC_MAE_ACTIONS_BUNDLE_VLAN_PUSH */
	rte_be16_t				vlan_push_tpid;
	rte_be16_t				vlan_push_tci;
};

/*
 * Combine configuration of RTE flow actions tracked by the bundle into a
 * single action and submit the result to MAE action set specification.
 * Do nothing in the case of dummy action bundle.
 */
static int
sfc_mae_actions_bundle_submit(const struct sfc_mae_actions_bundle *bundle,
			      efx_mae_actions_t *spec)
{
	int rc = 0;

	switch (bundle->type) {
	case SFC_MAE_ACTIONS_BUNDLE_EMPTY:
		break;
	case SFC_MAE_ACTIONS_BUNDLE_VLAN_PUSH:
		rc = efx_mae_action_set_populate_vlan_push(
			spec, bundle->vlan_push_tpid, bundle->vlan_push_tci);
		break;
	default:
		SFC_ASSERT(B_FALSE);
		break;
	}

	return rc;
}

/*
 * Given the type of the next RTE flow action in the line, decide
 * whether a new bundle is about to start, and, if this is the case,
 * submit and reset the current bundle.
 */
static int
sfc_mae_actions_bundle_sync(const struct rte_flow_action *action,
			    struct sfc_mae_actions_bundle *bundle,
			    efx_mae_actions_t *spec,
			    struct rte_flow_error *error)
{
	enum sfc_mae_actions_bundle_type bundle_type_new;
	int rc;

	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
		bundle_type_new = SFC_MAE_ACTIONS_BUNDLE_VLAN_PUSH;
		break;
	default:
		/*
		 * Self-sufficient actions, including END, are handled in this
		 * case. No checks for unsupported actions are needed here
		 * because parsing doesn't occur at this point.
		 */
		bundle_type_new = SFC_MAE_ACTIONS_BUNDLE_EMPTY;
		break;
	}

	if (bundle_type_new != bundle->type ||
	    (bundle->actions_mask & (1ULL << action->type)) != 0) {
		rc = sfc_mae_actions_bundle_submit(bundle, spec);
		if (rc != 0)
			goto fail_submit;

		memset(bundle, 0, sizeof(*bundle));
	}

	bundle->type = bundle_type_new;

	return 0;

fail_submit:
	return rte_flow_error_set(error, rc,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"Failed to request the (group of) action(s)");
}

static void
sfc_mae_rule_parse_action_of_push_vlan(
			    const struct rte_flow_action_of_push_vlan *conf,
			    struct sfc_mae_actions_bundle *bundle)
{
	bundle->vlan_push_tpid = conf->ethertype;
}

static void
sfc_mae_rule_parse_action_of_set_vlan_vid(
			    const struct rte_flow_action_of_set_vlan_vid *conf,
			    struct sfc_mae_actions_bundle *bundle)
{
	bundle->vlan_push_tci |= (conf->vlan_vid &
				  rte_cpu_to_be_16(RTE_LEN2MASK(12, uint16_t)));
}

static void
sfc_mae_rule_parse_action_of_set_vlan_pcp(
			    const struct rte_flow_action_of_set_vlan_pcp *conf,
			    struct sfc_mae_actions_bundle *bundle)
{
	uint16_t vlan_tci_pcp = (uint16_t)(conf->vlan_pcp &
					   RTE_LEN2MASK(3, uint8_t)) << 13;

	bundle->vlan_push_tci |= rte_cpu_to_be_16(vlan_tci_pcp);
}

static int
sfc_mae_rule_parse_action_phy_port(struct sfc_adapter *sa,
				   const struct rte_flow_action_phy_port *conf,
				   efx_mae_actions_t *spec)
{
	efx_mport_sel_t mport;
	uint32_t phy_port;
	int rc;

	if (conf->original != 0)
		phy_port = efx_nic_cfg_get(sa->nic)->enc_assigned_port;
	else
		phy_port = conf->index;

	rc = efx_mae_mport_by_phy_port(phy_port, &mport);
	if (rc != 0)
		return rc;

	return efx_mae_action_set_populate_deliver(spec, &mport);
}

static int
sfc_mae_rule_parse_action(struct sfc_adapter *sa,
			  const struct rte_flow_action *action,
			  struct sfc_mae_actions_bundle *bundle,
			  efx_mae_actions_t *spec,
			  struct rte_flow_error *error)
{
	int rc = 0;

	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
		SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_OF_POP_VLAN,
				       bundle->actions_mask);
		rc = efx_mae_action_set_populate_vlan_pop(spec);
		break;
	case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN,
				       bundle->actions_mask);
		sfc_mae_rule_parse_action_of_push_vlan(action->conf, bundle);
		break;
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
		SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID,
				       bundle->actions_mask);
		sfc_mae_rule_parse_action_of_set_vlan_vid(action->conf, bundle);
		break;
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
		SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP,
				       bundle->actions_mask);
		sfc_mae_rule_parse_action_of_set_vlan_pcp(action->conf, bundle);
		break;
	case RTE_FLOW_ACTION_TYPE_FLAG:
		SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_FLAG,
				       bundle->actions_mask);
		rc = efx_mae_action_set_populate_flag(spec);
		break;
	case RTE_FLOW_ACTION_TYPE_PHY_PORT:
		SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_PHY_PORT,
				       bundle->actions_mask);
		rc = sfc_mae_rule_parse_action_phy_port(sa, action->conf, spec);
		break;
	default:
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Unsupported action");
	}

	if (rc != 0) {
		rc = rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_ACTION,
				NULL, "Failed to request the action");
	} else {
		bundle->actions_mask |= (1ULL << action->type);
	}

	return rc;
}

int
sfc_mae_rule_parse_actions(struct sfc_adapter *sa,
			   const struct rte_flow_action actions[],
			   struct sfc_mae_action_set **action_setp,
			   struct rte_flow_error *error)
{
	struct sfc_mae_actions_bundle bundle = {0};
	const struct rte_flow_action *action;
	efx_mae_actions_t *spec;
	int rc;

	if (actions == NULL) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				"NULL actions");
	}

	rc = efx_mae_action_set_spec_init(sa->nic, &spec);
	if (rc != 0)
		goto fail_action_set_spec_init;

	for (action = actions;
	     action->type != RTE_FLOW_ACTION_TYPE_END; ++action) {
		rc = sfc_mae_actions_bundle_sync(action, &bundle, spec, error);
		if (rc != 0)
			goto fail_rule_parse_action;

		rc = sfc_mae_rule_parse_action(sa, action, &bundle, spec,
					       error);
		if (rc != 0)
			goto fail_rule_parse_action;
	}

	rc = sfc_mae_actions_bundle_sync(action, &bundle, spec, error);
	if (rc != 0)
		goto fail_rule_parse_action;

	*action_setp = sfc_mae_action_set_attach(sa, spec);
	if (*action_setp != NULL) {
		efx_mae_action_set_spec_fini(sa->nic, spec);
		return 0;
	}

	rc = sfc_mae_action_set_add(sa, spec, action_setp);
	if (rc != 0)
		goto fail_action_set_add;

	return 0;

fail_action_set_add:
fail_rule_parse_action:
	efx_mae_action_set_spec_fini(sa->nic, spec);

fail_action_set_spec_init:
	if (rc > 0) {
		rc = rte_flow_error_set(error, rc,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			NULL, "Failed to process the action");
	}
	return rc;
}

static bool
sfc_mae_rules_class_cmp(struct sfc_adapter *sa,
			const efx_mae_match_spec_t *left,
			const efx_mae_match_spec_t *right)
{
	bool have_same_class;
	int rc;

	rc = efx_mae_match_specs_class_cmp(sa->nic, left, right,
					   &have_same_class);

	return (rc == 0) ? have_same_class : false;
}

static int
sfc_mae_action_rule_class_verify(struct sfc_adapter *sa,
				 struct sfc_flow_spec_mae *spec)
{
	const struct rte_flow *entry;

	TAILQ_FOREACH_REVERSE(entry, &sa->flow_list, sfc_flow_list, entries) {
		const struct sfc_flow_spec *entry_spec = &entry->spec;
		const struct sfc_flow_spec_mae *es_mae = &entry_spec->mae;
		const efx_mae_match_spec_t *left = es_mae->match_spec;
		const efx_mae_match_spec_t *right = spec->match_spec;

		switch (entry_spec->type) {
		case SFC_FLOW_SPEC_FILTER:
			/* Ignore VNIC-level flows */
			break;
		case SFC_FLOW_SPEC_MAE:
			if (sfc_mae_rules_class_cmp(sa, left, right))
				return 0;
			break;
		default:
			SFC_ASSERT(false);
		}
	}

	sfc_info(sa, "for now, the HW doesn't support rule validation, and HW "
		 "support for inner frame pattern items is not guaranteed; "
		 "other than that, the items are valid from SW standpoint");
	return 0;
}

/**
 * Confirm that a given flow can be accepted by the FW.
 *
 * @param sa
 *   Software adapter context
 * @param flow
 *   Flow to be verified
 * @return
 *   Zero on success and non-zero in the case of error.
 *   A special value of EAGAIN indicates that the adapter is
 *   not in started state. This state is compulsory because
 *   it only makes sense to compare the rule class of the flow
 *   being validated with classes of the active rules.
 *   Such classes are wittingly supported by the FW.
 */
int
sfc_mae_flow_verify(struct sfc_adapter *sa,
		    struct rte_flow *flow)
{
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_mae *spec_mae = &spec->mae;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (sa->state != SFC_ADAPTER_STARTED)
		return EAGAIN;

	return sfc_mae_action_rule_class_verify(sa, spec_mae);
}

int
sfc_mae_flow_insert(struct sfc_adapter *sa,
		    struct rte_flow *flow)
{
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_mae *spec_mae = &spec->mae;
	struct sfc_mae_action_set *action_set = spec_mae->action_set;
	struct sfc_mae_fw_rsrc *fw_rsrc = &action_set->fw_rsrc;
	int rc;

	SFC_ASSERT(spec_mae->rule_id.id == EFX_MAE_RSRC_ID_INVALID);
	SFC_ASSERT(action_set != NULL);

	rc = sfc_mae_action_set_enable(sa, action_set);
	if (rc != 0)
		goto fail_action_set_enable;

	rc = efx_mae_action_rule_insert(sa->nic, spec_mae->match_spec,
					NULL, &fw_rsrc->aset_id,
					&spec_mae->rule_id);
	if (rc != 0)
		goto fail_action_rule_insert;

	return 0;

fail_action_rule_insert:
	(void)sfc_mae_action_set_disable(sa, action_set);

fail_action_set_enable:
	return rc;
}

int
sfc_mae_flow_remove(struct sfc_adapter *sa,
		    struct rte_flow *flow)
{
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_mae *spec_mae = &spec->mae;
	struct sfc_mae_action_set *action_set = spec_mae->action_set;
	int rc;

	SFC_ASSERT(spec_mae->rule_id.id != EFX_MAE_RSRC_ID_INVALID);
	SFC_ASSERT(action_set != NULL);

	rc = efx_mae_action_rule_remove(sa->nic, &spec_mae->rule_id);
	if (rc != 0)
		return rc;

	spec_mae->rule_id.id = EFX_MAE_RSRC_ID_INVALID;

	return sfc_mae_action_set_disable(sa, action_set);
}
