/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "tf_em_common.h"
#include "tf_msg_common.h"
#include "tf_device.h"
#include "tf_msg.h"
#include "tf_util.h"
#include "tf_common.h"
#include "tf_session.h"
#include "tfp.h"
#include "tf_em.h"

/* Specific msg size defines as we cannot use defines in tf.yaml. This
 * means we have to manually sync hwrm with these defines if the
 * tf.yaml changes.
 */
#define TF_MSG_SET_GLOBAL_CFG_DATA_SIZE  16
#define TF_MSG_EM_INSERT_KEY_SIZE        64
#define TF_MSG_EM_INSERT_RECORD_SIZE     80
#define TF_MSG_TBL_TYPE_SET_DATA_SIZE    88

/* Compile check - Catch any msg changes that we depend on, like the
 * defines listed above for array size checking.
 *
 * Checking array size is dangerous in that the type could change and
 * we wouldn't be able to catch it. Thus we check if the complete msg
 * changed instead. Best we can do.
 *
 * If failure is observed then both msg size (defines below) and the
 * array size (define above) should be checked and compared.
 */
#define TF_MSG_SIZE_HWRM_TF_GLOBAL_CFG_SET 56
static_assert(sizeof(struct hwrm_tf_global_cfg_set_input) ==
	      TF_MSG_SIZE_HWRM_TF_GLOBAL_CFG_SET,
	      "HWRM message size changed: hwrm_tf_global_cfg_set_input");

#define TF_MSG_SIZE_HWRM_TF_EM_INSERT      104
static_assert(sizeof(struct hwrm_tf_em_insert_input) ==
	      TF_MSG_SIZE_HWRM_TF_EM_INSERT,
	      "HWRM message size changed: hwrm_tf_em_insert_input");
#define TF_MSG_SIZE_HWRM_TF_TBL_TYPE_SET   128
static_assert(sizeof(struct hwrm_tf_tbl_type_set_input) ==
	      TF_MSG_SIZE_HWRM_TF_TBL_TYPE_SET,
	      "HWRM message size changed: hwrm_tf_tbl_type_set_input");

/**
 * This is the MAX data we can transport across regular HWRM
 */
#define TF_PCI_BUF_SIZE_MAX 88

/**
 * This is the length of shared session name "tf_share"
 */
#define TF_SHARED_SESSION_NAME_LEN 9

/**
 * This is the length of tcam shared session name "tf_shared-wc_tcam"
 */
#define TF_TCAM_SHARED_SESSION_NAME_LEN 17

/**
 * This is the length of tcam shared session name "tf_shared-poolx"
 */
#define TF_POOL_SHARED_SESSION_NAME_LEN 16

/**
 * If data bigger than TF_PCI_BUF_SIZE_MAX then use DMA method
 */
struct tf_msg_dma_buf {
	void *va_addr;
	uint64_t pa_addr;
};

/**
 * Allocates a DMA buffer that can be used for message transfer.
 *
 * [in] buf
 *   Pointer to DMA buffer structure
 *
 * [in] size
 *   Requested size of the buffer in bytes
 *
 * Returns:
 *    0      - Success
 *   -ENOMEM - Unable to allocate buffer, no memory
 */
static int
tf_msg_alloc_dma_buf(struct tf_msg_dma_buf *buf, int size)
{
	struct tfp_calloc_parms alloc_parms;
	int rc;

	/* Allocate session */
	alloc_parms.nitems = 1;
	alloc_parms.size = size;
	alloc_parms.alignment = 4096;
	rc = tfp_calloc(&alloc_parms);
	if (rc)
		return -ENOMEM;

	buf->pa_addr = (uintptr_t)alloc_parms.mem_pa;
	buf->va_addr = alloc_parms.mem_va;

	return 0;
}

/**
 * Free's a previous allocated DMA buffer.
 *
 * [in] buf
 *   Pointer to DMA buffer structure
 */
static void
tf_msg_free_dma_buf(struct tf_msg_dma_buf *buf)
{
	tfp_free(buf->va_addr);
}

/* HWRM Direct messages */

int
tf_msg_session_open(struct bnxt *bp,
		    char *ctrl_chan_name,
		    uint8_t *fw_session_id,
		    uint8_t *fw_session_client_id,
		    struct tf_dev_info *dev,
		    bool *shared_session_creator)
{
	int rc;
	struct hwrm_tf_session_open_input req = { 0 };
	struct hwrm_tf_session_open_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };
	char *session_name;
	char *tcam_session_name;
	char *pool_session_name;

	/*
	 * "tf_shared-wc_tcam" is defined for tf_fw version 1.0.0.
	 * "tf_shared-pool" is defined for version 1.0.1.
	 * "tf_shared" is used by both verions.
	 */
	tcam_session_name = strstr(ctrl_chan_name, "tf_shared-wc_tcam");
	pool_session_name = strstr(ctrl_chan_name, "tf_shared-pool");
	session_name = strstr(ctrl_chan_name, "tf_shared");
	if (tcam_session_name)
		tfp_memcpy(&req.session_name,
			   tcam_session_name,
			   TF_TCAM_SHARED_SESSION_NAME_LEN);
	else if (pool_session_name)
		tfp_memcpy(&req.session_name,
			   pool_session_name,
			   TF_POOL_SHARED_SESSION_NAME_LEN);
	else if (session_name)
		tfp_memcpy(&req.session_name,
			   session_name,
			   TF_SHARED_SESSION_NAME_LEN);
	else
		tfp_memcpy(&req.session_name, ctrl_chan_name, TF_SESSION_NAME_MAX);

	parms.tf_type = HWRM_TF_SESSION_OPEN;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(bp,
				 &parms);
	if (rc)
		return rc;

	*fw_session_id = (uint8_t)tfp_le_to_cpu_32(resp.fw_session_id);
	*fw_session_client_id =
		(uint8_t)tfp_le_to_cpu_32(resp.fw_session_client_id);
	*shared_session_creator = (bool)tfp_le_to_cpu_32(resp.flags
		& HWRM_TF_SESSION_OPEN_OUTPUT_FLAGS_SHARED_SESSION_CREATOR);

	return rc;
}

int
tf_msg_session_attach(struct tf *tfp __rte_unused,
		      char *ctrl_chan_name __rte_unused,
		      uint8_t tf_fw_session_id __rte_unused)
{
	return -1;
}

int
tf_msg_session_client_register(struct tf *tfp,
			       struct tf_session *tfs,
			       char *ctrl_channel_name,
			       uint8_t *fw_session_client_id)
{
	int rc;
	struct hwrm_tf_session_register_input req = { 0 };
	struct hwrm_tf_session_register_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };
	uint8_t fw_session_id;
	struct tf_dev_info *dev;
	char *session_name;
	char *tcam_session_name;
	char *pool_session_name;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup device, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Unable to lookup FW id, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);

	/*
	 * "tf_shared-wc_tcam" is defined for tf_fw version 1.0.0.
	 * "tf_shared-pool" is defined for version 1.0.1.
	 * "tf_shared" is used by both verions.
	 */
	tcam_session_name = strstr(ctrl_channel_name, "tf_shared-wc_tcam");
	pool_session_name = strstr(ctrl_channel_name, "tf_shared-pool");
	session_name = strstr(ctrl_channel_name, "tf_shared");
	if (tcam_session_name)
		tfp_memcpy(&req.session_client_name,
			   tcam_session_name,
			   TF_TCAM_SHARED_SESSION_NAME_LEN);
	else if (pool_session_name)
		tfp_memcpy(&req.session_client_name,
			   pool_session_name,
			   TF_POOL_SHARED_SESSION_NAME_LEN);
	else if (session_name)
		tfp_memcpy(&req.session_client_name,
			   session_name,
			   TF_SHARED_SESSION_NAME_LEN);
	else
		tfp_memcpy(&req.session_client_name,
			   ctrl_channel_name,
			   TF_SESSION_NAME_MAX);

	parms.tf_type = HWRM_TF_SESSION_REGISTER;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	if (rc)
		return rc;

	*fw_session_client_id =
		(uint8_t)tfp_le_to_cpu_32(resp.fw_session_client_id);

	return rc;
}

int
tf_msg_session_client_unregister(struct tf *tfp,
				 struct tf_session *tfs,
				 uint8_t fw_session_client_id)
{
	int rc;
	struct hwrm_tf_session_unregister_input req = { 0 };
	struct hwrm_tf_session_unregister_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };
	uint8_t fw_session_id;
	struct tf_dev_info *dev;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup device, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Unable to lookup FW id, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.fw_session_client_id = tfp_cpu_to_le_32(fw_session_client_id);

	parms.tf_type = HWRM_TF_SESSION_UNREGISTER;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);

	return rc;
}

int
tf_msg_session_close(struct tf *tfp,
		     uint8_t fw_session_id,
		     int mailbox)
{
	int rc;
	struct hwrm_tf_session_close_input req = { 0 };
	struct hwrm_tf_session_close_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);

	parms.tf_type = HWRM_TF_SESSION_CLOSE;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = mailbox;

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	return rc;
}

int
tf_msg_session_qcfg(struct tf *tfp)
{
	int rc;
	struct hwrm_tf_session_qcfg_input req = { 0 };
	struct hwrm_tf_session_qcfg_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };
	uint8_t fw_session_id;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup device, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Unable to lookup FW id, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);

	parms.tf_type = HWRM_TF_SESSION_QCFG,
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	return rc;
}

int
tf_msg_session_resc_qcaps(struct tf *tfp,
			  struct tf_dev_info *dev,
			  enum tf_dir dir,
			  uint16_t size,
			  struct tf_rm_resc_req_entry *query,
			  enum tf_rm_resc_resv_strategy *resv_strategy,
			  uint8_t *sram_profile)
{
	int rc;
	int i;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_session_resc_qcaps_input req = { 0 };
	struct hwrm_tf_session_resc_qcaps_output resp = { 0 };
	struct tf_msg_dma_buf qcaps_buf = { 0 };
	struct tf_rm_resc_req_entry *data;
	int dma_size;

	TF_CHECK_PARMS3(tfp, query, resv_strategy);

	/* Prepare DMA buffer */
	dma_size = size * sizeof(struct tf_rm_resc_req_entry);
	rc = tf_msg_alloc_dma_buf(&qcaps_buf, dma_size);
	if (rc)
		return rc;

	/* Populate the request */
	req.fw_session_id = 0;
	req.flags = tfp_cpu_to_le_16(dir);
	req.qcaps_size = size;
	req.qcaps_addr = tfp_cpu_to_le_64(qcaps_buf.pa_addr);

	parms.tf_type = HWRM_TF_SESSION_RESC_QCAPS;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp), &parms);
	if (rc)
		goto cleanup;

	/* Process the response
	 * Should always get expected number of entries
	 */
	if (tfp_le_to_cpu_32(resp.size) != size) {
		TFP_DRV_LOG(WARNING,
			    "%s: QCAPS message size error, rc:%s, request %d vs response %d\n",
			    tf_dir_2_str(dir),
			    strerror(EINVAL),
			    size,
			    resp.size);
	}

	/* Post process the response */
	data = (struct tf_rm_resc_req_entry *)qcaps_buf.va_addr;
	for (i = 0; i < resp.size; i++) {
		query[i].type = tfp_le_to_cpu_32(data[i].type);
		query[i].min = tfp_le_to_cpu_16(data[i].min);
		query[i].max = tfp_le_to_cpu_16(data[i].max);
	}

	*resv_strategy = resp.flags &
	      HWRM_TF_SESSION_RESC_QCAPS_OUTPUT_FLAGS_SESS_RESV_STRATEGY_MASK;

	if (sram_profile != NULL)
		*sram_profile = resp.sram_profile;

cleanup:
	tf_msg_free_dma_buf(&qcaps_buf);

	return rc;
}

int
tf_msg_session_resc_alloc(struct tf *tfp,
			  struct tf_dev_info *dev,
			  enum tf_dir dir,
			  uint16_t size,
			  struct tf_rm_resc_req_entry *request,
			  struct tf_rm_resc_entry *resv)
{
	int rc;
	int i;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_session_resc_alloc_input req = { 0 };
	struct hwrm_tf_session_resc_alloc_output resp = { 0 };
	uint8_t fw_session_id;
	struct tf_msg_dma_buf req_buf = { 0 };
	struct tf_msg_dma_buf resv_buf = { 0 };
	struct tf_rm_resc_req_entry *req_data;
	struct tf_rm_resc_entry *resv_data;
	int dma_size;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	TF_CHECK_PARMS3(tfp, request, resv);

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Prepare DMA buffers */
	dma_size = size * sizeof(struct tf_rm_resc_req_entry);
	rc = tf_msg_alloc_dma_buf(&req_buf, dma_size);
	if (rc)
		return rc;

	dma_size = size * sizeof(struct tf_rm_resc_entry);
	rc = tf_msg_alloc_dma_buf(&resv_buf, dma_size);
	if (rc) {
		tf_msg_free_dma_buf(&req_buf);
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);
	req.req_size = size;

	req_data = (struct tf_rm_resc_req_entry *)req_buf.va_addr;
	for (i = 0; i < size; i++) {
		req_data[i].type = tfp_cpu_to_le_32(request[i].type);
		req_data[i].min = tfp_cpu_to_le_16(request[i].min);
		req_data[i].max = tfp_cpu_to_le_16(request[i].max);
	}

	req.req_addr = tfp_cpu_to_le_64(req_buf.pa_addr);
	req.resc_addr = tfp_cpu_to_le_64(resv_buf.pa_addr);

	parms.tf_type = HWRM_TF_SESSION_RESC_ALLOC;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp), &parms);
	if (rc)
		goto cleanup;

	/* Process the response
	 * Should always get expected number of entries
	 */
	if (tfp_le_to_cpu_32(resp.size) != size) {
		TFP_DRV_LOG(ERR,
			    "%s: Alloc message size error, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(EINVAL));
		rc = -EINVAL;
		goto cleanup;
	}

	/* Post process the response */
	resv_data = (struct tf_rm_resc_entry *)resv_buf.va_addr;
	for (i = 0; i < size; i++) {
		resv[i].type = tfp_le_to_cpu_32(resv_data[i].type);
		resv[i].start = tfp_le_to_cpu_16(resv_data[i].start);
		resv[i].stride = tfp_le_to_cpu_16(resv_data[i].stride);
	}

cleanup:
	tf_msg_free_dma_buf(&req_buf);
	tf_msg_free_dma_buf(&resv_buf);

	return rc;
}

int
tf_msg_session_resc_info(struct tf *tfp,
			 struct tf_dev_info *dev,
			 enum tf_dir dir,
			 uint16_t size,
			 struct tf_rm_resc_req_entry *request,
			 struct tf_rm_resc_entry *resv)
{
	int rc;
	int i;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_session_resc_info_input req = { 0 };
	struct hwrm_tf_session_resc_info_output resp = { 0 };
	uint8_t fw_session_id;
	struct tf_msg_dma_buf req_buf = { 0 };
	struct tf_msg_dma_buf resv_buf = { 0 };
	struct tf_rm_resc_req_entry *req_data;
	struct tf_rm_resc_entry *resv_data;
	int dma_size;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	TF_CHECK_PARMS3(tfp, request, resv);

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Prepare DMA buffers */
	dma_size = size * sizeof(struct tf_rm_resc_req_entry);
	rc = tf_msg_alloc_dma_buf(&req_buf, dma_size);
	if (rc)
		return rc;

	dma_size = size * sizeof(struct tf_rm_resc_entry);
	rc = tf_msg_alloc_dma_buf(&resv_buf, dma_size);
	if (rc) {
		tf_msg_free_dma_buf(&req_buf);
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);
	req.req_size = size;

	req_data = (struct tf_rm_resc_req_entry *)req_buf.va_addr;
	for (i = 0; i < size; i++) {
		req_data[i].type = tfp_cpu_to_le_32(request[i].type);
		req_data[i].min = tfp_cpu_to_le_16(request[i].min);
		req_data[i].max = tfp_cpu_to_le_16(request[i].max);
	}

	req.req_addr = tfp_cpu_to_le_64(req_buf.pa_addr);
	req.resc_addr = tfp_cpu_to_le_64(resv_buf.pa_addr);

	parms.tf_type = HWRM_TF_SESSION_RESC_INFO;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp), &parms);
	if (rc)
		goto cleanup;

	/* Process the response
	 * Should always get expected number of entries
	 */
	if (tfp_le_to_cpu_32(resp.size) != size) {
		TFP_DRV_LOG(ERR,
			    "%s: Alloc message size error, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(EINVAL));
		rc = -EINVAL;
		goto cleanup;
	}

	/* Post process the response */
	resv_data = (struct tf_rm_resc_entry *)resv_buf.va_addr;
	for (i = 0; i < size; i++) {
		resv[i].type = tfp_le_to_cpu_32(resv_data[i].type);
		resv[i].start = tfp_le_to_cpu_16(resv_data[i].start);
		resv[i].stride = tfp_le_to_cpu_16(resv_data[i].stride);
	}

cleanup:
	tf_msg_free_dma_buf(&req_buf);
	tf_msg_free_dma_buf(&resv_buf);

	return rc;
}

int
tf_msg_session_resc_flush(struct tf *tfp,
			  enum tf_dir dir,
			  uint16_t size,
			  struct tf_rm_resc_entry *resv)
{
	int rc;
	int i;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_session_resc_flush_input req = { 0 };
	struct hwrm_tf_session_resc_flush_output resp = { 0 };
	uint8_t fw_session_id;
	struct tf_msg_dma_buf resv_buf = { 0 };
	struct tf_rm_resc_entry *resv_data;
	int dma_size;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	TF_CHECK_PARMS2(tfp, resv);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Prepare DMA buffers */
	dma_size = size * sizeof(struct tf_rm_resc_entry);
	rc = tf_msg_alloc_dma_buf(&resv_buf, dma_size);
	if (rc)
		return rc;

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);
	req.flush_size = size;

	resv_data = (struct tf_rm_resc_entry *)resv_buf.va_addr;
	for (i = 0; i < size; i++) {
		resv_data[i].type = tfp_cpu_to_le_32(resv[i].type);
		resv_data[i].start = tfp_cpu_to_le_16(resv[i].start);
		resv_data[i].stride = tfp_cpu_to_le_16(resv[i].stride);
	}

	req.flush_addr = tfp_cpu_to_le_64(resv_buf.pa_addr);

	parms.tf_type = HWRM_TF_SESSION_RESC_FLUSH;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp), &parms);

	tf_msg_free_dma_buf(&resv_buf);

	return rc;
}

int
tf_msg_insert_em_internal_entry(struct tf *tfp,
				struct tf_insert_em_entry_parms *em_parms,
				uint16_t *rptr_index,
				uint8_t *rptr_entry,
				uint8_t *num_of_entries)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_em_insert_input req = { 0 };
	struct hwrm_tf_em_insert_output resp = { 0 };
	struct tf_em_64b_entry *em_result =
		(struct tf_em_64b_entry *)em_parms->em_record;
	uint16_t flags;
	uint8_t fw_session_id;
	uint8_t msg_key_size;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	RTE_BUILD_BUG_ON(sizeof(struct hwrm_tf_em_insert_input) !=
			 TF_MSG_SIZE_HWRM_TF_EM_INSERT);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);

	/* Check for key size conformity */
	msg_key_size = (em_parms->key_sz_in_bits + 7) / 8;
	if (msg_key_size > TF_MSG_EM_INSERT_KEY_SIZE) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "%s: Invalid parameters for msg type, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	tfp_memcpy(req.em_key,
		   em_parms->key,
		   msg_key_size);

	flags = (em_parms->dir == TF_DIR_TX ?
		 HWRM_TF_EM_INSERT_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EM_INSERT_INPUT_FLAGS_DIR_RX);
	req.flags = tfp_cpu_to_le_16(flags);
	req.strength = (em_result->hdr.word1 &
			CFA_P4_EEM_ENTRY_STRENGTH_MASK) >>
			CFA_P4_EEM_ENTRY_STRENGTH_SHIFT;
	req.em_key_bitlen = em_parms->key_sz_in_bits;
	req.action_ptr = em_result->hdr.pointer;
	req.em_record_idx = *rptr_index;

	parms.tf_type = HWRM_TF_EM_INSERT;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	if (rc)
		return rc;

	*rptr_entry = resp.rptr_entry;
	*rptr_index = resp.rptr_index;
	*num_of_entries = resp.num_of_entries;

	return 0;
}

int
tf_msg_hash_insert_em_internal_entry(struct tf *tfp,
				     struct tf_insert_em_entry_parms *em_parms,
				     uint32_t key0_hash,
				     uint32_t key1_hash,
				     uint16_t *rptr_index,
				     uint8_t *rptr_entry,
				     uint8_t *num_of_entries)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_em_hash_insert_input req = { 0 };
	struct hwrm_tf_em_hash_insert_output resp = { 0 };
	uint16_t flags;
	uint8_t fw_session_id;
	uint8_t msg_record_size;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);

	/* Check for key size conformity */
	msg_record_size = (em_parms->em_record_sz_in_bits + 7) / 8;

	if (msg_record_size > TF_MSG_EM_INSERT_RECORD_SIZE) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "%s: Record size to large, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	tfp_memcpy((char *)req.em_record,
		   em_parms->em_record,
		   msg_record_size);

	flags = (em_parms->dir == TF_DIR_TX ?
		 HWRM_TF_EM_INSERT_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EM_INSERT_INPUT_FLAGS_DIR_RX);
	req.flags = tfp_cpu_to_le_16(flags);
	req.em_record_size_bits = em_parms->em_record_sz_in_bits;
	req.em_record_idx = *rptr_index;
	req.key0_hash = key0_hash;
	req.key1_hash = key1_hash;

	parms.tf_type = HWRM_TF_EM_HASH_INSERT;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	if (rc)
		return rc;

	*rptr_entry = resp.rptr_entry;
	*rptr_index = resp.rptr_index;
	*num_of_entries = resp.num_of_entries;

	return 0;
}

int
tf_msg_delete_em_entry(struct tf *tfp,
		       struct tf_delete_em_entry_parms *em_parms)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_em_delete_input req = { 0 };
	struct hwrm_tf_em_delete_output resp = { 0 };
	uint16_t flags;
	uint8_t fw_session_id;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);

	flags = (em_parms->dir == TF_DIR_TX ?
		 HWRM_TF_EM_DELETE_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EM_DELETE_INPUT_FLAGS_DIR_RX);
	req.flags = tfp_cpu_to_le_16(flags);
	req.flow_handle = tfp_cpu_to_le_64(em_parms->flow_handle);

	parms.tf_type = HWRM_TF_EM_DELETE;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	if (rc)
		return rc;

	em_parms->index = tfp_le_to_cpu_16(resp.em_index);

	return 0;
}

int
tf_msg_move_em_entry(struct tf *tfp,
		     struct tf_move_em_entry_parms *em_parms)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_em_move_input req = { 0 };
	struct hwrm_tf_em_move_output resp = { 0 };
	uint16_t flags;
	uint8_t fw_session_id;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(em_parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);

	flags = (em_parms->dir == TF_DIR_TX ?
		 HWRM_TF_EM_DELETE_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EM_DELETE_INPUT_FLAGS_DIR_RX);
	req.flags = tfp_cpu_to_le_16(flags);
	req.flow_handle = tfp_cpu_to_le_64(em_parms->flow_handle);
	req.new_index = tfp_cpu_to_le_32(em_parms->new_index);

	parms.tf_type = HWRM_TF_EM_MOVE;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	if (rc)
		return rc;

	em_parms->index = tfp_le_to_cpu_16(resp.em_index);

	return 0;
}

int tf_msg_ext_em_ctxt_mem_alloc(struct tf *tfp,
				struct hcapi_cfa_em_table *tbl,
				uint64_t *dma_addr,
				uint32_t *page_lvl,
				uint32_t *page_size)
{
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_ctxt_mem_alloc_input req = {0};
	struct hwrm_tf_ctxt_mem_alloc_output resp = {0};
	uint32_t mem_size_k;
	int rc = 0;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	uint32_t fw_se_id;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup device, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	/* Retrieve the session information */
	fw_se_id = tfs->session_id.internal.fw_session_id;

	if (tbl->num_entries && tbl->entry_size) {
		/* unit: kbytes */
		mem_size_k = (tbl->num_entries / TF_KILOBYTE) * tbl->entry_size;
		req.mem_size = tfp_cpu_to_le_32(mem_size_k);
		req.fw_session_id = tfp_cpu_to_le_32(fw_se_id);
		parms.tf_type = HWRM_TF_CTXT_MEM_ALLOC;
		parms.req_data = (uint32_t *)&req;
		parms.req_size = sizeof(req);
		parms.resp_data = (uint32_t *)&resp;
		parms.resp_size = sizeof(resp);
		parms.mailbox = dev->ops->tf_dev_get_mailbox();
		rc = tfp_send_msg_direct(tf_session_get_bp(tfp), &parms);
		if (rc) {
			TFP_DRV_LOG(ERR, "Failed ext_em_alloc error rc:%s\n",
				strerror(-rc));
			return rc;
		}

		*dma_addr = tfp_le_to_cpu_64(resp.page_dir);
		*page_lvl = resp.page_level;
		*page_size = resp.page_size;
	}

	return rc;
}

int tf_msg_ext_em_ctxt_mem_free(struct tf *tfp,
				uint32_t mem_size_k,
				uint64_t dma_addr,
				uint8_t page_level,
				uint8_t page_size)
{
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_ctxt_mem_free_input req = {0};
	struct hwrm_tf_ctxt_mem_free_output resp = {0};
	int rc = 0;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	uint32_t fw_se_id;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup device, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	/* Retrieve the session information */
	fw_se_id = tfs->session_id.internal.fw_session_id;

	req.fw_session_id = tfp_cpu_to_le_32(fw_se_id);
	req.mem_size = tfp_cpu_to_le_32(mem_size_k);
	req.page_dir = tfp_cpu_to_le_64(dma_addr);
	req.page_level = page_level;
	req.page_size = page_size;
	parms.tf_type = HWRM_TF_CTXT_MEM_FREE;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();
	rc = tfp_send_msg_direct(tf_session_get_bp(tfp), &parms);

	return rc;
}

int
tf_msg_em_mem_rgtr(struct tf *tfp,
		   int page_lvl,
		   int page_size,
		   uint64_t dma_addr,
		   uint16_t *ctx_id)
{
	int rc;
	struct hwrm_tf_ctxt_mem_rgtr_input req = { 0 };
	struct hwrm_tf_ctxt_mem_rgtr_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	uint32_t fw_se_id;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup device, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	fw_se_id = tfs->session_id.internal.fw_session_id;

	req.fw_session_id = tfp_cpu_to_le_32(fw_se_id);
	req.page_level = page_lvl;
	req.page_size = page_size;
	req.page_dir = tfp_cpu_to_le_64(dma_addr);

	parms.tf_type = HWRM_TF_CTXT_MEM_RGTR;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	if (rc)
		return rc;

	*ctx_id = tfp_le_to_cpu_16(resp.ctx_id);

	return rc;
}

int
tf_msg_em_mem_unrgtr(struct tf *tfp,
		     uint16_t *ctx_id)
{
	int rc;
	struct hwrm_tf_ctxt_mem_unrgtr_input req = {0};
	struct hwrm_tf_ctxt_mem_unrgtr_output resp = {0};
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	uint32_t fw_se_id;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup device, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	fw_se_id = tfs->session_id.internal.fw_session_id;
	req.fw_session_id = tfp_cpu_to_le_32(fw_se_id);

	req.ctx_id = tfp_cpu_to_le_32(*ctx_id);

	parms.tf_type = HWRM_TF_CTXT_MEM_UNRGTR;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	return rc;
}

int
tf_msg_em_qcaps(struct tf *tfp,
		int dir,
		struct tf_em_caps *em_caps)
{
	int rc;
	struct hwrm_tf_ext_em_qcaps_input  req = {0};
	struct hwrm_tf_ext_em_qcaps_output resp = { 0 };
	uint32_t             flags;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	uint32_t fw_se_id;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}
	fw_se_id = tfs->session_id.internal.fw_session_id;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	flags = (dir == TF_DIR_TX ? HWRM_TF_EXT_EM_QCAPS_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EXT_EM_QCAPS_INPUT_FLAGS_DIR_RX);
	req.flags = tfp_cpu_to_le_32(flags);

	parms.tf_type = HWRM_TF_EXT_EM_QCAPS;
	req.fw_session_id = tfp_cpu_to_le_32(fw_se_id);
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	if (rc)
		return rc;

	em_caps->supported = tfp_le_to_cpu_32(resp.supported);
	em_caps->max_entries_supported =
		tfp_le_to_cpu_32(resp.max_entries_supported);
	em_caps->key_entry_size = tfp_le_to_cpu_16(resp.key_entry_size);
	em_caps->record_entry_size =
		tfp_le_to_cpu_16(resp.record_entry_size);
	em_caps->efc_entry_size = tfp_le_to_cpu_16(resp.efc_entry_size);

	return rc;
}

int
tf_msg_em_cfg(struct tf *tfp,
	      uint32_t num_entries,
	      uint16_t key0_ctx_id,
	      uint16_t key1_ctx_id,
	      uint16_t record_ctx_id,
	      uint16_t efc_ctx_id,
	      uint8_t flush_interval,
	      int dir)
{
	int rc;
	struct hwrm_tf_ext_em_cfg_input  req = {0};
	struct hwrm_tf_ext_em_cfg_output resp = {0};
	uint32_t flags;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	flags = (dir == TF_DIR_TX ? HWRM_TF_EXT_EM_CFG_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EXT_EM_CFG_INPUT_FLAGS_DIR_RX);
	flags |= HWRM_TF_EXT_EM_QCAPS_INPUT_FLAGS_PREFERRED_OFFLOAD;

	req.flags = tfp_cpu_to_le_32(flags);
	req.num_entries = tfp_cpu_to_le_32(num_entries);

	req.flush_interval = flush_interval;

	req.key0_ctx_id = tfp_cpu_to_le_16(key0_ctx_id);
	req.key1_ctx_id = tfp_cpu_to_le_16(key1_ctx_id);
	req.record_ctx_id = tfp_cpu_to_le_16(record_ctx_id);
	req.efc_ctx_id = tfp_cpu_to_le_16(efc_ctx_id);

	parms.tf_type = HWRM_TF_EXT_EM_CFG;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	return rc;
}

int
tf_msg_ext_em_cfg(struct tf *tfp,
		  struct tf_tbl_scope_cb *tbl_scope_cb,
		  uint32_t st_buckets,
		  uint8_t flush_interval,
		  enum tf_dir dir)
{
	struct hcapi_cfa_em_ctx_mem_info *ctxp = &tbl_scope_cb->em_ctx_info[dir];
	struct hcapi_cfa_em_table *lkup_tbl, *act_tbl;
	struct hwrm_tf_ext_em_cfg_input  req = {0};
	struct hwrm_tf_ext_em_cfg_output resp = {0};
	struct tfp_send_msg_parms parms = { 0 };
	uint32_t flags;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	uint32_t fw_se_id;
	int rc;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}
	fw_se_id = tfs->session_id.internal.fw_session_id;

	lkup_tbl = &ctxp->em_tables[TF_EM_LKUP_TABLE];
	act_tbl = &ctxp->em_tables[TF_ACTION_TABLE];
	flags = (dir == TF_DIR_TX ? HWRM_TF_EXT_EM_CFG_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EXT_EM_CFG_INPUT_FLAGS_DIR_RX);
	flags |= HWRM_TF_EXT_EM_QCAPS_INPUT_FLAGS_PREFERRED_OFFLOAD;

	req.flags = tfp_cpu_to_le_32(flags);
	req.num_entries = tfp_cpu_to_le_32(act_tbl->num_entries);
	req.lkup_static_buckets = tfp_cpu_to_le_32(st_buckets);
	req.fw_session_id = tfp_cpu_to_le_32(fw_se_id);
	req.flush_interval = flush_interval;
	req.action_ctx_id = tfp_cpu_to_le_16(act_tbl->ctx_id);
	req.action_tbl_scope = tfp_cpu_to_le_16(tbl_scope_cb->tbl_scope_id);
	req.lkup_ctx_id = tfp_cpu_to_le_16(lkup_tbl->ctx_id);
	req.lkup_tbl_scope = tfp_cpu_to_le_16(tbl_scope_cb->tbl_scope_id);

	req.enables = (HWRM_TF_EXT_EM_CFG_INPUT_ENABLES_ACTION_CTX_ID |
		       HWRM_TF_EXT_EM_CFG_INPUT_ENABLES_ACTION_TBL_SCOPE |
		       HWRM_TF_EXT_EM_CFG_INPUT_ENABLES_LKUP_CTX_ID |
		       HWRM_TF_EXT_EM_CFG_INPUT_ENABLES_LKUP_TBL_SCOPE |
		       HWRM_TF_EXT_EM_CFG_INPUT_ENABLES_LKUP_STATIC_BUCKETS |
		       HWRM_TF_EXT_EM_CFG_INPUT_ENABLES_NUM_ENTRIES);

	parms.tf_type = HWRM_TF_EXT_EM_CFG;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	return rc;
}

int
tf_msg_em_op(struct tf *tfp,
	     int dir,
	     uint16_t op)
{
	int rc;
	struct hwrm_tf_ext_em_op_input req = {0};
	struct hwrm_tf_ext_em_op_output resp = {0};
	uint32_t flags;
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	flags = (dir == TF_DIR_TX ? HWRM_TF_EXT_EM_CFG_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EXT_EM_CFG_INPUT_FLAGS_DIR_RX);
	req.flags = tfp_cpu_to_le_32(flags);
	req.op = tfp_cpu_to_le_16(op);

	parms.tf_type = HWRM_TF_EXT_EM_OP;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	return rc;
}

int
tf_msg_tcam_entry_set(struct tf *tfp,
		      struct tf_dev_info *dev,
		      struct tf_tcam_set_parms *parms)
{
	int rc;
	struct tfp_send_msg_parms mparms = { 0 };
	struct hwrm_tf_tcam_set_input req = { 0 };
	struct hwrm_tf_tcam_set_output resp = { 0 };
	struct tf_msg_dma_buf buf = { 0 };
	uint8_t *data = NULL;
	int data_size = 0;
	uint8_t fw_session_id;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.type = parms->hcapi_type;
	req.idx = tfp_cpu_to_le_16(parms->idx);
	if (parms->dir == TF_DIR_TX)
		req.flags |= HWRM_TF_TCAM_SET_INPUT_FLAGS_DIR_TX;

	req.key_size = parms->key_size;
	req.mask_offset = parms->key_size;
	/* Result follows after key and mask, thus multiply by 2 */
	req.result_offset = 2 * parms->key_size;
	req.result_size = parms->result_size;
	data_size = 2 * req.key_size + req.result_size;

	if (data_size <= TF_PCI_BUF_SIZE_MAX) {
		/* use pci buffer */
		data = &req.dev_data[0];
	} else {
		/* use dma buffer */
		req.flags |= HWRM_TF_TCAM_SET_INPUT_FLAGS_DMA;
		rc = tf_msg_alloc_dma_buf(&buf, data_size);
		if (rc)
			goto cleanup;
		data = buf.va_addr;
		tfp_memcpy(&req.dev_data[0],
			   &buf.pa_addr,
			   sizeof(buf.pa_addr));
	}

	tfp_memcpy(&data[0], parms->key, parms->key_size);
	tfp_memcpy(&data[parms->key_size], parms->mask, parms->key_size);
	tfp_memcpy(&data[req.result_offset], parms->result, parms->result_size);

	mparms.tf_type = HWRM_TF_TCAM_SET;
	mparms.req_data = (uint32_t *)&req;
	mparms.req_size = sizeof(req);
	mparms.resp_data = (uint32_t *)&resp;
	mparms.resp_size = sizeof(resp);
	mparms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &mparms);

cleanup:
	tf_msg_free_dma_buf(&buf);

	return rc;
}

int
tf_msg_tcam_entry_get(struct tf *tfp,
		      struct tf_dev_info *dev,
		      struct tf_tcam_get_parms *parms)
{
	int rc;
	struct tfp_send_msg_parms mparms = { 0 };
	struct hwrm_tf_tcam_get_input req = { 0 };
	struct hwrm_tf_tcam_get_output resp = { 0 };
	uint8_t fw_session_id;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.type = parms->hcapi_type;
	req.idx = tfp_cpu_to_le_16(parms->idx);
	if (parms->dir == TF_DIR_TX)
		req.flags |= HWRM_TF_TCAM_GET_INPUT_FLAGS_DIR_TX;

	mparms.tf_type = HWRM_TF_TCAM_GET;
	mparms.req_data = (uint32_t *)&req;
	mparms.req_size = sizeof(req);
	mparms.resp_data = (uint32_t *)&resp;
	mparms.resp_size = sizeof(resp);
	mparms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &mparms);

	if (rc != 0)
		return rc;

	if (parms->key_size < resp.key_size ||
	    parms->result_size < resp.result_size) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "%s: Key buffer(%d) is smaller than the key(%d), rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    parms->key_size,
			    resp.key_size,
			    strerror(-rc));
		return rc;
	}
	parms->key_size = resp.key_size;
	parms->result_size = resp.result_size;
	tfp_memcpy(parms->key, resp.dev_data, resp.key_size);
	tfp_memcpy(parms->mask, &resp.dev_data[resp.key_size], resp.key_size);
	tfp_memcpy(parms->result, &resp.dev_data[resp.result_offset], resp.result_size);

	return 0;
}

int
tf_msg_tcam_entry_free(struct tf *tfp,
		       struct tf_dev_info *dev,
		       struct tf_tcam_free_parms *in_parms)
{
	int rc;
	struct hwrm_tf_tcam_free_input req =  { 0 };
	struct hwrm_tf_tcam_free_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };
	uint8_t fw_session_id;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(in_parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.type = in_parms->hcapi_type;
	req.count = 1;
	req.idx_list[0] = tfp_cpu_to_le_16(in_parms->idx);
	if (in_parms->dir == TF_DIR_TX)
		req.flags |= HWRM_TF_TCAM_FREE_INPUT_FLAGS_DIR_TX;

	parms.tf_type = HWRM_TF_TCAM_FREE;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	return rc;
}

int
tf_msg_set_tbl_entry(struct tf *tfp,
		     enum tf_dir dir,
		     uint16_t hcapi_type,
		     uint16_t size,
		     uint8_t *data,
		     uint32_t index)
{
	int rc;
	struct hwrm_tf_tbl_type_set_input req = { 0 };
	struct hwrm_tf_tbl_type_set_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };
	struct tf_msg_dma_buf buf = { 0 };
	uint8_t fw_session_id;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	RTE_BUILD_BUG_ON(sizeof(struct hwrm_tf_tbl_type_set_input) !=
			 TF_MSG_SIZE_HWRM_TF_TBL_TYPE_SET);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.flags = tfp_cpu_to_le_16(dir);
	req.type = tfp_cpu_to_le_32(hcapi_type);
	req.size = tfp_cpu_to_le_16(size);
	req.index = tfp_cpu_to_le_32(index);

	/* Check for data size conformity */
	if (size > TF_MSG_TBL_TYPE_SET_DATA_SIZE) {
		/* use dma buffer */
		req.flags |= HWRM_TF_TBL_TYPE_SET_INPUT_FLAGS_DMA;
		rc = tf_msg_alloc_dma_buf(&buf, size);
		if (rc)
			goto cleanup;
		tfp_memcpy(buf.va_addr, data, size);
		tfp_memcpy(&req.data[0],
			   &buf.pa_addr,
			   sizeof(buf.pa_addr));
	} else {
		tfp_memcpy(&req.data, data, size);
	}

	parms.tf_type = HWRM_TF_TBL_TYPE_SET;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
cleanup:
	tf_msg_free_dma_buf(&buf);

	return rc;
}

int
tf_msg_get_tbl_entry(struct tf *tfp,
		     enum tf_dir dir,
		     uint16_t hcapi_type,
		     uint16_t size,
		     uint8_t *data,
		     uint32_t index,
		     bool clear_on_read)
{
	int rc;
	struct hwrm_tf_tbl_type_get_input req = { 0 };
	struct hwrm_tf_tbl_type_get_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };
	uint8_t fw_session_id;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	uint32_t flags = 0;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}
	flags = (dir == TF_DIR_TX ?
		 HWRM_TF_TBL_TYPE_GET_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_TBL_TYPE_GET_INPUT_FLAGS_DIR_RX);

	if (clear_on_read)
		flags |= HWRM_TF_TBL_TYPE_GET_INPUT_FLAGS_CLEAR_ON_READ;

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.flags = tfp_cpu_to_le_16(flags);
	req.type = tfp_cpu_to_le_32(hcapi_type);
	req.index = tfp_cpu_to_le_32(index);

	parms.tf_type = HWRM_TF_TBL_TYPE_GET;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	if (rc)
		return rc;

	/*
	 * The response will be 64 bytes long, the response size will
	 * be in words (16). All we can test for is that the response
	 * size is < to the requested size.
	 */
	if ((tfp_le_to_cpu_32(resp.size) * 4) < size)
		return -EINVAL;

	/*
	 * Copy the requested number of bytes
	 */
	tfp_memcpy(data,
		   &resp.data,
		   size);

	return 0;
}

/* HWRM Tunneled messages */

int
tf_msg_get_global_cfg(struct tf *tfp,
		      struct tf_global_cfg_parms *params)
{
	int rc = 0;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_global_cfg_get_input req = { 0 };
	struct hwrm_tf_global_cfg_get_output resp = { 0 };
	uint32_t flags = 0;
	uint8_t fw_session_id;
	uint16_t resp_size = 0;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(params->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(params->dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(params->dir),
			    strerror(-rc));
		return rc;
	}

	flags = (params->dir == TF_DIR_TX ?
		 HWRM_TF_GLOBAL_CFG_GET_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_GLOBAL_CFG_GET_INPUT_FLAGS_DIR_RX);

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.flags = tfp_cpu_to_le_32(flags);
	req.type = tfp_cpu_to_le_32(params->type);
	req.offset = tfp_cpu_to_le_32(params->offset);
	req.size = tfp_cpu_to_le_32(params->config_sz_in_bytes);

	parms.tf_type = HWRM_TF_GLOBAL_CFG_GET;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp), &parms);
	if (rc != 0)
		return rc;

	/* Verify that we got enough buffer to return the requested data */
	resp_size = tfp_le_to_cpu_16(resp.size);
	if (resp_size < params->config_sz_in_bytes)
		return -EINVAL;

	if (params->config)
		tfp_memcpy(params->config,
			   resp.data,
			   resp_size);
	else
		return -EFAULT;

	return 0;
}

int
tf_msg_set_global_cfg(struct tf *tfp,
		      struct tf_global_cfg_parms *params)
{
	int rc = 0;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_global_cfg_set_input req = { 0 };
	struct hwrm_tf_global_cfg_set_output resp = { 0 };
	uint32_t flags = 0;
	uint8_t fw_session_id;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(params->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(params->dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(params->dir),
			    strerror(-rc));
		return rc;
	}

	flags = (params->dir == TF_DIR_TX ?
		 HWRM_TF_GLOBAL_CFG_SET_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_GLOBAL_CFG_SET_INPUT_FLAGS_DIR_RX);

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.flags = tfp_cpu_to_le_32(flags);
	req.type = tfp_cpu_to_le_32(params->type);
	req.offset = tfp_cpu_to_le_32(params->offset);

	/* Check for data size conformity */
	if (params->config_sz_in_bytes > TF_MSG_SET_GLOBAL_CFG_DATA_SIZE) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "%s: Invalid parameters for msg type, rc:%s\n",
			    tf_dir_2_str(params->dir),
			    strerror(-rc));
		return rc;
	}

	tfp_memcpy(req.data, params->config,
		   params->config_sz_in_bytes);

	/* Only set mask if pointer is provided
	 */
	if (params->config_mask) {
		tfp_memcpy(req.mask,
			   params->config_mask,
			   params->config_sz_in_bytes);
	}

	req.size = tfp_cpu_to_le_32(params->config_sz_in_bytes);

	parms.tf_type = HWRM_TF_GLOBAL_CFG_SET;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp), &parms);

	if (rc != 0)
		return rc;

	return 0;
}

int
tf_msg_bulk_get_tbl_entry(struct tf *tfp,
			  enum tf_dir dir,
			  uint16_t hcapi_type,
			  uint32_t starting_idx,
			  uint16_t num_entries,
			  uint16_t entry_sz_in_bytes,
			  uint64_t physical_mem_addr,
			  bool clear_on_read)
{
	int rc;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_tbl_type_bulk_get_input req = { 0 };
	struct hwrm_tf_tbl_type_bulk_get_output resp = { 0 };
	int data_size = 0;
	uint8_t fw_session_id;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	uint32_t flags = 0;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Unable to lookup FW id, rc:%s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}
	flags = (dir == TF_DIR_TX ?
		 HWRM_TF_TBL_TYPE_BULK_GET_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_TBL_TYPE_BULK_GET_INPUT_FLAGS_DIR_RX);

	if (clear_on_read)
		flags |= HWRM_TF_TBL_TYPE_BULK_GET_INPUT_FLAGS_CLEAR_ON_READ;

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.flags = tfp_cpu_to_le_16(flags);
	req.type = tfp_cpu_to_le_32(hcapi_type);
	req.start_index = tfp_cpu_to_le_32(starting_idx);
	req.num_entries = tfp_cpu_to_le_32(num_entries);

	data_size = num_entries * entry_sz_in_bytes;

	req.host_addr = tfp_cpu_to_le_64(physical_mem_addr);

	parms.tf_type = HWRM_TF_TBL_TYPE_BULK_GET;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	if (rc)
		return rc;

	/* Verify that we got enough buffer to return the requested data */
	if (tfp_le_to_cpu_32(resp.size) != data_size)
		return -EINVAL;

	return 0;
}

int
tf_msg_get_if_tbl_entry(struct tf *tfp,
			struct tf_if_tbl_get_parms *params)
{
	int rc = 0;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_if_tbl_get_input req = { 0 };
	struct hwrm_tf_if_tbl_get_output resp = { 0 };
	uint32_t flags = 0;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(params->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(params->dir),
			    strerror(-rc));
		return rc;
	}

	flags = (params->dir == TF_DIR_TX ?
		HWRM_TF_IF_TBL_GET_INPUT_FLAGS_DIR_TX :
		HWRM_TF_IF_TBL_GET_INPUT_FLAGS_DIR_RX);

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);
	req.flags = flags;
	req.type = params->hcapi_type;
	req.index = tfp_cpu_to_le_16(params->idx);
	req.size = tfp_cpu_to_le_16(params->data_sz_in_bytes);

	parms.tf_type = HWRM_TF_IF_TBL_GET;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp), &parms);

	if (rc != 0)
		return rc;

	tfp_memcpy(&params->data[0], resp.data, req.size);

	return 0;
}

int
tf_msg_set_if_tbl_entry(struct tf *tfp,
			struct tf_if_tbl_set_parms *params)
{
	int rc = 0;
	struct tfp_send_msg_parms parms = { 0 };
	struct hwrm_tf_if_tbl_set_input req = { 0 };
	struct hwrm_tf_if_tbl_get_output resp = { 0 };
	uint32_t flags = 0;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(params->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	flags = (params->dir == TF_DIR_TX ?
		HWRM_TF_IF_TBL_SET_INPUT_FLAGS_DIR_TX :
		HWRM_TF_IF_TBL_SET_INPUT_FLAGS_DIR_RX);

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);
	req.flags = flags;
	req.type = params->hcapi_type;
	req.index = tfp_cpu_to_le_32(params->idx);
	req.size = tfp_cpu_to_le_32(params->data_sz_in_bytes);
	tfp_memcpy(&req.data[0], params->data, params->data_sz_in_bytes);

	parms.tf_type = HWRM_TF_IF_TBL_SET;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp), &parms);

	if (rc != 0)
		return rc;

	return 0;
}

int
tf_msg_get_version(struct bnxt *bp,
		   struct tf_dev_info *dev,
		   struct tf_get_version_parms *params)

{
	int rc;
	struct hwrm_tf_version_get_input req = { 0 };
	struct hwrm_tf_version_get_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };

	/* Populate the request */
	parms.tf_type = HWRM_TF_VERSION_GET,
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(bp,
				 &parms);

	params->major = resp.major;
	params->minor = resp.minor;
	params->update = resp.update;

	dev->ops->tf_dev_map_hcapi_caps(resp.dev_caps_cfg,
					&params->dev_ident_caps,
					&params->dev_tcam_caps,
					&params->dev_tbl_caps,
					&params->dev_em_caps);

	return rc;
}

int
tf_msg_session_set_hotup_state(struct tf *tfp, uint16_t state)
{
	int rc;
	struct hwrm_tf_session_hotup_state_set_input req = { 0 };
	struct hwrm_tf_session_hotup_state_set_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };
	uint8_t fw_session_id;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup device, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Unable to lookup FW id, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);
	req.state = tfp_cpu_to_le_16(state);

	parms.tf_type = HWRM_TF_SESSION_HOTUP_STATE_SET;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);
	return rc;
}

int
tf_msg_session_get_hotup_state(struct tf *tfp,
			       uint16_t *state,
			       uint16_t *ref_cnt)
{
	int rc;
	struct hwrm_tf_session_hotup_state_get_input req = { 0 };
	struct hwrm_tf_session_hotup_state_get_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };
	uint8_t fw_session_id;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup device, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Unable to lookup FW id, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Populate the request */
	req.fw_session_id = tfp_cpu_to_le_32(fw_session_id);

	parms.tf_type = HWRM_TF_SESSION_HOTUP_STATE_GET;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = dev->ops->tf_dev_get_mailbox();

	rc = tfp_send_msg_direct(tf_session_get_bp(tfp),
				 &parms);

	*state = tfp_le_to_cpu_16(resp.state);
	*ref_cnt = tfp_le_to_cpu_16(resp.ref_cnt);

	return rc;
}
