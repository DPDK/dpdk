// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2018 Mellanox Technologies, Ltd
 */
#include <math.h>

#include <rte_tailq.h>
#include <rte_malloc.h>
#include <rte_mtr.h>
#include <rte_mtr_driver.h>

#include <mlx5_devx_cmds.h>
#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_flow.h"

/**
 * Create the meter action.
 *
 * @param priv
 *   Pointer to mlx5_priv.
 * @param[in] fm
 *   Pointer to flow meter to be converted.
 *
 * @return
 *   Pointer to the meter action on success, NULL otherwise.
 */
static void *
mlx5_flow_meter_action_create(struct mlx5_priv *priv,
			      struct mlx5_flow_meter *fm)
{
#ifdef HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER
	struct mlx5dv_dr_flow_meter_attr mtr_init;
	void *attr = fm->mfts->fmp;
	struct mlx5_flow_meter_srtcm_rfc2697_prm *srtcm =
						     &fm->profile->srtcm_prm;

	fm->mfts->fmp_size = MLX5_ST_SZ_BYTES(flow_meter_parameters);
	memset(attr, 0, fm->mfts->fmp_size);
	MLX5_SET(flow_meter_parameters, attr, valid, 1);
	MLX5_SET(flow_meter_parameters, attr, bucket_overflow, 1);
	MLX5_SET(flow_meter_parameters, attr,
		 start_color, MLX5_FLOW_COLOR_GREEN);
	MLX5_SET(flow_meter_parameters, attr, both_buckets_on_green, 0);
	MLX5_SET(flow_meter_parameters,
		 attr, cbs_exponent, srtcm->cbs_exponent);
	MLX5_SET(flow_meter_parameters,
		 attr, cbs_mantissa, srtcm->cbs_mantissa);
	MLX5_SET(flow_meter_parameters,
		 attr, cir_exponent, srtcm->cir_exponent);
	MLX5_SET(flow_meter_parameters,
		 attr, cir_mantissa, srtcm->cir_mantissa);
	MLX5_SET(flow_meter_parameters,
		 attr, ebs_exponent, srtcm->ebs_exponent);
	MLX5_SET(flow_meter_parameters,
		 attr, ebs_mantissa, srtcm->ebs_mantissa);
	mtr_init.next_table =
		fm->transfer ? fm->mfts->transfer.tbl->obj :
		    fm->egress ? fm->mfts->egress.tbl->obj :
				       fm->mfts->ingress.tbl->obj;
	mtr_init.reg_c_index = priv->mtr_color_reg - REG_C_0;
	mtr_init.flow_meter_parameter = fm->mfts->fmp;
	mtr_init.flow_meter_parameter_sz = fm->mfts->fmp_size;
	mtr_init.active = fm->active_state;
	return mlx5_glue->dv_create_flow_action_meter(&mtr_init);
#else
	(void)priv;
	(void)fm;
	return NULL;
#endif
}

/**
 * Find meter profile by id.
 *
 * @param priv
 *   Pointer to mlx5_priv.
 * @param meter_profile_id
 *   Meter profile id.
 *
 * @return
 *   Pointer to the profile found on success, NULL otherwise.
 */
static struct mlx5_flow_meter_profile *
mlx5_flow_meter_profile_find(struct mlx5_priv *priv, uint32_t meter_profile_id)
{
	struct mlx5_mtr_profiles *fmps = &priv->flow_meter_profiles;
	struct mlx5_flow_meter_profile *fmp;

	TAILQ_FOREACH(fmp, fmps, next)
		if (meter_profile_id == fmp->meter_profile_id)
			return fmp;
	return NULL;
}

/**
 * Validate the MTR profile.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_profile_id
 *   Meter profile id.
 * @param[in] profile
 *   Pointer to meter profile detail.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_profile_validate(struct rte_eth_dev *dev,
				 uint32_t meter_profile_id,
				 struct rte_mtr_meter_profile *profile,
				 struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_profile *fmp;

	/* Profile must not be NULL. */
	if (profile == NULL)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE,
					  NULL, "Meter profile is null.");
	/* Meter profile ID must be valid. */
	if (meter_profile_id == UINT32_MAX)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Meter profile id not valid.");
	/* Meter profile must not exist. */
	fmp = mlx5_flow_meter_profile_find(priv, meter_profile_id);
	if (fmp)
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL,
					  "Meter profile already exists.");
	if (profile->alg == RTE_MTR_SRTCM_RFC2697) {
		if (priv->config.hca_attr.qos.srtcm_sup) {
			/* Verify support for flow meter parameters. */
			if (profile->srtcm_rfc2697.cir > 0 &&
			    profile->srtcm_rfc2697.cir <= MLX5_SRTCM_CIR_MAX &&
			    profile->srtcm_rfc2697.cbs > 0 &&
			    profile->srtcm_rfc2697.cbs <= MLX5_SRTCM_CBS_MAX &&
			    profile->srtcm_rfc2697.ebs <= MLX5_SRTCM_EBS_MAX)
				return 0;
			else
				return -rte_mtr_error_set
					     (error, ENOTSUP,
					      RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					      NULL,
					      profile->srtcm_rfc2697.ebs ?
					      "Metering value ebs must be 0." :
					      "Invalid metering parameters.");
		}
	}
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_METER_PROFILE,
				  NULL, "Metering algorithm not supported.");
}

/**
 * Calculate mantissa and exponent for cir.
 *
 * @param[in] cir
 *   Value to be calculated.
 * @param[out] man
 *   Pointer to the mantissa.
 * @param[out] exp
 *   Pointer to the exp.
 */
static void
mlx5_flow_meter_cir_man_exp_calc(int64_t cir, uint8_t *man, uint8_t *exp)
{
	int64_t _cir;
	int64_t delta = INT64_MAX;
	uint8_t _man = 0;
	uint8_t _exp = 0;
	uint64_t m, e;

	for (m = 0; m <= 0xFF; m++) { /* man width 8 bit */
		for (e = 0; e <= 0x1F; e++) { /* exp width 5bit */
			_cir = (1000000000ULL * m) >> e;
			if (llabs(cir - _cir) <= delta) {
				delta = llabs(cir - _cir);
				_man = m;
				_exp = e;
			}
		}
	}
	*man = _man;
	*exp = _exp;
}

/**
 * Calculate mantissa and exponent for xbs.
 *
 * @param[in] xbs
 *   Value to be calculated.
 * @param[out] man
 *   Pointer to the mantissa.
 * @param[out] exp
 *   Pointer to the exp.
 */
static void
mlx5_flow_meter_xbs_man_exp_calc(uint64_t xbs, uint8_t *man, uint8_t *exp)
{
	int _exp;
	double _man;

	/* Special case xbs == 0 ? both exp and matissa are 0. */
	if (xbs == 0) {
		*man = 0;
		*exp = 0;
		return;
	}
	/* xbs = xbs_mantissa * 2^xbs_exponent */
	_man = frexp(xbs, &_exp);
	_man = _man * pow(2, MLX5_MAN_WIDTH);
	_exp = _exp - MLX5_MAN_WIDTH;
	*man = (uint8_t)ceil(_man);
	*exp = _exp;
}

/**
 * Fill the prm meter parameter.
 *
 * @param[in,out] fmp
 *   Pointer to meter profie to be converted.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_param_fill(struct mlx5_flow_meter_profile *fmp,
			  struct rte_mtr_error *error)
{
	struct mlx5_flow_meter_srtcm_rfc2697_prm *srtcm = &fmp->srtcm_prm;
	uint8_t man, exp;

	if (fmp->profile.alg != RTE_MTR_SRTCM_RFC2697)
		return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE,
				NULL, "Metering algorithm not supported.");
	 /* cbs = cbs_mantissa * 2^cbs_exponent */
	mlx5_flow_meter_xbs_man_exp_calc(fmp->profile.srtcm_rfc2697.cbs,
				    &man, &exp);
	srtcm->cbs_mantissa = man;
	srtcm->cbs_exponent = exp;
	/* Check if cbs mantissa is too large. */
	if (srtcm->cbs_exponent != exp)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "Metering profile parameter cbs is"
					  " invalid.");
	/* ebs = ebs_mantissa * 2^ebs_exponent */
	mlx5_flow_meter_xbs_man_exp_calc(fmp->profile.srtcm_rfc2697.ebs,
				    &man, &exp);
	srtcm->ebs_mantissa = man;
	srtcm->ebs_exponent = exp;
	/* Check if ebs mantissa is too large. */
	if (srtcm->ebs_exponent != exp)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "Metering profile parameter ebs is"
					  " invalid.");
	/* cir = 8G * cir_mantissa * 1/(2^cir_exponent)) Bytes/Sec */
	mlx5_flow_meter_cir_man_exp_calc(fmp->profile.srtcm_rfc2697.cir,
				    &man, &exp);
	srtcm->cir_mantissa = man;
	srtcm->cir_exponent = exp;
	/* Check if cir mantissa is too large. */
	if (srtcm->cir_exponent != exp)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "Metering profile parameter cir is"
					  " invalid.");
	return 0;
}

/**
 * Callback to get MTR capabilities.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] cap
 *   Pointer to save MTR capabilities.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_mtr_cap_get(struct rte_eth_dev *dev,
		 struct rte_mtr_capabilities *cap,
		 struct rte_mtr_error *error __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hca_qos_attr *qattr = &priv->config.hca_attr.qos;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	memset(cap, 0, sizeof(*cap));
	cap->n_max = 1 << qattr->log_max_flow_meter;
	cap->n_shared_max = cap->n_max;
	cap->identical = 1;
	cap->shared_identical = 1;
	cap->shared_n_flows_per_mtr_max = 4 << 20;
	/* 2M flows can share the same meter. */
	cap->chaining_n_mtrs_per_flow_max = 1; /* Chaining is not supported. */
	cap->meter_srtcm_rfc2697_n_max = qattr->srtcm_sup ? cap->n_max : 0;
	cap->meter_rate_max = 1ULL << 40; /* 1 Tera tokens per sec. */
	cap->policer_action_drop_supported = 1;
	cap->stats_mask = RTE_MTR_STATS_N_BYTES_DROPPED |
			  RTE_MTR_STATS_N_PKTS_DROPPED;
	return 0;
}

/**
 * Callback to add MTR profile.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_profile_id
 *   Meter profile id.
 * @param[in] profile
 *   Pointer to meter profile detail.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_profile_add(struct rte_eth_dev *dev,
		       uint32_t meter_profile_id,
		       struct rte_mtr_meter_profile *profile,
		       struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_mtr_profiles *fmps = &priv->flow_meter_profiles;
	struct mlx5_flow_meter_profile *fmp;
	int ret;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Check input params. */
	ret = mlx5_flow_meter_profile_validate(dev, meter_profile_id,
					       profile, error);
	if (ret)
		return ret;
	/* Meter profile memory allocation. */
	fmp = mlx5_malloc(MLX5_MEM_ZERO, sizeof(struct mlx5_flow_meter_profile),
			 RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (fmp == NULL)
		return -rte_mtr_error_set(error, ENOMEM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Meter profile memory "
					  "alloc failed.");
	/* Fill profile info. */
	fmp->meter_profile_id = meter_profile_id;
	fmp->profile = *profile;
	/* Fill the flow meter parameters for the PRM. */
	ret = mlx5_flow_meter_param_fill(fmp, error);
	if (ret)
		goto error;
	/* Add to list. */
	TAILQ_INSERT_TAIL(fmps, fmp, next);
	return 0;
error:
	mlx5_free(fmp);
	return ret;
}

/**
 * Callback to delete MTR profile.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_profile_id
 *   Meter profile id.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_profile_delete(struct rte_eth_dev *dev,
			  uint32_t meter_profile_id,
			  struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_profile *fmp;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter profile must exist. */
	fmp = mlx5_flow_meter_profile_find(priv, meter_profile_id);
	if (fmp == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  &meter_profile_id,
					  "Meter profile id is invalid.");
	/* Check profile is unused. */
	if (fmp->ref_cnt)
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Meter profile is in use.");
	/* Remove from list. */
	TAILQ_REMOVE(&priv->flow_meter_profiles, fmp, next);
	mlx5_free(fmp);
	return 0;
}

/**
 * Convert wrong color setting action to verbose error.
 *
 * @param[in] action
 *   Policy color action.
 *
 * @return
 *   Verbose meter color error type.
 */
static inline enum rte_mtr_error_type
action2error(enum rte_mtr_policer_action action)
{
	switch (action) {
	case MTR_POLICER_ACTION_COLOR_GREEN:
		return RTE_MTR_ERROR_TYPE_POLICER_ACTION_GREEN;
	case MTR_POLICER_ACTION_COLOR_YELLOW:
		return RTE_MTR_ERROR_TYPE_POLICER_ACTION_YELLOW;
	case MTR_POLICER_ACTION_COLOR_RED:
		return RTE_MTR_ERROR_TYPE_POLICER_ACTION_RED;
	default:
		break;
	}
	return RTE_MTR_ERROR_TYPE_UNSPECIFIED;
}

/**
 * Check meter validation.
 *
 * @param[in] priv
 *   Pointer to mlx5 private data structure.
 * @param[in] meter_id
 *   Meter id.
 * @param[in] params
 *   Pointer to rte meter parameters.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_validate(struct mlx5_priv *priv, uint32_t meter_id,
			 struct rte_mtr_params *params,
			 struct rte_mtr_error *error)
{
	static enum rte_mtr_policer_action
				valid_recol_action[RTE_COLORS] = {
					       MTR_POLICER_ACTION_COLOR_GREEN,
					       MTR_POLICER_ACTION_COLOR_YELLOW,
					       MTR_POLICER_ACTION_COLOR_RED };
	int i;

	/* Meter params must not be NULL. */
	if (params == NULL)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					  NULL, "Meter object params null.");
	/* Previous meter color is not supported. */
	if (params->use_prev_mtr_color)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					  NULL,
					  "Previous meter color "
					  "not supported.");
	/* Validate policer settings. */
	for (i = 0; i < RTE_COLORS; i++)
		if (params->action[i] != valid_recol_action[i] &&
		    params->action[i] != MTR_POLICER_ACTION_DROP)
			return -rte_mtr_error_set
					(error, ENOTSUP,
					 action2error(params->action[i]), NULL,
					 "Recolor action not supported.");
	/* Validate meter id. */
	if (mlx5_flow_meter_find(priv, meter_id))
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Meter object already exists.");
	return 0;
}

/**
 * Modify the flow meter action.
 *
 * @param[in] priv
 *   Pointer to mlx5 private data structure.
 * @param[in] fm
 *   Pointer to flow meter to be modified.
 * @param[in] srtcm
 *   Pointer to meter srtcm description parameter.
 * @param[in] modify_bits
 *   The bit in srtcm to be updated.
 * @param[in] active_state
 *   The state to be updated.
 * @return
 *   0 on success, o negative value otherwise.
 */
static int
mlx5_flow_meter_action_modify(struct mlx5_priv *priv,
		struct mlx5_flow_meter *fm,
		const struct mlx5_flow_meter_srtcm_rfc2697_prm *srtcm,
		uint64_t modify_bits, uint32_t active_state)
{
#ifdef HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER
	uint32_t in[MLX5_ST_SZ_DW(flow_meter_parameters)] = { 0 };
	uint32_t *attr;
	struct mlx5dv_dr_flow_meter_attr mod_attr = { 0 };
	int ret;

	/* Fill command parameters. */
	mod_attr.reg_c_index = priv->mtr_color_reg - REG_C_0;
	mod_attr.flow_meter_parameter = in;
	mod_attr.flow_meter_parameter_sz = fm->mfts->fmp_size;
	if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_ACTIVE)
		mod_attr.active = !!active_state;
	else
		mod_attr.active = 0;
	attr = in;
	if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CBS) {
		MLX5_SET(flow_meter_parameters,
			 attr, cbs_exponent, srtcm->cbs_exponent);
		MLX5_SET(flow_meter_parameters,
			 attr, cbs_mantissa, srtcm->cbs_mantissa);
	}
	if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CIR) {
		MLX5_SET(flow_meter_parameters,
			 attr, cir_exponent, srtcm->cir_exponent);
		MLX5_SET(flow_meter_parameters,
			 attr, cir_mantissa, srtcm->cir_mantissa);
	}
	if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_EBS) {
		MLX5_SET(flow_meter_parameters,
			 attr, ebs_exponent, srtcm->ebs_exponent);
		MLX5_SET(flow_meter_parameters,
			 attr, ebs_mantissa, srtcm->ebs_mantissa);
	}
	/* Apply modifications to meter only if it was created. */
	if (fm->mfts->meter_action) {
		ret = mlx5_glue->dv_modify_flow_action_meter
					(fm->mfts->meter_action, &mod_attr,
					rte_cpu_to_be_64(modify_bits));
		if (ret)
			return ret;
	}
	/* Update succeedded modify meter parameters. */
	if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_ACTIVE)
		fm->active_state = !!active_state;
	attr = fm->mfts->fmp;
	if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CBS) {
		MLX5_SET(flow_meter_parameters,
			 attr, cbs_exponent, srtcm->cbs_exponent);
		MLX5_SET(flow_meter_parameters,
			 attr, cbs_mantissa, srtcm->cbs_mantissa);
	}
	if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CIR) {
		MLX5_SET(flow_meter_parameters,
			 attr, cir_exponent, srtcm->cir_exponent);
		MLX5_SET(flow_meter_parameters,
			 attr, cir_mantissa, srtcm->cir_mantissa);
	}
	if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_EBS) {
		MLX5_SET(flow_meter_parameters,
			 attr, ebs_exponent, srtcm->ebs_exponent);
		MLX5_SET(flow_meter_parameters,
			 attr, ebs_mantissa, srtcm->ebs_mantissa);
	}

	return 0;
#else
	(void)priv;
	(void)fm;
	(void)srtcm;
	(void)modify_bits;
	(void)active_state;
	return -ENOTSUP;
#endif
}

/**
 * Create meter rules.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_id
 *   Meter id.
 * @param[in] params
 *   Pointer to rte meter parameters.
 * @param[in] shared
 *   Meter shared with other flow or not.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_create(struct rte_eth_dev *dev, uint32_t meter_id,
		       struct rte_mtr_params *params, int shared,
		       struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meters *fms = &priv->flow_meters;
	struct mlx5_flow_meter_profile *fmp;
	struct mlx5_flow_meter *fm;
	const struct rte_flow_attr attr = {
				.ingress = 1,
				.egress = 1,
				.transfer = priv->config.dv_esw_en ? 1 : 0,
			};
	int ret;
	unsigned int i;
	uint32_t idx = 0;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Validate the parameters. */
	ret = mlx5_flow_meter_validate(priv, meter_id, params, error);
	if (ret)
		return ret;
	/* Meter profile must exist. */
	fmp = mlx5_flow_meter_profile_find(priv, params->meter_profile_id);
	if (fmp == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Meter profile id not valid.");
	/* Allocate the flow meter memory. */
	fm = mlx5_ipool_zmalloc(priv->sh->ipool[MLX5_IPOOL_MTR], &idx);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOMEM,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Memory alloc failed for meter.");
	fm->idx = idx;
	/* Fill the flow meter parameters. */
	fm->meter_id = meter_id;
	fm->profile = fmp;
	memcpy(fm->action, params->action, sizeof(params->action));
	fm->stats_mask = params->stats_mask;

	/* Alloc policer counters. */
	for (i = 0; i < RTE_DIM(fm->policer_stats.cnt); i++) {
		fm->policer_stats.cnt[i] = mlx5_counter_alloc(dev);
		if (!fm->policer_stats.cnt[i])
			goto error;
	}
	fm->mfts = mlx5_flow_create_mtr_tbls(dev, fm);
	if (!fm->mfts)
		goto error;
	ret = mlx5_flow_create_policer_rules(dev, fm, &attr);
	if (ret)
		goto error;
	/* Add to the flow meter list. */
	TAILQ_INSERT_TAIL(fms, fm, next);
	fm->active_state = 1; /* Config meter starts as active. */
	fm->shared = !!shared;
	fm->policer_stats.stats_mask = params->stats_mask;
	fm->profile->ref_cnt++;
	rte_spinlock_init(&fm->sl);
	return 0;
error:
	mlx5_flow_destroy_policer_rules(dev, fm, &attr);
	mlx5_flow_destroy_mtr_tbls(dev, fm->mfts);
	/* Free policer counters. */
	for (i = 0; i < RTE_DIM(fm->policer_stats.cnt); i++)
		if (fm->policer_stats.cnt[i])
			mlx5_counter_free(dev, fm->policer_stats.cnt[i]);
	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MTR], idx);
	return -rte_mtr_error_set(error, -ret,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Failed to create devx meter.");
}

/**
 * Destroy meter rules.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_id
 *   Meter id.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_destroy(struct rte_eth_dev *dev, uint32_t meter_id,
			struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meters *fms = &priv->flow_meters;
	struct mlx5_flow_meter_profile *fmp;
	struct mlx5_flow_meter *fm;
	const struct rte_flow_attr attr = {
				.ingress = 1,
				.egress = 1,
				.transfer = priv->config.dv_esw_en ? 1 : 0,
			};
	unsigned int i;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter object id not valid.");
	/* Meter object must not have any owner. */
	if (fm->ref_cnt > 0)
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Meter object is being used.");
	/* Get the meter profile. */
	fmp = fm->profile;
	MLX5_ASSERT(fmp);
	/* Update dependencies. */
	fmp->ref_cnt--;
	/* Remove from the flow meter list. */
	TAILQ_REMOVE(fms, fm, next);
	/* Free policer counters. */
	for (i = 0; i < RTE_DIM(fm->policer_stats.cnt); i++)
		if (fm->policer_stats.cnt[i])
			mlx5_counter_free(dev, fm->policer_stats.cnt[i]);
	/* Free meter flow table */
	mlx5_flow_destroy_policer_rules(dev, fm, &attr);
	mlx5_flow_destroy_mtr_tbls(dev, fm->mfts);
	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MTR], fm->idx);
	return 0;
}

/**
 * Modify meter state.
 *
 * @param[in] priv
 *   Pointer to mlx5 private data structure.
 * @param[in] fm
 *   Pointer to flow meter.
 * @param[in] new_state
 *   New state to update.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_modify_state(struct mlx5_priv *priv,
			     struct mlx5_flow_meter *fm,
			     uint32_t new_state,
			     struct rte_mtr_error *error)
{
	static const struct mlx5_flow_meter_srtcm_rfc2697_prm srtcm = {
		.cbs_exponent = 20,
		.cbs_mantissa = 191,
		.cir_exponent = 0,
		.cir_mantissa = 200,
		.ebs_exponent = 0,
		.ebs_mantissa = 0,
	};
	uint64_t modify_bits = MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CBS |
			       MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CIR;
	int ret;

	if (new_state == MLX5_FLOW_METER_DISABLE)
		ret = mlx5_flow_meter_action_modify(priv, fm, &srtcm,
						    modify_bits, 0);
	else
		ret = mlx5_flow_meter_action_modify(priv, fm,
						   &fm->profile->srtcm_prm,
						    modify_bits, 0);
	if (ret)
		return -rte_mtr_error_set(error, -ret,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					  NULL,
					  new_state ?
					  "Failed to enable meter." :
					  "Failed to disable meter.");
	return 0;
}

/**
 * Callback to enable flow meter.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_id
 *   Meter id.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_enable(struct rte_eth_dev *dev,
		       uint32_t meter_id,
		       struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter *fm;
	int ret;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter not found.");
	if (fm->active_state == MLX5_FLOW_METER_ENABLE)
		return 0;
	ret = mlx5_flow_meter_modify_state(priv, fm, MLX5_FLOW_METER_ENABLE,
					   error);
	if (!ret)
		fm->active_state = MLX5_FLOW_METER_ENABLE;
	return ret;
}

/**
 * Callback to disable flow meter.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_id
 *   Meter id.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_disable(struct rte_eth_dev *dev,
			uint32_t meter_id,
			struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter *fm;
	int ret;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter not found.");
	if (fm->active_state == MLX5_FLOW_METER_DISABLE)
		return 0;
	ret = mlx5_flow_meter_modify_state(priv, fm, MLX5_FLOW_METER_DISABLE,
					   error);
	if (!ret)
		fm->active_state = MLX5_FLOW_METER_DISABLE;
	return ret;
}

/**
 * Callback to update meter profile.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_id
 *   Meter id.
 * @param[in] meter_profile_id
 *   To be updated meter profile id.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_profile_update(struct rte_eth_dev *dev,
			       uint32_t meter_id,
			       uint32_t meter_profile_id,
			       struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_profile *fmp;
	struct mlx5_flow_meter_profile *old_fmp;
	struct mlx5_flow_meter *fm;
	uint64_t modify_bits = MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CBS |
			       MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CIR;
	int ret;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter profile must exist. */
	fmp = mlx5_flow_meter_profile_find(priv, meter_profile_id);
	if (fmp == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Meter profile not found.");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter not found.");
	/* MTR object already set to meter profile id. */
	old_fmp = fm->profile;
	if (fmp == old_fmp)
		return 0;
	/* Update the profile. */
	fm->profile = fmp;
	/* Update meter params in HW (if not disabled). */
	if (fm->active_state == MLX5_FLOW_METER_DISABLE)
		return 0;
	ret = mlx5_flow_meter_action_modify(priv, fm, &fm->profile->srtcm_prm,
					      modify_bits, fm->active_state);
	if (ret) {
		fm->profile = old_fmp;
		return -rte_mtr_error_set(error, -ret,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					  NULL, "Failed to update meter"
					  " parmeters in hardware.");
	}
	old_fmp->ref_cnt--;
	fmp->ref_cnt++;
	return 0;
}

/**
 * Callback to update meter stats mask.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_id
 *   Meter id.
 * @param[in] stats_mask
 *   To be updated stats_mask.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_stats_update(struct rte_eth_dev *dev,
			     uint32_t meter_id,
			     uint64_t stats_mask,
			     struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter *fm;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter object id not valid.");
	fm->policer_stats.stats_mask = stats_mask;
	return 0;
}

/**
 * Callback to read meter statistics.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_id
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
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_stats_read(struct rte_eth_dev *dev,
			   uint32_t meter_id,
			   struct rte_mtr_stats *stats,
			   uint64_t *stats_mask,
			   int clear,
			   struct rte_mtr_error *error)
{
	static uint64_t meter2mask[RTE_MTR_DROPPED + 1] = {
		RTE_MTR_STATS_N_PKTS_GREEN | RTE_MTR_STATS_N_BYTES_GREEN,
		RTE_MTR_STATS_N_PKTS_YELLOW | RTE_MTR_STATS_N_BYTES_YELLOW,
		RTE_MTR_STATS_N_PKTS_RED | RTE_MTR_STATS_N_BYTES_RED,
		RTE_MTR_STATS_N_PKTS_DROPPED | RTE_MTR_STATS_N_BYTES_DROPPED
	};
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter *fm;
	struct mlx5_flow_policer_stats *ps;
	uint64_t pkts_dropped = 0;
	uint64_t bytes_dropped = 0;
	uint64_t pkts;
	uint64_t bytes;
	int i;
	int ret = 0;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter object id not valid.");
	ps = &fm->policer_stats;
	*stats_mask = ps->stats_mask;
	for (i = 0; i < RTE_MTR_DROPPED; i++) {
		if (*stats_mask & meter2mask[i]) {
			ret = mlx5_counter_query(dev, ps->cnt[i], clear, &pkts,
						 &bytes);
			if (ret)
				goto error;
			if (fm->action[i] == MTR_POLICER_ACTION_DROP) {
				pkts_dropped += pkts;
				bytes_dropped += bytes;
			}
			/* If need to read the packets, set it. */
			if ((1 << i) & (*stats_mask & meter2mask[i]))
				stats->n_pkts[i] = pkts;
			/* If need to read the bytes, set it. */
			if ((1 << (RTE_MTR_DROPPED + 1 + i)) &
			   (*stats_mask & meter2mask[i]))
				stats->n_bytes[i] = bytes;
		}
	}
	/* Dropped packets/bytes are treated differently. */
	if (*stats_mask & meter2mask[i]) {
		ret = mlx5_counter_query(dev, ps->cnt[i], clear, &pkts,
					 &bytes);
		if (ret)
			goto error;
		pkts += pkts_dropped;
		bytes += bytes_dropped;
		/* If need to read the packets, set it. */
		if ((*stats_mask & meter2mask[i]) &
		   RTE_MTR_STATS_N_PKTS_DROPPED)
			stats->n_pkts_dropped = pkts;
		/* If need to read the bytes, set it. */
		if ((*stats_mask & meter2mask[i]) &
		   RTE_MTR_STATS_N_BYTES_DROPPED)
			stats->n_bytes_dropped = bytes;
	}
	return 0;
error:
	return -rte_mtr_error_set(error, ret, RTE_MTR_ERROR_TYPE_STATS, NULL,
				 "Failed to read policer counters.");
}

static const struct rte_mtr_ops mlx5_flow_mtr_ops = {
	.capabilities_get = mlx5_flow_mtr_cap_get,
	.meter_profile_add = mlx5_flow_meter_profile_add,
	.meter_profile_delete = mlx5_flow_meter_profile_delete,
	.create = mlx5_flow_meter_create,
	.destroy = mlx5_flow_meter_destroy,
	.meter_enable = mlx5_flow_meter_enable,
	.meter_disable = mlx5_flow_meter_disable,
	.meter_profile_update = mlx5_flow_meter_profile_update,
	.meter_dscp_table_update = NULL,
	.policer_actions_update = NULL,
	.stats_update = mlx5_flow_meter_stats_update,
	.stats_read = mlx5_flow_meter_stats_read,
};

/**
 * Get meter operations.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param arg
 *   Pointer to set the mtr operations.
 *
 * @return
 *   Always 0.
 */
int
mlx5_flow_meter_ops_get(struct rte_eth_dev *dev __rte_unused, void *arg)
{
	*(const struct rte_mtr_ops **)arg = &mlx5_flow_mtr_ops;
	return 0;
}

/**
 * Find meter by id.
 *
 * @param priv
 *   Pointer to mlx5_priv.
 * @param meter_id
 *   Meter id.
 *
 * @return
 *   Pointer to the profile found on success, NULL otherwise.
 */
struct mlx5_flow_meter *
mlx5_flow_meter_find(struct mlx5_priv *priv, uint32_t meter_id)
{
	struct mlx5_flow_meters *fms = &priv->flow_meters;
	struct mlx5_flow_meter *fm;

	TAILQ_FOREACH(fm, fms, next)
		if (meter_id == fm->meter_id)
			return fm;
	return NULL;
}

/**
 * Attach meter to flow.
 * Unidirectional Meter creation can only be done
 * when flow direction is known, i.e. when calling meter_attach.
 *
 * @param [in] priv
 *  Pointer to mlx5 private data.
 * @param [in] meter_id
 *  Flow meter id.
 * @param [in] attr
 *  Pointer to flow attributes.
 * @param [out] error
 *  Pointer to error structure.
 *
 * @return the flow meter pointer, NULL otherwise.
 */
struct mlx5_flow_meter *
mlx5_flow_meter_attach(struct mlx5_priv *priv, uint32_t meter_id,
		       const struct rte_flow_attr *attr,
		       struct rte_flow_error *error)
{
	struct mlx5_flow_meter *fm;
	int ret = 0;

	fm = mlx5_flow_meter_find(priv, meter_id);
	if (fm == NULL) {
		rte_flow_error_set(error, ENOENT,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Meter object id not valid");
		return fm;
	}
	rte_spinlock_lock(&fm->sl);
	if (fm->mfts->meter_action) {
		if (fm->shared &&
		    attr->transfer == fm->transfer &&
		    attr->ingress == fm->ingress &&
		    attr->egress == fm->egress)
			fm->ref_cnt++;
		else
			ret = -1;
	} else {
		fm->ingress = attr->ingress;
		fm->egress = attr->egress;
		fm->transfer = attr->transfer;
		 fm->ref_cnt = 1;
		/* This also creates the meter object. */
		fm->mfts->meter_action = mlx5_flow_meter_action_create(priv,
								       fm);
		if (!fm->mfts->meter_action) {
			fm->ref_cnt = 0;
			fm->ingress = 0;
			fm->egress = 0;
			fm->transfer = 0;
			ret = -1;
			DRV_LOG(ERR, "Meter action create failed.");
		}
	}
	rte_spinlock_unlock(&fm->sl);
	if (ret)
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   fm->mfts->meter_action ?
				   "Meter attr not match" :
				   "Meter action create failed");
	return ret ? NULL : fm;
}

/**
 * Detach meter from flow.
 *
 * @param [in] fm
 *  Pointer to flow meter.
 */
void
mlx5_flow_meter_detach(struct mlx5_flow_meter *fm)
{
#ifdef HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER
	rte_spinlock_lock(&fm->sl);
	MLX5_ASSERT(fm->ref_cnt);
	if (--fm->ref_cnt == 0) {
		mlx5_glue->destroy_flow_action(fm->mfts->meter_action);
		fm->mfts->meter_action = NULL;
		fm->ingress = 0;
		fm->egress = 0;
		fm->transfer = 0;
	}
	rte_spinlock_unlock(&fm->sl);
#else
	(void)fm;
#endif
}

/**
 * Flush meter configuration.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_meter_flush(struct rte_eth_dev *dev, struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meters *fms = &priv->flow_meters;
	struct mlx5_mtr_profiles *fmps = &priv->flow_meter_profiles;
	struct mlx5_flow_meter_profile *fmp;
	struct mlx5_flow_meter *fm;
	const struct rte_flow_attr attr = {
				.ingress = 1,
				.egress = 1,
				.transfer = priv->config.dv_esw_en ? 1 : 0,
			};
	void *tmp;
	uint32_t i;

	TAILQ_FOREACH_SAFE(fm, fms, next, tmp) {
		/* Meter object must not have any owner. */
		MLX5_ASSERT(!fm->ref_cnt);
		/* Get meter profile. */
		fmp = fm->profile;
		if (fmp == NULL)
			return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
				NULL, "MTR object meter profile invalid.");
		/* Update dependencies. */
		fmp->ref_cnt--;
		/* Remove from list. */
		TAILQ_REMOVE(fms, fm, next);
		/* Free policer counters. */
		for (i = 0; i < RTE_DIM(fm->policer_stats.cnt); i++)
			if (fm->policer_stats.cnt[i])
				mlx5_counter_free(dev,
						  fm->policer_stats.cnt[i]);
		/* Free meter flow table. */
		mlx5_flow_destroy_policer_rules(dev, fm, &attr);
		mlx5_flow_destroy_mtr_tbls(dev, fm->mfts);
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MTR], fm->idx);
	}
	TAILQ_FOREACH_SAFE(fmp, fmps, next, tmp) {
		/* Check unused. */
		MLX5_ASSERT(!fmp->ref_cnt);
		/* Remove from list. */
		TAILQ_REMOVE(&priv->flow_meter_profiles, fmp, next);
		mlx5_free(fmp);
	}
	return 0;
}
