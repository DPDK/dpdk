/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "tf_device.h"
#include "tf_device_p4.h"
#include "tf_device_p58.h"
#include "tfp.h"
#include "tf_em.h"
#include "tf_rm.h"
#include "tf_tcam_shared.h"
#include "tf_tbl_sram.h"

struct tf;

/* Forward declarations */
static int tf_dev_unbind_p4(struct tf *tfp);
static int tf_dev_unbind_p58(struct tf *tfp);

/**
 * Resource Reservation Check function
 *
 * [in] count
 *   Number of module subtypes
 *
 * [in] cfg
 *   Pointer to rm element config
 *
 * [in] reservations
 *   Pointer to resource reservation array
 *
 * Returns
 *   - (n) number of tables in module that have non-zero reservation count.
 */
static int
tf_dev_reservation_check(uint16_t count,
			 struct tf_rm_element_cfg *cfg,
			 uint16_t *reservations)
{
	uint16_t cnt = 0;
	uint16_t *rm_num;
	int i, j;

	for (i = 0; i < TF_DIR_MAX; i++) {
		rm_num = (uint16_t *)reservations + i * count;
		for (j = 0; j < count; j++) {
			if ((cfg[j].cfg_type == TF_RM_ELEM_CFG_HCAPI ||
			     cfg[j].cfg_type == TF_RM_ELEM_CFG_HCAPI_BA ||
			     cfg[j].cfg_type ==
				TF_RM_ELEM_CFG_HCAPI_BA_PARENT ||
			     cfg[j].cfg_type ==
				TF_RM_ELEM_CFG_HCAPI_BA_CHILD) &&
			     rm_num[j] > 0)
				cnt++;
		}
	}

	return cnt;
}

/**
 * Device specific bind function, WH+
 *
 * [in] tfp
 *   Pointer to TF handle
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
	       struct tf_session_resources *resources,
	       struct tf_dev_info *dev_handle,
	       enum tf_wc_num_slice wc_num_slices)
{
	int rc;
	int frc;
	int rsv_cnt;
	bool no_rsv_flag = true;
	struct tf_ident_cfg_parms ident_cfg;
	struct tf_tbl_cfg_parms tbl_cfg;
	struct tf_tcam_cfg_parms tcam_cfg;
	struct tf_em_cfg_parms em_cfg;
	struct tf_if_tbl_cfg_parms if_tbl_cfg;
	struct tf_global_cfg_cfg_parms global_cfg;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Initial function initialization */
	dev_handle->ops = &tf_dev_ops_p4_init;

	/* Initialize the modules */

	rsv_cnt = tf_dev_reservation_check(TF_IDENT_TYPE_MAX,
					   tf_ident_p4,
					   (uint16_t *)resources->ident_cnt);
	if (rsv_cnt) {
		ident_cfg.num_elements = TF_IDENT_TYPE_MAX;
		ident_cfg.cfg = tf_ident_p4;
		ident_cfg.resources = resources;
		rc = tf_ident_bind(tfp, &ident_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "Identifier initialization failure\n");
			goto fail;
		}

		no_rsv_flag = false;
	}

	rsv_cnt = tf_dev_reservation_check(TF_TBL_TYPE_MAX,
					   tf_tbl_p4[TF_DIR_RX],
					   (uint16_t *)resources->tbl_cnt);
	if (rsv_cnt) {
		tbl_cfg.num_elements = TF_TBL_TYPE_MAX;
		tbl_cfg.cfg = tf_tbl_p4[TF_DIR_RX];
		tbl_cfg.resources = resources;
		rc = tf_tbl_bind(tfp, &tbl_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "Table initialization failure\n");
			goto fail;
		}

		no_rsv_flag = false;
	}

	rsv_cnt = tf_dev_reservation_check(TF_TCAM_TBL_TYPE_MAX,
					   tf_tcam_p4,
					   (uint16_t *)resources->tcam_cnt);
	if (rsv_cnt) {
		tcam_cfg.num_elements = TF_TCAM_TBL_TYPE_MAX;
		tcam_cfg.cfg = tf_tcam_p4;
		tcam_cfg.resources = resources;
		tcam_cfg.wc_num_slices = wc_num_slices;
		rc = tf_tcam_shared_bind(tfp, &tcam_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "TCAM initialization failure\n");
			goto fail;
		}
		no_rsv_flag = false;
	}

	/*
	 * EEM
	 */

	em_cfg.cfg = tf_em_ext_p4;
	rsv_cnt = tf_dev_reservation_check(TF_EM_TBL_TYPE_MAX,
					   em_cfg.cfg,
					   (uint16_t *)resources->em_cnt);
	if (rsv_cnt) {
		em_cfg.num_elements = TF_EM_TBL_TYPE_MAX;
		em_cfg.resources = resources;
		em_cfg.mem_type = TF_EEM_MEM_TYPE_HOST;
		rc = tf_em_ext_common_bind(tfp, &em_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "EEM initialization failure\n");
			goto fail;
		}
		no_rsv_flag = false;
	}

	/*
	 * EM
	 */
	rsv_cnt = tf_dev_reservation_check(TF_EM_TBL_TYPE_MAX,
					   tf_em_int_p4,
					   (uint16_t *)resources->em_cnt);
	if (rsv_cnt) {
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
		no_rsv_flag = false;
	}

	/*
	 * There is no rm reserved for any tables
	 *
	 */
	if (no_rsv_flag) {
		TFP_DRV_LOG(ERR,
			    "No rm reserved for any tables\n");
		return -ENOMEM;
	}

	/*
	 * IF_TBL
	 */
	if_tbl_cfg.num_elements = TF_IF_TBL_TYPE_MAX;
	if_tbl_cfg.cfg = tf_if_tbl_p4;
	rc = tf_if_tbl_bind(tfp, &if_tbl_cfg);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "IF Table initialization failure\n");
		goto fail;
	}

	if (!tf_session_is_shared_session(tfs)) {
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
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Unbind all the support modules. As this is only done on
	 * close we only report errors as everything has to be cleaned
	 * up regardless.
	 *
	 * In case of residuals TCAMs are cleaned up first as to
	 * invalidate the pipeline in a clean manner.
	 */
	rc = tf_tcam_shared_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(INFO,
			    "Device unbind failed, TCAM\n");
		fail = true;
	}

	rc = tf_ident_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(INFO,
			    "Device unbind failed, Identifier\n");
		fail = true;
	}

	rc = tf_tbl_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(INFO,
			    "Device unbind failed, Table Type\n");
		fail = true;
	}

	rc = tf_em_ext_common_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(INFO,
			    "Device unbind failed, EEM\n");
		fail = true;
	}

	rc = tf_em_int_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(INFO,
			    "Device unbind failed, EM\n");
		fail = true;
	}

	if (!tf_session_is_shared_session(tfs)) {
		rc = tf_if_tbl_unbind(tfp);
		if (rc) {
			TFP_DRV_LOG(INFO,
				    "Device unbind failed, IF Table Type\n");
			fail = true;
		}

		rc = tf_global_cfg_unbind(tfp);
		if (rc) {
			TFP_DRV_LOG(INFO,
				    "Device unbind failed, Global Cfg Type\n");
			fail = true;
		}
	}

	if (fail)
		return -1;

	return rc;
}

/**
 * Device specific bind function, P5
 *
 * [in] tfp
 *   Pointer to TF handle
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
tf_dev_bind_p58(struct tf *tfp,
		struct tf_session_resources *resources,
		struct tf_dev_info *dev_handle,
		enum tf_wc_num_slice wc_num_slices)
{
	int rc;
	int frc;
	int rsv_cnt;
	bool no_rsv_flag = true;
	struct tf_ident_cfg_parms ident_cfg;
	struct tf_tbl_cfg_parms tbl_cfg;
	struct tf_tcam_cfg_parms tcam_cfg;
	struct tf_em_cfg_parms em_cfg;
	struct tf_if_tbl_cfg_parms if_tbl_cfg;
	struct tf_global_cfg_cfg_parms global_cfg;
	struct tf_session *tfs;

	/* Initial function initialization */
	dev_handle->ops = &tf_dev_ops_p58_init;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	rsv_cnt = tf_dev_reservation_check(TF_IDENT_TYPE_MAX,
					   tf_ident_p58,
					   (uint16_t *)resources->ident_cnt);
	if (rsv_cnt) {
		ident_cfg.num_elements = TF_IDENT_TYPE_MAX;
		ident_cfg.cfg = tf_ident_p58;
		ident_cfg.resources = resources;
		rc = tf_ident_bind(tfp, &ident_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "Identifier initialization failure\n");
			goto fail;
		}
		no_rsv_flag = false;
	}

	rsv_cnt = tf_dev_reservation_check(TF_TBL_TYPE_MAX,
					   tf_tbl_p58[TF_DIR_RX],
					   (uint16_t *)resources->tbl_cnt);
	rsv_cnt += tf_dev_reservation_check(TF_TBL_TYPE_MAX,
					   tf_tbl_p58[TF_DIR_TX],
					   (uint16_t *)resources->tbl_cnt);
	if (rsv_cnt) {
		tbl_cfg.num_elements = TF_TBL_TYPE_MAX;
		tbl_cfg.cfg = tf_tbl_p58[TF_DIR_RX];
		tbl_cfg.resources = resources;
		rc = tf_tbl_bind(tfp, &tbl_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "Table initialization failure\n");
			goto fail;
		}
		no_rsv_flag = false;

		rc = tf_tbl_sram_bind(tfp);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "SRAM table initialization failure\n");
			goto fail;
		}
	}

	rsv_cnt = tf_dev_reservation_check(TF_TCAM_TBL_TYPE_MAX,
					   tf_tcam_p58,
					   (uint16_t *)resources->tcam_cnt);
	if (rsv_cnt) {
		tcam_cfg.num_elements = TF_TCAM_TBL_TYPE_MAX;
		tcam_cfg.cfg = tf_tcam_p58;
		tcam_cfg.resources = resources;
		tcam_cfg.wc_num_slices = wc_num_slices;
		rc = tf_tcam_shared_bind(tfp, &tcam_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "TCAM initialization failure\n");
			goto fail;
		}
		no_rsv_flag = false;
	}

	/*
	 * EM
	 */
	rsv_cnt = tf_dev_reservation_check(TF_EM_TBL_TYPE_MAX,
					   tf_em_int_p58,
					   (uint16_t *)resources->em_cnt);
	if (rsv_cnt) {
		em_cfg.num_elements = TF_EM_TBL_TYPE_MAX;
		em_cfg.cfg = tf_em_int_p58;
		em_cfg.resources = resources;
		em_cfg.mem_type = 0; /* Not used by EM */

		rc = tf_em_int_bind(tfp, &em_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "EM initialization failure\n");
			goto fail;
		}
		no_rsv_flag = false;
	}

	/*
	 * There is no rm reserved for any tables
	 *
	 */
	if (no_rsv_flag) {
		TFP_DRV_LOG(ERR,
			    "No rm reserved for any tables\n");
		return -ENOMEM;
	}

	/*
	 * IF_TBL
	 */
	if_tbl_cfg.num_elements = TF_IF_TBL_TYPE_MAX;
	if_tbl_cfg.cfg = tf_if_tbl_p58;
	rc = tf_if_tbl_bind(tfp, &if_tbl_cfg);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "IF Table initialization failure\n");
		goto fail;
	}

	if (!tf_session_is_shared_session(tfs)) {
		/*
		 * GLOBAL_CFG
		 */
		global_cfg.num_elements = TF_GLOBAL_CFG_TYPE_MAX;
		global_cfg.cfg = tf_global_cfg_p58;
		rc = tf_global_cfg_bind(tfp, &global_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "Global Cfg initialization failure\n");
			goto fail;
		}
	}

	/* Final function initialization */
	dev_handle->ops = &tf_dev_ops_p58;

	return 0;

 fail:
	/* Cleanup of already created modules */
	frc = tf_dev_unbind_p58(tfp);
	if (frc)
		return frc;

	return rc;
}

/**
 * Device specific unbind function, P5
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_unbind_p58(struct tf *tfp)
{
	int rc = 0;
	bool fail = false;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Unbind all the support modules. As this is only done on
	 * close we only report errors as everything has to be cleaned
	 * up regardless.
	 *
	 * In case of residuals TCAMs are cleaned up first as to
	 * invalidate the pipeline in a clean manner.
	 */
	rc = tf_tcam_shared_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(INFO,
			    "Device unbind failed, TCAM\n");
		fail = true;
	}

	rc = tf_ident_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(INFO,
			    "Device unbind failed, Identifier\n");
		fail = true;
	}

	/* Unbind the SRAM table prior to table as the table manager
	 * owns and frees the table DB while the SRAM table manager owns
	 * and manages it's internal data structures.  SRAM table manager
	 * relies on the table rm_db to exist.
	 */
	rc = tf_tbl_sram_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device unbind failed, SRAM table\n");
		fail = true;
	}

	rc = tf_tbl_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(INFO,
			    "Device unbind failed, Table Type\n");
		fail = true;
	}

	rc = tf_em_int_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(INFO,
			    "Device unbind failed, EM\n");
		fail = true;
	}

	rc = tf_if_tbl_unbind(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device unbind failed, IF Table Type\n");
		fail = true;
	}

	if (!tf_session_is_shared_session(tfs)) {
		rc = tf_global_cfg_unbind(tfp);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "Device unbind failed, Global Cfg Type\n");
			fail = true;
		}
	}

	if (fail)
		return -1;

	return rc;
}

int
tf_dev_bind(struct tf *tfp __rte_unused,
	    enum tf_device_type type,
	    struct tf_session_resources *resources,
	    uint16_t wc_num_slices,
	    struct tf_dev_info *dev_handle)
{
	switch (type) {
	case TF_DEVICE_TYPE_P4:
	case TF_DEVICE_TYPE_SR:
		dev_handle->type = type;
		return tf_dev_bind_p4(tfp,
				      resources,
				      dev_handle,
				      wc_num_slices);
	case TF_DEVICE_TYPE_P5:
		dev_handle->type = type;
		return tf_dev_bind_p58(tfp,
				       resources,
				       dev_handle,
				       wc_num_slices);
	default:
		TFP_DRV_LOG(ERR,
			    "No such device\n");
		return -ENODEV;
	}
}

int
tf_dev_bind_ops(enum tf_device_type type,
		struct tf_dev_info *dev_handle)
{
	switch (type) {
	case TF_DEVICE_TYPE_P4:
	case TF_DEVICE_TYPE_SR:
		dev_handle->ops = &tf_dev_ops_p4_init;
		break;
	case TF_DEVICE_TYPE_P5:
		dev_handle->ops = &tf_dev_ops_p58_init;
		break;
	default:
		TFP_DRV_LOG(ERR,
			    "No such device\n");
		return -ENODEV;
	}

	return 0;
}

int
tf_dev_unbind(struct tf *tfp,
	      struct tf_dev_info *dev_handle)
{
	switch (dev_handle->type) {
	case TF_DEVICE_TYPE_P4:
	case TF_DEVICE_TYPE_SR:
		return tf_dev_unbind_p4(tfp);
	case TF_DEVICE_TYPE_P5:
		return tf_dev_unbind_p58(tfp);
	default:
		TFP_DRV_LOG(ERR,
			    "No such device\n");
		return -ENODEV;
	}
}
