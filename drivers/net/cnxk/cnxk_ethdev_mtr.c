/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"
#include <rte_mtr_driver.h>

#define NIX_MTR_COUNT_MAX      73 /* 64(leaf) + 8(mid) + 1(top) */
#define NIX_MTR_COUNT_PER_FLOW 3  /* 1(leaf) + 1(mid) + 1(top) */

static struct rte_mtr_capabilities mtr_capa = {
	.n_max = NIX_MTR_COUNT_MAX,
	.n_shared_max = NIX_MTR_COUNT_PER_FLOW,
	/* .identical = , */
	.shared_identical = true,
	/* .shared_n_flows_per_mtr_max = ,*/
	.chaining_n_mtrs_per_flow_max = NIX_MTR_COUNT_PER_FLOW,
	.chaining_use_prev_mtr_color_supported = true,
	.chaining_use_prev_mtr_color_enforced = true,
	.meter_srtcm_rfc2697_n_max = NIX_MTR_COUNT_MAX,
	.meter_trtcm_rfc2698_n_max = NIX_MTR_COUNT_MAX,
	.meter_trtcm_rfc4115_n_max = NIX_MTR_COUNT_MAX,
	.meter_rate_max = NIX_BPF_RATE_MAX / 8, /* Bytes per second */
	.meter_policy_n_max = NIX_MTR_COUNT_MAX,
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
	RTE_SET_USED(dev);

	if (!capa)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "NULL input parameter");
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
cnxk_nix_mtr_policy_validate(struct rte_eth_dev *dev,
			     struct rte_mtr_meter_policy_params *policy,
			     struct rte_mtr_error *error)
{
	static const char *const action_color[] = {"Green", "Yellow", "Red"};
	bool supported[RTE_COLORS] = {false, false, false};
	const struct rte_flow_action *action;
	char message[1024];
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

				if (!supported[i]) {
					sprintf(message,
						"%s action is not valid",
						action_color[i]);
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY, NULL,
					  message);
				}
			}
		} else {
			sprintf(message, "%s action is null", action_color[i]);
			return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY, NULL,
				message);
		}
	}

	return 0;
}

const struct rte_mtr_ops nix_mtr_ops = {
	.capabilities_get = cnxk_nix_mtr_capabilities_get,
	.meter_profile_add = cnxk_nix_mtr_profile_add,
	.meter_profile_delete = cnxk_nix_mtr_profile_delete,
	.meter_policy_validate = cnxk_nix_mtr_policy_validate,
};

int
cnxk_nix_mtr_ops_get(struct rte_eth_dev *dev, void *ops)
{
	RTE_SET_USED(dev);

	*(const void **)ops = &nix_mtr_ops;
	return 0;
}
