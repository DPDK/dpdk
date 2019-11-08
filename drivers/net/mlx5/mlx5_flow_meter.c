// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2018 Mellanox Technologies, Ltd
 */
#include <math.h>

#include <rte_malloc.h>
#include <rte_mtr.h>
#include <rte_mtr_driver.h>

#include "mlx5.h"
#include "mlx5_flow.h"

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
					  "Meter is not support");
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
					  "Meter is not support");
	/* Meter profile memory allocation. */
	fmp = rte_calloc(__func__, 1, sizeof(struct mlx5_flow_meter_profile),
			 RTE_CACHE_LINE_SIZE);
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
	rte_free(fmp);
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
					  "Meter is not support");
	/* Meter profile must exist. */
	fmp = mlx5_flow_meter_profile_find(priv, meter_profile_id);
	if (fmp == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  &meter_profile_id,
					  "Meter profile id invalid.");
	/* Check profile is unused. */
	if (fmp->ref_cnt)
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Meter profile in use.");
	/* Remove from list. */
	TAILQ_REMOVE(&priv->flow_meter_profiles, fmp, next);
	rte_free(fmp);
	return 0;
}

static const struct rte_mtr_ops mlx5_flow_mtr_ops = {
	.capabilities_get = mlx5_flow_mtr_cap_get,
	.meter_profile_add = mlx5_flow_meter_profile_add,
	.meter_profile_delete = mlx5_flow_meter_profile_delete,
	.create = NULL,
	.destroy = NULL,
	.meter_enable = NULL,
	.meter_disable = NULL,
	.meter_profile_update = NULL,
	.meter_dscp_table_update = NULL,
	.policer_actions_update = NULL,
	.stats_update = NULL,
	.stats_read = NULL,
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
