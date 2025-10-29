/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include <math.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>
#include <rte_alarm.h>
#include <rte_cycles.h>
#include <rte_mtr.h>
#include <rte_mtr_driver.h>

#include "bnxt.h"
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_ring.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"
#include "bnxt_vnic.h"
#include "hsi_struct_def_dpdk.h"

#include "tfp.h"
#include "bnxt_tf_common.h"
#include "bnxt_ulp_tf.h"
#include "bnxt_ulp_utils.h"
#include "ulp_rte_parser.h"
#include "ulp_matcher.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_fc_mgr.h"
#include "ulp_port_db.h"
#include "ulp_ha_mgr.h"
#include "ulp_tun.h"

/**
 * Meter init status
 */
int bnxt_mtr_initialized;
int32_t
bnxt_flow_mtr_init(struct bnxt *bp __rte_unused)
{
	/*
	 ** Enable metering. The meter refresh interval is set to 1K
	 ** in FW. The meters is set to drop packet and meter cache is
	 ** enabled by HW default.
	 **/
	bnxt_mtr_initialized = 1;
	BNXT_DRV_DBG(DEBUG, "Bnxt flow meter has been initialized\n");
	return 0;
}

/**
 * Get meter capabilities.
 */
static int
bnxt_flow_mtr_cap_get(struct rte_eth_dev *dev,
		      struct rte_mtr_capabilities *cap,
		      struct rte_mtr_error *error)
{
	struct bnxt *bp = dev->data->dev_private;
	uint32_t ulp_dev_id = BNXT_ULP_DEVICE_ID_LAST;
	int32_t rc = 0;

	if (!bnxt_mtr_initialized)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Bnxt meter is not initialized");

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &ulp_dev_id);
	if (rc)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Unable to get device id from ulp");

	rc = bp->ulp_ctx->ops->ulp_mtr_cap_get(bp, cap);
	if (rc)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Unable to get meter capabilities");

	return 0;
}

/**
 * Calculate mantissa and exponent for cir / eir reg.
 */
#define BNXT_CPU_CLOCK 800
#define MEGA 1000000
static inline void
bnxt_ulp_flow_mtr_xir_calc(int64_t xir, uint32_t *reg)
{
	int64_t temp;
	uint16_t m = 0;
	uint16_t e = 0;
	uint8_t *swap = 0;

	/* Special case xir == 0 ? both exp and matissa are 0. */
	if (xir == 0) {
		*reg = 0;
		return;
	}

	/*
	 * e = floor(log2(cir)) + 27
	 * a (MBps) = xir (bps) / MEGA
	 * b (MBpc) = a (MBps) / CPU_CLOCK (Mcps)
	 * e = floor(log2(b)) + 27
	 */
	temp = xir * (1 << 24) / (BNXT_CPU_CLOCK >> 3) / MEGA;
	e = log2(temp);

	/*
	 * m = round(b/2^(e-27) - 1) * 2048
	 *   = round(b*2^(27-e) - 1) * 2^11
	 *   = round(b*2^(38-e) - 2^11)
	 *
	 */
	m = (xir * (1 << (38 - e)) / BNXT_CPU_CLOCK / (MEGA / 10) + 5) / 10  - (1 << 11);
	*reg = ((m & 0x7FF) << 6) | (e & 0x3F);
	swap = (uint8_t *)reg;
	*reg = swap[0] << 16 | swap[1] << 8 | swap[2];
}

/**
 * Calculate mantissa and exponent for cbs / ebs reg.
 */
static inline void
bnxt_ulp_flow_mtr_xbs_calc(int64_t xbs, uint16_t *reg)
{
	uint16_t m = 0;
	uint16_t e = 0;

	if (xbs == 0) {
		*reg = 0;
		return;
	}

	/*
	 * e = floor(log2(xbs)) + 1
	 */
	e = log2(xbs) + 1;

	/*
	 * m = round(xbs/2^(e-1) - 1) * 128
	 *   = round(xbs*2^(1-e) - 1) * 2^7
	 *   = round(xbs*2^(8-e) - 2^7)
	 *
	 */
	m = xbs / (1 << (e - 8)) - (1 << 7);
	*reg = ((m & 0x7F) << 5) | (e & 0x1F);
	*reg = rte_cpu_to_be_16(*reg);
}

/**
 * Parse the meter profile.
 */
static inline int
bnxt_ulp_mtr_profile_parse(struct ulp_rte_act_prop *act_prop,
			     const struct rte_mtr_meter_profile *profile,
			     struct rte_mtr_error *error)
{
	uint64_t cir, cbs, eir, ebs;
	uint32_t cir_reg, eir_reg;
	uint16_t cbs_reg, ebs_reg;
	bool alg_rfc2698 = false;
	bool pm = false;

	/* Profile must not be NULL. */
	if (profile == NULL)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE,
					  NULL, "Meter profile is null.");

	if (profile->packet_mode)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE,
					  NULL,
					  "Metering packet_mode is not supported");

	switch (profile->alg) {
	case RTE_MTR_SRTCM_RFC2697:
		cir = profile->srtcm_rfc2697.cir;
		cbs = profile->srtcm_rfc2697.cbs;
		eir = 0;
		ebs = profile->srtcm_rfc2697.ebs;
		break;
	case RTE_MTR_TRTCM_RFC2698:
		cir = profile->trtcm_rfc2698.cir;
		cbs = profile->trtcm_rfc2698.cbs;
		eir = profile->trtcm_rfc2698.pir;
		ebs = profile->trtcm_rfc2698.pbs;
		alg_rfc2698 = true;
		break;
	case RTE_MTR_TRTCM_RFC4115:
		cir = profile->trtcm_rfc4115.cir;
		cbs = profile->trtcm_rfc4115.cbs;
		eir = profile->trtcm_rfc4115.eir;
		ebs = profile->trtcm_rfc4115.ebs;
		alg_rfc2698 = true;
		break;
	default:
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE,
					  NULL,
					  "Metering algorithm type is invalid");
	}

	/* The CBS and EBS must be configured so that at least one
	 * of them is larger than 0.  It is recommended that when
	 * the value of the CBS or the EBS is larger than 0, it
	 * is larger than or equal to the size of the largest possible
	 * IP packet in the stream.
	 */
	if (cbs == 0 && ebs == 0)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE,
					  NULL,
					  "CBS & EBS cannot both be 0. One of"
					  " them should be larger than the MTU");

	if (alg_rfc2698 && eir < cir)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE,
					  NULL,
					  "PIR must be equal to or greater than CIR");

	bnxt_ulp_flow_mtr_xir_calc(cir, &cir_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CIR],
	       &cir_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_CIR);

	bnxt_ulp_flow_mtr_xir_calc(eir, &eir_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EIR],
	       &eir_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_EIR);

	bnxt_ulp_flow_mtr_xbs_calc(cbs, &cbs_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBS],
	       &cbs_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBS);

	bnxt_ulp_flow_mtr_xbs_calc(ebs, &ebs_reg);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBS],
	       &ebs_reg,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBS);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_RFC2698],
	       &alg_rfc2698,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_RFC2698);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_PM],
	       &pm,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_PM);

	return 0;
}

/**
 * Add MTR profile.
 */
static int
bnxt_flow_mtr_profile_add(struct rte_eth_dev *dev,
			    uint32_t mtr_profile_id,
			    struct rte_mtr_meter_profile *profile,
			    struct rte_mtr_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	struct ulp_rte_parser_params params;
	struct ulp_rte_act_prop *act_prop = &params.act_prop;
	struct bnxt_ulp_mapper_parms mparms = { 0 };
	uint32_t act_tid;
	uint16_t func_id;
	int ret;
	uint32_t tmp_profile_id;

	if (!bnxt_mtr_initialized)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Bnxt meter is not initialized");

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (!ulp_ctx)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "ULP context is not initialized");

	/* Initialize the parser params */
	memset(&params, 0, sizeof(struct ulp_rte_parser_params));
	params.ulp_ctx = ulp_ctx;
	params.act_bitmap.bits = BNXT_ULP_ACT_BIT_METER_PROFILE;
	/* not direction from rte_mtr. Set ingress by default */
	params.dir_attr |= BNXT_ULP_FLOW_ATTR_INGRESS;

	tmp_profile_id = tfp_cpu_to_be_32(mtr_profile_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID],
	       &tmp_profile_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_ID);

	ret = bnxt_ulp_mtr_profile_parse(act_prop, profile, error);
	if (ret)
		goto parse_error;

	ret = ulp_matcher_action_match(&params, &act_tid);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto act_error;

	bnxt_ulp_init_mapper_params(&mparms, &params,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mparms.act_tid = act_tid;

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx,
					 dev->data->port_id,
					 &func_id)) {
		BNXT_DRV_DBG(ERR, "conversion of port to func id failed\n");
		goto act_error;
	}

	/* Protect flow creation */
	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		goto act_error;
	}

	ret = ulp_mapper_flow_create(params.ulp_ctx, &mparms,
				     (void *)error);
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	if (ret)
		goto act_error;

	return 0;
parse_error:
	return ret;
act_error:
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "Failed to add meter profile.");
}

/**
 * Delete meter profile.
 */
static int
bnxt_flow_mtr_profile_delete(struct rte_eth_dev *dev,
			       uint32_t mtr_profile_id,
			       struct rte_mtr_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	struct ulp_rte_parser_params params;
	struct ulp_rte_act_prop *act_prop = &params.act_prop;
	struct bnxt_ulp_mapper_parms mparms = { 0 };
	uint32_t act_tid;
	uint16_t func_id;
	int ret;
	uint32_t tmp_profile_id;

	if (!bnxt_mtr_initialized)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Bnxt meter is not initialized");

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (!ulp_ctx)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "ULP context is not initialized");

	/* Initialize the parser params */
	memset(&params, 0, sizeof(struct ulp_rte_parser_params));
	params.ulp_ctx = ulp_ctx;
	params.act_bitmap.bits = BNXT_ULP_ACT_BIT_METER_PROFILE;
	params.act_bitmap.bits |= BNXT_ULP_ACT_BIT_DELETE;
	/* not direction from rte_mtr. Set ingress by default */
	params.dir_attr |= BNXT_ULP_FLOW_ATTR_INGRESS;

	tmp_profile_id = tfp_cpu_to_be_32(mtr_profile_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID],
	       &tmp_profile_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_ID);

	ret = ulp_matcher_action_match(&params, &act_tid);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto parse_error;

	bnxt_ulp_init_mapper_params(&mparms, &params,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mparms.act_tid = act_tid;

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx,
					 dev->data->port_id,
					 &func_id)) {
		BNXT_DRV_DBG(ERR, "conversion of port to func id failed\n");
		goto parse_error;
	}

	/* Protect flow creation */
	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		goto parse_error;
	}

	ret = ulp_mapper_flow_create(params.ulp_ctx, &mparms,
				     (void *)error);
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	if (ret)
		goto parse_error;

	BNXT_DRV_DBG(DEBUG, "Bnxt flow meter profile %d deleted\n",
		     mtr_profile_id);

	return 0;

parse_error:
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "Failed to delete meter profile.");
}

/**
 * Create meter.
 */
static int
bnxt_flow_mtr_create(struct rte_eth_dev *dev, uint32_t mtr_id,
		       struct rte_mtr_params *params, int shared __rte_unused,
		       struct rte_mtr_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	struct ulp_rte_parser_params pparams;
	struct ulp_rte_act_prop *act_prop = &pparams.act_prop;
	struct bnxt_ulp_mapper_parms mparms = { 0 };
	uint32_t act_tid;
	uint16_t func_id;
	bool mtr_en = params->meter_enable ? true : false;
	int ret;
	uint32_t tmp_mtr_id, tmp_profile_id;

	if (!bnxt_mtr_initialized)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Bnxt meter is not initialized");

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (!ulp_ctx)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "ULP context is not initialized");

	/* Initialize the parser params */
	memset(&pparams, 0, sizeof(struct ulp_rte_parser_params));
	pparams.ulp_ctx = ulp_ctx;
	pparams.act_bitmap.bits = BNXT_ULP_ACT_BIT_SHARED_METER;
	/* not direction from rte_mtr. Set ingress by default */
	pparams.dir_attr |= BNXT_ULP_FLOW_ATTR_INGRESS;

	tmp_mtr_id = tfp_cpu_to_be_32(mtr_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_INST_ID],
	       &tmp_mtr_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_INST_ID);

	tmp_profile_id = tfp_cpu_to_be_32(params->meter_profile_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID],
	       &tmp_profile_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_PROF_ID);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL],
	       &mtr_en,
	       BNXT_ULP_ACT_PROP_SZ_METER_INST_MTR_VAL);

	ret = ulp_matcher_action_match(&pparams, &act_tid);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto parse_error;

	bnxt_ulp_init_mapper_params(&mparms, &pparams,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mparms.act_tid = act_tid;

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx,
					 dev->data->port_id,
					 &func_id)) {
		BNXT_DRV_DBG(ERR, "conversion of port to func id failed\n");
		goto parse_error;
	}

	/* Protect flow creation */
	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		goto parse_error;
	}

	ret = ulp_mapper_flow_create(pparams.ulp_ctx, &mparms,
				     (void *)error);
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	if (ret)
		goto parse_error;

	BNXT_DRV_DBG(DEBUG, "Bnxt flow meter %d is created\n", mtr_id);

	return 0;
parse_error:
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "Failed to add meter.");
}

/**
 * Destroy meter.
 */
static int
bnxt_flow_mtr_destroy(struct rte_eth_dev *dev,
			uint32_t mtr_id,
			struct rte_mtr_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	struct ulp_rte_parser_params pparams;
	struct ulp_rte_act_prop *act_prop = &pparams.act_prop;
	struct bnxt_ulp_mapper_parms mparms = { 0 };
	uint32_t act_tid;
	uint16_t func_id;
	int ret;
	uint32_t tmp_mtr_id;
	const struct ulp_mapper_core_ops *oper;

	if (!bnxt_mtr_initialized)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Bnxt meter is not initialized");

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (!ulp_ctx)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "ULP context is not initialized");

	/* Initialize the parser params */
	memset(&pparams, 0, sizeof(struct ulp_rte_parser_params));
	pparams.ulp_ctx = ulp_ctx;
	pparams.act_bitmap.bits = BNXT_ULP_ACT_BIT_SHARED_METER;
	pparams.act_bitmap.bits |= BNXT_ULP_ACT_BIT_DELETE;
	/* not direction from rte_mtr. Set ingress by default */
	pparams.dir_attr |= BNXT_ULP_FLOW_ATTR_INGRESS;

	tmp_mtr_id = tfp_cpu_to_be_32(mtr_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_INST_ID],
	       &tmp_mtr_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_INST_ID);

	ret = ulp_matcher_action_match(&pparams, &act_tid);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto parse_error;

	bnxt_ulp_init_mapper_params(&mparms, &pparams,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mparms.act_tid = act_tid;

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx,
					 dev->data->port_id,
					 &func_id)) {
		BNXT_DRV_DBG(ERR, "conversion of port to func id failed\n");
		goto parse_error;
	}

	/* Protect flow creation */
	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		goto parse_error;
	}

	ret = ulp_mapper_flow_create(pparams.ulp_ctx, &mparms,
				     (void *)error);
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	if (ret)
		goto parse_error;

	BNXT_DRV_DBG(DEBUG, "Bnxt flow meter %d is deleted\n", mtr_id);

	return 0;
parse_error:
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "Failed to delete meter.");
}

/**
 * Set meter valid/invalid.
 */
static int
bnxt_flow_mtr_enable_set(struct rte_eth_dev *dev,
			   uint32_t mtr_id,
			   uint8_t val,
			   struct rte_mtr_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	struct ulp_rte_parser_params pparams;
	struct ulp_rte_act_prop *act_prop = &pparams.act_prop;
	struct bnxt_ulp_mapper_parms mparms = { 0 };
	uint32_t act_tid;
	uint16_t func_id;
	int ret;
	uint32_t tmp_mtr_id;

	if (!bnxt_mtr_initialized)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Bnxt meter is not initialized");

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (!ulp_ctx)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "ULP context is not initialized");

	/* Initialize the parser params */
	memset(&pparams, 0, sizeof(struct ulp_rte_parser_params));
	pparams.ulp_ctx = ulp_ctx;
	pparams.act_bitmap.bits = BNXT_ULP_ACT_BIT_SHARED_METER;
	pparams.act_bitmap.bits |= BNXT_ULP_ACT_BIT_UPDATE;
	/* not direction from rte_mtr. Set ingress by default */
	pparams.dir_attr |= BNXT_ULP_FLOW_ATTR_INGRESS;

	tmp_mtr_id = tfp_cpu_to_be_32(mtr_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_INST_ID],
	       &tmp_mtr_id,
	       BNXT_ULP_ACT_PROP_SZ_METER_INST_ID);
	act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL_UPDATE] = 1;
	act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL] = val;

	ret = ulp_matcher_action_match(&pparams, &act_tid);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto parse_error;

	bnxt_ulp_init_mapper_params(&mparms, &pparams,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mparms.act_tid = act_tid;

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx,
					 dev->data->port_id,
					 &func_id)) {
		BNXT_DRV_DBG(ERR, "conversion of port to func id failed\n");
		goto parse_error;
	}

	/* Protect flow creation */
	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		goto parse_error;
	}

	ret = ulp_mapper_flow_create(pparams.ulp_ctx, &mparms,
				     (void *)error);
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	if (ret)
		goto parse_error;

	BNXT_DRV_DBG(DEBUG, "Bnxt flow meter %d is %s\n",
		     mtr_id, val ? "enabled" : "disabled");

	return 0;
parse_error:
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "Failed to enable/disable meter.");
}

/**
 * Enable flow meter.
 */
static int
bnxt_flow_mtr_enable(struct rte_eth_dev *dev,
		       uint32_t mtr_id,
		       struct rte_mtr_error *error)
{
	return bnxt_flow_mtr_enable_set(dev, mtr_id, 1, error);
}

/**
 * Disable flow meter.
 */
static int
bnxt_flow_mtr_disable(struct rte_eth_dev *dev,
			uint32_t mtr_id,
			struct rte_mtr_error *error)
{
	return bnxt_flow_mtr_enable_set(dev, mtr_id, 0, error);
}

/**
 * Update meter profile.
 */
static int
bnxt_flow_mtr_profile_update(struct rte_eth_dev *dev __rte_unused,
			       uint32_t mtr_id __rte_unused,
			       uint32_t mtr_profile_id __rte_unused,
			       struct rte_mtr_error *error)
{
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "Meter_profile_update not supported");
}

/**
 * Udate meter stats mask.
 */
static int
bnxt_flow_mtr_stats_update(struct rte_eth_dev *dev __rte_unused,
			     uint32_t mtr_id __rte_unused,
			     uint64_t stats_mask __rte_unused,
			     struct rte_mtr_error *error)
{
	return -rte_mtr_error_set(error, ENOTSUP,
				  RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "Meter_stats_update not supported");
}

/**
 * Read meter statistics.
 */
static int
bnxt_flow_mtr_stats_read(struct rte_eth_dev *dev,
			 uint32_t mtr_id,
			 struct rte_mtr_stats *stats,
			 uint64_t *stats_mask,
			 int clear,
			 struct rte_mtr_error *error)
{
	int rc = 0;
	struct bnxt_ulp_context *ulp_ctx;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		goto error;
	}

	memset(stats, 0, sizeof(*stats));
	rc = ulp_mtr_query_count_get(ulp_ctx, mtr_id, clear, stats);
	if (unlikely(rc))
		goto error;

	*stats_mask = 0;
	*stats_mask |= RTE_MTR_STATS_N_PKTS_GREEN;
	*stats_mask |= RTE_MTR_STATS_N_PKTS_RED;
	*stats_mask |= RTE_MTR_STATS_N_PKTS_DROPPED;
	*stats_mask |= RTE_MTR_STATS_N_BYTES_GREEN;
	*stats_mask |= RTE_MTR_STATS_N_BYTES_RED;
	*stats_mask |= RTE_MTR_STATS_N_BYTES_DROPPED;

	return rc;
error:
	return -rte_mtr_error_set(error, EINVAL,
				  RTE_MTR_ERROR_TYPE_STATS, NULL,
				  "Failed to query meter.");
}

static const struct rte_mtr_ops bnxt_flow_mtr_ops = {
	.capabilities_get = bnxt_flow_mtr_cap_get,
	.meter_profile_add = bnxt_flow_mtr_profile_add,
	.meter_profile_delete = bnxt_flow_mtr_profile_delete,
	.meter_policy_validate = NULL,
	.meter_policy_add = NULL,
	.meter_policy_delete = NULL,
	.create = bnxt_flow_mtr_create,
	.destroy = bnxt_flow_mtr_destroy,
	.meter_enable = bnxt_flow_mtr_enable,
	.meter_disable = bnxt_flow_mtr_disable,
	.meter_profile_update = bnxt_flow_mtr_profile_update,
	.meter_dscp_table_update = NULL,
	.stats_update = bnxt_flow_mtr_stats_update,
	.stats_read = bnxt_flow_mtr_stats_read,
};

/**
 * Get meter operations.
 */
int
bnxt_flow_meter_ops_get(struct rte_eth_dev *dev __rte_unused, void *arg)
{
	*(const struct rte_mtr_ops **)arg = &bnxt_flow_mtr_ops;

	return 0;
}
