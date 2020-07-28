/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include "tf_device.h"
#include "tf_device_p4.h"
#include "tfp.h"
#include "tf_em.h"

struct tf;

/* Forward declarations */
static int tf_dev_unbind_p4(struct tf *tfp);

/**
 * Device specific bind function, WH+
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] shadow_copy
 *   Flag controlling shadow copy DB creation
 *
 * [in] resources
 *   Pointer to resource allocation information
 *
 * [out] dev_handle
 *   Device handle
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on parameter or internal failure.
 */
static int
tf_dev_bind_p4(struct tf *tfp,
	       bool shadow_copy,
	       struct tf_session_resources *resources,
	       struct tf_dev_info *dev_handle)
{
	int rc;
	int frc;
	struct tf_ident_cfg_parms ident_cfg;
	struct tf_tbl_cfg_parms tbl_cfg;
	struct tf_tcam_cfg_parms tcam_cfg;
	struct tf_em_cfg_parms em_cfg;
	struct tf_if_tbl_cfg_parms if_tbl_cfg;
	struct tf_global_cfg_cfg_parms global_cfg;

	dev_handle->type = TF_DEVICE_TYPE_WH;
	/* Initial function initialization */
	dev_handle->ops = &tf_dev_ops_p4_init;

	/* Initialize the modules */

	ident_cfg.num_elements = TF_IDENT_TYPE_MAX;
	ident_cfg.cfg = tf_ident_p4;
	ident_cfg.shadow_copy = shadow_copy;
	ident_cfg.resources = resources;
	rc = tf_ident_bind(tfp, &ident_cfg);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Identifier initialization failure\n");
		goto fail;
	}

	tbl_cfg.num_elements = TF_TBL_TYPE_MAX;
	tbl_cfg.cfg = tf_tbl_p4;
	tbl_cfg.shadow_copy = shadow_copy;
	tbl_cfg.resources = resources;
	rc = tf_tbl_bind(tfp, &tbl_cfg);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Table initialization failure\n");
		goto fail;
	}

	tcam_cfg.num_elements = TF_TCAM_TBL_TYPE_MAX;
	tcam_cfg.cfg = tf_tcam_p4;
	tcam_cfg.shadow_copy = shadow_copy;
	tcam_cfg.resources = resources;
	rc = tf_tcam_bind(tfp, &tcam_cfg);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "TCAM initialization failure\n");
		goto fail;
	}

	/*
	 * EEM
	 */
	em_cfg.num_elements = TF_EM_TBL_TYPE_MAX;
	em_cfg.cfg = tf_em_ext_p4;
	em_cfg.resources = resources;
	em_cfg.mem_type = TF_EEM_MEM_TYPE_HOST;
	rc = tf_em_ext_common_bind(tfp, &em_cfg);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "EEM initialization failure\n");
		goto fail;
	}

	/*
	 * EM
	 */
	em_cfg.num_elements = TF_EM_TBL_TYPE_MAX;
	em_cfg.cfg = tf_em_int_p4;
	em_cfg.resources = resources;
	em_cfg.mem_type = 0; /* Not used by EM */

	rc = tf_em_int_bind(tfp, &em_cfg);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "EM initialization failure\n");
		goto fail;
	}

	/*
	 * IF_TBL
	 */
	if_tbl_cfg.num_elements = TF_IF_TBL_TYPE_MAX;
	if_tbl_cfg.cfg = tf_if_tbl_p4;
	if_tbl_cfg.shadow_copy = shadow_copy;
	rc = tf_if_tbl_bind(tfp, &if_tbl_cfg);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "IF Table initialization failure\n");
		goto fail;
	}

	/*
	 * GLOBAL_CFG
	 */
	global_cfg.num_elements = TF_GLOBAL_CFG_TYPE_MAX;
	global_cfg.cfg = tf_global_cfg_p4;
	rc = tf_global_cfg_bind(tfp, &global_cfg);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Global Cfg initialization failure\n");
		goto fail;
	}

	/* Final function initialization */
	dev_handle->ops = &tf_dev_ops_p4;

	return 0;

 fail:
	/* Cleanup of already created modules */
	frc = tf_dev_unbind_p4(tfp);
	if (frc)
		return frc;

	return rc;
}

/**
 * Device specific unbind function, WH+
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_unbind_p4(struct tf *tfp)
{
	int rc = 0;
	bool fail = false;

	/* Unbind all the support modules. As this is only done on
	 * close we only report errors as everything has to be cleaned
	 * up regardless.
	 *
	 * In case of residuals TCAMs are cleaned up first as to
	 * invalidate the pipeline in a clean manner.
	 */
	rc = tf_tcam_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device unbind failed, TCAM\n");
		fail = true;
	}

	rc = tf_ident_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device unbind failed, Identifier\n");
		fail = true;
	}

	rc = tf_tbl_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device unbind failed, Table Type\n");
		fail = true;
	}

	rc = tf_em_ext_common_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device unbind failed, EEM\n");
		fail = true;
	}

	rc = tf_em_int_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device unbind failed, EM\n");
		fail = true;
	}

	rc = tf_if_tbl_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device unbind failed, IF Table Type\n");
		fail = true;
	}

	rc = tf_global_cfg_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device unbind failed, Global Cfg Type\n");
		fail = true;
	}

	if (fail)
		return -1;

	return rc;
}

int
tf_dev_bind(struct tf *tfp __rte_unused,
	    enum tf_device_type type,
	    bool shadow_copy,
	    struct tf_session_resources *resources,
	    struct tf_dev_info *dev_handle)
{
	switch (type) {
	case TF_DEVICE_TYPE_WH:
		return tf_dev_bind_p4(tfp,
				      shadow_copy,
				      resources,
				      dev_handle);
	default:
		TFP_DRV_LOG(ERR,
			    "No such device\n");
		return -ENODEV;
	}
}

int
tf_dev_unbind(struct tf *tfp,
	      struct tf_dev_info *dev_handle)
{
	switch (dev_handle->type) {
	case TF_DEVICE_TYPE_WH:
		return tf_dev_unbind_p4(tfp);
	default:
		TFP_DRV_LOG(ERR,
			    "No such device\n");
		return -ENODEV;
	}
}
