/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include <rte_mtr_driver.h>
#include <bus_pci_driver.h>
#include <rte_malloc.h>
#include <rte_alarm.h>

#include "nfp_common.h"
#include "nfp_mtr.h"
#include "nfp_logs.h"
#include "flower/nfp_flower.h"
#include "flower/nfp_flower_cmsg.h"
#include "flower/nfp_flower_representor.h"

#define NFP_FL_QOS_PPS          RTE_BIT32(15)
#define NFP_FL_QOS_METER        RTE_BIT32(10)
#define NFP_FL_QOS_RFC2697      RTE_BIT32(0)

/* Alarm timeout value in microseconds */
#define NFP_METER_STATS_INTERVAL 1000000  /* 1 second */

/**
 * Callback to get MTR capabilities.
 *
 * @param[in] dev
 *   Pointer to the device (unused).
 * @param[out] cap
 *   Pointer to the meter object capabilities.
 * @param[out] error
 *   Pointer to the error (unused).
 *
 * @returns
 *   0 on success, a negative value otherwise and rte_errno is set.
 */
static int
nfp_mtr_cap_get(struct rte_eth_dev *dev __rte_unused,
		struct rte_mtr_capabilities *cap,
		struct rte_mtr_error *error)
{
	if (cap == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL, "NULL pointer for capabilitie argument");
	}

	memset(cap, 0, sizeof(struct rte_mtr_capabilities));

	cap->n_max                               = NFP_MAX_MTR_CNT;
	cap->n_shared_max                        = NFP_MAX_MTR_CNT;
	cap->identical                           = 1;
	cap->shared_identical                    = 1;
	cap->chaining_n_mtrs_per_flow_max        = 1;
	cap->meter_srtcm_rfc2697_n_max           = NFP_MAX_MTR_CNT;
	cap->meter_trtcm_rfc2698_n_max           = NFP_MAX_MTR_CNT;
	cap->meter_rate_max                      = UINT64_MAX;
	cap->meter_policy_n_max                  = NFP_MAX_POLICY_CNT;
	cap->srtcm_rfc2697_byte_mode_supported   = 1;
	cap->srtcm_rfc2697_packet_mode_supported = 1;
	cap->trtcm_rfc2698_byte_mode_supported   = 1;
	cap->trtcm_rfc2698_packet_mode_supported = 1;
	cap->stats_mask = RTE_MTR_STATS_N_PKTS_GREEN |
			RTE_MTR_STATS_N_PKTS_DROPPED |
			RTE_MTR_STATS_N_BYTES_GREEN |
			RTE_MTR_STATS_N_BYTES_DROPPED;

	return 0;
}

static int
nfp_mtr_profile_validate(uint32_t mtr_profile_id,
		struct rte_mtr_meter_profile *profile,
		struct rte_mtr_error *error)
{
	/* Profile must not be NULL. */
	if (profile == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE,
				NULL, "Meter profile is null");
	}

	/* Meter profile ID must be valid. */
	if (mtr_profile_id >= NFP_MAX_PROFILE_CNT) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
				NULL, "Meter profile id not valid");
	}

	switch (profile->alg) {
	case RTE_MTR_SRTCM_RFC2697:
	case RTE_MTR_TRTCM_RFC2698:
		return 0;
	case RTE_MTR_TRTCM_RFC4115:
		return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE,
				NULL, "Unsupported metering algorithm");
	default:
		return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE,
				NULL, "Unknown metering algorithm");
	}
}

static void
nfp_mtr_profile_config_2698(uint32_t mtr_profile_id,
		struct rte_mtr_meter_profile *profile,
		struct nfp_profile_conf *conf)
{
	if (profile->packet_mode != 0)
		conf->head.flags_opts |= rte_cpu_to_be_32(NFP_FL_QOS_PPS);

	conf->head.flags_opts |= rte_cpu_to_be_32(NFP_FL_QOS_METER);
	conf->head.profile_id = rte_cpu_to_be_32(mtr_profile_id);

	conf->bkt_tkn_c = rte_cpu_to_be_32(profile->trtcm_rfc2698.cbs);
	conf->bkt_tkn_p = rte_cpu_to_be_32(profile->trtcm_rfc2698.pbs);
	conf->cbs = rte_cpu_to_be_32(profile->trtcm_rfc2698.cbs);
	conf->pbs = rte_cpu_to_be_32(profile->trtcm_rfc2698.pbs);
	conf->cir = rte_cpu_to_be_32(profile->trtcm_rfc2698.cir);
	conf->pir = rte_cpu_to_be_32(profile->trtcm_rfc2698.pir);
}

static void
nfp_mtr_profile_config_2697(uint32_t mtr_profile_id,
		struct rte_mtr_meter_profile *profile,
		struct nfp_profile_conf *conf)
{
	if (profile->packet_mode != 0)
		conf->head.flags_opts |= rte_cpu_to_be_32(NFP_FL_QOS_PPS);

	conf->head.flags_opts |= rte_cpu_to_be_32(NFP_FL_QOS_RFC2697);
	conf->head.flags_opts |= rte_cpu_to_be_32(NFP_FL_QOS_METER);
	conf->head.profile_id = rte_cpu_to_be_32(mtr_profile_id);

	conf->bkt_tkn_c = rte_cpu_to_be_32(profile->srtcm_rfc2697.cbs);
	conf->bkt_tkn_p = rte_cpu_to_be_32(profile->srtcm_rfc2697.ebs);
	conf->cbs = rte_cpu_to_be_32(profile->srtcm_rfc2697.cbs);
	conf->pbs = rte_cpu_to_be_32(profile->srtcm_rfc2697.ebs);
	conf->cir = rte_cpu_to_be_32(profile->srtcm_rfc2697.cir);
	conf->pir = rte_cpu_to_be_32(profile->srtcm_rfc2697.cir);
}

static int
nfp_mtr_profile_conf_mod(uint32_t mtr_profile_id,
		struct rte_mtr_meter_profile *profile,
		struct nfp_profile_conf *conf)
{
	switch (profile->alg) {
	case RTE_MTR_SRTCM_RFC2697:
		nfp_mtr_profile_config_2697(mtr_profile_id, profile, conf);
		return 0;
	case RTE_MTR_TRTCM_RFC2698:
		nfp_mtr_profile_config_2698(mtr_profile_id, profile, conf);
		return 0;
	case RTE_MTR_TRTCM_RFC4115:
		return -ENOTSUP;
	default:
		return -EINVAL;
	}
}

static int
nfp_mtr_profile_conf_insert(uint32_t mtr_profile_id,
		struct rte_mtr_meter_profile *profile,
		struct nfp_mtr_profile *mtr_profile)
{
	mtr_profile->profile_id = mtr_profile_id;
	mtr_profile->in_use = false;

	return nfp_mtr_profile_conf_mod(mtr_profile_id, profile,
			&mtr_profile->conf);
}

static struct nfp_mtr_profile *
nfp_mtr_profile_search(struct nfp_mtr_priv *priv, uint32_t mtr_profile_id)
{
	struct nfp_mtr_profile *mtr_profile;

	LIST_FOREACH(mtr_profile, &priv->profiles, next)
		if (mtr_profile->profile_id == mtr_profile_id)
			break;

	return mtr_profile;
}

static int
nfp_mtr_profile_insert(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mtr_meter_profile *profile,
		uint32_t mtr_profile_id,
		struct rte_mtr_error *error)
{
	int ret;
	struct nfp_mtr_priv *priv;
	struct nfp_mtr_profile *mtr_profile;

	priv = app_fw_flower->mtr_priv;

	/* Meter profile memory allocation. */
	mtr_profile = rte_zmalloc(NULL, sizeof(struct nfp_mtr_profile), 0);
	if (mtr_profile == NULL) {
		return -rte_mtr_error_set(error, ENOMEM,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL, "Meter profile alloc failed");
	}

	ret = nfp_mtr_profile_conf_insert(mtr_profile_id,
			profile, mtr_profile);
	if (ret != 0) {
		rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL, "Insert profile config failed");
		goto free_profile;
	}

	ret = nfp_flower_cmsg_qos_add(app_fw_flower, &mtr_profile->conf);
	if (ret != 0) {
		rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL, "Add meter to firmware failed");
		goto free_profile;
	}

	/* Insert profile into profile list */
	LIST_INSERT_HEAD(&priv->profiles, mtr_profile, next);

	return 0;

free_profile:
	rte_free(mtr_profile);

	return ret;
}

static int
nfp_mtr_profile_mod(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mtr_meter_profile *profile,
		struct nfp_mtr_profile *mtr_profile,
		struct rte_mtr_error *error)
{
	int ret;
	struct nfp_profile_conf old_conf;

	/* Get the old profile config */
	rte_memcpy(&old_conf, &mtr_profile->conf, sizeof(old_conf));

	memset(&mtr_profile->conf, 0, sizeof(struct nfp_profile_conf));

	ret = nfp_mtr_profile_conf_mod(mtr_profile->profile_id,
			profile, &mtr_profile->conf);
	if (ret != 0) {
		rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL, "Mod profile config failed");
		goto rollback;
	}

	ret = nfp_flower_cmsg_qos_add(app_fw_flower, &mtr_profile->conf);
	if (ret != 0) {
		rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL, "Mod meter to firmware failed");
		goto rollback;
	}

	return 0;

rollback:
	rte_memcpy(&mtr_profile->conf, &old_conf, sizeof(old_conf));

	return ret;
}

/**
 * Callback to add MTR profile.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_profile_id
 *   Meter profile id.
 * @param[in] profile
 *   Pointer to meter profile detail.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative value otherwise and rte_errno is set.
 */
static int
nfp_mtr_profile_add(struct rte_eth_dev *dev,
		uint32_t mtr_profile_id,
		struct rte_mtr_meter_profile *profile,
		struct rte_mtr_error *error)
{
	int ret;
	struct nfp_mtr_priv *priv;
	struct nfp_mtr_profile *mtr_profile;
	struct nfp_app_fw_flower *app_fw_flower;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	app_fw_flower = representor->app_fw_flower;
	priv = app_fw_flower->mtr_priv;

	/* Check input params */
	ret = nfp_mtr_profile_validate(mtr_profile_id, profile, error);
	if (ret != 0)
		return ret;

	/* Check if mtr profile id exist */
	mtr_profile = nfp_mtr_profile_search(priv, mtr_profile_id);
	if (mtr_profile == NULL) {
		ret = nfp_mtr_profile_insert(app_fw_flower,
				profile, mtr_profile_id, error);
	} else {
		ret = nfp_mtr_profile_mod(app_fw_flower,
				profile, mtr_profile, error);
	}

	return ret;
}

/**
 * Callback to delete MTR profile.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_profile_id
 *   Meter profile id.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative value otherwise and rte_errno is set.
 */
static int
nfp_mtr_profile_delete(struct rte_eth_dev *dev,
		uint32_t mtr_profile_id,
		struct rte_mtr_error *error)
{
	int ret;
	struct nfp_mtr_priv *priv;
	struct nfp_mtr_profile *mtr_profile;
	struct nfp_app_fw_flower *app_fw_flower;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	app_fw_flower = representor->app_fw_flower;
	priv = app_fw_flower->mtr_priv;

	/* Check if mtr profile id exist */
	mtr_profile = nfp_mtr_profile_search(priv, mtr_profile_id);
	if (mtr_profile == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
				NULL, "Request meter profile not exist");
	}

	if (mtr_profile->in_use) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE,
				NULL, "Request meter profile is been used");
	}

	ret = nfp_flower_cmsg_qos_delete(app_fw_flower, &mtr_profile->conf);
	if (ret != 0) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL, "Delete meter from firmware failed");
	}

	/* Remove profile from profile list */
	LIST_REMOVE(mtr_profile, next);
	rte_free(mtr_profile);

	return 0;
}

static struct nfp_mtr_policy *
nfp_mtr_policy_search(struct nfp_mtr_priv *priv, uint32_t mtr_policy_id)
{
	struct nfp_mtr_policy *mtr_policy;

	LIST_FOREACH(mtr_policy, &priv->policies, next)
		if (mtr_policy->policy_id == mtr_policy_id)
			break;

	return mtr_policy;
}

static int
nfp_mtr_policy_validate(uint32_t mtr_policy_id,
		struct rte_mtr_meter_policy_params *policy,
		struct rte_mtr_error *error)
{
	const struct rte_flow_action *action;

	/* Policy must not be NULL */
	if (policy == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL, "Meter policy is null.");
	}

	/* Meter policy ID must be valid. */
	if (mtr_policy_id >= NFP_MAX_POLICY_CNT) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
				NULL, "Meter policy id not valid.");
	}

	/* Check green action
	 * Actions equal NULL means end action
	 */
	action = policy->actions[RTE_COLOR_GREEN];
	if (action != NULL && action->type != RTE_FLOW_ACTION_TYPE_VOID) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL, "Green action must be void or end");
	}

	/* Check yellow action
	 * Actions equal NULL means end action
	 */
	action = policy->actions[RTE_COLOR_YELLOW];
	if (action != NULL && action->type != RTE_FLOW_ACTION_TYPE_VOID) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL, "Yellow action must be void or end");
	}

	/* Check red action */
	action = policy->actions[RTE_COLOR_RED];
	if (action == NULL || action->type != RTE_FLOW_ACTION_TYPE_DROP) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL, "Red action must be drop");
	}

	return 0;
}

/**
 * Callback to add MTR policy.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_policy_id
 *   Meter policy id.
 * @param[in] policy
 *   Pointer to meter policy detail.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative value otherwise and rte_errno is set.
 */
static int
nfp_mtr_policy_add(struct rte_eth_dev *dev,
		uint32_t mtr_policy_id,
		struct rte_mtr_meter_policy_params *policy,
		struct rte_mtr_error *error)
{
	int ret;
	struct nfp_mtr_priv *priv;
	struct nfp_mtr_policy *mtr_policy;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	priv = representor->app_fw_flower->mtr_priv;

	/* Check if mtr policy id exist */
	mtr_policy = nfp_mtr_policy_search(priv, mtr_policy_id);
	if (mtr_policy != NULL) {
		return -rte_mtr_error_set(error, EEXIST,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
				NULL, "Meter policy already exist");
	}

	/* Check input params */
	ret = nfp_mtr_policy_validate(mtr_policy_id, policy, error);
	if (ret != 0)
		return ret;

	/* Meter policy memory alloc */
	mtr_policy = rte_zmalloc(NULL, sizeof(struct nfp_mtr_policy), 0);
	if (mtr_policy == NULL) {
		return -rte_mtr_error_set(error, ENOMEM,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL, "Meter policy alloc failed");
	}

	mtr_policy->policy_id = mtr_policy_id;
	rte_memcpy(&mtr_policy->policy, policy,
			sizeof(struct rte_mtr_meter_policy_params));

	/* Insert policy into policy list */
	LIST_INSERT_HEAD(&priv->policies, mtr_policy, next);

	return 0;
}

/**
 * Callback to delete MTR policy.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_policy_id
 *   Meter policy id.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative value otherwise and rte_errno is set.
 */
static int
nfp_mtr_policy_delete(struct rte_eth_dev *dev,
		uint32_t mtr_policy_id,
		struct rte_mtr_error *error)
{
	struct nfp_mtr_priv *priv;
	struct nfp_mtr_policy *mtr_policy;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	priv = representor->app_fw_flower->mtr_priv;

	/* Check if mtr policy id exist */
	mtr_policy = nfp_mtr_policy_search(priv, mtr_policy_id);
	if (mtr_policy == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
				NULL, "Request meter policy not exist");
	}

	if (mtr_policy->ref_cnt > 0) {
		return -rte_mtr_error_set(error, EBUSY,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL, "Request mtr policy is been used");
	}

	/* Remove profile from profile list */
	LIST_REMOVE(mtr_policy, next);
	rte_free(mtr_policy);

	return 0;
}

struct nfp_mtr *
nfp_mtr_find_by_mtr_id(struct nfp_mtr_priv *priv, uint32_t mtr_id)
{
	struct nfp_mtr *mtr;

	LIST_FOREACH(mtr, &priv->mtrs, next)
		if (mtr->mtr_id == mtr_id)
			break;

	return mtr;
}

struct nfp_mtr *
nfp_mtr_find_by_profile_id(struct nfp_mtr_priv *priv, uint32_t profile_id)
{
	struct nfp_mtr *mtr;

	LIST_FOREACH(mtr, &priv->mtrs, next)
		if (mtr->mtr_profile->profile_id == profile_id)
			break;

	return mtr;
}

static int
nfp_mtr_stats_mask_validate(uint64_t stats_mask, struct rte_mtr_error *error)
{
	if ((stats_mask & RTE_MTR_STATS_N_PKTS_YELLOW) != 0) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_PARAMS,
				NULL, "RTE_MTR_STATS_N_PKTS_YELLOW not support");
	}

	if ((stats_mask & RTE_MTR_STATS_N_PKTS_RED) != 0) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_PARAMS,
				NULL, "RTE_MTR_STATS_N_PKTS_RED not support");
	}

	if ((stats_mask & RTE_MTR_STATS_N_BYTES_YELLOW) != 0) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_PARAMS,
				NULL, "RTE_MTR_STATS_N_BYTES_YELLOW not support");
	}

	if ((stats_mask & RTE_MTR_STATS_N_BYTES_RED) != 0) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_PARAMS,
				NULL, "RTE_MTR_STATS_N_BYTES_RED not support");
	}

	return 0;
}

static int
nfp_mtr_validate(uint32_t meter_id,
		struct rte_mtr_params *params,
		struct rte_mtr_error *error)
{
	/* Params must not be NULL */
	if (params == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_PARAMS,
				NULL, "Meter params is null.");
	}

	/* Meter policy ID must be valid. */
	if (meter_id >= NFP_MAX_MTR_CNT) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Meter id not valid.");
	}

	if (params->use_prev_mtr_color != 0) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_PARAMS,
				NULL, "Feature use_prev_mtr_color not support");
	}

	return nfp_mtr_stats_mask_validate(params->stats_mask, error);
}

static void
nfp_mtr_config(uint32_t mtr_id,
		int shared,
		struct rte_mtr_params *params,
		struct nfp_mtr_profile *mtr_profile,
		struct nfp_mtr_policy *mtr_policy,
		struct nfp_mtr *mtr)
{
	mtr->mtr_id = mtr_id;

	if (shared != 0)
		mtr->shared = true;

	if (params->meter_enable != 0)
		mtr->enable = true;

	mtr->mtr_profile = mtr_profile;
	mtr->mtr_policy = mtr_policy;
	mtr->stats_mask = params->stats_mask;
}

/**
 * Create meter rules.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_id
 *   Meter id.
 * @param[in] params
 *   Pointer to rte meter parameters.
 * @param[in] shared
 *   Meter shared with other flow or not.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative value otherwise and rte_errno is set.
 */
static int
nfp_mtr_create(struct rte_eth_dev *dev,
		uint32_t mtr_id,
		struct rte_mtr_params *params,
		int shared,
		struct rte_mtr_error *error)
{
	int ret;
	struct nfp_mtr *mtr;
	struct nfp_mtr_priv *priv;
	struct nfp_mtr_policy *mtr_policy;
	struct nfp_mtr_profile *mtr_profile;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	priv = representor->app_fw_flower->mtr_priv;

	/* Check if meter id exist */
	mtr = nfp_mtr_find_by_mtr_id(priv, mtr_id);
	if (mtr != NULL) {
		return -rte_mtr_error_set(error, EEXIST,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Meter already exist");
	}

	/* Check input meter params */
	ret = nfp_mtr_validate(mtr_id, params, error);
	if (ret != 0)
		return ret;

	mtr_profile = nfp_mtr_profile_search(priv, params->meter_profile_id);
	if (mtr_profile == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
				NULL, "Request meter profile not exist");
	}

	if (mtr_profile->in_use) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
				NULL, "Request meter profile is been used");
	}

	mtr_policy = nfp_mtr_policy_search(priv, params->meter_policy_id);
	if (mtr_policy == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
				NULL, "Request meter policy not exist");
	}

	/* Meter param memory alloc */
	mtr = rte_zmalloc(NULL, sizeof(struct nfp_mtr), 0);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, ENOMEM,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL, "Meter param alloc failed");
	}

	nfp_mtr_config(mtr_id, shared, params, mtr_profile, mtr_policy, mtr);

	/* Update profile/policy status */
	mtr->mtr_policy->ref_cnt++;
	mtr->mtr_profile->in_use = true;

	/* Insert mtr into mtr list */
	LIST_INSERT_HEAD(&priv->mtrs, mtr, next);

	return 0;
}

/**
 * Destroy meter rules.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_id
 *   Meter id.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative value otherwise and rte_errno is set.
 */
static int
nfp_mtr_destroy(struct rte_eth_dev *dev,
		uint32_t mtr_id,
		struct rte_mtr_error *error)
{
	struct nfp_mtr *mtr;
	struct nfp_mtr_priv *priv;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	priv = representor->app_fw_flower->mtr_priv;

	/* Check if meter id exist */
	mtr = nfp_mtr_find_by_mtr_id(priv, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Request meter not exist");
	}

	if (mtr->ref_cnt > 0) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Meter object is being used");
	}

	/* Update profile/policy status */
	mtr->mtr_policy->ref_cnt--;
	mtr->mtr_profile->in_use = false;

	/* Remove mtr from mtr list */
	LIST_REMOVE(mtr, next);
	rte_free(mtr);

	return 0;
}

/**
 * Enable meter object.
 *
 * @param[in] dev
 *   Pointer to the device.
 * @param[in] mtr_id
 *   Id of the meter.
 * @param[out] error
 *   Pointer to the error.
 *
 * @returns
 *   0 in success, negative value otherwise and rte_errno is set..
 */
static int
nfp_mtr_enable(struct rte_eth_dev *dev,
		uint32_t mtr_id,
		struct rte_mtr_error *error)
{
	struct nfp_mtr *mtr;
	struct nfp_mtr_priv *priv;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	priv = representor->app_fw_flower->mtr_priv;

	/* Check if meter id exist */
	mtr = nfp_mtr_find_by_mtr_id(priv, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Request meter not exist");
	}

	mtr->enable = true;

	return 0;
}

/**
 * Disable meter object.
 *
 * @param[in] dev
 *   Pointer to the device.
 * @param[in] mtr_id
 *   Id of the meter.
 * @param[out] error
 *   Pointer to the error.
 *
 * @returns
 *   0 on success, negative value otherwise and rte_errno is set..
 */
static int
nfp_mtr_disable(struct rte_eth_dev *dev,
		uint32_t mtr_id,
		struct rte_mtr_error *error)
{
	struct nfp_mtr *mtr;
	struct nfp_mtr_priv *priv;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	priv = representor->app_fw_flower->mtr_priv;

	/* Check if meter id exist */
	mtr = nfp_mtr_find_by_mtr_id(priv, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Request meter not exist");
	}

	if (mtr->ref_cnt > 0) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Can't disable a used meter");
	}

	mtr->enable = false;

	return 0;
}

/**
 * Callback to update meter profile.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_id
 *   Meter id.
 * @param[in] mtr_profile_id
 *   To be updated meter profile id.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative value otherwise and rte_errno is set.
 */
static int
nfp_mtr_profile_update(struct rte_eth_dev *dev,
		uint32_t mtr_id,
		uint32_t mtr_profile_id,
		struct rte_mtr_error *error)
{
	struct nfp_mtr *mtr;
	struct nfp_mtr_priv *priv;
	struct nfp_mtr_profile *mtr_profile;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	priv = representor->app_fw_flower->mtr_priv;

	/* Check if meter id exist */
	mtr = nfp_mtr_find_by_mtr_id(priv, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Request meter not exist");
	}

	if (mtr->ref_cnt > 0) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Request meter is been used");
	}

	if (mtr->mtr_profile->profile_id == mtr_profile_id)
		return 0;

	mtr_profile = nfp_mtr_profile_search(priv, mtr_profile_id);
	if (mtr_profile == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
				NULL, "Request meter profile not exist");
	}

	if (mtr_profile->in_use) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
				NULL, "Request meter profile is been used");
	}

	mtr_profile->in_use = true;
	mtr->mtr_profile->in_use = false;
	mtr->mtr_profile = mtr_profile;

	return 0;
}

/**
 * Callback to update meter stats mask.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_id
 *   Meter id.
 * @param[in] stats_mask
 *   To be updated stats_mask.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative value otherwise and rte_errno is set.
 */
static int
nfp_mtr_stats_update(struct rte_eth_dev *dev,
		uint32_t mtr_id,
		uint64_t stats_mask,
		struct rte_mtr_error *error)
{
	int ret;
	struct nfp_mtr *mtr;
	struct nfp_mtr_priv *priv;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	priv = representor->app_fw_flower->mtr_priv;

	/* Check if meter id exist */
	mtr = nfp_mtr_find_by_mtr_id(priv, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, EEXIST,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Request meter id not exist");
	}

	ret = nfp_mtr_stats_mask_validate(stats_mask, error);
	if (ret != 0)
		return ret;

	mtr->stats_mask = stats_mask;

	return 0;
}

/**
 * Callback to read meter statistics.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_id
 *   Meter id.
 * @param[out] stats
 *   Pointer to store the statistics.
 * @param[out] stats_mask
 *   Pointer to store the stats_mask.
 * @param[in] clear
 *   Statistic to be cleared after read or not.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative value otherwise and rte_errno is set.
 */
static int
nfp_mtr_stats_read(struct rte_eth_dev *dev,
		uint32_t mtr_id,
		struct rte_mtr_stats *stats,
		uint64_t *stats_mask,
		int clear,
		struct rte_mtr_error *error)
{
	struct nfp_mtr *mtr;
	struct nfp_mtr_priv *priv;
	struct nfp_mtr_stats curr;
	struct nfp_mtr_stats *prev;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;
	priv = representor->app_fw_flower->mtr_priv;

	/* Check if meter id exist */
	mtr = nfp_mtr_find_by_mtr_id(priv, mtr_id);
	if (mtr == NULL) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_ID,
				NULL, "Request meter not exist");
	}

	*stats_mask = mtr->stats_mask;

	rte_spinlock_lock(&priv->mtr_stats_lock);
	rte_memcpy(&curr, &mtr->mtr_stats.curr, sizeof(curr));
	rte_spinlock_unlock(&priv->mtr_stats_lock);

	prev = &mtr->mtr_stats.prev;

	stats->n_pkts[RTE_COLOR_GREEN] = curr.pass_pkts - prev->pass_pkts;
	stats->n_bytes[RTE_COLOR_GREEN] = curr.pass_bytes - prev->pass_bytes;
	stats->n_pkts_dropped = curr.drop_pkts - prev->drop_pkts;
	stats->n_bytes_dropped = curr.drop_bytes - prev->drop_bytes;

	if (clear != 0) {
		prev->pass_pkts = curr.pass_pkts;
		prev->pass_bytes = curr.pass_bytes;
		prev->drop_pkts = curr.drop_pkts;
		prev->drop_bytes = curr.drop_bytes;
	}

	return 0;
}

static const struct rte_mtr_ops nfp_mtr_ops = {
	.capabilities_get      = nfp_mtr_cap_get,
	.meter_profile_add     = nfp_mtr_profile_add,
	.meter_profile_delete  = nfp_mtr_profile_delete,
	.meter_policy_add      = nfp_mtr_policy_add,
	.meter_policy_delete   = nfp_mtr_policy_delete,
	.create                = nfp_mtr_create,
	.destroy               = nfp_mtr_destroy,
	.meter_enable          = nfp_mtr_enable,
	.meter_disable         = nfp_mtr_disable,
	.meter_profile_update  = nfp_mtr_profile_update,
	.stats_update          = nfp_mtr_stats_update,
	.stats_read            = nfp_mtr_stats_read,
};

int
nfp_net_mtr_ops_get(struct rte_eth_dev *dev, void *arg)
{
	if ((dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR) == 0) {
		PMD_DRV_LOG(ERR, "Port is not a representor");
		return -EINVAL;
	}

	*(const struct rte_mtr_ops **)arg = &nfp_mtr_ops;

	return 0;
}

static void
nfp_mtr_stats_request(void *arg)
{
	struct nfp_mtr *mtr;
	struct nfp_app_fw_flower *app_fw_flower = arg;

	LIST_FOREACH(mtr, &app_fw_flower->mtr_priv->mtrs, next)
		nfp_flower_cmsg_qos_stats(app_fw_flower, &mtr->mtr_profile->conf.head);

	rte_eal_alarm_set(NFP_METER_STATS_INTERVAL, nfp_mtr_stats_request, arg);
}

int
nfp_mtr_priv_init(struct nfp_pf_dev *pf_dev)
{
	int ret;
	struct nfp_mtr_priv *priv;
	struct nfp_app_fw_flower *app_fw_flower;

	priv = rte_zmalloc("nfp_app_mtr_priv", sizeof(struct nfp_mtr_priv), 0);
	if (priv == NULL) {
		PMD_INIT_LOG(ERR, "nfp app mtr priv creation failed");
		return -ENOMEM;
	}

	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(pf_dev->app_fw_priv);
	app_fw_flower->mtr_priv = priv;

	ret = rte_eal_alarm_set(NFP_METER_STATS_INTERVAL, nfp_mtr_stats_request,
			(void *)app_fw_flower);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "nfp mtr timer init failed.");
		rte_free(priv);
		return ret;
	}

	LIST_INIT(&priv->mtrs);
	LIST_INIT(&priv->profiles);
	LIST_INIT(&priv->policies);

	rte_spinlock_init(&priv->mtr_stats_lock);

	return 0;
}

void
nfp_mtr_priv_uninit(struct nfp_pf_dev *pf_dev)
{
	struct nfp_mtr *mtr;
	struct nfp_mtr_priv *priv;
	struct nfp_mtr_policy *mtr_policy;
	struct nfp_mtr_profile *mtr_profile;
	struct nfp_app_fw_flower *app_fw_flower;

	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(pf_dev->app_fw_priv);
	priv = app_fw_flower->mtr_priv;

	rte_eal_alarm_cancel(nfp_mtr_stats_request, (void *)app_fw_flower);

	LIST_FOREACH(mtr, &priv->mtrs, next) {
		LIST_REMOVE(mtr, next);
		rte_free(mtr);
	}

	LIST_FOREACH(mtr_profile, &priv->profiles, next) {
		LIST_REMOVE(mtr_profile, next);
		rte_free(mtr_profile);
	}

	LIST_FOREACH(mtr_policy, &priv->policies, next) {
		LIST_REMOVE(mtr_policy, next);
		rte_free(mtr_policy);
	}

	rte_free(priv);
}

int
nfp_mtr_update_ref_cnt(struct nfp_mtr_priv *priv,
		uint32_t mtr_id,
		bool add)
{
	struct nfp_mtr *mtr;

	mtr = nfp_mtr_find_by_mtr_id(priv, mtr_id);
	if (mtr == NULL)
		return -EINVAL;

	mtr->ref_cnt += add ? 1 : -1;

	return 0;
}
