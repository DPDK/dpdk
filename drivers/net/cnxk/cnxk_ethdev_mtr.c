/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"
#include <rte_mtr_driver.h>

#define NIX_MTR_COUNT_MAX      73 /* 64(leaf) + 8(mid) + 1(top) */
#define NIX_MTR_COUNT_PER_FLOW 3  /* 1(leaf) + 1(mid) + 1(top) */

#define NIX_BPF_STATS_MASK_ALL                                                 \
	{                                                                      \
		ROC_NIX_BPF_GREEN_PKT_F_PASS | ROC_NIX_BPF_GREEN_OCTS_F_PASS | \
			ROC_NIX_BPF_GREEN_PKT_F_DROP |                         \
			ROC_NIX_BPF_GREEN_OCTS_F_DROP |                        \
			ROC_NIX_BPF_YELLOW_PKT_F_PASS |                        \
			ROC_NIX_BPF_YELLOW_OCTS_F_PASS |                       \
			ROC_NIX_BPF_YELLOW_PKT_F_DROP |                        \
			ROC_NIX_BPF_YELLOW_OCTS_F_DROP |                       \
			ROC_NIX_BPF_RED_PKT_F_PASS |                           \
			ROC_NIX_BPF_RED_OCTS_F_PASS |                          \
			ROC_NIX_BPF_RED_PKT_F_DROP |                           \
			ROC_NIX_BPF_RED_OCTS_F_DROP                            \
	}

static const enum roc_nix_bpf_level_flag lvl_map[] = {ROC_NIX_BPF_LEVEL_F_LEAF,
						      ROC_NIX_BPF_LEVEL_F_MID,
						      ROC_NIX_BPF_LEVEL_F_TOP};

static struct rte_mtr_capabilities mtr_capa = {
	.n_shared_max = NIX_MTR_COUNT_PER_FLOW,
	/* .identical = , */
	.shared_identical = true,
	/* .shared_n_flows_per_mtr_max = ,*/
	.chaining_n_mtrs_per_flow_max = NIX_MTR_COUNT_PER_FLOW,
	.chaining_use_prev_mtr_color_supported = true,
	.chaining_use_prev_mtr_color_enforced = true,
	.meter_rate_max = NIX_BPF_RATE_MAX / 8, /* Bytes per second */
	.color_aware_srtcm_rfc2697_supported = true,
	.color_aware_trtcm_rfc2698_supported = true,
	.color_aware_trtcm_rfc4115_supported = true,
	.srtcm_rfc2697_byte_mode_supported = true,
	.srtcm_rfc2697_packet_mode_supported = true,
	.trtcm_rfc2698_byte_mode_supported = true,
	.trtcm_rfc2698_packet_mode_supported = true,
	.trtcm_rfc4115_byte_mode_supported = true,
	.trtcm_rfc4115_packet_mode_supported = true,
	.stats_mask = RTE_MTR_STATS_N_PKTS_GREEN | RTE_MTR_STATS_N_PKTS_YELLOW |
		      RTE_MTR_STATS_N_PKTS_RED | RTE_MTR_STATS_N_PKTS_DROPPED |
		      RTE_MTR_STATS_N_BYTES_GREEN |
		      RTE_MTR_STATS_N_BYTES_YELLOW | RTE_MTR_STATS_N_BYTES_RED |
		      RTE_MTR_STATS_N_BYTES_DROPPED};

static struct cnxk_meter_node *
nix_mtr_find(struct cnxk_eth_dev *dev, uint32_t meter_id)
{
	struct cnxk_mtr *fms = &dev->mtr;
	struct cnxk_meter_node *fm;

	TAILQ_FOREACH(fm, fms, next)
		if (meter_id == fm->id)
			return fm;
	return NULL;
}

static struct cnxk_mtr_profile_node *
nix_mtr_profile_find(struct cnxk_eth_dev *dev, uint32_t profile_id)
{
	struct cnxk_mtr_profiles *fmps = &dev->mtr_profiles;
	struct cnxk_mtr_profile_node *fmp;

	TAILQ_FOREACH(fmp, fmps, next)
		if (profile_id == fmp->id)
			return fmp;

	return NULL;
}

static struct cnxk_mtr_policy_node *
nix_mtr_policy_find(struct cnxk_eth_dev *dev, uint32_t meter_policy_id)
{
	struct cnxk_mtr_policy *fmps = &dev->mtr_policy;
	struct cnxk_mtr_policy_node *fmp;

	TAILQ_FOREACH(fmp, fmps, next)
		if (meter_policy_id == fmp->id)
			return fmp;
	return NULL;
}

static int
nix_mtr_profile_validate(struct cnxk_eth_dev *dev, uint32_t profile_id,
			 struct rte_mtr_meter_profile *profile,
			 struct rte_mtr_error *error)
{
	int rc = 0;

	PLT_SET_USED(dev);

	if (profile == NULL)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE,
					  NULL, "Meter profile is null.");

	if (profile_id == UINT32_MAX)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Meter profile id not valid.");

	switch (profile->alg) {
	case RTE_MTR_SRTCM_RFC2697:
		if (profile->srtcm_rfc2697.cir > mtr_capa.meter_rate_max)
			rc = -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"CIR exceeds max meter rate");

		if (profile->srtcm_rfc2697.cbs > NIX_BPF_BURST_MAX)
			rc = -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"CBS exceeds max meter burst size");

		if (profile->srtcm_rfc2697.ebs > NIX_BPF_BURST_MAX)
			rc = -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"EBS exceeds max meter burst size");
		break;

	case RTE_MTR_TRTCM_RFC2698:
		if (profile->trtcm_rfc2698.cir > mtr_capa.meter_rate_max)
			rc = -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"CIR exceeds max meter rate");

		if (profile->trtcm_rfc2698.pir > mtr_capa.meter_rate_max)
			rc = -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"PIR exceeds max meter rate");

		if (profile->trtcm_rfc2698.cbs > NIX_BPF_BURST_MAX)
			rc = -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"CBS exceeds max meter burst size");

		if (profile->trtcm_rfc2698.pbs > NIX_BPF_BURST_MAX)
			rc = -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"PBS exceeds max meter burst size");
		break;

	case RTE_MTR_TRTCM_RFC4115:
		if ((profile->trtcm_rfc4115.cir + profile->trtcm_rfc4115.eir) >
		    mtr_capa.meter_rate_max)
			rc = -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"PIR + EIR exceeds max rate");

		if (profile->trtcm_rfc4115.cbs > NIX_BPF_BURST_MAX)
			rc = -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"CBS exceeds max meter burst size");

		if (profile->trtcm_rfc4115.ebs > NIX_BPF_BURST_MAX)
			rc = -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"PBS exceeds max meter burst size");
		break;

	default:
		rc = -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
					"alg is invalid");
		break;
	}

	return rc;
}

static int
cnxk_nix_mtr_capabilities_get(struct rte_eth_dev *dev,
			      struct rte_mtr_capabilities *capa,
			      struct rte_mtr_error *error)
{
	struct cnxk_eth_dev *eth_dev = cnxk_eth_pmd_priv(dev);
	uint16_t count[ROC_NIX_BPF_LEVEL_MAX] = {0};
	uint8_t lvl_mask = ROC_NIX_BPF_LEVEL_F_LEAF | ROC_NIX_BPF_LEVEL_F_MID |
			   ROC_NIX_BPF_LEVEL_F_TOP;
	struct roc_nix *nix = &eth_dev->nix;
	int rc;
	int i;

	RTE_SET_USED(dev);

	if (!capa)
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
				"NULL input parameter");

	rc = roc_nix_bpf_count_get(nix, lvl_mask, count);
	if (rc)
		return rc;

	for (i = 0; i < ROC_NIX_BPF_LEVEL_MAX; i++)
		mtr_capa.n_max += count[i];

	mtr_capa.meter_srtcm_rfc2697_n_max = mtr_capa.n_max;
	mtr_capa.meter_trtcm_rfc2698_n_max = mtr_capa.n_max;
	mtr_capa.meter_trtcm_rfc4115_n_max = mtr_capa.n_max;
	mtr_capa.meter_policy_n_max = mtr_capa.n_max;

	*capa = mtr_capa;
	return 0;
}

static int
cnxk_nix_mtr_profile_add(struct rte_eth_dev *eth_dev, uint32_t profile_id,
			 struct rte_mtr_meter_profile *profile,
			 struct rte_mtr_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_mtr_profiles *fmps = &dev->mtr_profiles;
	struct cnxk_mtr_profile_node *fmp;
	int ret;

	/* Check input params. */
	ret = nix_mtr_profile_validate(dev, profile_id, profile, error);
	if (ret)
		return ret;

	fmp = nix_mtr_profile_find(dev, profile_id);
	if (fmp) {
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Profile already exist");
	}

	fmp = plt_zmalloc(sizeof(struct cnxk_mtr_profile_node), ROC_ALIGN);
	if (fmp == NULL)
		return -rte_mtr_error_set(error, ENOMEM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter profile memory "
					  "alloc failed.");

	fmp->id = profile_id;
	fmp->profile = *profile;

	TAILQ_INSERT_TAIL(fmps, fmp, next);

	return 0;
}

static int
cnxk_nix_mtr_profile_delete(struct rte_eth_dev *eth_dev, uint32_t profile_id,
			    struct rte_mtr_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_mtr_profile_node *fmp;

	if (profile_id == UINT32_MAX)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Meter profile id not valid.");

	fmp = nix_mtr_profile_find(dev, profile_id);
	if (fmp == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  &profile_id,
					  "Meter profile is invalid.");

	if (fmp->ref_cnt)
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Meter profile is in use.");

	TAILQ_REMOVE(&dev->mtr_profiles, fmp, next);
	plt_free(fmp);
	return 0;
}

static int
update_mtr_err(uint32_t act_color, struct rte_mtr_error *error, bool action)
{
	const char *str = NULL;
	switch (act_color) {
	case RTE_COLOR_GREEN:
		if (action) {
			str = "Green action is not valid";
			goto notsup;
		} else {
			str = "Green action is null";
			goto notvalid;
		}
		break;
	case RTE_COLOR_YELLOW:
		if (action) {
			str = "Yellow action is not valid";
			goto notsup;
		} else {
			str = "Yellow action is null";
			goto notvalid;
		}
		break;
	case RTE_COLOR_RED:
		if (action) {
			str = "Red action is not valid";
			goto notsup;
		} else {
			str = "Red action is null";
			goto notvalid;
		}
		break;
	}
notsup:
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_METER_POLICY, NULL, str);
notvalid:
	return -rte_mtr_error_set(error, EINVAL,
				  RTE_MTR_ERROR_TYPE_METER_POLICY, NULL, str);
}

static int
cnxk_nix_mtr_policy_validate(struct rte_eth_dev *dev,
			     struct rte_mtr_meter_policy_params *policy,
			     struct rte_mtr_error *error)
{
	bool supported[RTE_COLORS] = {false, false, false};
	const struct rte_flow_action *action;
	uint32_t i;

	RTE_SET_USED(dev);

	if (!policy)
		return 0; /* Nothing to be validated */

	for (i = 0; i < RTE_COLORS; i++) {
		if (policy->actions[i]) {
			for (action = policy->actions[i];
			     action->type != RTE_FLOW_ACTION_TYPE_END;
			     action++) {
				if (action->type == RTE_FLOW_ACTION_TYPE_METER)
					supported[i] = true;

				if (action->type == RTE_FLOW_ACTION_TYPE_DROP)
					supported[i] = true;

				if (!supported[i])
					return update_mtr_err(i, error, true);
			}
		} else {
			return update_mtr_err(i, error, false);
		}
	}

	return 0;
}

static void
cnxk_fill_policy_actions(struct cnxk_mtr_policy_node *fmp,
			 struct rte_mtr_meter_policy_params *policy)

{
	const struct rte_flow_action_meter *mtr;
	const struct rte_flow_action *action;
	int i;

	for (i = 0; i < RTE_COLORS; i++) {
		if (policy->actions[i]) {
			for (action = policy->actions[i];
			     action->type != RTE_FLOW_ACTION_TYPE_END;
			     action++) {
				if (action->type ==
				    RTE_FLOW_ACTION_TYPE_METER) {
					fmp->actions[i].action_fate =
						action->type;
					mtr = (const struct
					       rte_flow_action_meter *)
						      action->conf;
					fmp->actions[i].mtr_id = mtr->mtr_id;
				}

				if (action->type == RTE_FLOW_ACTION_TYPE_DROP) {
					fmp->actions[i].action_fate =
						action->type;
				}
			}
		}
	}
}

static int
cnxk_nix_mtr_policy_add(struct rte_eth_dev *eth_dev, uint32_t policy_id,
			struct rte_mtr_meter_policy_params *policy,
			struct rte_mtr_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_mtr_policy *fmps = &dev->mtr_policy;
	struct cnxk_mtr_policy_node *fmp;
	int rc;

	fmp = nix_mtr_policy_find(dev, policy_id);
	if (fmp) {
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					  NULL, "Policy already exist");
	}

	fmp = plt_zmalloc(sizeof(struct cnxk_mtr_policy_node), ROC_ALIGN);
	if (fmp == NULL) {
		return -rte_mtr_error_set(error, ENOMEM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Memory allocation failure");
	} else {
		rc = cnxk_nix_mtr_policy_validate(eth_dev, policy, error);
		if (rc)
			goto exit;
	}

	fmp->id = policy_id;
	cnxk_fill_policy_actions(fmp, policy);
	TAILQ_INSERT_TAIL(fmps, fmp, next);
	return 0;

exit:
	plt_free(fmp);
	return rc;
}

static int
cnxk_nix_mtr_policy_delete(struct rte_eth_dev *eth_dev, uint32_t policy_id,
			   struct rte_mtr_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_mtr_policy_node *fmp;

	fmp = nix_mtr_policy_find(dev, policy_id);
	if (fmp == NULL) {
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					  NULL, "No policy found");
	}

	if (fmp->ref_cnt)
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					  NULL, "Meter policy is in use.");

	TAILQ_REMOVE(&dev->mtr_policy, fmp, next);
	plt_free(fmp);

	return 0;
}

static int
cnxk_nix_mtr_create(struct rte_eth_dev *eth_dev, uint32_t mtr_id,
		    struct rte_mtr_params *params, int shared,
		    struct rte_mtr_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_mtr_profile_node *profile;
	struct cnxk_mtr_policy_node *policy;
	struct cnxk_mtr *fm = &dev->mtr;
	struct cnxk_meter_node *mtr;
	int i;

	RTE_SET_USED(shared);

	if (params == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "Meter params are invalid.");

	profile = nix_mtr_profile_find(dev, params->meter_profile_id);
	if (profile == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  &params->meter_profile_id,
					  "Meter profile is invalid.");

	policy = nix_mtr_policy_find(dev, params->meter_policy_id);
	if (policy == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					  &params->meter_policy_id,
					  "Meter policy is invalid.");

	mtr = nix_mtr_find(dev, mtr_id);
	if (mtr) {
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter already exist");
	}

	mtr = plt_zmalloc(sizeof(struct cnxk_meter_node), ROC_ALIGN);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, ENOMEM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter memory alloc failed.");
	}

	mtr->id = mtr_id;
	mtr->profile = profile;
	mtr->policy = policy;
	mtr->params = *params;
	mtr->bpf_id = ROC_NIX_BPF_ID_INVALID;
	mtr->prev_cnt = 0;
	for (i = 0; i < MAX_PRV_MTR_NODES; i++)
		mtr->prev_id[i] = ROC_NIX_BPF_ID_INVALID;

	mtr->next_id = ROC_NIX_BPF_ID_INVALID;
	mtr->is_next = false;
	mtr->level = ROC_NIX_BPF_LEVEL_IDX_INVALID;

	if (params->dscp_table) {
		mtr->params.dscp_table =
			plt_zmalloc(ROC_NIX_BPF_PRE_COLOR_MAX, ROC_ALIGN);
		if (mtr->params.dscp_table == NULL) {
			plt_free(mtr);
			return -rte_mtr_error_set(error, ENOMEM,
					RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					NULL, "Memory alloc failed.");
		}

		for (i = 0; i < ROC_NIX_BPF_PRE_COLOR_MAX; i++)
			mtr->params.dscp_table[i] = params->dscp_table[i];
	}

	profile->ref_cnt++;
	policy->ref_cnt++;
	TAILQ_INSERT_TAIL(fm, mtr, next);
	return 0;
}

static int
cnxk_nix_mtr_destroy(struct rte_eth_dev *eth_dev, uint32_t mtr_id,
		     struct rte_mtr_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_bpf_objs profs = {0};
	struct cnxk_mtr *fm = &dev->mtr;
	struct roc_nix *nix = &dev->nix;
	struct cnxk_meter_node *mtr;
	struct cnxk_meter_node *mid_mtr;
	struct cnxk_meter_node *top_mtr;
	int rc = 0;

	mtr = nix_mtr_find(dev, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID, &mtr_id,
					  "Meter id is invalid.");
	}

	if (mtr->ref_cnt) {
		return -rte_mtr_error_set(error, EADDRINUSE,
					  RTE_MTR_ERROR_TYPE_MTR_ID, &mtr_id,
					  "Meter id in use.");
	}

	switch (lvl_map[mtr->level]) {
	case ROC_NIX_BPF_LEVEL_F_LEAF:
		if (mtr->is_next) {
			rc = roc_nix_bpf_connect(nix, ROC_NIX_BPF_LEVEL_F_LEAF,
						 mtr->bpf_id,
						 ROC_NIX_BPF_ID_INVALID);
		}
		break;
	case ROC_NIX_BPF_LEVEL_F_MID:
		while ((mtr->prev_cnt) + 1) {
			mid_mtr =
				nix_mtr_find(dev, mtr->prev_id[mtr->prev_cnt]);
			rc = roc_nix_bpf_connect(nix, ROC_NIX_BPF_LEVEL_F_LEAF,
						 mid_mtr->bpf_id,
						 ROC_NIX_BPF_ID_INVALID);
			mtr->prev_cnt--;
		}
		if (mtr->is_next) {
			rc = roc_nix_bpf_connect(nix, ROC_NIX_BPF_LEVEL_F_MID,
						 mtr->bpf_id,
						 ROC_NIX_BPF_ID_INVALID);
		}
		break;
	case ROC_NIX_BPF_LEVEL_F_TOP:
		while (mtr->prev_cnt) {
			top_mtr =
				nix_mtr_find(dev, mtr->prev_id[mtr->prev_cnt]);
			rc = roc_nix_bpf_connect(nix, ROC_NIX_BPF_LEVEL_F_MID,
						 top_mtr->bpf_id,
						 ROC_NIX_BPF_ID_INVALID);
			mtr->prev_cnt--;
		}
		break;
	default:
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Invalid meter level");
	}

	if (rc)
		goto exit;

	profs.level = mtr->level;
	profs.count = 1;
	profs.ids[0] = mtr->bpf_id;
	rc = roc_nix_bpf_free(nix, &profs, 1);
	if (rc)
		goto exit;

	mtr->policy->ref_cnt--;
	mtr->profile->ref_cnt--;
	TAILQ_REMOVE(fm, mtr, next);
	plt_free(mtr->params.dscp_table);
	plt_free(mtr);

exit:
	return rc;
}

static int
cnxk_nix_mtr_enable(struct rte_eth_dev *eth_dev, uint32_t mtr_id,
		    struct rte_mtr_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	struct cnxk_meter_node *mtr;
	struct roc_nix_rq *rq;
	uint32_t i;
	int rc = 0;

	mtr = nix_mtr_find(dev, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter id is invalid.");
	}

	if (mtr->level != 0)
		return 0;

	for (i = 0; i < mtr->rq_num; i++) {
		rq = &dev->rqs[mtr->rq_id[i]];
		rc |= roc_nix_bpf_ena_dis(nix, mtr->bpf_id, rq, true);
	}

	return rc;
}

static int
cnxk_nix_mtr_disable(struct rte_eth_dev *eth_dev, uint32_t mtr_id,
		     struct rte_mtr_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	struct cnxk_meter_node *mtr;
	struct roc_nix_rq *rq;
	uint32_t i;
	int rc = 0;

	mtr = nix_mtr_find(dev, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter id is invalid.");
	}

	if (mtr->level != 0)
		return 0;

	for (i = 0; i < mtr->rq_num; i++) {
		rq = &dev->rqs[mtr->rq_id[i]];
		rc |= roc_nix_bpf_ena_dis(nix, mtr->bpf_id, rq, false);
	}

	return rc;
}

static int
cnxk_nix_mtr_dscp_table_update(struct rte_eth_dev *eth_dev, uint32_t mtr_id,
			       enum rte_color *dscp_table,
			       struct rte_mtr_error *error)
{
	enum roc_nix_bpf_color nix_dscp_tbl[ROC_NIX_BPF_PRE_COLOR_MAX];
	enum roc_nix_bpf_color color_map[] = {ROC_NIX_BPF_COLOR_GREEN,
					      ROC_NIX_BPF_COLOR_YELLOW,
					      ROC_NIX_BPF_COLOR_RED};
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_bpf_precolor table;
	struct roc_nix *nix = &dev->nix;
	struct cnxk_meter_node *mtr;
	int rc, i;

	mtr = nix_mtr_find(dev, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter object not found");
	}

	if (!dscp_table) {
		for (i = 0; i < ROC_NIX_BPF_PRE_COLOR_MAX; i++)
			nix_dscp_tbl[i] = ROC_NIX_BPF_COLOR_GREEN;
	} else {
		for (i = 0; i < ROC_NIX_BPF_PRE_COLOR_MAX; i++)
			nix_dscp_tbl[i] = color_map[dscp_table[i]];
	}

	table.count = ROC_NIX_BPF_PRE_COLOR_MAX;
	table.mode = ROC_NIX_BPF_PC_MODE_DSCP_OUTER;
	for (i = 0; i < ROC_NIX_BPF_PRE_COLOR_MAX; i++)
		table.color[i] = nix_dscp_tbl[i];

	rc = roc_nix_bpf_pre_color_tbl_setup(nix, mtr->bpf_id,
					     lvl_map[mtr->level], &table);
	if (rc) {
		rte_mtr_error_set(error, rc, RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL, NULL);
		goto exit;
	}

	for (i = 0; i < ROC_NIX_BPF_PRE_COLOR_MAX; i++)
		dev->precolor_tbl[i] = nix_dscp_tbl[i];

exit:
	return rc;
}

static int
cnxk_nix_mtr_stats_update(struct rte_eth_dev *eth_dev, uint32_t mtr_id,
			  uint64_t stats_mask, struct rte_mtr_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_meter_node *mtr;

	if (!stats_mask)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "no bit is set to stats mask");

	mtr = nix_mtr_find(dev, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter object not found");
	}

	mtr->params.stats_mask = stats_mask;
	return 0;
}

static int
cnxk_nix_mtr_stats_read(struct rte_eth_dev *eth_dev, uint32_t mtr_id,
			struct rte_mtr_stats *stats, uint64_t *stats_mask,
			int clear, struct rte_mtr_error *error)
{
	uint8_t yellow_pkt_pass, yellow_octs_pass, yellow_pkt_drop;
	uint8_t green_octs_drop, yellow_octs_drop, red_octs_drop;
	uint8_t green_pkt_pass, green_octs_pass, green_pkt_drop;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint8_t red_pkt_pass, red_octs_pass, red_pkt_drop;
	uint64_t bpf_stats[ROC_NIX_BPF_STATS_MAX] = {0};
	uint64_t mask = NIX_BPF_STATS_MASK_ALL;
	struct roc_nix *nix = &dev->nix;
	struct cnxk_meter_node *mtr;
	int rc;

	if (!stats)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "stats pointer is NULL");

	mtr = nix_mtr_find(dev, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter object not found");
	}

	rc = roc_nix_bpf_stats_read(nix, mtr->bpf_id, mask, lvl_map[mtr->level],
				    bpf_stats);
	if (rc) {
		rte_mtr_error_set(error, rc, RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL, NULL);
		goto exit;
	}

	green_pkt_pass = roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_GREEN_PKT_F_PASS);
	green_octs_pass =
		roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_GREEN_OCTS_F_PASS);
	green_pkt_drop = roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_GREEN_PKT_F_DROP);
	green_octs_drop =
		roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_GREEN_OCTS_F_DROP);
	yellow_pkt_pass =
		roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_YELLOW_PKT_F_PASS);
	yellow_octs_pass =
		roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_YELLOW_OCTS_F_PASS);
	yellow_pkt_drop =
		roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_YELLOW_PKT_F_DROP);
	yellow_octs_drop =
		roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_YELLOW_OCTS_F_DROP);
	red_pkt_pass = roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_RED_PKT_F_PASS);
	red_octs_pass = roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_RED_OCTS_F_PASS);
	red_pkt_drop = roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_RED_PKT_F_DROP);
	red_octs_drop = roc_nix_bpf_stats_to_idx(ROC_NIX_BPF_RED_OCTS_F_DROP);

	if (mtr->params.stats_mask & RTE_MTR_STATS_N_PKTS_GREEN)
		stats->n_pkts[RTE_COLOR_GREEN] = bpf_stats[green_pkt_pass];

	if (mtr->params.stats_mask & RTE_MTR_STATS_N_PKTS_YELLOW)
		stats->n_pkts[RTE_COLOR_YELLOW] = bpf_stats[yellow_pkt_pass];

	if (mtr->params.stats_mask & RTE_MTR_STATS_N_PKTS_RED)
		stats->n_pkts[RTE_COLOR_RED] = bpf_stats[red_pkt_pass];

	if (mtr->params.stats_mask & RTE_MTR_STATS_N_BYTES_GREEN)
		stats->n_bytes[RTE_COLOR_GREEN] = bpf_stats[green_octs_pass];

	if (mtr->params.stats_mask & RTE_MTR_STATS_N_BYTES_YELLOW)
		stats->n_bytes[RTE_COLOR_YELLOW] = bpf_stats[yellow_octs_pass];

	if (mtr->params.stats_mask & RTE_MTR_STATS_N_BYTES_RED)
		stats->n_bytes[RTE_COLOR_RED] = bpf_stats[red_octs_pass];

	if (mtr->params.stats_mask & RTE_MTR_STATS_N_PKTS_DROPPED)
		stats->n_pkts_dropped = bpf_stats[green_pkt_drop] +
					bpf_stats[yellow_pkt_drop] +
					bpf_stats[red_pkt_drop];

	if (mtr->params.stats_mask & RTE_MTR_STATS_N_BYTES_DROPPED)
		stats->n_bytes_dropped = bpf_stats[green_octs_drop] +
					 bpf_stats[yellow_octs_drop] +
					 bpf_stats[red_octs_drop];

	if (stats_mask)
		*stats_mask = mtr->params.stats_mask;

	if (clear) {
		rc = roc_nix_bpf_stats_reset(nix, mtr->bpf_id, mask,
					     lvl_map[mtr->level]);
		if (rc) {
			rte_mtr_error_set(error, rc,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  NULL);
			goto exit;
		}
	}

exit:
	return rc;
}

const struct rte_mtr_ops nix_mtr_ops = {
	.capabilities_get = cnxk_nix_mtr_capabilities_get,
	.meter_profile_add = cnxk_nix_mtr_profile_add,
	.meter_profile_delete = cnxk_nix_mtr_profile_delete,
	.meter_policy_validate = cnxk_nix_mtr_policy_validate,
	.meter_policy_add = cnxk_nix_mtr_policy_add,
	.meter_policy_delete = cnxk_nix_mtr_policy_delete,
	.create = cnxk_nix_mtr_create,
	.destroy = cnxk_nix_mtr_destroy,
	.meter_enable = cnxk_nix_mtr_enable,
	.meter_disable = cnxk_nix_mtr_disable,
	.meter_dscp_table_update = cnxk_nix_mtr_dscp_table_update,
	.stats_update = cnxk_nix_mtr_stats_update,
	.stats_read = cnxk_nix_mtr_stats_read,
};

int
cnxk_nix_mtr_ops_get(struct rte_eth_dev *dev, void *ops)
{
	RTE_SET_USED(dev);

	*(const void **)ops = &nix_mtr_ops;
	return 0;
}

int
nix_mtr_validate(struct rte_eth_dev *eth_dev, uint32_t id)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_mtr_profile_node *profile;
	struct cnxk_mtr_policy_node *policy;
	struct cnxk_meter_node *mtr;

	mtr = nix_mtr_find(dev, id);
	if (mtr == NULL)
		return -EINVAL;

	profile = nix_mtr_profile_find(dev, mtr->params.meter_profile_id);
	if (profile == NULL)
		return -EINVAL;

	policy = nix_mtr_policy_find(dev, mtr->params.meter_policy_id);
	if (policy == NULL)
		return -EINVAL;

	return 0;
}

int
nix_mtr_policy_act_get(struct rte_eth_dev *eth_dev, uint32_t id,
		       struct cnxk_mtr_policy_node **policy_act)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_mtr_policy_node *policy;
	struct cnxk_meter_node *mtr;

	mtr = nix_mtr_find(dev, id);
	if (mtr == NULL)
		return -EINVAL;

	policy = nix_mtr_policy_find(dev, mtr->params.meter_policy_id);
	if (policy == NULL)
		return -EINVAL;

	*policy_act = policy;

	return 0;
}

int
nix_mtr_rq_update(struct rte_eth_dev *eth_dev, uint32_t id, uint32_t queue_num,
		  const uint16_t *queue)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_meter_node *mtr;
	uint32_t i;

	mtr = nix_mtr_find(dev, id);
	if (mtr == NULL)
		return -EINVAL;

	mtr->rq_id = plt_zmalloc(queue_num * sizeof(uint32_t), ROC_ALIGN);
	if (mtr->rq_id == NULL)
		return -ENOMEM;

	mtr->rq_num = queue_num;
	for (i = 0; i < queue_num; i++)
		mtr->rq_id[i] = queue[i];

	return 0;
}

int
nix_mtr_chain_reset(struct rte_eth_dev *eth_dev, uint32_t cur_id)
{
	struct cnxk_meter_node *mtr[ROC_NIX_BPF_LEVEL_MAX] = {0};
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint32_t mtr_id = cur_id;
	int i = 0, j = 0;

	for (i = 0; i < ROC_NIX_BPF_LEVEL_MAX; i++) {
		mtr[i] = nix_mtr_find(dev, mtr_id);
		if (mtr[i])
			mtr_id = mtr[i]->next_id;
	}
	for (i = 0; i < ROC_NIX_BPF_LEVEL_MAX; i++) {
		if (mtr[i]) {
			for (j = 0; j < MAX_PRV_MTR_NODES; j++)
				mtr[i]->prev_id[i] = ROC_NIX_BPF_ID_INVALID;
			mtr[i]->level = ROC_NIX_BPF_LEVEL_IDX_INVALID;
			mtr[i]->next_id = ROC_NIX_BPF_ID_INVALID;
			mtr[i]->is_next = false;
			mtr[i]->prev_cnt = 0;
		}
	}
	return 0;
}

int
nix_mtr_chain_update(struct rte_eth_dev *eth_dev, uint32_t cur_id,
		     uint32_t prev_id, uint32_t next_id)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_meter_node *mtr;

	mtr = nix_mtr_find(dev, cur_id);
	if (mtr == NULL)
		return -EINVAL;

	switch (lvl_map[mtr->level]) {
	case ROC_NIX_BPF_LEVEL_F_LEAF:
		mtr->prev_id[mtr->prev_cnt] = ROC_NIX_BPF_ID_INVALID;
		mtr->next_id = next_id;
		mtr->is_next = true;
		break;
	case ROC_NIX_BPF_LEVEL_F_MID:
		mtr->prev_id[mtr->prev_cnt] = prev_id;
		mtr->next_id = next_id;
		mtr->is_next = true;
		break;
	case ROC_NIX_BPF_LEVEL_F_TOP:
		mtr->prev_id[mtr->prev_cnt] = prev_id;
		mtr->next_id = ROC_NIX_BPF_ID_INVALID;
		mtr->is_next = false;
		break;
	default:
		plt_err("Invalid meter level");
		return -EINVAL;
	}

	return 0;
}

struct cnxk_meter_node *
nix_get_mtr(struct rte_eth_dev *eth_dev, uint32_t id)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_meter_node *mtr;

	mtr = nix_mtr_find(dev, id);
	if (mtr == NULL)
		return NULL;

	return mtr;
}

int
nix_mtr_level_update(struct rte_eth_dev *eth_dev, uint32_t id, uint32_t level)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_meter_node *mtr;

	mtr = nix_mtr_find(dev, id);
	if (mtr == NULL)
		return -EINVAL;

	mtr->level = level;
	return 0;
}

static void
nix_mtr_config_map(struct cnxk_meter_node *mtr, struct roc_nix_bpf_cfg *cfg)
{
	enum roc_nix_bpf_algo alg_map[] = {
		ROC_NIX_BPF_ALGO_NONE, ROC_NIX_BPF_ALGO_2697,
		ROC_NIX_BPF_ALGO_2698, ROC_NIX_BPF_ALGO_4115};
	struct cnxk_mtr_profile_node *profile = mtr->profile;
	struct cnxk_mtr_policy_node *policy = mtr->policy;

	cfg->alg = alg_map[profile->profile.alg];
	cfg->lmode = profile->profile.packet_mode;

	switch (cfg->alg) {
	case ROC_NIX_BPF_ALGO_2697:
		cfg->algo2697.cir = profile->profile.srtcm_rfc2697.cir * 8;
		cfg->algo2697.cbs = profile->profile.srtcm_rfc2697.cbs;
		cfg->algo2697.ebs = profile->profile.srtcm_rfc2697.ebs;
		break;
	case ROC_NIX_BPF_ALGO_2698:
		cfg->algo2698.cir = profile->profile.trtcm_rfc2698.cir * 8;
		cfg->algo2698.pir = profile->profile.trtcm_rfc2698.pir * 8;
		cfg->algo2698.cbs = profile->profile.trtcm_rfc2698.cbs;
		cfg->algo2698.pbs = profile->profile.trtcm_rfc2698.pbs;
		break;
	case ROC_NIX_BPF_ALGO_4115:
		cfg->algo4115.cir = profile->profile.trtcm_rfc4115.cir * 8;
		cfg->algo4115.eir = profile->profile.trtcm_rfc4115.eir * 8;
		cfg->algo4115.cbs = profile->profile.trtcm_rfc4115.cbs;
		cfg->algo4115.ebs = profile->profile.trtcm_rfc4115.ebs;
		break;
	default:
		break;
	}

	cfg->action[ROC_NIX_BPF_COLOR_GREEN] = ROC_NIX_BPF_ACTION_PASS;
	cfg->action[ROC_NIX_BPF_COLOR_YELLOW] = ROC_NIX_BPF_ACTION_PASS;
	cfg->action[ROC_NIX_BPF_COLOR_RED] = ROC_NIX_BPF_ACTION_PASS;

	if (policy->actions[RTE_COLOR_GREEN].action_fate ==
	    RTE_FLOW_ACTION_TYPE_DROP)
		cfg->action[ROC_NIX_BPF_COLOR_GREEN] = ROC_NIX_BPF_ACTION_DROP;

	if (policy->actions[RTE_COLOR_YELLOW].action_fate ==
	    RTE_FLOW_ACTION_TYPE_DROP)
		cfg->action[ROC_NIX_BPF_COLOR_YELLOW] = ROC_NIX_BPF_ACTION_DROP;

	if (policy->actions[RTE_COLOR_RED].action_fate ==
	    RTE_FLOW_ACTION_TYPE_DROP)
		cfg->action[ROC_NIX_BPF_COLOR_RED] = ROC_NIX_BPF_ACTION_DROP;
}

static void
nix_dscp_table_map(struct cnxk_meter_node *mtr,
		   struct roc_nix_bpf_precolor *tbl)
{
	enum roc_nix_bpf_color color_map[] = {ROC_NIX_BPF_COLOR_GREEN,
					      ROC_NIX_BPF_COLOR_YELLOW,
					      ROC_NIX_BPF_COLOR_RED};
	int i;

	tbl->count = ROC_NIX_BPF_PRE_COLOR_MAX;
	tbl->mode = ROC_NIX_BPF_PC_MODE_DSCP_OUTER;

	for (i = 0; i < ROC_NIX_BPF_PRE_COLOR_MAX; i++)
		tbl->color[i] = ROC_NIX_BPF_COLOR_GREEN;

	if (mtr->params.dscp_table) {
		for (i = 0; i < ROC_NIX_BPF_PRE_COLOR_MAX; i++)
			tbl->color[i] = color_map[mtr->params.dscp_table[i]];
	}
}

int
nix_mtr_destroy(struct rte_eth_dev *eth_dev, uint32_t id,
		struct rte_mtr_error *error)
{
	return cnxk_nix_mtr_destroy(eth_dev, id, error);
}

int
nix_mtr_connect(struct rte_eth_dev *eth_dev, uint32_t id)
{
	enum roc_nix_bpf_level_flag lvl_flag = ROC_NIX_BPF_LEVEL_IDX_INVALID;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_meter_node *base_mtr, *next_mtr;
	struct roc_nix *nix = &dev->nix;
	uint32_t cur_mtr_id = id;
	int rc, i;

	for (i = 0; i < ROC_NIX_BPF_LEVEL_MAX; i++) {
		base_mtr = nix_mtr_find(dev, cur_mtr_id);
		if (base_mtr) {
			lvl_flag = lvl_map[base_mtr->level];
			if (base_mtr->is_next) {
				next_mtr = nix_mtr_find(dev, base_mtr->next_id);
				if (next_mtr) {
					if (!base_mtr->is_used) {
						rc = roc_nix_bpf_connect(nix,
							lvl_flag,
							base_mtr->bpf_id,
							next_mtr->bpf_id);
						if (rc)
							return rc;
					}
				}
				cur_mtr_id = base_mtr->next_id;
			}
		}
	}
	return 0;
}

int
nix_mtr_configure(struct rte_eth_dev *eth_dev, uint32_t id)
{
	struct cnxk_meter_node *mtr[ROC_NIX_BPF_LEVEL_MAX] = {0};
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_bpf_objs profs[ROC_NIX_BPF_LEVEL_MAX];
	uint8_t idx0 = ROC_NIX_BPF_LEVEL_IDX_INVALID;
	uint8_t idx1 = ROC_NIX_BPF_LEVEL_IDX_INVALID;
	uint8_t idx2 = ROC_NIX_BPF_LEVEL_IDX_INVALID;
	uint16_t per_lvl_cnt[ROC_NIX_BPF_LEVEL_MAX];
	int num_mtr[ROC_NIX_BPF_LEVEL_MAX] = {0};
	struct roc_nix *nix = &dev->nix;
	struct roc_nix_bpf_precolor tbl;
	struct roc_nix_bpf_cfg cfg;
	struct roc_nix_rq *rq;
	uint8_t lvl_mask;
	uint32_t i;
	uint32_t j;
	int rc;

	mtr[0] = nix_mtr_find(dev, id);
	if (mtr[0] == NULL)
		return -EINVAL;

	num_mtr[0] = 1;
	idx0 = roc_nix_bpf_level_to_idx(lvl_map[mtr[0]->level]);
	if (idx0 == ROC_NIX_BPF_LEVEL_IDX_INVALID)
		return -EINVAL;

	lvl_mask = ROC_NIX_BPF_LEVEL_F_LEAF;
	if (mtr[0]->is_used)
		per_lvl_cnt[idx0] = 0;
	else
		per_lvl_cnt[idx0] = 1;

	if (mtr[0]->is_next) {
		mtr[1] = nix_mtr_find(dev, mtr[0]->next_id);
		if (mtr[1] == NULL)
			return -EINVAL;
		num_mtr[1] = 1;
		idx1 = roc_nix_bpf_level_to_idx(lvl_map[mtr[1]->level]);
		if (idx1 == ROC_NIX_BPF_LEVEL_IDX_INVALID)
			return -EINVAL;

		lvl_mask |= ROC_NIX_BPF_LEVEL_F_MID;
		if (mtr[1]->is_used)
			per_lvl_cnt[idx1] = 0;
		else
			per_lvl_cnt[idx1] = 1;
	}

	if (mtr[1] && mtr[1]->is_next) {
		mtr[2] = nix_mtr_find(dev, mtr[1]->next_id);
		if (mtr[2] == NULL)
			return -EINVAL;

		num_mtr[2] = 1;
		idx2 = roc_nix_bpf_level_to_idx(lvl_map[mtr[2]->level]);
		if (idx2 == ROC_NIX_BPF_LEVEL_IDX_INVALID)
			return -EINVAL;

		lvl_mask |= ROC_NIX_BPF_LEVEL_F_TOP;
		if (mtr[2]->is_used)
			per_lvl_cnt[idx2] = 0;
		else
			per_lvl_cnt[idx2] = 1;
	}

	rc = roc_nix_bpf_alloc(nix, lvl_mask, per_lvl_cnt, profs);
	if (rc)
		return rc;
	if (mtr[0]->bpf_id == ROC_NIX_BPF_ID_INVALID)
		mtr[0]->bpf_id = profs[idx0].ids[0];

	if (num_mtr[0])
		if (mtr[0]->is_next && idx1 != ROC_NIX_BPF_LEVEL_IDX_INVALID)
			if (mtr[1]->bpf_id == ROC_NIX_BPF_ID_INVALID)
				mtr[1]->bpf_id = profs[idx1].ids[0];

	if (num_mtr[1])
		if (mtr[1]->is_next && idx2 != ROC_NIX_BPF_LEVEL_IDX_INVALID)
			if (mtr[2]->bpf_id == ROC_NIX_BPF_ID_INVALID)
				mtr[2]->bpf_id = profs[idx2].ids[0];

	for (i = 0; i < ROC_NIX_BPF_LEVEL_MAX; i++) {
		if (num_mtr[i]) {
			if (!mtr[i]->is_used) {
				memset(&cfg, 0, sizeof(struct roc_nix_bpf_cfg));
				nix_mtr_config_map(mtr[i], &cfg);
				rc = roc_nix_bpf_config(nix, mtr[i]->bpf_id,
							lvl_map[mtr[i]->level],
							&cfg);

				memset(&tbl, 0,
				       sizeof(struct roc_nix_bpf_precolor));
				nix_dscp_table_map(mtr[i], &tbl);
				rc = roc_nix_bpf_pre_color_tbl_setup(nix,
					mtr[i]->bpf_id, lvl_map[mtr[i]->level],
					&tbl);

				if (mtr[i]->params.meter_enable) {
					for (j = 0; j < mtr[i]->rq_num; j++) {
						rq = &dev->rqs[mtr[i]->rq_id
								       [j]];
						rc = roc_nix_bpf_ena_dis(nix,
							mtr[i]->bpf_id, rq,
							true);
					}
				}
			}
		}
	}

	return rc;
}

int
nix_mtr_color_action_validate(struct rte_eth_dev *eth_dev, uint32_t id,
			      uint32_t *prev_id, uint32_t *next_id,
			      struct cnxk_mtr_policy_node *policy,
			      int *tree_level)
{
	uint32_t action_fate_red = policy->actions[RTE_COLOR_RED].action_fate;
	uint32_t action_fate_green =
		policy->actions[RTE_COLOR_GREEN].action_fate;
	uint32_t action_fate_yellow =
		policy->actions[RTE_COLOR_YELLOW].action_fate;
	uint32_t cur_mtr_id = *next_id;
	uint32_t next_mtr_id = 0xffff;
	uint32_t prev_mtr_id = 0xffff;
	struct cnxk_meter_node *mtr;

	if (action_fate_green == RTE_FLOW_ACTION_TYPE_METER)
		next_mtr_id = policy->actions[RTE_COLOR_GREEN].mtr_id;

	if (action_fate_yellow == RTE_FLOW_ACTION_TYPE_METER)
		next_mtr_id = policy->actions[RTE_COLOR_YELLOW].mtr_id;

	if (action_fate_red == RTE_FLOW_ACTION_TYPE_METER)
		next_mtr_id = policy->actions[RTE_COLOR_RED].mtr_id;

	if (next_mtr_id != 0xffff) {
		switch (*tree_level) {
		case 0:
			mtr = nix_get_mtr(eth_dev, cur_mtr_id);
			if (mtr->level == ROC_NIX_BPF_LEVEL_IDX_INVALID) {
				nix_mtr_level_update(eth_dev, cur_mtr_id, 0);
				nix_mtr_chain_update(eth_dev, cur_mtr_id, -1,
						     next_mtr_id);
			} else {
				if (mtr->level == 0)
					mtr->is_used = true;
				else
					return -EINVAL;
			}
			(*tree_level)++;
			*next_id = next_mtr_id;
			break;
		case 1:
			mtr = nix_get_mtr(eth_dev, cur_mtr_id);
			if (mtr->level == ROC_NIX_BPF_LEVEL_IDX_INVALID) {
				nix_mtr_level_update(eth_dev, cur_mtr_id, 1);
				prev_mtr_id = id;
				nix_mtr_chain_update(eth_dev, cur_mtr_id,
						     prev_mtr_id, next_mtr_id);
			} else {
				if (mtr->level == 1) {
					mtr->prev_cnt++;
					prev_mtr_id = id;
					nix_mtr_chain_update(eth_dev,
						cur_mtr_id, prev_mtr_id,
						next_mtr_id);

					mtr->is_used = true;
				} else {
					return -EINVAL;
				}
			}
			(*tree_level)++;
			*next_id = next_mtr_id;
			*prev_id = cur_mtr_id;
			break;
		case 2:
			nix_mtr_chain_reset(eth_dev, id);
			return -EINVAL;
		}
	} else {
		switch (*tree_level) {
		case 0:
			mtr = nix_get_mtr(eth_dev, cur_mtr_id);
			if (mtr->level == ROC_NIX_BPF_LEVEL_IDX_INVALID) {
				nix_mtr_level_update(eth_dev, cur_mtr_id, 0);
			} else {
				if (mtr->level == 0)
					mtr->is_used = true;
				else
					return -EINVAL;
			}
			break;
		case 1:
			mtr = nix_get_mtr(eth_dev, cur_mtr_id);
			if (mtr->level == ROC_NIX_BPF_LEVEL_IDX_INVALID) {
				nix_mtr_level_update(eth_dev, cur_mtr_id, 1);
				prev_mtr_id = id;
				nix_mtr_chain_update(eth_dev, cur_mtr_id,
						     prev_mtr_id, -1);
			} else {
				if (mtr->level == 1) {
					mtr->prev_cnt++;
					prev_mtr_id = id;
					nix_mtr_chain_update(eth_dev,
							     cur_mtr_id,
							     prev_mtr_id, -1);
					mtr->is_used = true;
				} else {
					return -EINVAL;
				}
			}
			break;
		case 2:
			mtr = nix_get_mtr(eth_dev, cur_mtr_id);
			if (mtr->level == ROC_NIX_BPF_LEVEL_IDX_INVALID) {
				nix_mtr_level_update(eth_dev, cur_mtr_id, 2);
				prev_mtr_id = *prev_id;
				nix_mtr_chain_update(eth_dev, cur_mtr_id,
						     prev_mtr_id, -1);
			} else {
				if (mtr->level == 2) {
					mtr->prev_cnt++;
					prev_mtr_id = *prev_id;
					nix_mtr_chain_update(eth_dev,
							     cur_mtr_id,
							     prev_mtr_id, -1);
					mtr->is_used = true;
				} else {
					return -EINVAL;
				}
			}
			break;
		}
		*next_id = 0xffff;
	}

	return 0;
}

int
nix_mtr_capabilities_init(struct rte_eth_dev *eth_dev)
{
	struct rte_mtr_capabilities capa;
	struct rte_mtr_error error;

	return cnxk_nix_mtr_capabilities_get(eth_dev, &capa, &error);
}
