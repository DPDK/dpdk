/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "bnxt.h"
#include "tf_core.h"
#include "tf_session.h"
#include "tfp.h"

#include "tf_msg_common.h"
#include "tf_msg.h"
#include "hsi_struct_def_dpdk.h"
#include "hwrm_tf.h"

/**
 * Endian converts min and max values from the HW response to the query
 */
#define TF_HW_RESP_TO_QUERY(query, index, response, element) do {            \
	(query)->hw_query[index].min =                                       \
		tfp_le_to_cpu_16(response. element ## _min);                 \
	(query)->hw_query[index].max =                                       \
		tfp_le_to_cpu_16(response. element ## _max);                 \
} while (0)

/**
 * Endian converts the number of entries from the alloc to the request
 */
#define TF_HW_ALLOC_TO_REQ(alloc, index, request, element)                   \
	(request. num_ ## element = tfp_cpu_to_le_16((alloc)->hw_num[index]))

/**
 * Endian converts the start and stride value from the free to the request
 */
#define TF_HW_FREE_TO_REQ(hw_entry, index, request, element) do {            \
	request.element ## _start =                                          \
		tfp_cpu_to_le_16(hw_entry[index].start);                     \
	request.element ## _stride =                                         \
		tfp_cpu_to_le_16(hw_entry[index].stride);                    \
} while (0)

/**
 * Endian converts the start and stride from the HW response to the
 * alloc
 */
#define TF_HW_RESP_TO_ALLOC(hw_entry, index, response, element) do {         \
	hw_entry[index].start =                                              \
		tfp_le_to_cpu_16(response.element ## _start);                \
	hw_entry[index].stride =                                             \
		tfp_le_to_cpu_16(response.element ## _stride);               \
} while (0)

/**
 * Endian converts min and max values from the SRAM response to the
 * query
 */
#define TF_SRAM_RESP_TO_QUERY(query, index, response, element) do {          \
	(query)->sram_query[index].min =                                     \
		tfp_le_to_cpu_16(response.element ## _min);                  \
	(query)->sram_query[index].max =                                     \
		tfp_le_to_cpu_16(response.element ## _max);                  \
} while (0)

/**
 * Endian converts the number of entries from the action (alloc) to
 * the request
 */
#define TF_SRAM_ALLOC_TO_REQ(action, index, request, element)                \
	(request. num_ ## element = tfp_cpu_to_le_16((action)->sram_num[index]))

/**
 * Endian converts the start and stride value from the free to the request
 */
#define TF_SRAM_FREE_TO_REQ(sram_entry, index, request, element) do {        \
	request.element ## _start =                                          \
		tfp_cpu_to_le_16(sram_entry[index].start);                   \
	request.element ## _stride =                                         \
		tfp_cpu_to_le_16(sram_entry[index].stride);                  \
} while (0)

/**
 * Endian converts the start and stride from the HW response to the
 * alloc
 */
#define TF_SRAM_RESP_TO_ALLOC(sram_entry, index, response, element) do {     \
	sram_entry[index].start =                                            \
		tfp_le_to_cpu_16(response.element ## _start);                \
	sram_entry[index].stride =                                           \
		tfp_le_to_cpu_16(response.element ## _stride);               \
} while (0)

/**
 * Sends session open request to TF Firmware
 */
int
tf_msg_session_open(struct tf *tfp,
		    char *ctrl_chan_name,
		    uint8_t *fw_session_id)
{
	int rc;
	struct hwrm_tf_session_open_input req = { 0 };
	struct hwrm_tf_session_open_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };

	/* Populate the request */
	memcpy(&req.session_name, ctrl_chan_name, TF_SESSION_NAME_MAX);

	parms.tf_type = HWRM_TF_SESSION_OPEN;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = TF_KONG_MB;

	rc = tfp_send_msg_direct(tfp,
				 &parms);
	if (rc)
		return rc;

	*fw_session_id = resp.fw_session_id;

	return rc;
}

/**
 * Sends session attach request to TF Firmware
 */
int
tf_msg_session_attach(struct tf *tfp __rte_unused,
		      char *ctrl_chan_name __rte_unused,
		      uint8_t tf_fw_session_id __rte_unused)
{
	return -1;
}

/**
 * Sends session close request to TF Firmware
 */
int
tf_msg_session_close(struct tf *tfp)
{
	int rc;
	struct hwrm_tf_session_close_input req = { 0 };
	struct hwrm_tf_session_close_output resp = { 0 };
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);
	struct tfp_send_msg_parms parms = { 0 };

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);

	parms.tf_type = HWRM_TF_SESSION_CLOSE;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = TF_KONG_MB;

	rc = tfp_send_msg_direct(tfp,
				 &parms);
	return rc;
}

/**
 * Sends session query config request to TF Firmware
 */
int
tf_msg_session_qcfg(struct tf *tfp)
{
	int rc;
	struct hwrm_tf_session_qcfg_input  req = { 0 };
	struct hwrm_tf_session_qcfg_output resp = { 0 };
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);
	struct tfp_send_msg_parms parms = { 0 };

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);

	parms.tf_type = HWRM_TF_SESSION_QCFG,
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = TF_KONG_MB;

	rc = tfp_send_msg_direct(tfp,
				 &parms);
	return rc;
}

/**
 * Sends session HW resource query capability request to TF Firmware
 */
int
tf_msg_session_hw_resc_qcaps(struct tf *tfp,
			     enum tf_dir dir,
			     struct tf_rm_hw_query *query)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_session_hw_resc_qcaps_input req = { 0 };
	struct tf_session_hw_resc_qcaps_output resp = { 0 };
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	memset(query, 0, sizeof(*query));

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);

	MSG_PREP(parms,
		 TF_KONG_MB,
		 HWRM_TF,
		 HWRM_TFT_SESSION_HW_RESC_QCAPS,
		 req,
		 resp);

	rc = tfp_send_msg_tunneled(tfp, &parms);
	if (rc)
		return rc;

	/* Process the response */
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_L2_CTXT_TCAM, resp,
			    l2_ctx_tcam_entries);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_PROF_FUNC, resp,
			    prof_func);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_PROF_TCAM, resp,
			    prof_tcam_entries);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_EM_PROF_ID, resp,
			    em_prof_id);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_EM_REC, resp,
			    em_record_entries);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_WC_TCAM_PROF_ID, resp,
			    wc_tcam_prof_id);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_WC_TCAM, resp,
			    wc_tcam_entries);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_METER_PROF, resp,
			    meter_profiles);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_METER_INST,
			    resp, meter_inst);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_MIRROR, resp,
			    mirrors);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_UPAR, resp,
			    upar);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_SP_TCAM, resp,
			    sp_tcam_entries);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_L2_FUNC, resp,
			    l2_func);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_FKB, resp,
			    flex_key_templ);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_TBL_SCOPE, resp,
			    tbl_scope);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_EPOCH0, resp,
			    epoch0_entries);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_EPOCH1, resp,
			    epoch1_entries);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_METADATA, resp,
			    metadata);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_CT_STATE, resp,
			    ct_state);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_RANGE_PROF, resp,
			    range_prof);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_RANGE_ENTRY, resp,
			    range_entries);
	TF_HW_RESP_TO_QUERY(query, TF_RESC_TYPE_HW_LAG_ENTRY, resp,
			    lag_tbl_entries);

	return tfp_le_to_cpu_32(parms.tf_resp_code);
}

/**
 * Sends session HW resource allocation request to TF Firmware
 */
int
tf_msg_session_hw_resc_alloc(struct tf *tfp __rte_unused,
			     enum tf_dir dir,
			     struct tf_rm_hw_alloc *hw_alloc __rte_unused,
			     struct tf_rm_entry *hw_entry __rte_unused)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_session_hw_resc_alloc_input req = { 0 };
	struct tf_session_hw_resc_alloc_output resp = { 0 };
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	memset(hw_entry, 0, sizeof(*hw_entry));

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);

	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_L2_CTXT_TCAM, req,
			   l2_ctx_tcam_entries);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_PROF_FUNC, req,
			   prof_func_entries);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_PROF_TCAM, req,
			   prof_tcam_entries);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_EM_PROF_ID, req,
			   em_prof_id);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_EM_REC, req,
			   em_record_entries);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_WC_TCAM_PROF_ID, req,
			   wc_tcam_prof_id);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_WC_TCAM, req,
			   wc_tcam_entries);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_METER_PROF, req,
			   meter_profiles);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_METER_INST, req,
			   meter_inst);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_MIRROR, req,
			   mirrors);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_UPAR, req,
			   upar);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_SP_TCAM, req,
			   sp_tcam_entries);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_L2_FUNC, req,
			   l2_func);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_FKB, req,
			   flex_key_templ);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_TBL_SCOPE, req,
			   tbl_scope);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_EPOCH0, req,
			   epoch0_entries);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_EPOCH1, req,
			   epoch1_entries);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_METADATA, req,
			   metadata);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_CT_STATE, req,
			   ct_state);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_RANGE_PROF, req,
			   range_prof);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_RANGE_ENTRY, req,
			   range_entries);
	TF_HW_ALLOC_TO_REQ(hw_alloc, TF_RESC_TYPE_HW_LAG_ENTRY, req,
			   lag_tbl_entries);

	MSG_PREP(parms,
		 TF_KONG_MB,
		 HWRM_TF,
		 HWRM_TFT_SESSION_HW_RESC_ALLOC,
		 req,
		 resp);

	rc = tfp_send_msg_tunneled(tfp, &parms);
	if (rc)
		return rc;

	/* Process the response */
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_L2_CTXT_TCAM, resp,
			    l2_ctx_tcam_entries);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_PROF_FUNC, resp,
			    prof_func);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_PROF_TCAM, resp,
			    prof_tcam_entries);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_EM_PROF_ID, resp,
			    em_prof_id);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_EM_REC, resp,
			    em_record_entries);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_WC_TCAM_PROF_ID, resp,
			    wc_tcam_prof_id);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_WC_TCAM, resp,
			    wc_tcam_entries);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_METER_PROF, resp,
			    meter_profiles);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_METER_INST, resp,
			    meter_inst);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_MIRROR, resp,
			    mirrors);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_UPAR, resp,
			    upar);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_SP_TCAM, resp,
			    sp_tcam_entries);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_L2_FUNC, resp,
			    l2_func);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_FKB, resp,
			    flex_key_templ);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_TBL_SCOPE, resp,
			    tbl_scope);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_EPOCH0, resp,
			    epoch0_entries);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_EPOCH1, resp,
			    epoch1_entries);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_METADATA, resp,
			    metadata);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_CT_STATE, resp,
			    ct_state);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_RANGE_PROF, resp,
			    range_prof);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_RANGE_ENTRY, resp,
			    range_entries);
	TF_HW_RESP_TO_ALLOC(hw_entry, TF_RESC_TYPE_HW_LAG_ENTRY, resp,
			    lag_tbl_entries);

	return tfp_le_to_cpu_32(parms.tf_resp_code);
}

/**
 * Sends session HW resource free request to TF Firmware
 */
int
tf_msg_session_hw_resc_free(struct tf *tfp,
			    enum tf_dir dir,
			    struct tf_rm_entry *hw_entry)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_session_hw_resc_free_input req = { 0 };
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	memset(hw_entry, 0, sizeof(*hw_entry));

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);

	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_L2_CTXT_TCAM, req,
			  l2_ctx_tcam_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_PROF_FUNC, req,
			  prof_func);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_PROF_TCAM, req,
			  prof_tcam_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_EM_PROF_ID, req,
			  em_prof_id);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_EM_REC, req,
			  em_record_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_WC_TCAM_PROF_ID, req,
			  wc_tcam_prof_id);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_WC_TCAM, req,
			  wc_tcam_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_METER_PROF, req,
			  meter_profiles);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_METER_INST, req,
			  meter_inst);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_MIRROR, req,
			  mirrors);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_UPAR, req,
			  upar);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_SP_TCAM, req,
			  sp_tcam_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_L2_FUNC, req,
			  l2_func);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_FKB, req,
			  flex_key_templ);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_TBL_SCOPE, req,
			  tbl_scope);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_EPOCH0, req,
			  epoch0_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_EPOCH1, req,
			  epoch1_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_METADATA, req,
			  metadata);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_CT_STATE, req,
			  ct_state);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_RANGE_PROF, req,
			  range_prof);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_RANGE_ENTRY, req,
			  range_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_LAG_ENTRY, req,
			  lag_tbl_entries);

	MSG_PREP_NO_RESP(parms,
			 TF_KONG_MB,
			 HWRM_TF,
			 HWRM_TFT_SESSION_HW_RESC_FREE,
			 req);

	rc = tfp_send_msg_tunneled(tfp, &parms);
	if (rc)
		return rc;

	return tfp_le_to_cpu_32(parms.tf_resp_code);
}

/**
 * Sends session HW resource flush request to TF Firmware
 */
int
tf_msg_session_hw_resc_flush(struct tf *tfp,
			     enum tf_dir dir,
			     struct tf_rm_entry *hw_entry)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_session_hw_resc_free_input req = { 0 };
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);

	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_L2_CTXT_TCAM, req,
			  l2_ctx_tcam_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_PROF_FUNC, req,
			  prof_func);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_PROF_TCAM, req,
			  prof_tcam_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_EM_PROF_ID, req,
			  em_prof_id);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_EM_REC, req,
			  em_record_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_WC_TCAM_PROF_ID, req,
			  wc_tcam_prof_id);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_WC_TCAM, req,
			  wc_tcam_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_METER_PROF, req,
			  meter_profiles);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_METER_INST, req,
			  meter_inst);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_MIRROR, req,
			  mirrors);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_UPAR, req,
			  upar);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_SP_TCAM, req,
			  sp_tcam_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_L2_FUNC, req,
			  l2_func);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_FKB, req,
			  flex_key_templ);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_TBL_SCOPE, req,
			  tbl_scope);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_EPOCH0, req,
			  epoch0_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_EPOCH1, req,
			  epoch1_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_METADATA, req,
			  metadata);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_CT_STATE, req,
			  ct_state);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_RANGE_PROF, req,
			  range_prof);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_RANGE_ENTRY, req,
			  range_entries);
	TF_HW_FREE_TO_REQ(hw_entry, TF_RESC_TYPE_HW_LAG_ENTRY, req,
			  lag_tbl_entries);

	MSG_PREP_NO_RESP(parms,
			 TF_KONG_MB,
			 TF_TYPE_TRUFLOW,
			 HWRM_TFT_SESSION_HW_RESC_FLUSH,
			 req);

	rc = tfp_send_msg_tunneled(tfp, &parms);
	if (rc)
		return rc;

	return tfp_le_to_cpu_32(parms.tf_resp_code);
}

/**
 * Sends session SRAM resource query capability request to TF Firmware
 */
int
tf_msg_session_sram_resc_qcaps(struct tf *tfp __rte_unused,
			       enum tf_dir dir,
			       struct tf_rm_sram_query *query __rte_unused)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_session_sram_resc_qcaps_input req = { 0 };
	struct tf_session_sram_resc_qcaps_output resp = { 0 };
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);

	MSG_PREP(parms,
		 TF_KONG_MB,
		 HWRM_TF,
		 HWRM_TFT_SESSION_SRAM_RESC_QCAPS,
		 req,
		 resp);

	rc = tfp_send_msg_tunneled(tfp, &parms);
	if (rc)
		return rc;

	/* Process the response */
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_FULL_ACTION, resp,
			      full_action);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_MCG, resp,
			      mcg);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_ENCAP_8B, resp,
			      encap_8b);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_ENCAP_16B, resp,
			      encap_16b);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_ENCAP_64B, resp,
			      encap_64b);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_SP_SMAC, resp,
			      sp_smac);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_SP_SMAC_IPV4, resp,
			      sp_smac_ipv4);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_SP_SMAC_IPV6, resp,
			      sp_smac_ipv6);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_COUNTER_64B, resp,
			      counter_64b);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_NAT_SPORT, resp,
			      nat_sport);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_NAT_DPORT, resp,
			      nat_dport);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_NAT_S_IPV4, resp,
			      nat_s_ipv4);
	TF_SRAM_RESP_TO_QUERY(query, TF_RESC_TYPE_SRAM_NAT_D_IPV4, resp,
			      nat_d_ipv4);

	return tfp_le_to_cpu_32(parms.tf_resp_code);
}

/**
 * Sends session SRAM resource allocation request to TF Firmware
 */
int
tf_msg_session_sram_resc_alloc(struct tf *tfp __rte_unused,
			       enum tf_dir dir,
			       struct tf_rm_sram_alloc *sram_alloc __rte_unused,
			       struct tf_rm_entry *sram_entry __rte_unused)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_session_sram_resc_alloc_input req = { 0 };
	struct tf_session_sram_resc_alloc_output resp;
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	memset(&resp, 0, sizeof(resp));

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);

	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_FULL_ACTION, req,
			     full_action);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_MCG, req,
			     mcg);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_ENCAP_8B, req,
			     encap_8b);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_ENCAP_16B, req,
			     encap_16b);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_ENCAP_64B, req,
			     encap_64b);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_SP_SMAC, req,
			     sp_smac);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_SP_SMAC_IPV4,
			     req, sp_smac_ipv4);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_SP_SMAC_IPV6,
			     req, sp_smac_ipv6);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_COUNTER_64B,
			     req, counter_64b);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_NAT_SPORT, req,
			     nat_sport);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_NAT_DPORT, req,
			     nat_dport);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_NAT_S_IPV4, req,
			     nat_s_ipv4);
	TF_SRAM_ALLOC_TO_REQ(sram_alloc, TF_RESC_TYPE_SRAM_NAT_D_IPV4, req,
			     nat_d_ipv4);

	MSG_PREP(parms,
		 TF_KONG_MB,
		 HWRM_TF,
		 HWRM_TFT_SESSION_SRAM_RESC_ALLOC,
		 req,
		 resp);

	rc = tfp_send_msg_tunneled(tfp, &parms);
	if (rc)
		return rc;

	/* Process the response */
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_FULL_ACTION,
			      resp, full_action);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_MCG, resp,
			      mcg);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_ENCAP_8B, resp,
			      encap_8b);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_ENCAP_16B, resp,
			      encap_16b);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_ENCAP_64B, resp,
			      encap_64b);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_SP_SMAC, resp,
			      sp_smac);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_SP_SMAC_IPV4,
			      resp, sp_smac_ipv4);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_SP_SMAC_IPV6,
			      resp, sp_smac_ipv6);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_COUNTER_64B, resp,
			      counter_64b);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_NAT_SPORT, resp,
			      nat_sport);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_NAT_DPORT, resp,
			      nat_dport);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_NAT_S_IPV4, resp,
			      nat_s_ipv4);
	TF_SRAM_RESP_TO_ALLOC(sram_entry, TF_RESC_TYPE_SRAM_NAT_D_IPV4, resp,
			      nat_d_ipv4);

	return tfp_le_to_cpu_32(parms.tf_resp_code);
}

/**
 * Sends session SRAM resource free request to TF Firmware
 */
int
tf_msg_session_sram_resc_free(struct tf *tfp __rte_unused,
			      enum tf_dir dir,
			      struct tf_rm_entry *sram_entry __rte_unused)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_session_sram_resc_free_input req = { 0 };
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);

	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_FULL_ACTION, req,
			    full_action);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_MCG, req,
			    mcg);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_ENCAP_8B, req,
			    encap_8b);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_ENCAP_16B, req,
			    encap_16b);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_ENCAP_64B, req,
			    encap_64b);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_SP_SMAC, req,
			    sp_smac);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_SP_SMAC_IPV4, req,
			    sp_smac_ipv4);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_SP_SMAC_IPV6, req,
			    sp_smac_ipv6);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_COUNTER_64B, req,
			    counter_64b);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_NAT_SPORT, req,
			    nat_sport);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_NAT_DPORT, req,
			    nat_dport);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_NAT_S_IPV4, req,
			    nat_s_ipv4);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_NAT_D_IPV4, req,
			    nat_d_ipv4);

	MSG_PREP_NO_RESP(parms,
			 TF_KONG_MB,
			 HWRM_TF,
			 HWRM_TFT_SESSION_SRAM_RESC_FREE,
			 req);

	rc = tfp_send_msg_tunneled(tfp, &parms);
	if (rc)
		return rc;

	return tfp_le_to_cpu_32(parms.tf_resp_code);
}

/**
 * Sends session SRAM resource flush request to TF Firmware
 */
int
tf_msg_session_sram_resc_flush(struct tf *tfp,
			       enum tf_dir dir,
			       struct tf_rm_entry *sram_entry)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_session_sram_resc_free_input req = { 0 };
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);

	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_FULL_ACTION, req,
			    full_action);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_MCG, req,
			    mcg);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_ENCAP_8B, req,
			    encap_8b);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_ENCAP_16B, req,
			    encap_16b);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_ENCAP_64B, req,
			    encap_64b);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_SP_SMAC, req,
			    sp_smac);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_SP_SMAC_IPV4, req,
			    sp_smac_ipv4);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_SP_SMAC_IPV6, req,
			    sp_smac_ipv6);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_COUNTER_64B, req,
			    counter_64b);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_NAT_SPORT, req,
			    nat_sport);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_NAT_DPORT, req,
			    nat_dport);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_NAT_S_IPV4, req,
			    nat_s_ipv4);
	TF_SRAM_FREE_TO_REQ(sram_entry, TF_RESC_TYPE_SRAM_NAT_D_IPV4, req,
			    nat_d_ipv4);

	MSG_PREP_NO_RESP(parms,
			 TF_KONG_MB,
			 TF_TYPE_TRUFLOW,
			 HWRM_TFT_SESSION_SRAM_RESC_FLUSH,
			 req);

	rc = tfp_send_msg_tunneled(tfp, &parms);
	if (rc)
		return rc;

	return tfp_le_to_cpu_32(parms.tf_resp_code);
}
