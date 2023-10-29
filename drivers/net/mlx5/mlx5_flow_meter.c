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

static int mlx5_flow_meter_disable(struct rte_eth_dev *dev,
		uint32_t meter_id, struct rte_mtr_error *error);

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
	enum mlx5_meter_domain domain =
		fm->transfer ? MLX5_MTR_DOMAIN_TRANSFER :
			fm->egress ? MLX5_MTR_DOMAIN_EGRESS :
				MLX5_MTR_DOMAIN_INGRESS;
	struct mlx5_flow_meter_def_policy *def_policy =
		priv->sh->mtrmng->def_policy[domain];

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
	val = (cbs_cir >> ASO_DSEG_XIR_EXP_OFFSET) & ASO_DSEG_EXP_MASK;
	MLX5_SET(flow_meter_parameters, fmp, cir_exponent, val);
	val = (cbs_cir & ASO_DSEG_MAN_MASK);
	MLX5_SET(flow_meter_parameters, fmp, cir_mantissa, val);
	val = (ebs_eir >> ASO_DSEG_EBS_EXP_OFFSET) & ASO_DSEG_EXP_MASK;
	MLX5_SET(flow_meter_parameters, fmp, ebs_exponent, val);
	val = (ebs_eir >> ASO_DSEG_EBS_MAN_OFFSET) & ASO_DSEG_MAN_MASK;
	MLX5_SET(flow_meter_parameters, fmp, ebs_mantissa, val);
	mtr_init.next_table = def_policy->sub_policy.tbl_rsc->obj;
	mtr_init.reg_c_index = priv->sh->registers.aso_reg - REG_C_0;
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
	struct mlx5_flow_meter_profile *fmp;
	union mlx5_l3t_data data;
	int32_t ret;

	if (priv->mtr_profile_arr)
		return &priv->mtr_profile_arr[meter_profile_id];
	if (mlx5_l3t_get_entry(priv->mtr_profile_tbl,
			       meter_profile_id, &data) || !data.ptr)
		return NULL;
	fmp = data.ptr;
	/* Remove reference taken by the mlx5_l3t_get_entry. */
	ret = mlx5_l3t_clear_entry(priv->mtr_profile_tbl,
				   meter_profile_id);
	if (!ret || ret == -1)
		return NULL;
	return fmp;
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
	uint32_t ls_factor;
	int ret;
	uint64_t cir, cbs;
	uint64_t eir, ebs;
	uint64_t pir, pbs;

	/* Profile must not be NULL. */
	if (profile == NULL)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE,
					  NULL, "Meter profile is null.");
	/* Meter profile ID must be valid. */
	if (priv->mtr_profile_arr) {
		if (meter_profile_id >= priv->mtr_config.nb_meter_profiles)
			return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					NULL, "Meter profile id not valid.");
		fmp = mlx5_flow_meter_profile_find(priv, meter_profile_id);
		/* Meter profile must not exist. */
		if (fmp->initialized)
			return -rte_mtr_error_set(error, EEXIST,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					NULL, "Meter profile already exists.");
	} else {
		if (meter_profile_id == UINT32_MAX)
			return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					NULL, "Meter profile id not valid.");
		fmp = mlx5_flow_meter_profile_find(priv, meter_profile_id);
		/* Meter profile must not exist. */
		if (fmp)
			return -rte_mtr_error_set(error, EEXIST,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					NULL, "Meter profile already exists.");
	}
	if (!priv->sh->meter_aso_en) {
		/* Old version is even not supported. */
		if (!priv->sh->cdev->config.hca_attr.qos.flow_meter_old)
			return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE,
				NULL, "Metering is not supported.");
		/* Old FW metering only supports srTCM. */
		if (profile->alg != RTE_MTR_SRTCM_RFC2697) {
			return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE,
				NULL, "Metering algorithm is not supported.");
		} else if (profile->srtcm_rfc2697.ebs) {
			/* EBS is not supported for old metering. */
			return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE,
				NULL, "EBS is not supported.");
		}
		if (profile->packet_mode)
			return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"Metering algorithm packet mode is not supported.");
	}
	ls_factor = profile->packet_mode ? MLX5_MTRS_PPS_MAP_BPS_SHIFT : 0;
	switch (profile->alg) {
	case RTE_MTR_SRTCM_RFC2697:
		cir = profile->srtcm_rfc2697.cir << ls_factor;
		cbs = profile->srtcm_rfc2697.cbs << ls_factor;
		ebs = profile->srtcm_rfc2697.ebs << ls_factor;
		/* EBS could be zero for old metering. */
		if (cir > 0 && cir <= MLX5_SRTCM_XIR_MAX &&
		    cbs > 0 && cbs <= MLX5_SRTCM_XBS_MAX &&
		    ebs <= MLX5_SRTCM_XBS_MAX) {
			ret = 0;
		} else {
			ret = -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					"Profile values out of range.");
		}
		break;
	case RTE_MTR_TRTCM_RFC2698:
		cir = profile->trtcm_rfc2698.cir << ls_factor;
		cbs = profile->trtcm_rfc2698.cbs << ls_factor;
		pir = profile->trtcm_rfc2698.pir << ls_factor;
		pbs = profile->trtcm_rfc2698.pbs << ls_factor;
		if (cir > 0 && cir <= MLX5_SRTCM_XIR_MAX &&
		    cbs > 0 && cbs <= MLX5_SRTCM_XBS_MAX &&
		    pir >= cir && pir <= (MLX5_SRTCM_XIR_MAX * 2) &&
		    pbs >= cbs && pbs <= (MLX5_SRTCM_XBS_MAX * 2)) {
			ret = 0;
		} else {
			ret = -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					"Profile values out of range.");
		}
		break;
	case RTE_MTR_TRTCM_RFC4115:
		cir = profile->trtcm_rfc4115.cir << ls_factor;
		cbs = profile->trtcm_rfc4115.cbs << ls_factor;
		eir = profile->trtcm_rfc4115.eir << ls_factor;
		ebs = profile->trtcm_rfc4115.ebs << ls_factor;
		if (cir > 0 && cir <= MLX5_SRTCM_XIR_MAX &&
		    cbs > 0 && cbs <= MLX5_SRTCM_XBS_MAX &&
		    eir <= MLX5_SRTCM_XIR_MAX && ebs <= MLX5_SRTCM_XBS_MAX) {
			ret = 0;
		} else {
			ret = -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					"Profile values out of range.");
		}
		break;
	default:
		ret = -rte_mtr_error_set(error, ENOTSUP,
					 RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					 "Unknown metering algorithm.");
		break;
	}
	return ret;
}

/*
 * Calculate mantissa and exponent for cir / eir.
 *
 * @param[in] xir
 *   Value to be calculated.
 * @param[out] man
 *   Pointer to the mantissa.
 * @param[out] exp
 *   Pointer to the exp.
 */
static inline void
mlx5_flow_meter_xir_man_exp_calc(int64_t xir, uint8_t *man, uint8_t *exp)
{
	int64_t _xir;
	int64_t delta = INT64_MAX;
	uint8_t _man = 0;
	uint8_t _exp = 0;
	uint64_t m, e;

	/* Special case xir == 0 ? both exp and mantissa are 0. */
	if (xir == 0) {
		*man = 0;
		*exp = 0;
		return;
	}
	for (m = 0; m <= 0xFF; m++) { /* man width 8 bit */
		for (e = 0; e <= 0x1F; e++) { /* exp width 5bit */
			_xir = (1000000000ULL * m) >> e;
			if (llabs(xir - _xir) <= delta) {
				delta = llabs(xir - _xir);
				_man = m;
				_exp = e;
			}
		}
	}
	*man = _man;
	*exp = _exp;
}

/*
 * Calculate mantissa and exponent for xbs.
 *
 * @param[in] xbs
 *   Value to be calculated.
 * @param[out] man
 *   Pointer to the mantissa.
 * @param[out] exp
 *   Pointer to the exp.
 */
static inline void
mlx5_flow_meter_xbs_man_exp_calc(uint64_t xbs, uint8_t *man, uint8_t *exp)
{
	int _exp;
	double _man;

	/* Special case xbs == 0 ? both exp and mantissa are 0. */
	if (xbs == 0) {
		*man = 0;
		*exp = 0;
		return;
	}
	/* xbs = xbs_mantissa * 2^xbs_exponent */
	_man = frexp(xbs, &_exp);
	if (_exp >= MLX5_MAN_WIDTH) {
		_man = _man * pow(2, MLX5_MAN_WIDTH);
		_exp = _exp - MLX5_MAN_WIDTH;
	}
	*man = (uint8_t)ceil(_man);
	*exp = _exp;
}

/**
 * Fill the prm meter parameter.
 *
 * @param[in,out] fmp
 *   Pointer to meter profile to be converted.
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
	uint32_t eir_exp, eir_man, ebs_exp, ebs_man;
	uint64_t cir, cbs, eir, ebs;

	switch (fmp->profile.alg) {
	case RTE_MTR_SRTCM_RFC2697:
		cir = fmp->profile.srtcm_rfc2697.cir;
		cbs = fmp->profile.srtcm_rfc2697.cbs;
		eir = 0;
		ebs = fmp->profile.srtcm_rfc2697.ebs;
		break;
	case RTE_MTR_TRTCM_RFC2698:
		MLX5_ASSERT(fmp->profile.trtcm_rfc2698.pir >
			    fmp->profile.trtcm_rfc2698.cir &&
			    fmp->profile.trtcm_rfc2698.pbs >
			    fmp->profile.trtcm_rfc2698.cbs);
		cir = fmp->profile.trtcm_rfc2698.cir;
		cbs = fmp->profile.trtcm_rfc2698.cbs;
		/* EIR / EBS are filled with PIR / PBS. */
		eir = fmp->profile.trtcm_rfc2698.pir;
		ebs = fmp->profile.trtcm_rfc2698.pbs;
		break;
	case RTE_MTR_TRTCM_RFC4115:
		cir = fmp->profile.trtcm_rfc4115.cir;
		cbs = fmp->profile.trtcm_rfc4115.cbs;
		eir = fmp->profile.trtcm_rfc4115.eir;
		ebs = fmp->profile.trtcm_rfc4115.ebs;
		break;
	default:
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"Metering algorithm mode is invalid");
	}
	/* Adjust the values for PPS mode. */
	if (fmp->profile.packet_mode) {
		cir <<= MLX5_MTRS_PPS_MAP_BPS_SHIFT;
		cbs <<= MLX5_MTRS_PPS_MAP_BPS_SHIFT;
		eir <<= MLX5_MTRS_PPS_MAP_BPS_SHIFT;
		ebs <<= MLX5_MTRS_PPS_MAP_BPS_SHIFT;
	}
	/* cir = 8G * cir_mantissa * 1/(2^cir_exponent)) Bytes/Sec */
	mlx5_flow_meter_xir_man_exp_calc(cir, &man, &exp);
	/* Check if cir mantissa is too large. */
	if (exp > ASO_DSEG_XIR_EXP_MASK)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "meter profile parameter cir is not supported.");
	cir_man = man;
	cir_exp = exp;
	 /* cbs = cbs_mantissa * 2^cbs_exponent */
	mlx5_flow_meter_xbs_man_exp_calc(cbs, &man, &exp);
	/* Check if cbs mantissa is too large. */
	if (exp > ASO_DSEG_EXP_MASK)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "meter profile parameter cbs is not supported.");
	cbs_man = man;
	cbs_exp = exp;
	srtcm->cbs_cir = rte_cpu_to_be_32(cbs_exp << ASO_DSEG_CBS_EXP_OFFSET |
					  cbs_man << ASO_DSEG_CBS_MAN_OFFSET |
					  cir_exp << ASO_DSEG_XIR_EXP_OFFSET |
					  cir_man);
	mlx5_flow_meter_xir_man_exp_calc(eir, &man, &exp);
	/* Check if eir mantissa is too large. */
	if (exp > ASO_DSEG_XIR_EXP_MASK)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "meter profile parameter eir is not supported.");
	eir_man = man;
	eir_exp = exp;
	mlx5_flow_meter_xbs_man_exp_calc(ebs, &man, &exp);
	/* Check if ebs mantissa is too large. */
	if (exp > ASO_DSEG_EXP_MASK)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "meter profile parameter ebs is not supported.");
	ebs_man = man;
	ebs_exp = exp;
	srtcm->ebs_eir = rte_cpu_to_be_32(ebs_exp << ASO_DSEG_EBS_EXP_OFFSET |
					  ebs_man << ASO_DSEG_EBS_MAN_OFFSET |
					  eir_exp << ASO_DSEG_XIR_EXP_OFFSET |
					  eir_man);
	if (srtcm->cbs_cir)
		fmp->g_support = 1;
	if (srtcm->ebs_eir)
		fmp->y_support = 1;
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
	struct mlx5_hca_qos_attr *qattr = &priv->sh->cdev->config.hca_attr.qos;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not supported");
	memset(cap, 0, sizeof(*cap));
	if (priv->sh->meter_aso_en) {
		/* 2 meters per one ASO cache line. */
		cap->n_max = 1 << (qattr->log_max_num_meter_aso + 1);
		cap->srtcm_rfc2697_packet_mode_supported = 1;
		cap->trtcm_rfc2698_packet_mode_supported = 1;
		cap->trtcm_rfc4115_packet_mode_supported = 1;
	} else {
		cap->n_max = 1 << qattr->log_max_flow_meter;
	}
	cap->srtcm_rfc2697_byte_mode_supported = 1;
	cap->trtcm_rfc2698_byte_mode_supported = 1;
	cap->trtcm_rfc4115_byte_mode_supported = 1;
	cap->n_shared_max = cap->n_max;
	cap->identical = 1;
	cap->shared_identical = 1;
	cap->shared_n_flows_per_mtr_max = 4 << 20;
	/* 2M flows can share the same meter. */
	cap->chaining_n_mtrs_per_flow_max = 1; /* Chaining is not supported. */
	cap->meter_srtcm_rfc2697_n_max = qattr->flow_meter_old ? cap->n_max : 0;
	cap->meter_trtcm_rfc2698_n_max = qattr->flow_meter_old ? cap->n_max : 0;
	cap->meter_trtcm_rfc4115_n_max = qattr->flow_meter_old ? cap->n_max : 0;
	cap->meter_rate_max = 1ULL << 40; /* 1 Tera tokens per sec. */
	cap->meter_policy_n_max = cap->n_max;
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
	struct mlx5_flow_meter_profile *fmp;
	union mlx5_l3t_data data;
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
	data.ptr = fmp;
	ret = mlx5_l3t_set_entry(priv->mtr_profile_tbl,
				 meter_profile_id, &data);
	if (ret)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Meter profile insert fail.");
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
	if (mlx5_l3t_clear_entry(priv->mtr_profile_tbl, meter_profile_id))
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Meter profile remove fail.");
	mlx5_free(fmp);
	return 0;
}

/**
 * Callback to get MTR profile.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] meter_profile_id
 *   Meter profile id.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   A valid handle in case of success, NULL otherwise.
 */
static struct rte_flow_meter_profile *
mlx5_flow_meter_profile_get(struct rte_eth_dev *dev,
			  uint32_t meter_profile_id,
			  struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->mtr_en) {
		rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
				  "Meter is not supported");
		return NULL;
	}
	return (void *)(uintptr_t)mlx5_flow_meter_profile_find(priv,
							meter_profile_id);
}

/**
 * Callback to add MTR profile with HWS.
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
mlx5_flow_meter_profile_hws_add(struct rte_eth_dev *dev,
			uint32_t meter_profile_id,
			struct rte_mtr_meter_profile *profile,
			struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_profile *fmp;
	int ret;

	if (!priv->mtr_profile_arr)
		return mlx5_flow_meter_profile_add(dev, meter_profile_id, profile, error);
	/* Check input params. */
	ret = mlx5_flow_meter_profile_validate(dev, meter_profile_id,
					       profile, error);
	if (ret)
		return ret;
	fmp = mlx5_flow_meter_profile_find(priv, meter_profile_id);
	/* Fill profile info. */
	fmp->id = meter_profile_id;
	fmp->profile = *profile;
	fmp->initialized = 1;
	/* Fill the flow meter parameters for the PRM. */
	return mlx5_flow_meter_param_fill(fmp, error);
}

/**
 * Callback to delete MTR profile with HWS.
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
mlx5_flow_meter_profile_hws_delete(struct rte_eth_dev *dev,
			uint32_t meter_profile_id,
			struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_profile *fmp;

	if (!priv->mtr_profile_arr)
		return mlx5_flow_meter_profile_delete(dev, meter_profile_id, error);
	/* Meter profile must exist. */
	fmp = mlx5_flow_meter_profile_find(priv, meter_profile_id);
	if (!fmp->initialized)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  &meter_profile_id,
					  "Meter profile id is invalid.");
	/* Check profile is unused. */
	if (fmp->ref_cnt)
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					  NULL, "Meter profile is in use.");
	memset(fmp, 0, sizeof(struct mlx5_flow_meter_profile));
	return 0;
}

/**
 * Find policy by id.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param policy_id
 *   Policy id.
 *
 * @return
 *   Pointer to the policy found on success, NULL otherwise.
 */
struct mlx5_flow_meter_policy *
mlx5_flow_meter_policy_find(struct rte_eth_dev *dev,
			    uint32_t policy_id,
			    uint32_t *policy_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_sub_policy *sub_policy = NULL;
	union mlx5_l3t_data data;

	if (priv->mtr_policy_arr) {
		if (policy_idx)
			*policy_idx = policy_id;
		return &priv->mtr_policy_arr[policy_id];
	}
	if (policy_id > MLX5_MAX_SUB_POLICY_TBL_NUM || !priv->policy_idx_tbl)
		return NULL;
	if (mlx5_l3t_get_entry(priv->policy_idx_tbl, policy_id, &data) ||
				!data.dword)
		return NULL;
	if (policy_idx)
		*policy_idx = data.dword;
	sub_policy = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_MTR_POLICY],
					data.dword);
	/* Remove reference taken by the mlx5_l3t_get_entry. */
	mlx5_l3t_clear_entry(priv->policy_idx_tbl, policy_id);
	if (sub_policy)
		if (sub_policy->main_policy_id)
			return sub_policy->main_policy;
	return NULL;
}

/**
 * Get the next meter from one meter's policy in hierarchy chain.
 * Lock free, mutex should be acquired by caller.
 *
 * @param[in] priv
 *   Pointer to mlx5_priv.
 * @param[in] policy
 *   Pointer to flow meter policy.
 * @param[out] mtr_idx
 *   Pointer to Meter index.
 *
 * @return
 *   Pointer to the next meter, or NULL when fail.
 */
struct mlx5_flow_meter_info *
mlx5_flow_meter_hierarchy_next_meter(struct mlx5_priv *priv,
				     struct mlx5_flow_meter_policy *policy,
				     uint32_t *mtr_idx)
{
	int i;

	for (i = 0; i < MLX5_MTR_RTE_COLORS; i++) {
		if (policy->act_cnt[i].fate_action == MLX5_FLOW_FATE_MTR)
			return mlx5_flow_meter_find(priv,
						    policy->act_cnt[i].next_mtr_id,
						    mtr_idx);
	}
	return NULL;
}

/**
 * Get the last meter's policy from one meter's policy in hierarchy.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] policy
 *   Pointer to flow meter policy.
 *
 * @return
 *   Pointer to the final meter's policy, or NULL when fail.
 */
struct mlx5_flow_meter_policy *
mlx5_flow_meter_hierarchy_get_final_policy(struct rte_eth_dev *dev,
					struct mlx5_flow_meter_policy *policy)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_info *next_fm;
	struct mlx5_flow_meter_policy *next_policy = policy;

	while (next_policy->is_hierarchy) {
		rte_spinlock_lock(&next_policy->sl);
		next_fm = mlx5_flow_meter_hierarchy_next_meter(priv, next_policy, NULL);
		rte_spinlock_unlock(&next_policy->sl);
		if (!next_fm || next_fm->def_policy)
			return NULL;
		next_policy = mlx5_flow_meter_policy_find(dev,
						next_fm->policy_id, NULL);
		MLX5_ASSERT(next_policy);
	}
	return next_policy;
}

/**
 * Callback to check MTR policy action validate
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] actions
 *   Pointer to meter policy action detail.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_policy_validate(struct rte_eth_dev *dev,
	struct rte_mtr_meter_policy_params *policy,
	struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_attr attr = { .transfer = priv->sh->config.dv_esw_en ?
						  1 : 0 };
	bool is_rss = false;
	uint8_t policy_mode;
	uint8_t domain_bitmap;
	int ret;

	if (!priv->mtr_en || !priv->sh->meter_aso_en)
		return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL, "meter policy unsupported.");
	ret = mlx5_flow_validate_mtr_acts(dev, policy->actions, &attr,
			&is_rss, &domain_bitmap, &policy_mode, error);
	if (ret)
		return ret;
	return 0;
}

/**
 * Callback to check MTR policy action validate for HWS
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] actions
 *   Pointer to meter policy action detail.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_policy_hws_validate(struct rte_eth_dev *dev,
	struct rte_mtr_meter_policy_params *policy,
	struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_actions_template_attr attr = {
		.transfer = priv->sh->config.dv_esw_en ? 1 : 0 };
	int ret;
	int i;

	if (!priv->mtr_en || !priv->sh->meter_aso_en)
		return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL, "meter policy unsupported.");
	for (i = 0; i < RTE_COLORS; i++) {
		ret = mlx5_flow_actions_validate(dev, &attr, policy->actions[i],
						 policy->actions[i], NULL);
		if (ret)
			return ret;
	}
	return 0;
}

static int
__mlx5_flow_meter_policy_delete(struct rte_eth_dev *dev,
			uint32_t policy_id,
			struct mlx5_flow_meter_policy *mtr_policy,
			struct rte_mtr_error *error,
			bool clear_l3t)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_sub_policy *sub_policy;
	uint32_t i, j;
	uint16_t sub_policy_num;

	rte_spinlock_lock(&mtr_policy->sl);
	if (mtr_policy->ref_cnt) {
		rte_spinlock_unlock(&mtr_policy->sl);
		return -rte_mtr_error_set(error, EBUSY,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
				 NULL,
				"Meter policy object is being used.");
	}
	mlx5_flow_destroy_policy_rules(dev, mtr_policy);
	mlx5_flow_destroy_mtr_acts(dev, mtr_policy);
	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		sub_policy_num = (mtr_policy->sub_policy_num >>
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * i)) &
			MLX5_MTR_SUB_POLICY_NUM_MASK;
		if (sub_policy_num) {
			for (j = 0; j < sub_policy_num; j++) {
				sub_policy = mtr_policy->sub_policys[i][j];
				if (sub_policy)
					mlx5_ipool_free
					(priv->sh->ipool[MLX5_IPOOL_MTR_POLICY],
					sub_policy->idx);
			}
		}
	}
	if (priv->policy_idx_tbl && clear_l3t) {
		if (mlx5_l3t_clear_entry(priv->policy_idx_tbl, policy_id)) {
			rte_spinlock_unlock(&mtr_policy->sl);
			return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID, NULL,
				"Fail to delete policy in index table.");
		}
	}
	rte_spinlock_unlock(&mtr_policy->sl);
	return 0;
}

/**
 * Callback to add MTR policy.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] policy_id
 *   Pointer to policy id
 * @param[in] actions
 *   Pointer to meter policy action detail.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_policy_add(struct rte_eth_dev *dev,
			uint32_t policy_id,
			struct rte_mtr_meter_policy_params *policy,
			struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_attr attr = { .transfer = priv->sh->config.dv_esw_en ?
						  1 : 0 };
	uint32_t sub_policy_idx = 0;
	uint32_t policy_idx = 0;
	struct mlx5_flow_meter_policy *mtr_policy = NULL;
	struct mlx5_flow_meter_sub_policy *sub_policy;
	bool is_rss = false;
	uint8_t policy_mode;
	uint32_t i;
	int ret;
	uint32_t policy_size = sizeof(struct mlx5_flow_meter_policy);
	uint16_t sub_policy_num;
	uint8_t domain_bitmap = 0;
	union mlx5_l3t_data data;
	bool skip_rule = false;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "meter policy unsupported. ");
	if (policy_id == MLX5_INVALID_POLICY_ID)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					  NULL, "policy ID is invalid. ");
	if (policy_id == priv->sh->mtrmng->def_policy_id)
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					  NULL, "default policy ID exists. ");
	mtr_policy = mlx5_flow_meter_policy_find(dev, policy_id, &policy_idx);
	if (mtr_policy)
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					  NULL, "policy ID exists. ");
	ret = mlx5_flow_validate_mtr_acts(dev, policy->actions, &attr,
					  &is_rss, &domain_bitmap,
					  &policy_mode, error);
	if (ret)
		return ret;
	if (!domain_bitmap)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "fail to find policy domain.");
	if (policy_mode == MLX5_MTR_POLICY_MODE_DEF) {
		if (priv->sh->mtrmng->def_policy_id != MLX5_INVALID_POLICY_ID)
			return -rte_mtr_error_set(error, EEXIST,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
				NULL, "a policy with similar actions "
				"is already configured");
		if (mlx5_flow_create_def_policy(dev))
			return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL,
				"fail to create non-terminated policy.");
		priv->sh->mtrmng->def_policy_id = policy_id;
		return 0;
	}
	if (!priv->sh->meter_aso_en)
		return -rte_mtr_error_set(error, ENOTSUP,
			RTE_MTR_ERROR_TYPE_METER_POLICY, NULL,
			"no ASO capability to support the policy ");
	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		if (!(domain_bitmap & (1 << i)))
			continue;
		/*
		 * If RSS is found, it means that only the ingress domain can
		 * be supported. It is invalid to support RSS for one color
		 * and egress / transfer domain actions for another. Drop and
		 * jump action should have no impact.
		 */
		if (is_rss) {
			policy_size +=
				sizeof(struct mlx5_flow_meter_sub_policy *) *
				MLX5_MTR_RSS_MAX_SUB_POLICY;
			break;
		}
		policy_size += sizeof(struct mlx5_flow_meter_sub_policy *);
	}
	mtr_policy = mlx5_malloc(MLX5_MEM_ZERO, policy_size,
				 RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!mtr_policy)
		return -rte_mtr_error_set(error, ENOMEM,
				RTE_MTR_ERROR_TYPE_METER_POLICY, NULL,
				"Memory alloc failed for meter policy.");
	if (policy_mode == MLX5_MTR_POLICY_MODE_OG)
		mtr_policy->skip_y = 1;
	else if (policy_mode == MLX5_MTR_POLICY_MODE_OY)
		mtr_policy->skip_g = 1;
	policy_size = sizeof(struct mlx5_flow_meter_policy);
	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		if (!(domain_bitmap & (1 << i)))
			continue;
		if (i == MLX5_MTR_DOMAIN_INGRESS)
			mtr_policy->ingress = 1;
		if (i == MLX5_MTR_DOMAIN_EGRESS)
			mtr_policy->egress = 1;
		if (i == MLX5_MTR_DOMAIN_TRANSFER)
			mtr_policy->transfer = 1;
		sub_policy = mlx5_ipool_zmalloc
				(priv->sh->ipool[MLX5_IPOOL_MTR_POLICY],
				 &sub_policy_idx);
		if (!sub_policy ||
		    sub_policy_idx > MLX5_MAX_SUB_POLICY_TBL_NUM)
			goto policy_add_err;
		sub_policy->idx = sub_policy_idx;
		sub_policy->main_policy = mtr_policy;
		if (!policy_idx) {
			policy_idx = sub_policy_idx;
			sub_policy->main_policy_id = 1;
		}
		mtr_policy->sub_policys[i] =
			(struct mlx5_flow_meter_sub_policy **)
			((uint8_t *)mtr_policy + policy_size);
		mtr_policy->sub_policys[i][0] = sub_policy;
		sub_policy_num = (mtr_policy->sub_policy_num >>
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * i)) &
			MLX5_MTR_SUB_POLICY_NUM_MASK;
		sub_policy_num++;
		mtr_policy->sub_policy_num &= ~(MLX5_MTR_SUB_POLICY_NUM_MASK <<
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * i));
		mtr_policy->sub_policy_num |=
			(sub_policy_num & MLX5_MTR_SUB_POLICY_NUM_MASK) <<
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * i);
		/*
		 * If RSS is found, it means that only the ingress domain can
		 * be supported. It is invalid to support RSS for one color
		 * and egress / transfer domain actions for another. Drop and
		 * jump action should have no impact.
		 */
		if (is_rss) {
			mtr_policy->is_rss = 1;
			break;
		}
		policy_size += sizeof(struct mlx5_flow_meter_sub_policy *);
	}
	rte_spinlock_init(&mtr_policy->sl);
	ret = mlx5_flow_create_mtr_acts(dev, mtr_policy,
					policy->actions, &attr, error);
	if (ret)
		goto policy_add_err;
	if (mtr_policy->is_hierarchy) {
		struct mlx5_flow_meter_policy *final_policy;

		final_policy =
		mlx5_flow_meter_hierarchy_get_final_policy(dev, mtr_policy);
		if (!final_policy)
			goto policy_add_err;
		skip_rule = (final_policy->is_rss || final_policy->is_queue);
	}
	/*
	 * If either Green or Yellow has queue / RSS action, all the policy
	 * rules will be created later in the flow splitting stage.
	 */
	if (!is_rss && !mtr_policy->is_queue && !skip_rule) {
		/* Create policy rules in HW. */
		ret = mlx5_flow_create_policy_rules(dev, mtr_policy);
		if (ret)
			goto policy_add_err;
	}
	data.dword = policy_idx;
	if (!priv->policy_idx_tbl) {
		priv->policy_idx_tbl = mlx5_l3t_create(MLX5_L3T_TYPE_DWORD);
		if (!priv->policy_idx_tbl)
			goto policy_add_err;
	}
	if (mlx5_l3t_set_entry(priv->policy_idx_tbl, policy_id, &data))
		goto policy_add_err;
	return 0;
policy_add_err:
	if (mtr_policy) {
		ret = __mlx5_flow_meter_policy_delete(dev, policy_id,
			mtr_policy, error, false);
		mlx5_free(mtr_policy);
		if (ret)
			return ret;
	}
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Failed to create devx policy.");
}

/**
 * Callback to delete MTR policy.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] policy_id
 *   Meter policy id.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_policy_delete(struct rte_eth_dev *dev,
			  uint32_t policy_id,
			  struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_policy *mtr_policy;
	uint32_t policy_idx;
	int ret;

	if (policy_id == priv->sh->mtrmng->def_policy_id) {
		if (priv->sh->mtrmng->def_policy_ref_cnt > 0)
			return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID, NULL,
				"Meter policy object is being used.");
		priv->sh->mtrmng->def_policy_id = MLX5_INVALID_POLICY_ID;
		return 0;
	}
	mtr_policy = mlx5_flow_meter_policy_find(dev, policy_id, &policy_idx);
	if (!mtr_policy)
		return -rte_mtr_error_set(error, ENOTSUP,
			RTE_MTR_ERROR_TYPE_METER_POLICY_ID, NULL,
			"Meter policy id is invalid. ");
	ret = __mlx5_flow_meter_policy_delete(dev, policy_id, mtr_policy,
						error, true);
	if (ret)
		return ret;
	mlx5_free(mtr_policy);
	return 0;
}

/**
 * Callback to get MTR policy.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] policy_id
 *   Meter policy id.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   A valid handle in case of success, NULL otherwise.
 */
static struct rte_flow_meter_policy *
mlx5_flow_meter_policy_get(struct rte_eth_dev *dev,
			  uint32_t policy_id,
			  struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t policy_idx;

	if (!priv->mtr_en) {
		rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
				  "Meter is not supported");
		return NULL;
	}
	return (void *)(uintptr_t)mlx5_flow_meter_policy_find(dev, policy_id,
							      &policy_idx);
}

/**
 * Callback to delete MTR policy for HWS.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] policy_id
 *   Meter policy id.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_policy_hws_delete(struct rte_eth_dev *dev,
			  uint32_t policy_id,
			  struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_policy *mtr_policy;
	uint32_t i, j;
	uint32_t nb_flows = 0;
	int ret;
	struct rte_flow_op_attr op_attr = { .postpone = 1 };
	struct rte_flow_op_result result[RTE_COLORS * MLX5_MTR_DOMAIN_MAX];

	if (!priv->mtr_policy_arr)
		return mlx5_flow_meter_policy_delete(dev, policy_id, error);
	/* Meter policy must exist. */
	mtr_policy = mlx5_flow_meter_policy_find(dev, policy_id, NULL);
	if (!mtr_policy->initialized)
		return -rte_mtr_error_set(error, ENOENT,
			RTE_MTR_ERROR_TYPE_METER_POLICY_ID, NULL,
			"Meter policy does not exists.");
	/* Check policy is unused. */
	if (mtr_policy->ref_cnt)
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					  NULL, "Meter policy is in use.");
	rte_spinlock_lock(&priv->hw_ctrl_lock);
	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		for (j = 0; j < RTE_COLORS; j++) {
			if (mtr_policy->hws_flow_rule[i][j]) {
				ret = rte_flow_async_destroy(dev->data->port_id,
					CTRL_QUEUE_ID(priv), &op_attr,
					mtr_policy->hws_flow_rule[i][j],
					NULL, NULL);
				if (ret < 0)
					continue;
				nb_flows++;
			}
		}
	}
	ret = rte_flow_push(dev->data->port_id, CTRL_QUEUE_ID(priv), NULL);
	while (nb_flows && (ret >= 0)) {
		ret = rte_flow_pull(dev->data->port_id,
					CTRL_QUEUE_ID(priv), result,
					nb_flows, NULL);
		nb_flows -= ret;
	}
	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		if (mtr_policy->hws_flow_table[i])
			rte_flow_template_table_destroy(dev->data->port_id,
				 mtr_policy->hws_flow_table[i], NULL);
	}
	for (i = 0; i < RTE_COLORS; i++) {
		if (mtr_policy->hws_act_templ[i])
			rte_flow_actions_template_destroy(dev->data->port_id,
				 mtr_policy->hws_act_templ[i], NULL);
	}
	if (mtr_policy->hws_item_templ)
		rte_flow_pattern_template_destroy(dev->data->port_id,
				mtr_policy->hws_item_templ, NULL);
	rte_spinlock_unlock(&priv->hw_ctrl_lock);
	memset(mtr_policy, 0, sizeof(struct mlx5_flow_meter_policy));
	return 0;
}

/**
 * Callback to add MTR policy for HWS.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] policy_id
 *   Pointer to policy id
 * @param[in] actions
 *   Pointer to meter policy action detail.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_policy_hws_add(struct rte_eth_dev *dev,
			uint32_t policy_id,
			struct rte_mtr_meter_policy_params *policy,
			struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_policy *mtr_policy = NULL;
	const struct rte_flow_action *act;
	const struct rte_flow_action_meter *mtr;
	struct mlx5_flow_meter_info *fm;
	struct mlx5_flow_meter_policy *plc;
	uint8_t domain_color = MLX5_MTR_ALL_DOMAIN_BIT;
	bool is_rss = false;
	bool is_hierarchy = false;
	int i, j;
	uint32_t nb_colors = 0;
	uint32_t nb_flows = 0;
	int color;
	int ret;
	struct rte_flow_pattern_template_attr pta = {0};
	struct rte_flow_actions_template_attr ata = {0};
	struct rte_flow_template_table_attr ta = { {0}, 0 };
	struct rte_flow_op_attr op_attr = { .postpone = 1 };
	struct rte_flow_op_result result[RTE_COLORS * MLX5_MTR_DOMAIN_MAX];
	const uint32_t color_mask = (UINT32_C(1) << MLX5_MTR_COLOR_BITS) - 1;
	int color_reg_c_idx = mlx5_flow_get_reg_id(dev, MLX5_MTR_COLOR,
						   0, NULL);
	struct rte_flow_item_tag tag_spec = {
		.data = 0,
		.index = color_reg_c_idx
	};
	struct rte_flow_item_tag tag_mask = {
		.data = color_mask,
		.index = 0xff};
	struct rte_flow_item pattern[] = {
		[0] = {
			.type = (enum rte_flow_item_type)
				MLX5_RTE_FLOW_ITEM_TYPE_TAG,
			.spec = &tag_spec,
			.mask = &tag_mask,
		},
		[1] = { .type = RTE_FLOW_ITEM_TYPE_END }
	};

	if (!priv->mtr_policy_arr)
		return mlx5_flow_meter_policy_add(dev, policy_id, policy, error);
	mtr_policy = mlx5_flow_meter_policy_find(dev, policy_id, NULL);
	if (mtr_policy->initialized)
		return -rte_mtr_error_set(error, EEXIST,
			RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
			NULL, "Meter policy already exists.");
	if (!policy ||
	    (!policy->actions[RTE_COLOR_RED] &&
	    !policy->actions[RTE_COLOR_YELLOW] &&
	    !policy->actions[RTE_COLOR_GREEN]))
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "Meter policy actions are not valid.");
	if (policy->actions[RTE_COLOR_RED] == RTE_FLOW_ACTION_TYPE_END)
		mtr_policy->skip_r = 1;
	if (policy->actions[RTE_COLOR_YELLOW] == RTE_FLOW_ACTION_TYPE_END)
		mtr_policy->skip_y = 1;
	if (policy->actions[RTE_COLOR_GREEN] == RTE_FLOW_ACTION_TYPE_END)
		mtr_policy->skip_g = 1;
	if (mtr_policy->skip_r && mtr_policy->skip_y && mtr_policy->skip_g)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					  NULL, "Meter policy actions are empty.");
	for (i = 0; i < RTE_COLORS; i++) {
		act = policy->actions[i];
		while (act && act->type != RTE_FLOW_ACTION_TYPE_END) {
			switch (act->type) {
			case RTE_FLOW_ACTION_TYPE_PORT_ID:
				/* fall-through. */
			case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
				domain_color &= ~(MLX5_MTR_DOMAIN_INGRESS_BIT |
						  MLX5_MTR_DOMAIN_EGRESS_BIT);
				break;
			case RTE_FLOW_ACTION_TYPE_RSS:
				is_rss = true;
				/* fall-through. */
			case RTE_FLOW_ACTION_TYPE_QUEUE:
				domain_color &= ~(MLX5_MTR_DOMAIN_EGRESS_BIT |
						  MLX5_MTR_DOMAIN_TRANSFER_BIT);
				break;
			case RTE_FLOW_ACTION_TYPE_METER:
				is_hierarchy = true;
				mtr = act->conf;
				fm = mlx5_flow_meter_find(priv,
							  mtr->mtr_id, NULL);
				if (!fm)
					return -rte_mtr_error_set(error, EINVAL,
						RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
						"Meter not found in meter hierarchy.");
				plc = mlx5_flow_meter_policy_find(dev,
								  fm->policy_id,
								  NULL);
				MLX5_ASSERT(plc);
				domain_color &= MLX5_MTR_ALL_DOMAIN_BIT &
					(plc->ingress <<
					 MLX5_MTR_DOMAIN_INGRESS);
				domain_color &= MLX5_MTR_ALL_DOMAIN_BIT &
					(plc->egress <<
					 MLX5_MTR_DOMAIN_EGRESS);
				domain_color &= MLX5_MTR_ALL_DOMAIN_BIT &
					(plc->transfer <<
					 MLX5_MTR_DOMAIN_TRANSFER);
				break;
			default:
				break;
			}
			act++;
		}
	}
	if (priv->sh->config.dv_esw_en)
		domain_color &= ~(MLX5_MTR_DOMAIN_EGRESS_BIT |
				  MLX5_MTR_DOMAIN_TRANSFER_BIT);
	else
		domain_color &= ~MLX5_MTR_DOMAIN_TRANSFER_BIT;
	if (!domain_color)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					  NULL, "Meter policy domains are conflicting.");
	mtr_policy->is_rss = is_rss;
	mtr_policy->ingress = !!(domain_color & MLX5_MTR_DOMAIN_INGRESS_BIT);
	pta.ingress = mtr_policy->ingress;
	mtr_policy->egress = !!(domain_color & MLX5_MTR_DOMAIN_EGRESS_BIT);
	pta.egress = mtr_policy->egress;
	mtr_policy->transfer = !!(domain_color & MLX5_MTR_DOMAIN_TRANSFER_BIT);
	pta.transfer = mtr_policy->transfer;
	mtr_policy->group = MLX5_FLOW_TABLE_HWS_POLICY - policy_id;
	mtr_policy->is_hierarchy = is_hierarchy;
	mtr_policy->initialized = 1;
	rte_spinlock_lock(&priv->hw_ctrl_lock);
	mtr_policy->hws_item_templ =
		rte_flow_pattern_template_create(dev->data->port_id,
						 &pta, pattern, NULL);
	if (!mtr_policy->hws_item_templ)
		goto policy_add_err;
	for (i = 0; i < RTE_COLORS; i++) {
		if (mtr_policy->skip_g && i == RTE_COLOR_GREEN)
			continue;
		if (mtr_policy->skip_y && i == RTE_COLOR_YELLOW)
			continue;
		if (mtr_policy->skip_r && i == RTE_COLOR_RED)
			continue;
		mtr_policy->hws_act_templ[nb_colors] =
			rte_flow_actions_template_create(dev->data->port_id,
						&ata, policy->actions[i],
						policy->actions[i], NULL);
		if (!mtr_policy->hws_act_templ[nb_colors])
			goto policy_add_err;
		nb_colors++;
	}
	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		memset(&ta, 0, sizeof(ta));
		ta.nb_flows = RTE_COLORS;
		ta.flow_attr.group = mtr_policy->group;
		if (i == MLX5_MTR_DOMAIN_INGRESS) {
			if (!mtr_policy->ingress)
				continue;
			ta.flow_attr.ingress = 1;
		} else if (i == MLX5_MTR_DOMAIN_EGRESS) {
			if (!mtr_policy->egress)
				continue;
			ta.flow_attr.egress = 1;
		} else if (i == MLX5_MTR_DOMAIN_TRANSFER) {
			if (!mtr_policy->transfer)
				continue;
			ta.flow_attr.transfer = 1;
		}
		mtr_policy->hws_flow_table[i] =
			rte_flow_template_table_create(dev->data->port_id,
					&ta, &mtr_policy->hws_item_templ, 1,
					mtr_policy->hws_act_templ, nb_colors,
					NULL);
		if (!mtr_policy->hws_flow_table[i])
			goto policy_add_err;
		nb_colors = 0;
		for (j = 0; j < RTE_COLORS; j++) {
			if (mtr_policy->skip_g && j == RTE_COLOR_GREEN)
				continue;
			if (mtr_policy->skip_y && j == RTE_COLOR_YELLOW)
				continue;
			if (mtr_policy->skip_r && j == RTE_COLOR_RED)
				continue;
			color = rte_col_2_mlx5_col((enum rte_color)j);
			tag_spec.data = color;
			mtr_policy->hws_flow_rule[i][j] =
				rte_flow_async_create(dev->data->port_id,
					CTRL_QUEUE_ID(priv), &op_attr,
					mtr_policy->hws_flow_table[i],
					pattern, 0, policy->actions[j],
					nb_colors, NULL, NULL);
			if (!mtr_policy->hws_flow_rule[i][j])
				goto policy_add_err;
			nb_colors++;
			nb_flows++;
		}
		ret = rte_flow_push(dev->data->port_id,
				    CTRL_QUEUE_ID(priv), NULL);
		if (ret < 0)
			goto policy_add_err;
		while (nb_flows) {
			ret = rte_flow_pull(dev->data->port_id,
					    CTRL_QUEUE_ID(priv), result,
					    nb_flows, NULL);
			if (ret < 0)
				goto policy_add_err;
			for (j = 0; j < ret; j++) {
				if (result[j].status == RTE_FLOW_OP_ERROR)
					goto policy_add_err;
			}
			nb_flows -= ret;
		}
	}
	rte_spinlock_unlock(&priv->hw_ctrl_lock);
	return 0;
policy_add_err:
	rte_spinlock_unlock(&priv->hw_ctrl_lock);
	ret = mlx5_flow_meter_policy_hws_delete(dev, policy_id, error);
	memset(mtr_policy, 0, sizeof(struct mlx5_flow_meter_policy));
	if (ret)
		return ret;
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Failed to create meter policy.");
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
	/* Meter must use global drop action. */
	if (!priv->sh->dr_drop_action)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					  NULL,
					  "No drop action ready for meter.");
	/* Meter params must not be NULL. */
	if (params == NULL)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					  NULL, "Meter object params null.");
	/* Previous meter color is not supported. */
	if (params->use_prev_mtr_color && !priv->sh->meter_aso_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					  NULL,
					  "Previous meter color "
					  "not supported.");
	if (params->meter_policy_id == MLX5_INVALID_POLICY_ID)
		return -rte_mtr_error_set(error, ENOENT,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
				NULL, "Meter policy id not valid.");
	/* Validate meter id. */
	if (mlx5_flow_meter_find(priv, meter_id, NULL))
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
		struct mlx5_flow_meter_info *fm,
		const struct mlx5_flow_meter_srtcm_rfc2697_prm *srtcm,
		uint64_t modify_bits, uint32_t active_state, uint32_t is_enable)
{
#ifdef HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	uint32_t in[MLX5_ST_SZ_DW(flow_meter_parameters)] = { 0 };
	uint32_t *attr;
	struct mlx5dv_dr_flow_meter_attr mod_attr = { 0 };
	int ret;
	struct mlx5_aso_mtr *aso_mtr = NULL;
	uint32_t cbs_cir, ebs_eir, val;

	if (sh->meter_aso_en) {
		fm->is_enable = !!is_enable;
		aso_mtr = container_of(fm, struct mlx5_aso_mtr, fm);
		ret = mlx5_aso_meter_update_by_wqe(sh, MLX5_HW_INV_QUEUE,
						   aso_mtr, &priv->mtr_bulk,
						   NULL, true);
		if (ret)
			return ret;
		ret = mlx5_aso_mtr_wait(sh, MLX5_HW_INV_QUEUE, aso_mtr);
		if (ret)
			return ret;
	} else {
		/* Fill command parameters. */
		mod_attr.reg_c_index = sh->registers.aso_reg - REG_C_0;
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
			val = (cbs_cir >> ASO_DSEG_XIR_EXP_OFFSET) &
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
		if (fm->meter_action_g) {
			ret = mlx5_glue->dv_modify_flow_action_meter
					(fm->meter_action_g, &mod_attr,
					rte_cpu_to_be_64(modify_bits));
			if (ret)
				return ret;
		}
		/* Update succeeded modify meter parameters. */
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

static int
mlx5_flow_meter_stats_enable_update(struct rte_eth_dev *dev,
				struct mlx5_flow_meter_info *fm,
				uint64_t stats_mask)
{
	fm->bytes_dropped =
		(stats_mask & RTE_MTR_STATS_N_BYTES_DROPPED) ? 1 : 0;
	fm->pkts_dropped = (stats_mask & RTE_MTR_STATS_N_PKTS_DROPPED) ? 1 : 0;
	if (fm->bytes_dropped || fm->pkts_dropped) {
		if (!fm->drop_cnt) {
			/* Alloc policer counters. */
			fm->drop_cnt = mlx5_counter_alloc(dev);
			if (!fm->drop_cnt)
				return -1;
		}
	} else {
		if (fm->drop_cnt) {
			mlx5_counter_free(dev, fm->drop_cnt);
			fm->drop_cnt = 0;
		}
	}
	return 0;
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
	struct mlx5_legacy_flow_meters *fms = &priv->flow_meters;
	struct mlx5_flow_meter_profile *fmp;
	struct mlx5_flow_meter_info *fm;
	/* GCC fails to infer legacy_fm is set when !priv->sh->meter_aso_en. */
	struct mlx5_legacy_flow_meter *legacy_fm = NULL;
	struct mlx5_flow_meter_policy *mtr_policy = NULL;
	struct mlx5_indexed_pool_config flow_ipool_cfg = {
		.size = 0,
		.trunk_size = 64,
		.need_lock = 1,
		.type = "mlx5_flow_mtr_flow_id_pool",
	};
	struct mlx5_aso_mtr *aso_mtr;
	uint32_t mtr_idx, policy_idx;
	union mlx5_l3t_data data;
	int ret;
	uint8_t domain_bitmap;
	uint8_t mtr_id_bits;
	uint8_t mtr_reg_bits = priv->mtr_reg_share ?
				MLX5_MTR_IDLE_BITS_IN_COLOR_REG : MLX5_REG_BITS;

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
	/* Meter policy must exist. */
	if (params->meter_policy_id == priv->sh->mtrmng->def_policy_id) {
		__atomic_fetch_add
			(&priv->sh->mtrmng->def_policy_ref_cnt,
			1, __ATOMIC_RELAXED);
		domain_bitmap = MLX5_MTR_ALL_DOMAIN_BIT;
		if (!priv->sh->config.dv_esw_en)
			domain_bitmap &= ~MLX5_MTR_DOMAIN_TRANSFER_BIT;
	} else {
		if (!priv->sh->meter_aso_en)
			return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
				"Part of the policies cannot be "
				"supported without ASO ");
		mtr_policy = mlx5_flow_meter_policy_find(dev,
				params->meter_policy_id, &policy_idx);
		if (!mtr_policy)
			return -rte_mtr_error_set(error, ENOENT,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
				NULL, "Meter policy id not valid.");
		domain_bitmap = (mtr_policy->ingress ?
					MLX5_MTR_DOMAIN_INGRESS_BIT : 0) |
				(mtr_policy->egress ?
					MLX5_MTR_DOMAIN_EGRESS_BIT : 0) |
				(mtr_policy->transfer ?
					MLX5_MTR_DOMAIN_TRANSFER_BIT : 0);
		if (fmp->g_support && mtr_policy->skip_g)
			return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					NULL, "Meter green policy is empty.");
		if (fmp->y_support && mtr_policy->skip_y)
			return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					NULL, "Meter yellow policy is empty.");
	}
	/* Allocate the flow meter memory. */
	if (priv->sh->meter_aso_en) {
		mtr_idx = mlx5_flow_mtr_alloc(dev);
		if (!mtr_idx)
			return -rte_mtr_error_set(error, ENOMEM,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
				"Memory alloc failed for meter.");
		aso_mtr = mlx5_aso_meter_by_idx(priv, mtr_idx);
		fm = &aso_mtr->fm;
	} else {
		if (fmp->y_support)
			return -rte_mtr_error_set(error, ENOMEM,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
				"Unsupported profile with yellow.");
		legacy_fm = mlx5_ipool_zmalloc
				(priv->sh->ipool[MLX5_IPOOL_MTR], &mtr_idx);
		if (legacy_fm == NULL)
			return -rte_mtr_error_set(error, ENOMEM,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
				"Memory alloc failed for meter.");
		legacy_fm->idx = mtr_idx;
		fm = &legacy_fm->fm;
	}
	mtr_id_bits = MLX5_REG_BITS - rte_clz32(mtr_idx);
	if ((mtr_id_bits + priv->sh->mtrmng->max_mtr_flow_bits) >
	    mtr_reg_bits) {
		DRV_LOG(ERR, "Meter number exceeds max limit.");
		goto error;
	}
	if (mtr_id_bits > priv->sh->mtrmng->max_mtr_bits)
		priv->sh->mtrmng->max_mtr_bits = mtr_id_bits;
	/* Fill the flow meter parameters. */
	fm->meter_id = meter_id;
	fm->policy_id = params->meter_policy_id;
	fm->profile = fmp;
	if (mlx5_flow_meter_stats_enable_update(dev, fm, params->stats_mask))
		goto error;
	if (mlx5_flow_create_mtr_tbls(dev, fm, mtr_idx, domain_bitmap))
		goto error;
	/* Add to the flow meter list. */
	if (!priv->sh->meter_aso_en) {
		MLX5_ASSERT(legacy_fm != NULL);
		TAILQ_INSERT_TAIL(fms, legacy_fm, next);
	}
	/* Add to the flow meter list. */
	fm->active_state = 1; /* Config meter starts as active. */
	fm->is_enable = params->meter_enable;
	fm->shared = !!shared;
	fm->color_aware = !!params->use_prev_mtr_color;
	__atomic_fetch_add(&fm->profile->ref_cnt, 1, __ATOMIC_RELAXED);
	if (params->meter_policy_id == priv->sh->mtrmng->def_policy_id) {
		fm->def_policy = 1;
		fm->flow_ipool = mlx5_ipool_create(&flow_ipool_cfg);
		if (!fm->flow_ipool)
			goto error;
	}
	rte_spinlock_init(&fm->sl);
	/* If ASO meter supported, update ASO flow meter by wqe. */
	if (priv->sh->meter_aso_en) {
		aso_mtr = container_of(fm, struct mlx5_aso_mtr, fm);
		ret = mlx5_aso_meter_update_by_wqe(priv->sh, MLX5_HW_INV_QUEUE,
						   aso_mtr, &priv->mtr_bulk, NULL, true);
		if (ret)
			goto error;
		if (!priv->mtr_idx_tbl) {
			priv->mtr_idx_tbl =
				mlx5_l3t_create(MLX5_L3T_TYPE_DWORD);
			if (!priv->mtr_idx_tbl)
				goto error;
		}
		data.dword = mtr_idx;
		if (mlx5_l3t_set_entry(priv->mtr_idx_tbl, meter_id, &data))
			goto error;
	} else if (!params->meter_enable && mlx5_flow_meter_disable(dev, meter_id, error)) {
		goto error;
	}
	fm->active_state = params->meter_enable;
	if (mtr_policy)
		__atomic_fetch_add(&mtr_policy->ref_cnt, 1, __ATOMIC_RELAXED);
	return 0;
error:
	mlx5_flow_destroy_mtr_tbls(dev, fm);
	/* Free policer counters. */
	if (fm->drop_cnt)
		mlx5_counter_free(dev, fm->drop_cnt);
	if (priv->sh->meter_aso_en)
		mlx5_flow_mtr_free(dev, mtr_idx);
	else
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MTR], mtr_idx);
	return -rte_mtr_error_set(error, ENOTSUP,
		RTE_MTR_ERROR_TYPE_UNSPECIFIED,
		NULL, "Failed to create devx meter.");
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
mlx5_flow_meter_hws_create(struct rte_eth_dev *dev, uint32_t meter_id,
		       struct rte_mtr_params *params, int shared,
		       struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_profile *profile;
	struct mlx5_flow_meter_info *fm;
	struct mlx5_flow_meter_policy *policy = NULL;
	struct mlx5_aso_mtr *aso_mtr;
	int ret;

	if (!priv->mtr_profile_arr ||
	    !priv->mtr_policy_arr ||
	    !priv->mtr_bulk.aso)
		return -rte_mtr_error_set(error, ENOTSUP,
			RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
			"Meter bulk array is not allocated.");
	/* Meter profile must exist. */
	profile = mlx5_flow_meter_profile_find(priv, params->meter_profile_id);
	if (!profile->initialized)
		return -rte_mtr_error_set(error, ENOENT,
			RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
			NULL, "Meter profile id not valid.");
	/* Meter policy must exist. */
	policy = mlx5_flow_meter_policy_find(dev,
			params->meter_policy_id, NULL);
	if (!policy->initialized)
		return -rte_mtr_error_set(error, ENOENT,
			RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
			NULL, "Meter policy id not valid.");
	/* Meter ID must be valid. */
	if (meter_id >= priv->mtr_config.nb_meters)
		return -rte_mtr_error_set(error, EINVAL,
			RTE_MTR_ERROR_TYPE_MTR_ID,
			NULL, "Meter id not valid.");
	/* Find ASO object. */
	aso_mtr = mlx5_aso_meter_by_idx(priv, meter_id);
	fm = &aso_mtr->fm;
	if (fm->initialized)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter object already exists.");
	/* Fill the flow meter parameters. */
	fm->meter_id = meter_id;
	fm->policy_id = params->meter_policy_id;
	fm->profile = profile;
	fm->meter_offset = meter_id;
	fm->group = policy->group;
	/* Add to the flow meter list. */
	fm->active_state = 1; /* Config meter starts as active. */
	fm->is_enable = params->meter_enable;
	fm->shared = !!shared;
	fm->initialized = 1;
	/* Update ASO flow meter by wqe. */
	ret = mlx5_aso_meter_update_by_wqe(priv->sh, MLX5_HW_INV_QUEUE, aso_mtr,
					   &priv->mtr_bulk, NULL, true);
	if (ret)
		return -rte_mtr_error_set(error, ENOTSUP,
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,
			NULL, "Failed to create devx meter.");
	fm->active_state = params->meter_enable;
	__atomic_fetch_add(&fm->profile->ref_cnt, 1, __ATOMIC_RELAXED);
	__atomic_fetch_add(&policy->ref_cnt, 1, __ATOMIC_RELAXED);
	return 0;
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
	struct mlx5_flow_meter_policy *mtr_policy;

	/* Meter object must not have any owner. */
	MLX5_ASSERT(!fm->ref_cnt);
	/* Get meter profile. */
	fmp = fm->profile;
	if (fmp == NULL)
		return -1;
	/* Update dependencies. */
	__atomic_fetch_sub(&fmp->ref_cnt, 1, __ATOMIC_RELAXED);
	fm->profile = NULL;
	/* Remove from list. */
	if (!priv->sh->meter_aso_en) {
		legacy_fm = container_of(fm,
			struct mlx5_legacy_flow_meter, fm);
		TAILQ_REMOVE(fms, legacy_fm, next);
	}
	/* Free drop counters. */
	if (fm->drop_cnt)
		mlx5_counter_free(dev, fm->drop_cnt);
	/* Free meter flow table. */
	if (fm->flow_ipool) {
		mlx5_ipool_destroy(fm->flow_ipool);
		fm->flow_ipool = 0;
	}
	mlx5_flow_destroy_mtr_tbls(dev, fm);
	if (fm->def_policy)
		__atomic_fetch_sub(&priv->sh->mtrmng->def_policy_ref_cnt,
				1, __ATOMIC_RELAXED);
	if (priv->sh->meter_aso_en) {
		if (!fm->def_policy) {
			mtr_policy = mlx5_flow_meter_policy_find(dev,
						fm->policy_id, NULL);
			if (mtr_policy)
				__atomic_fetch_sub(&mtr_policy->ref_cnt,
						1, __ATOMIC_RELAXED);
			fm->policy_id = 0;
		}
		fm->def_policy = 0;
		if (mlx5_l3t_clear_entry(priv->mtr_idx_tbl, fm->meter_id))
			return -1;
		mlx5_flow_mtr_free(dev, mtr_idx);
	} else {
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MTR],
					legacy_fm->idx);
	}
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
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Meter is not supported");
	/* Meter object must exist. */
	fm = mlx5_flow_meter_find(priv, meter_id, &mtr_idx);
	if (fm == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL,
					  "Meter object id not valid.");
	/* Meter object must not have any owner. */
	if (fm->ref_cnt > 0)
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Meter object is being used.");
	/* Destroy the meter profile. */
	if (mlx5_flow_meter_params_flush(dev, fm, mtr_idx))
		return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
					NULL,
					"MTR object meter profile invalid.");
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
mlx5_flow_meter_hws_destroy(struct rte_eth_dev *dev, uint32_t meter_id,
			struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_flow_meter_info *fm;
	struct mlx5_flow_meter_policy *policy;

	if (!priv->mtr_profile_arr ||
	    !priv->mtr_policy_arr ||
	    !priv->mtr_bulk.aso)
		return -rte_mtr_error_set(error, ENOTSUP,
			RTE_MTR_ERROR_TYPE_METER_POLICY, NULL,
			"Meter bulk array is not allocated.");
	/* Find ASO object. */
	aso_mtr = mlx5_aso_meter_by_idx(priv, meter_id);
	fm = &aso_mtr->fm;
	if (!fm->initialized)
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter object id not valid.");
	/* Meter object must not have any owner. */
	if (fm->ref_cnt > 0)
		return -rte_mtr_error_set(error, EBUSY,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Meter object is being used.");
	/* Destroy the meter profile. */
	__atomic_fetch_sub(&fm->profile->ref_cnt,
						1, __ATOMIC_RELAXED);
	/* Destroy the meter policy. */
	policy = mlx5_flow_meter_policy_find(dev,
			fm->policy_id, NULL);
	__atomic_fetch_sub(&policy->ref_cnt,
						1, __ATOMIC_RELAXED);
	memset(fm, 0, sizeof(struct mlx5_flow_meter_info));
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
		goto dec_ref_cnt;
	ret = mlx5_flow_meter_action_modify(priv, fm, &fm->profile->srtcm_prm,
					      modify_bits, fm->active_state, 1);
	if (ret) {
		fm->profile = old_fmp;
		return -rte_mtr_error_set(error, -ret,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					  NULL, "Failed to update meter"
					  " parameters in hardware.");
	}
dec_ref_cnt:
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
	if (mlx5_flow_meter_stats_enable_update(dev, fm, stats_mask))
		return -rte_mtr_error_set(error, ENOENT,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Fail to allocate "
					  "counter for meter.");
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
						 &bytes, NULL);
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
	.meter_profile_get = mlx5_flow_meter_profile_get,
	.meter_policy_validate = mlx5_flow_meter_policy_validate,
	.meter_policy_add = mlx5_flow_meter_policy_add,
	.meter_policy_delete = mlx5_flow_meter_policy_delete,
	.meter_policy_get = mlx5_flow_meter_policy_get,
	.create = mlx5_flow_meter_create,
	.destroy = mlx5_flow_meter_destroy,
	.meter_enable = mlx5_flow_meter_enable,
	.meter_disable = mlx5_flow_meter_disable,
	.meter_profile_update = mlx5_flow_meter_profile_update,
	.meter_dscp_table_update = NULL,
	.stats_update = mlx5_flow_meter_stats_update,
	.stats_read = mlx5_flow_meter_stats_read,
};

static const struct rte_mtr_ops mlx5_flow_mtr_hws_ops = {
	.capabilities_get = mlx5_flow_mtr_cap_get,
	.meter_profile_add = mlx5_flow_meter_profile_hws_add,
	.meter_profile_delete = mlx5_flow_meter_profile_hws_delete,
	.meter_profile_get = mlx5_flow_meter_profile_get,
	.meter_policy_validate = mlx5_flow_meter_policy_hws_validate,
	.meter_policy_add = mlx5_flow_meter_policy_hws_add,
	.meter_policy_delete = mlx5_flow_meter_policy_hws_delete,
	.meter_policy_get = mlx5_flow_meter_policy_get,
	.create = mlx5_flow_meter_hws_create,
	.destroy = mlx5_flow_meter_hws_destroy,
	.meter_enable = mlx5_flow_meter_enable,
	.meter_disable = mlx5_flow_meter_disable,
	.meter_profile_update = mlx5_flow_meter_profile_update,
	.meter_dscp_table_update = NULL,
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
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->sh->config.dv_flow_en == 2)
		*(const struct rte_mtr_ops **)arg = &mlx5_flow_mtr_hws_ops;
	else
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
 *   Pointer to the meter info found on success, NULL otherwise.
 */
struct mlx5_flow_meter_info *
mlx5_flow_meter_find(struct mlx5_priv *priv, uint32_t meter_id,
		uint32_t *mtr_idx)
{
	struct mlx5_legacy_flow_meter *legacy_fm;
	struct mlx5_legacy_flow_meters *fms = &priv->flow_meters;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_aso_mtr_pools_mng *pools_mng =
				&priv->sh->mtrmng->pools_mng;
	union mlx5_l3t_data data;
	uint16_t n_valid;

	if (priv->mtr_bulk.aso) {
		if (mtr_idx)
			*mtr_idx = meter_id;
		aso_mtr = priv->mtr_bulk.aso + meter_id;
		return &aso_mtr->fm;
	}
	if (priv->sh->meter_aso_en) {
		rte_rwlock_read_lock(&pools_mng->resize_mtrwl);
		n_valid = pools_mng->n_valid;
		rte_rwlock_read_unlock(&pools_mng->resize_mtrwl);
		if (!n_valid || !priv->mtr_idx_tbl ||
		    (mlx5_l3t_get_entry(priv->mtr_idx_tbl, meter_id, &data) ||
		    !data.dword))
			return NULL;
		if (mtr_idx)
			*mtr_idx = data.dword;
		aso_mtr = mlx5_aso_meter_by_idx(priv, data.dword);
		/* Remove reference taken by the mlx5_l3t_get_entry. */
		mlx5_l3t_clear_entry(priv->mtr_idx_tbl, meter_id);
		if (!aso_mtr || aso_mtr->state == ASO_METER_FREE)
			return NULL;
		return &aso_mtr->fm;
	}
	TAILQ_FOREACH(legacy_fm, fms, next)
		if (meter_id == legacy_fm->fm.meter_id) {
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
 *   Pointer to the meter info found on success, NULL otherwise.
 */
struct mlx5_flow_meter_info *
flow_dv_meter_find_by_idx(struct mlx5_priv *priv, uint32_t idx)
{
	struct mlx5_aso_mtr *aso_mtr;

	if (priv->sh->meter_aso_en) {
		aso_mtr = mlx5_aso_meter_by_idx(priv, idx);
		if (!aso_mtr)
			return NULL;
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
		if (mlx5_aso_mtr_wait(priv->sh, MLX5_HW_INV_QUEUE, aso_mtr)) {
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
		if (fm->meter_action_g) {
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
			fm->meter_action_g = mlx5_flow_meter_action_create(priv,
									 fm);
			if (!fm->meter_action_g) {
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
		mlx5_glue->destroy_flow_action(fm->meter_action_g);
		fm->meter_action_g = NULL;
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
 * Flush meter with Rx queue configuration.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 */
void
mlx5_flow_meter_rxq_flush(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_sub_policy *sub_policy;
	struct mlx5_flow_meter_policy *mtr_policy;
	void *entry;
	uint32_t i, policy_idx;

	if (!priv->mtr_en)
		return;
	if (priv->policy_idx_tbl) {
		MLX5_L3T_FOREACH(priv->policy_idx_tbl, i, entry) {
			policy_idx = *(uint32_t *)entry;
			sub_policy = mlx5_ipool_get
				(priv->sh->ipool[MLX5_IPOOL_MTR_POLICY],
				policy_idx);
			if (!sub_policy || !sub_policy->main_policy)
				continue;
			mtr_policy = sub_policy->main_policy;
			if (mtr_policy->is_queue || mtr_policy->is_rss)
				mlx5_flow_destroy_sub_policy_with_rxq(dev,
					mtr_policy);
		}
	}
}

/**
 * Iterate a meter hierarchy and flush all meters and policies if possible.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] fm
 *   Pointer to flow meter.
 * @param[in] mtr_idx
 *   .Meter's index
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_flush_hierarchy(struct rte_eth_dev *dev,
				struct mlx5_flow_meter_info *fm,
				uint32_t mtr_idx,
				struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_policy *policy;
	uint32_t policy_id;
	struct mlx5_flow_meter_info *next_fm;
	uint32_t next_mtr_idx;
	struct mlx5_flow_meter_policy *next_policy = NULL;

	policy = mlx5_flow_meter_policy_find(dev, fm->policy_id, NULL);
	MLX5_ASSERT(policy);
	while (!fm->ref_cnt && policy->is_hierarchy) {
		policy_id = fm->policy_id;
		next_fm = mlx5_flow_meter_hierarchy_next_meter(priv, policy, &next_mtr_idx);
		if (next_fm) {
			next_policy = mlx5_flow_meter_policy_find(dev,
							next_fm->policy_id,
							NULL);
			MLX5_ASSERT(next_policy);
		}
		if (mlx5_flow_meter_params_flush(dev, fm, mtr_idx))
			return -rte_mtr_error_set(error, ENOTSUP,
						RTE_MTR_ERROR_TYPE_MTR_ID,
						NULL,
						"Failed to flush meter.");
		if (policy->ref_cnt)
			break;
		if (__mlx5_flow_meter_policy_delete(dev, policy_id,
						policy, error, true))
			return -rte_errno;
		mlx5_free(policy);
		if (!next_fm || !next_policy)
			break;
		fm = next_fm;
		mtr_idx = next_mtr_idx;
		policy = next_policy;
	}
	return 0;
}

/**
 * Flush all the hierarchy meters and their policies.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] error
 *   Pointer to rte meter error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_meter_flush_all_hierarchies(struct rte_eth_dev *dev,
				      struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_info *fm;
	struct mlx5_flow_meter_policy *policy;
	struct mlx5_flow_meter_sub_policy *sub_policy;
	struct mlx5_flow_meter_info *next_fm;
	struct mlx5_aso_mtr *aso_mtr;
	uint32_t mtr_idx = 0;
	uint32_t i, policy_idx;
	void *entry;

	if (!priv->mtr_idx_tbl || !priv->policy_idx_tbl)
		return 0;
	MLX5_L3T_FOREACH(priv->mtr_idx_tbl, i, entry) {
		mtr_idx = *(uint32_t *)entry;
		if (!mtr_idx)
			continue;
		aso_mtr = mlx5_aso_meter_by_idx(priv, mtr_idx);
		fm = &aso_mtr->fm;
		if (fm->ref_cnt || fm->def_policy)
			continue;
		if (mlx5_flow_meter_flush_hierarchy(dev, fm, mtr_idx, error))
			return -rte_errno;
	}
	MLX5_L3T_FOREACH(priv->policy_idx_tbl, i, entry) {
		policy_idx = *(uint32_t *)entry;
		sub_policy = mlx5_ipool_get
				(priv->sh->ipool[MLX5_IPOOL_MTR_POLICY],
				policy_idx);
		if (!sub_policy)
			return -rte_mtr_error_set(error,
					EINVAL,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					NULL, "Meter policy invalid.");
		policy = sub_policy->main_policy;
		if (!policy || !policy->is_hierarchy || policy->ref_cnt)
			continue;
		next_fm = mlx5_flow_meter_hierarchy_next_meter(priv, policy, &mtr_idx);
		if (__mlx5_flow_meter_policy_delete(dev, i, policy,
						    error, true))
			return -rte_mtr_error_set(error,
					EINVAL,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					NULL, "Meter policy invalid.");
		mlx5_free(policy);
		if (!next_fm || next_fm->ref_cnt || next_fm->def_policy)
			continue;
		if (mlx5_flow_meter_flush_hierarchy(dev, next_fm,
						    mtr_idx, error))
			return -rte_errno;
	}
	return 0;
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
	struct mlx5_legacy_flow_meters *fms = &priv->flow_meters;
	struct mlx5_flow_meter_profile *fmp;
	struct mlx5_legacy_flow_meter *legacy_fm;
	struct mlx5_flow_meter_info *fm;
	struct mlx5_flow_meter_policy *policy;
	struct mlx5_flow_meter_sub_policy *sub_policy;
	void *tmp;
	uint32_t i, mtr_idx, policy_idx;
	void *entry;
	struct mlx5_aso_mtr *aso_mtr;

	if (!priv->mtr_en)
		return 0;
	if (priv->sh->meter_aso_en) {
		if (mlx5_flow_meter_flush_all_hierarchies(dev, error))
			return -rte_errno;
		if (priv->mtr_idx_tbl) {
			MLX5_L3T_FOREACH(priv->mtr_idx_tbl, i, entry) {
				mtr_idx = *(uint32_t *)entry;
				if (mtr_idx) {
					aso_mtr =
					mlx5_aso_meter_by_idx(priv, mtr_idx);
					fm = &aso_mtr->fm;
					(void)mlx5_flow_meter_params_flush(dev,
						fm, mtr_idx);
				}
			}
			mlx5_l3t_destroy(priv->mtr_idx_tbl);
			priv->mtr_idx_tbl = NULL;
		}
	} else {
		RTE_TAILQ_FOREACH_SAFE(legacy_fm, fms, next, tmp) {
			fm = &legacy_fm->fm;
			if (mlx5_flow_meter_params_flush(dev, fm, 0))
				return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
				NULL, "MTR object meter profile invalid.");
		}
	}
	if (priv->mtr_bulk.aso) {
		for (i = 0; i < priv->mtr_config.nb_meters; i++) {
			aso_mtr = mlx5_aso_meter_by_idx(priv, i);
			fm = &aso_mtr->fm;
			if (fm->initialized)
				mlx5_flow_meter_hws_destroy(dev, i, error);
		}
	}
	if (priv->policy_idx_tbl) {
		MLX5_L3T_FOREACH(priv->policy_idx_tbl, i, entry) {
			policy_idx = *(uint32_t *)entry;
			sub_policy = mlx5_ipool_get
				(priv->sh->ipool[MLX5_IPOOL_MTR_POLICY],
				policy_idx);
			if (!sub_policy)
				return -rte_mtr_error_set(error,
						EINVAL,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
						NULL, "MTR object "
						"meter policy invalid.");
			if (__mlx5_flow_meter_policy_delete(dev, i,
						sub_policy->main_policy,
						error, true))
				return -rte_mtr_error_set(error,
						EINVAL,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
						NULL, "MTR object "
						"meter policy invalid.");
			mlx5_free(sub_policy->main_policy);
		}
		mlx5_l3t_destroy(priv->policy_idx_tbl);
		priv->policy_idx_tbl = NULL;
	}
	if (priv->mtr_policy_arr) {
		for (i = 0; i < priv->mtr_config.nb_meter_policies; i++) {
			policy = mlx5_flow_meter_policy_find(dev, i,
							     &policy_idx);
			if (policy->initialized)
				mlx5_flow_meter_policy_hws_delete(dev, i,
								  error);
		}
	}
	if (priv->mtr_profile_tbl) {
		MLX5_L3T_FOREACH(priv->mtr_profile_tbl, i, entry) {
			fmp = entry;
			if (mlx5_flow_meter_profile_delete(dev, fmp->id,
							   error))
				return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
						NULL, "Fail to destroy "
						"meter profile.");
		}
		mlx5_l3t_destroy(priv->mtr_profile_tbl);
		priv->mtr_profile_tbl = NULL;
	}
	if (priv->mtr_profile_arr) {
		for (i = 0; i < priv->mtr_config.nb_meter_profiles; i++) {
			fmp = mlx5_flow_meter_profile_find(priv, i);
			if (fmp->initialized)
				mlx5_flow_meter_profile_hws_delete(dev, i,
								   error);
		}
	}
	/* Delete default policy table. */
	mlx5_flow_destroy_def_policy(dev);
	if (priv->sh->refcnt == 1)
		mlx5_flow_destroy_mtr_drop_tbls(dev);
#ifdef HAVE_MLX5_HWS_SUPPORT
	/* Destroy HWS configuration. */
	mlx5_flow_meter_uninit(dev);
#endif
	return 0;
}
