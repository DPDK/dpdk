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
			      struct mlx5_flow_meter_info *fm)
{
#ifdef HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER
	struct mlx5dv_dr_flow_meter_attr mtr_init;
	uint32_t fmp[MLX5_ST_SZ_DW(flow_meter_parameters)];
	struct mlx5_flow_meter_srtcm_rfc2697_prm *srtcm =
						     &fm->profile->srtcm_prm;
	uint32_t cbs_cir = rte_be_to_cpu_32(srtcm->cbs_cir);
	uint32_t ebs_eir = rte_be_to_cpu_32(srtcm->ebs_eir);
	uint32_t val;

	memset(fmp, 0, MLX5_ST_SZ_BYTES(flow_meter_parameters));
	MLX5_SET(flow_meter_parameters, fmp, valid, 1);
	MLX5_SET(flow_meter_parameters, fmp, bucket_overflow, 1);
	MLX5_SET(flow_meter_parameters, fmp,
		start_color, MLX5_FLOW_COLOR_GREEN);
	MLX5_SET(flow_meter_parameters, fmp, both_buckets_on_green, 0);
	val = (cbs_cir >> ASO_DSEG_CBS_EXP_OFFSET) & ASO_DSEG_EXP_MASK;
	MLX5_SET(flow_meter_parameters, fmp, cbs_exponent, val);
	val = (cbs_cir >> ASO_DSEG_CBS_MAN_OFFSET) & ASO_DSEG_MAN_MASK;
	MLX5_SET(flow_meter_parameters, fmp, cbs_mantissa, val);
	val = (cbs_cir >> ASO_DSEG_CIR_EXP_OFFSET) & ASO_DSEG_EXP_MASK;
	MLX5_SET(flow_meter_parameters, fmp, cir_exponent, val);
	val = (cbs_cir & ASO_DSEG_MAN_MASK);
	MLX5_SET(flow_meter_parameters, fmp, cir_mantissa, val);
	val = (ebs_eir >> ASO_DSEG_EBS_EXP_OFFSET) & ASO_DSEG_EXP_MASK;
	MLX5_SET(flow_meter_parameters, fmp, ebs_exponent, val);
	val = (ebs_eir >> ASO_DSEG_EBS_MAN_OFFSET) & ASO_DSEG_MAN_MASK;
	MLX5_SET(flow_meter_parameters, fmp, ebs_mantissa, val);
	mtr_init.next_table =
		fm->transfer ? fm->mfts->transfer.tbl->obj :
			fm->egress ? fm->mfts->egress.tbl->obj :
				fm->mfts->ingress.tbl->obj;
	mtr_init.reg_c_index = priv->mtr_color_reg - REG_C_0;
	mtr_init.flow_meter_parameter = fmp;
	mtr_init.flow_meter_parameter_sz =
		MLX5_ST_SZ_BYTES(flow_meter_parameters);
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
		if (meter_profile_id == fmp->id)
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
		if (priv->config.hca_attr.qos.flow_meter_old) {
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
	uint32_t cbs_exp, cbs_man, cir_exp, cir_man;
	uint32_t ebs_exp, ebs_man;

	if (fmp->profile.alg != RTE_MTR_SRTCM_RFC2697)
		return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE,
				NULL, "Metering algorithm not supported.");
	/* cir = 8G * cir_mantissa * 1/(2^cir_exponent)) Bytes/Sec */
	mlx5_flow_meter_cir_man_exp_calc(fmp->profile.srtcm_rfc2697.cir,
				    &man, &exp);
	/* Check if cir mantissa is too large. */
	if (exp > ASO_DSEG_CIR_EXP_MASK)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "meter profile parameter cir is"
					  " not supported.");
	cir_man = man;
	cir_exp = exp;
	 /* cbs = cbs_mantissa * 2^cbs_exponent */
	mlx5_flow_meter_xbs_man_exp_calc(fmp->profile.srtcm_rfc2697.cbs,
				    &man, &exp);
	/* Check if cbs mantissa is too large. */
	if (exp > ASO_DSEG_EXP_MASK)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "meter profile parameter cbs is"
					  " not supported.");
	cbs_man = man;
	cbs_exp = exp;
	srtcm->cbs_cir = rte_cpu_to_be_32(cbs_exp << ASO_DSEG_CBS_EXP_OFFSET |
				cbs_man << ASO_DSEG_CBS_MAN_OFFSET |
				cir_exp << ASO_DSEG_CIR_EXP_OFFSET |
				cir_man);
	mlx5_flow_meter_xbs_man_exp_calc(fmp->profile.srtcm_rfc2697.ebs,
				    &man, &exp);
	/* Check if ebs mantissa is too large. */
	if (exp > ASO_DSEG_EXP_MASK)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "meter profile parameter ebs is"
					  " not supported.");
	ebs_man = man;
	ebs_exp = exp;
	srtcm->ebs_eir = rte_cpu_to_be_32(ebs_exp << ASO_DSEG_EBS_EXP_OFFSET |
					ebs_man << ASO_DSEG_EBS_MAN_OFFSET);
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
	if (priv->sh->meter_aso_en)
	    /* 2 meters per one ASO cache line. */
		cap->n_max = 1 << (qattr->log_max_num_meter_aso + 1);
	else
		cap->n_max = 1 << qattr->log_max_flow_meter;
	cap->n_shared_max = cap->n_max;
	cap->identical = 1;
	cap->shared_identical = 1;
	cap->shared_n_flows_per_mtr_max = 4 << 20;
	/* 2M flows can share the same meter. */
	cap->chaining_n_mtrs_per_flow_max = 1; /* Chaining is not supported. */
	cap->meter_srtcm_rfc2697_n_max = qattr->flow_meter_old ? cap->n_max : 0;
	cap->meter_rate_max = 1ULL << 40; /* 1 Tera tokens per sec. */
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
	fmp->id = meter_profile_id;
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
		struct mlx5_flow_meter_info *fm,
		const struct mlx5_flow_meter_srtcm_rfc2697_prm *srtcm,
		uint64_t modify_bits, uint32_t active_state, uint32_t is_enable)
{
#ifdef HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER
	uint32_t in[MLX5_ST_SZ_DW(flow_meter_parameters)] = { 0 };
	uint32_t *attr;
	struct mlx5dv_dr_flow_meter_attr mod_attr = { 0 };
	int ret;
	struct mlx5_aso_mtr *aso_mtr = NULL;
	uint32_t cbs_cir, ebs_eir, val;

	if (priv->sh->meter_aso_en) {
		fm->is_enable = !!is_enable;
		aso_mtr = container_of(fm, struct mlx5_aso_mtr, fm);
		ret = mlx5_aso_meter_update_by_wqe(priv->sh, aso_mtr);
		if (ret)
			return ret;
		ret = mlx5_aso_mtr_wait(priv->sh, aso_mtr);
		if (ret)
			return ret;
	} else {
		/* Fill command parameters. */
		mod_attr.reg_c_index = priv->mtr_color_reg - REG_C_0;
		mod_attr.flow_meter_parameter = in;
		mod_attr.flow_meter_parameter_sz =
				MLX5_ST_SZ_BYTES(flow_meter_parameters);
		if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_ACTIVE)
			mod_attr.active = !!active_state;
		else
			mod_attr.active = 0;
		attr = in;
		cbs_cir = rte_be_to_cpu_32(srtcm->cbs_cir);
		ebs_eir = rte_be_to_cpu_32(srtcm->ebs_eir);
		if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CBS) {
			val = (cbs_cir >> ASO_DSEG_CBS_EXP_OFFSET) &
				ASO_DSEG_EXP_MASK;
			MLX5_SET(flow_meter_parameters, attr,
				cbs_exponent, val);
			val = (cbs_cir >> ASO_DSEG_CBS_MAN_OFFSET) &
				ASO_DSEG_MAN_MASK;
			MLX5_SET(flow_meter_parameters, attr,
				cbs_mantissa, val);
		}
		if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CIR) {
			val = (cbs_cir >> ASO_DSEG_CIR_EXP_OFFSET) &
				ASO_DSEG_EXP_MASK;
			MLX5_SET(flow_meter_parameters, attr,
				cir_exponent, val);
			val = cbs_cir & ASO_DSEG_MAN_MASK;
			MLX5_SET(flow_meter_parameters, attr,
				cir_mantissa, val);
		}
		if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_EBS) {
			val = (ebs_eir >> ASO_DSEG_EBS_EXP_OFFSET) &
				ASO_DSEG_EXP_MASK;
			MLX5_SET(flow_meter_parameters, attr,
				ebs_exponent, val);
			val = (ebs_eir >> ASO_DSEG_EBS_MAN_OFFSET) &
				ASO_DSEG_MAN_MASK;
			MLX5_SET(flow_meter_parameters, attr,
				ebs_mantissa, val);
		}
		/* Apply modifications to meter only if it was created. */
		if (fm->meter_action) {
			ret = mlx5_glue->dv_modify_flow_action_meter
					(fm->meter_action, &mod_attr,
					rte_cpu_to_be_64(modify_bits));
			if (ret)
				return ret;
		}
		/* Update succeedded modify meter parameters. */
		if (modify_bits & MLX5_FLOW_METER_OBJ_MODIFY_FIELD_ACTIVE)
			fm->active_state = !!active_state;
	}
	return 0;
#else
	(void)priv;
	(void)fm;
	(void)srtcm;
	(void)modify_bits;
	(void)active_state;
	(void)is_enable;
	return -ENOTSUP;
#endif
}

static void
mlx5_flow_meter_stats_enable_update(struct mlx5_flow_meter_info *fm,
				uint64_t stats_mask)
{
	fm->bytes_dropped =
		(stats_mask & RTE_MTR_STATS_N_BYTES_DROPPED) ? 1 : 0;
	fm->pkts_dropped = (stats_mask & RTE_MTR_STATS_N_PKTS_DROPPED) ? 1 : 0;
}

static int
mlx5_flow_meter_params_flush(struct rte_eth_dev *dev,
			struct mlx5_flow_meter_info *fm,
			uint32_t mtr_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_legacy_flow_meters *fms = &priv->flow_meters;
	struct mlx5_flow_meter_profile *fmp;
	struct mlx5_legacy_flow_meter *legacy_fm = NULL;

	/* Meter object must not have any owner. */
	MLX5_ASSERT(!fm->ref_cnt);
	/* Get meter profile. */
	fmp = fm->profile;
	if (fmp == NULL)
		return -1;
	/* Update dependencies. */
	__atomic_sub_fetch(&fmp->ref_cnt, 1, __ATOMIC_RELAXED);
	/* Remove from list. */
	if (!priv->sh->meter_aso_en) {
		legacy_fm = container_of(fm, struct mlx5_legacy_flow_meter, fm);
		TAILQ_REMOVE(fms, legacy_fm, next);
	}
	/* Free drop counters. */
	if (fm->drop_cnt)
		mlx5_counter_free(dev, fm->drop_cnt);
	/* Free meter flow table. */
	if (fm->flow_ipool)
		mlx5_ipool_destroy(fm->flow_ipool);
	mlx5_flow_destroy_mtr_tbls(dev, fm->mfts);
	if (priv->sh->meter_aso_en)
		mlx5_flow_mtr_free(dev, mtr_idx);
	else
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MTR],
					legacy_fm->idx);
	return 0;
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
	struct mlx5_flow_meter_info *fm;
	uint32_t mtr_idx = 0;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id, &mtr_idx);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter object id not valid.");
	/* Meter object must not have any owner. */
	if (fm->ref_cnt > 0)
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Meter object is being used.");
	if (priv->sh->meter_aso_en) {
		if (mlx5_l3t_clear_entry(priv->mtr_idx_tbl, meter_id))
			return -rte_mtr_error_set(error, EBUSY,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
				"Fail to delete ASO Meter in index table.");
	}
	/* Destroy the meter profile. */
	if (mlx5_flow_meter_params_flush(dev, fm, mtr_idx))
		return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					NULL, "MTR object meter profile invalid.");
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
			     struct mlx5_flow_meter_info *fm,
			     uint32_t new_state,
			     struct rte_mtr_error *error)
{
	static const struct mlx5_flow_meter_srtcm_rfc2697_prm srtcm = {
		.cbs_cir = RTE_BE32(MLX5_IFC_FLOW_METER_DISABLE_CBS_CIR_VAL),
		.ebs_eir = 0,
	};
	uint64_t modify_bits = MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CBS |
			       MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CIR;
	int ret;

	if (new_state == MLX5_FLOW_METER_DISABLE)
		ret = mlx5_flow_meter_action_modify(priv, fm,
				&srtcm, modify_bits, 0, 0);
	else
		ret = mlx5_flow_meter_action_modify(priv, fm,
						   &fm->profile->srtcm_prm,
						    modify_bits, 0, 1);
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
	struct mlx5_flow_meter_info *fm;
	int ret;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id, NULL);
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
	struct mlx5_flow_meter_info *fm;
	int ret;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id, NULL);
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
	struct mlx5_flow_meter_info *fm;
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
	fm = mlx5_flow_meter_find(priv, meter_id, NULL);
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
					      modify_bits, fm->active_state, 1);
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
	struct mlx5_flow_meter_info *fm;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id, NULL);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter object id not valid.");
	mlx5_flow_meter_stats_enable_update(fm, stats_mask);
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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_info *fm;
	uint64_t pkts;
	uint64_t bytes;
	int ret = 0;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id, NULL);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter object id not valid.");
	*stats_mask = 0;
	if (fm->bytes_dropped)
		*stats_mask |= RTE_MTR_STATS_N_BYTES_DROPPED;
	if (fm->pkts_dropped)
		*stats_mask |= RTE_MTR_STATS_N_PKTS_DROPPED;
	memset(stats, 0, sizeof(*stats));
	if (fm->drop_cnt) {
		ret = mlx5_counter_query(dev, fm->drop_cnt, clear, &pkts,
						 &bytes);
		if (ret)
			goto error;
		/* If need to read the packets, set it. */
		if (fm->pkts_dropped)
			stats->n_pkts_dropped = pkts;
		/* If need to read the bytes, set it. */
		if (fm->bytes_dropped)
			stats->n_bytes_dropped = bytes;
	}
	return 0;
error:
	return -rte_mtr_error_set(error, ret, RTE_MTR_ERROR_TYPE_STATS, NULL,
				 "Failed to read meter drop counters.");
}

static const struct rte_mtr_ops mlx5_flow_mtr_ops = {
	.capabilities_get = mlx5_flow_mtr_cap_get,
	.meter_profile_add = mlx5_flow_meter_profile_add,
	.meter_profile_delete = mlx5_flow_meter_profile_delete,
	.destroy = mlx5_flow_meter_destroy,
	.meter_enable = mlx5_flow_meter_enable,
	.meter_disable = mlx5_flow_meter_disable,
	.meter_profile_update = mlx5_flow_meter_profile_update,
	.meter_dscp_table_update = NULL,
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
 * @param mtr_idx
 *   Pointer to Meter index.
 *
 * @return
 *   Pointer to the profile found on success, NULL otherwise.
 */
struct mlx5_flow_meter_info *
mlx5_flow_meter_find(struct mlx5_priv *priv, uint32_t meter_id,
		uint32_t *mtr_idx)
{
	struct mlx5_legacy_flow_meter *legacy_fm;
	struct mlx5_legacy_flow_meters *fms = &priv->flow_meters;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_aso_mtr_pools_mng *mtrmng = priv->sh->mtrmng;
	union mlx5_l3t_data data;

	if (priv->sh->meter_aso_en) {
		rte_spinlock_lock(&mtrmng->mtrsl);
		if (mlx5_l3t_get_entry(priv->mtr_idx_tbl, meter_id, &data) ||
			!data.dword) {
			rte_spinlock_unlock(&mtrmng->mtrsl);
			return NULL;
		}
		if (mtr_idx)
			*mtr_idx = data.dword;
		aso_mtr = mlx5_aso_meter_by_idx(priv, data.dword);
		/* Remove reference taken by the mlx5_l3t_get_entry. */
		mlx5_l3t_clear_entry(priv->mtr_idx_tbl, meter_id);
		rte_spinlock_unlock(&mtrmng->mtrsl);
		return &aso_mtr->fm;
	}
	TAILQ_FOREACH(legacy_fm, fms, next)
		if (meter_id == legacy_fm->meter_id) {
			if (mtr_idx)
				*mtr_idx = legacy_fm->idx;
			return &legacy_fm->fm;
		}
	return NULL;
}

/**
 * Find meter by index.
 *
 * @param priv
 *   Pointer to mlx5_priv.
 * @param idx
 *   Meter index.
 *
 * @return
 *   Pointer to the profile found on success, NULL otherwise.
 */
struct mlx5_flow_meter_info *
flow_dv_meter_find_by_idx(struct mlx5_priv *priv, uint32_t idx)
{
	struct mlx5_aso_mtr *aso_mtr;

	if (priv->sh->meter_aso_en) {
		aso_mtr = mlx5_aso_meter_by_idx(priv, idx);
		return &aso_mtr->fm;
	} else {
		return mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_MTR], idx);
	}
}

/**
 * Attach meter to flow.
 * Unidirectional Meter creation can only be done
 * when flow direction is known, i.e. when calling meter_attach.
 *
 * @param [in] priv
 *  Pointer to mlx5 private data.
 * @param[in] fm
 *   Pointer to flow meter.
 * @param [in] attr
 *  Pointer to flow attributes.
 * @param [out] error
 *  Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_meter_attach(struct mlx5_priv *priv,
		       struct mlx5_flow_meter_info *fm,
		       const struct rte_flow_attr *attr,
		       struct rte_flow_error *error)
{
	int ret = 0;

	if (priv->sh->meter_aso_en) {
		struct mlx5_aso_mtr *aso_mtr;

		aso_mtr = container_of(fm, struct mlx5_aso_mtr, fm);
		if (mlx5_aso_mtr_wait(priv->sh, aso_mtr)) {
			return rte_flow_error_set(error, ENOENT,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL,
					"Timeout in meter configuration");
		}
		rte_spinlock_lock(&fm->sl);
		if (fm->shared || !fm->ref_cnt) {
			fm->ref_cnt++;
		} else {
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Meter cannot be shared");
			ret = -1;
		}
		rte_spinlock_unlock(&fm->sl);
	} else {
		rte_spinlock_lock(&fm->sl);
		if (fm->meter_action) {
			if (fm->shared &&
			    attr->transfer == fm->transfer &&
			    attr->ingress == fm->ingress &&
			    attr->egress == fm->egress) {
				fm->ref_cnt++;
			} else {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					fm->shared ?
					"Meter attr not match." :
					"Meter cannot be shared.");
				ret = -1;
			}
		} else {
			fm->ingress = attr->ingress;
			fm->egress = attr->egress;
			fm->transfer = attr->transfer;
			fm->ref_cnt = 1;
			/* This also creates the meter object. */
			fm->meter_action = mlx5_flow_meter_action_create(priv,
									 fm);
			if (!fm->meter_action) {
				fm->ref_cnt = 0;
				fm->ingress = 0;
				fm->egress = 0;
				fm->transfer = 0;
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"Meter action create failed.");
				ret = -1;
			}
		}
		rte_spinlock_unlock(&fm->sl);
	}
	return ret ? -rte_errno : 0;
}

/**
 * Detach meter from flow.
 *
 * @param [in] priv
 *  Pointer to mlx5 private data.
 * @param [in] fm
 *  Pointer to flow meter.
 */
void
mlx5_flow_meter_detach(struct mlx5_priv *priv,
		       struct mlx5_flow_meter_info *fm)
{
#ifdef HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER
	rte_spinlock_lock(&fm->sl);
	MLX5_ASSERT(fm->ref_cnt);
	if (--fm->ref_cnt == 0 && !priv->sh->meter_aso_en) {
		mlx5_glue->destroy_flow_action(fm->meter_action);
		fm->meter_action = NULL;
		fm->ingress = 0;
		fm->egress = 0;
		fm->transfer = 0;
	}
	rte_spinlock_unlock(&fm->sl);
#else
	(void)priv;
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
	struct mlx5_aso_mtr_pools_mng *mtrmng = priv->sh->mtrmng;
	struct mlx5_legacy_flow_meters *fms = &priv->flow_meters;
	struct mlx5_mtr_profiles *fmps = &priv->flow_meter_profiles;
	struct mlx5_flow_meter_profile *fmp;
	struct mlx5_legacy_flow_meter *legacy_fm;
	struct mlx5_flow_meter_info *fm;
	struct mlx5_aso_mtr_pool *mtr_pool;
	void *tmp;
	uint32_t i, offset, mtr_idx;

	if (priv->sh->meter_aso_en) {
		i = mtrmng->n_valid;
		while (i--) {
			mtr_pool = mtrmng->pools[i];
			for (offset = 0; offset < MLX5_ASO_MTRS_PER_POOL;
				offset++) {
				fm = &mtr_pool->mtrs[offset].fm;
				mtr_idx = MLX5_MAKE_MTR_IDX(i, offset);
				if (mlx5_flow_meter_params_flush(dev,
						fm, mtr_idx))
					return -rte_mtr_error_set
					(error, EINVAL,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					NULL, "MTR object meter profile invalid.");
			}
		}
	} else {
		TAILQ_FOREACH_SAFE(legacy_fm, fms, next, tmp) {
			fm = &legacy_fm->fm;
			if (mlx5_flow_meter_params_flush(dev, fm, 0))
				return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					NULL, "MTR object meter profile invalid.");
		}
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
