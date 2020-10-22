/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Marvell International Ltd.
 * Copyright(c) 2018 Semihalf.
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_malloc.h>

#include "mrvl_mtr.h"

/** Maximum meter rate */
#define MRVL_SRTCM_RFC2697_CIR_MAX 1023000

/** Invalid plcr bit */
#define MRVL_PLCR_BIT_INVALID -1

/**
 * Return meter object capabilities.
 *
 * @param dev Pointer to the device (unused).
 * @param cap Pointer to the meter object capabilities.
 * @param error Pointer to the error (unused).
 * @returns 0 always.
 */
static int
mrvl_capabilities_get(struct rte_eth_dev *dev __rte_unused,
			  struct rte_mtr_capabilities *cap,
			  struct rte_mtr_error *error __rte_unused)
{
	struct rte_mtr_capabilities capa = {
		.n_max = PP2_CLS_PLCR_NUM,
		.n_shared_max = PP2_CLS_PLCR_NUM,
		.shared_n_flows_per_mtr_max = -1,
		.meter_srtcm_rfc2697_n_max = PP2_CLS_PLCR_NUM,
		.meter_rate_max = MRVL_SRTCM_RFC2697_CIR_MAX,
	};

	memcpy(cap, &capa, sizeof(capa));

	return 0;
}

/**
 * Get profile using it's id.
 *
 * @param priv Pointer to the port's private data.
 * @param meter_profile_id Profile id used by the meter.
 * @returns Pointer to the profile if exists, NULL otherwise.
 */
static struct mrvl_mtr_profile *
mrvl_mtr_profile_from_id(struct mrvl_priv *priv, uint32_t meter_profile_id)
{
	struct mrvl_mtr_profile *profile = NULL;

	LIST_FOREACH(profile, &priv->profiles, next)
		if (profile->profile_id == meter_profile_id)
			break;

	return profile;
}

/**
 * Add profile to the list of profiles.
 *
 * @param dev Pointer to the device.
 * @param meter_profile_id Id of the new profile.
 * @param profile Pointer to the profile configuration.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_meter_profile_add(struct rte_eth_dev *dev, uint32_t meter_profile_id,
		       struct rte_mtr_meter_profile *profile,
		       struct rte_mtr_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_mtr_profile *prof;

	if (!profile)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, NULL);

	if (profile->alg != RTE_MTR_SRTCM_RFC2697)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Only srTCM RFC 2697 is supported\n");

	prof = mrvl_mtr_profile_from_id(priv, meter_profile_id);
	if (prof)
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Profile id already exists\n");

	prof = rte_zmalloc_socket(NULL, sizeof(*prof), 0, rte_socket_id());
	if (!prof)
		return -rte_mtr_error_set(error, ENOMEM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, NULL);

	prof->profile_id = meter_profile_id;
	memcpy(&prof->profile, profile, sizeof(*profile));

	LIST_INSERT_HEAD(&priv->profiles, prof, next);

	return 0;
}

/**
 * Remove profile from the list of profiles.
 *
 * @param dev Pointer to the device.
 * @param meter_profile_id Id of the profile to remove.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_meter_profile_delete(struct rte_eth_dev *dev,
			      uint32_t meter_profile_id,
			      struct rte_mtr_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_mtr_profile *profile;

	profile = mrvl_mtr_profile_from_id(priv, meter_profile_id);
	if (!profile)
		return -rte_mtr_error_set(error, ENODEV,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Profile id does not exist\n");

	if (profile->refcnt)
		return -rte_mtr_error_set(error, EPERM,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Profile is used\n");

	LIST_REMOVE(profile, next);
	rte_free(profile);

	return 0;
}

/**
 * Get meter using it's id.
 *
 * @param priv Pointer to port's private data.
 * @param mtr_id Id of the meter.
 * @returns Pointer to the meter if exists, NULL otherwise.
 */
static struct mrvl_mtr *
mrvl_mtr_from_id(struct mrvl_priv *priv, uint32_t mtr_id)
{
	struct mrvl_mtr *mtr = NULL;

	LIST_FOREACH(mtr, &priv->mtrs, next)
		if (mtr->mtr_id == mtr_id)
			break;

	return mtr;
}

/**
 * Reserve a policer bit in a bitmap.
 *
 * @param plcrs Pointer to the policers bitmap.
 * @returns Reserved bit number on success, negative value otherwise.
 */
static int
mrvl_reserve_plcr(uint32_t *plcrs)
{
	uint32_t i, num;

	num = PP2_CLS_PLCR_NUM;
	if (num > sizeof(uint32_t) * 8) {
		num = sizeof(uint32_t) * 8;
		MRVL_LOG(WARNING, "Plcrs number was limited to 32.");
	}

	for (i = 0; i < num; i++) {
		uint32_t bit = BIT(i);

		if (!(*plcrs & bit)) {
			*plcrs |= bit;

			return i;
		}
	}

	return -1;
}

/**
 * Enable meter object.
 *
 * @param dev Pointer to the device.
 * @param mtr_id Id of the meter.
 * @param error Pointer to the error.
 * @returns 0 in success, negative value otherwise.
 */
static int
mrvl_meter_enable(struct rte_eth_dev *dev, uint32_t mtr_id,
		  struct rte_mtr_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_mtr *mtr = mrvl_mtr_from_id(priv, mtr_id);
	struct pp2_cls_plcr_params params;
	char match[MRVL_MATCH_LEN];
	struct rte_flow *flow;
	int ret;

	if (!priv->ppio)
		return -rte_mtr_error_set(error, EPERM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Port is uninitialized\n");

	if (!mtr)
		return -rte_mtr_error_set(error, ENODEV,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter id does not exist\n");

	if (mtr->plcr)
		goto skip;

	mtr->plcr_bit = mrvl_reserve_plcr(&priv->used_plcrs);
	if (mtr->plcr_bit < 0)
		return -rte_mtr_error_set(error, ENOSPC,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Failed to reserve plcr entry\n");

	memset(&params, 0, sizeof(params));
	snprintf(match, sizeof(match), "policer-%d:%d", priv->pp_id,
		 mtr->plcr_bit);
	params.match = match;
	params.token_unit = PP2_CLS_PLCR_BYTES_TOKEN_UNIT;
	params.color_mode = PP2_CLS_PLCR_COLOR_BLIND_MODE;
	params.cir = mtr->profile->profile.srtcm_rfc2697.cir;
	params.cbs = mtr->profile->profile.srtcm_rfc2697.cbs;
	params.ebs = mtr->profile->profile.srtcm_rfc2697.ebs;

	ret = pp2_cls_plcr_init(&params, &mtr->plcr);
	if (ret) {
		priv->used_plcrs &= ~BIT(mtr->plcr_bit);
		mtr->plcr_bit = MRVL_PLCR_BIT_INVALID;

		return -rte_mtr_error_set(error, -ret,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Failed to setup policer\n");
	}

	mtr->enabled = 1;
skip:
	/* iterate over flows that have this mtr attached */
	LIST_FOREACH(flow, &priv->flows, next) {
		if (flow->mtr != mtr)
			continue;

		flow->action.plcr = mtr->plcr;

		ret = pp2_cls_tbl_modify_rule(priv->cls_tbl, &flow->rule,
					      &flow->action);
		if (ret)
			return -rte_mtr_error_set(error, -ret,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Failed to update cls rule\n");
	}

	return 0;
}

/**
 * Disable meter object.
 *
 * @param dev Pointer to the device.
 * @param mtr Id of the meter.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_meter_disable(struct rte_eth_dev *dev, uint32_t mtr_id,
		       struct rte_mtr_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_mtr *mtr = mrvl_mtr_from_id(priv, mtr_id);
	struct rte_flow *flow;
	int ret;

	if (!mtr)
		return -rte_mtr_error_set(error, ENODEV,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter id does not exist\n");

	LIST_FOREACH(flow, &priv->flows, next) {
		if (flow->mtr != mtr)
			continue;

		flow->action.plcr = NULL;

		ret = pp2_cls_tbl_modify_rule(priv->cls_tbl, &flow->rule,
					      &flow->action);
		if (ret)
			return -rte_mtr_error_set(error, -ret,
					RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					NULL, "Failed to disable meter\n");
	}

	mtr->enabled = 0;

	return 0;
}

/**
 * Create new meter.
 *
 * @param dev Pointer to the device.
 * @param mtr_id Id of the meter.
 * @param params Pointer to the meter parameters.
 * @param shared Flags indicating whether meter is shared.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_create(struct rte_eth_dev *dev, uint32_t mtr_id,
	    struct rte_mtr_params *params, int shared,
	    struct rte_mtr_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_mtr_profile *profile;
	struct mrvl_mtr *mtr;

	profile = mrvl_mtr_profile_from_id(priv, params->meter_profile_id);
	if (!profile)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Profile id does not exist\n");

	mtr = mrvl_mtr_from_id(priv, mtr_id);
	if (mtr)
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter id already exists\n");

	mtr = rte_zmalloc_socket(NULL, sizeof(*mtr), 0, rte_socket_id());
	if (!mtr)
		return -rte_mtr_error_set(error, ENOMEM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, NULL);

	mtr->shared = shared;
	mtr->mtr_id = mtr_id;
	mtr->plcr_bit = MRVL_PLCR_BIT_INVALID;
	mtr->profile = profile;
	profile->refcnt++;
	LIST_INSERT_HEAD(&priv->mtrs, mtr, next);

	if (params->meter_enable)
		return mrvl_meter_enable(dev, mtr_id, error);

	return 0;
}

/**
 * Destroy meter object.
 *
 * @param dev Pointer to the device.
 * @param mtr_id Id of the meter object.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_destroy(struct rte_eth_dev *dev, uint32_t mtr_id,
		 struct rte_mtr_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_mtr *mtr;

	if (!priv->ppio)
		return -rte_mtr_error_set(error, EPERM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Port is uninitialized\n");

	mtr = mrvl_mtr_from_id(priv, mtr_id);
	if (!mtr)
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter id does not exist\n");

	if (mtr->refcnt)
		return -rte_mtr_error_set(error, EPERM,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter is used\n");

	LIST_REMOVE(mtr, next);
	mtr->profile->refcnt--;

	if (mtr->plcr_bit != MRVL_PLCR_BIT_INVALID)
		priv->used_plcrs &= ~BIT(mtr->plcr_bit);

	if (mtr->plcr)
		pp2_cls_plcr_deinit(mtr->plcr);

	rte_free(mtr);

	return 0;
}

/**
 * Update profile used by the meter.
 *
 * @param dev Pointer to the device.
 * @param mtr_id Id of the meter object.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_meter_profile_update(struct rte_eth_dev *dev, uint32_t mtr_id,
			  uint32_t meter_profile_id,
			  struct rte_mtr_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_mtr_profile *profile;
	struct mrvl_mtr *mtr;
	int ret, enabled = 0;

	if (!priv->ppio)
		return -rte_mtr_error_set(error, EPERM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Port is uninitialized\n");

	mtr = mrvl_mtr_from_id(priv, mtr_id);
	if (!mtr)
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter id does not exist\n");

	profile = mrvl_mtr_profile_from_id(priv, meter_profile_id);
	if (!profile)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Profile id does not exist\n");

	ret = mrvl_meter_disable(dev, mtr_id, error);
	if (ret)
		return -rte_mtr_error_set(error, EPERM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  NULL);

	if (mtr->plcr) {
		enabled = 1;
		pp2_cls_plcr_deinit(mtr->plcr);
		mtr->plcr = NULL;
	}

	mtr->profile->refcnt--;
	mtr->profile = profile;
	profile->refcnt++;

	if (enabled)
		return mrvl_meter_enable(dev, mtr_id, error);

	return 0;
}

const struct rte_mtr_ops mrvl_mtr_ops = {
	.capabilities_get = mrvl_capabilities_get,
	.meter_profile_add = mrvl_meter_profile_add,
	.meter_profile_delete = mrvl_meter_profile_delete,
	.create = mrvl_create,
	.destroy = mrvl_destroy,
	.meter_enable = mrvl_meter_enable,
	.meter_disable = mrvl_meter_disable,
	.meter_profile_update = mrvl_meter_profile_update,
};

/**
 * Initialize metering resources.
 *
 * @param dev Pointer to the device.
 */
void
mrvl_mtr_init(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;

	LIST_INIT(&priv->profiles);
	LIST_INIT(&priv->mtrs);
}

/**
 * Cleanup metering resources.
 *
 * @param dev Pointer to the device.
 */
void
mrvl_mtr_deinit(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_mtr_profile *profile, *tmp_profile;
	struct mrvl_mtr *mtr, *tmp_mtr;

	for (mtr = LIST_FIRST(&priv->mtrs);
	     mtr && (tmp_mtr = LIST_NEXT(mtr, next), 1);
	     mtr = tmp_mtr)
		mrvl_destroy(dev, mtr->mtr_id, NULL);

	for (profile = LIST_FIRST(&priv->profiles);
	     profile && (tmp_profile = LIST_NEXT(profile, next), 1);
	     profile = tmp_profile)
		mrvl_meter_profile_delete(dev, profile->profile_id, NULL);
}
