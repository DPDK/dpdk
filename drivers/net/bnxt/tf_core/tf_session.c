/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <string.h>
#include <rte_common.h>
#include "tf_session.h"
#include "tf_common.h"
#include "tf_msg.h"
#include "tfp.h"
#include "bnxt.h"

struct tf_session_client_create_parms {
	/**
	 * [in] Pointer to the control channel name string
	 */
	char *ctrl_chan_name;

	/**
	 * [out] Firmware Session Client ID
	 */
	union tf_session_client_id *session_client_id;
};

struct tf_session_client_destroy_parms {
	/**
	 * FW Session Client Identifier
	 */
	union tf_session_client_id session_client_id;
};

/**
 * Creates a Session and the associated client.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to session client create parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 *   - (-ENOMEM) if max session clients has been reached.
 */
static int
tf_session_create(struct tf *tfp,
		  struct tf_session_open_session_parms *parms)
{
	int rc;
	struct tf_session *session = NULL;
	struct tf_session_client *client;
	struct tfp_calloc_parms cparms;
	uint8_t fw_session_id;
	uint8_t fw_session_client_id;
	union tf_session_id *session_id;
	struct tf_dev_info dev;
	bool shared_session_creator;
	char *shared_name;
	char *tcam_session_name;
	char *pool_session_name;

	TF_CHECK_PARMS2(tfp, parms);

	tf_dev_bind_ops(parms->open_cfg->device_type,
			&dev);

	/* Open FW session and get a new session_id */
	rc = tf_msg_session_open(parms->open_cfg->bp,
				 parms->open_cfg->ctrl_chan_name,
				 &fw_session_id,
				 &fw_session_client_id,
				 &dev,
				 &shared_session_creator);
	if (rc) {
		/* Log error */
		if (rc == -EEXIST)
			TFP_DRV_LOG(ERR,
				    "Session is already open, rc:%s\n",
				    strerror(-rc));
		else
			TFP_DRV_LOG(ERR,
				    "Open message send failed, rc:%s\n",
				    strerror(-rc));

		parms->open_cfg->session_id.id = TF_FW_SESSION_ID_INVALID;
		return rc;
	}

	/* Allocate session */
	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_session_info);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Failed to allocate session info, rc:%s\n",
			    strerror(-rc));
		goto cleanup;
	}
	tfp->session = (struct tf_session_info *)cparms.mem_va;

	/* Allocate core data for the session */
	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_session);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Failed to allocate session data, rc:%s\n",
			    strerror(-rc));
		goto cleanup;
	}
	tfp->session->core_data = cparms.mem_va;
	session_id = &parms->open_cfg->session_id;

	/* Update Session Info, which is what is visible to the caller */
	tfp->session->ver.major = 0;
	tfp->session->ver.minor = 0;
	tfp->session->ver.update = 0;

	tfp->session->session_id.internal.domain = session_id->internal.domain;
	tfp->session->session_id.internal.bus = session_id->internal.bus;
	tfp->session->session_id.internal.device = session_id->internal.device;
	tfp->session->session_id.internal.fw_session_id = fw_session_id;

	/* Initialize Session and Device, which is private */
	session = (struct tf_session *)tfp->session->core_data;
	session->ver.major = 0;
	session->ver.minor = 0;
	session->ver.update = 0;

	session->session_id.internal.domain = session_id->internal.domain;
	session->session_id.internal.bus = session_id->internal.bus;
	session->session_id.internal.device = session_id->internal.device;
	session->session_id.internal.fw_session_id = fw_session_id;
	/* Return the allocated session id */
	session_id->id = session->session_id.id;

	/* Init session client list */
	ll_init(&session->client_ll);

	/* Create the local session client, initialize and attach to
	 * the session
	 */
	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_session_client);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Failed to allocate session client, rc:%s\n",
			    strerror(-rc));
		goto cleanup;
	}
	client = cparms.mem_va;

	/* Register FID with the client */
	rc = tfp_get_fid(tfp, &client->fw_fid);
	if (rc)
		return rc;

	client->session_client_id.internal.fw_session_id = fw_session_id;
	client->session_client_id.internal.fw_session_client_id =
		fw_session_client_id;

	tfp_memcpy(client->ctrl_chan_name,
		   parms->open_cfg->ctrl_chan_name,
		   TF_SESSION_NAME_MAX);

	ll_insert(&session->client_ll, &client->ll_entry);
	session->ref_count++;

	/* Init session em_ext_db */
	session->em_ext_db_handle = NULL;

	/* Populate the request */
	shared_name = strstr(parms->open_cfg->ctrl_chan_name, "tf_shared");
	if (shared_name) {
		session->shared_session = true;
		/*
		 * "tf_shared-wc_tcam" is defined for tf_fw version 1.0.0.
		 * "tf_shared-pool" is defined for version 1.0.1.
		 */
		tcam_session_name = strstr(parms->open_cfg->ctrl_chan_name, "tf_shared-wc_tcam");
		pool_session_name = strstr(parms->open_cfg->ctrl_chan_name, "tf_shared-pool");
		if (tcam_session_name || pool_session_name)
			session->shared_session_hotup = true;
	}

	if (session->shared_session && shared_session_creator) {
		session->shared_session_creator = true;
		parms->open_cfg->shared_session_creator = true;
	}

	rc = tf_dev_bind(tfp,
			 parms->open_cfg->device_type,
			 &parms->open_cfg->resources,
			 parms->open_cfg->wc_num_slices,
			 &session->dev);

	/* Logging handled by dev_bind */
	if (rc)
		goto cleanup;

	if (session->dev.ops->tf_dev_get_mailbox == NULL) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "No tf_dev_get_mailbox() defined for device\n");
		goto cleanup;
	}

	session->dev_init = true;

	return 0;

 cleanup:
	rc = tf_msg_session_close(tfp,
			fw_session_id,
			dev.ops->tf_dev_get_mailbox());
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "FW Session close failed, rc:%s\n",
			    strerror(-rc));
	}
	if (tfp->session) {
		tfp_free(tfp->session->core_data);
		tfp_free(tfp->session);
		tfp->session = NULL;
	}

	return rc;
}

/**
 * Creates a Session Client on an existing Session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to session client create parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 *   - (-ENOMEM) if max session clients has been reached.
 */
static int
tf_session_client_create(struct tf *tfp,
			 struct tf_session_client_create_parms *parms)
{
	int rc;
	struct tf_session *session = NULL;
	struct tf_session_client *client;
	struct tfp_calloc_parms cparms;
	union tf_session_client_id session_client_id;

	TF_CHECK_PARMS2(tfp, parms);

	/* Using internal version as session client may not exist yet */
	rc = tf_session_get_session_internal(tfp, &session);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	client = tf_session_find_session_client_by_name(session,
							parms->ctrl_chan_name);
	if (client) {
		TFP_DRV_LOG(ERR,
			    "Client %s, already registered with this session\n",
			    parms->ctrl_chan_name);
		return -EOPNOTSUPP;
	}

	rc = tf_msg_session_client_register
		    (tfp,
		     session,
		     parms->ctrl_chan_name,
		     &session_client_id.internal.fw_session_client_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to create client on session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Create the local session client, initialize and attach to
	 * the session
	 */
	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_session_client);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to allocate session client, rc:%s\n",
			    strerror(-rc));
		goto cleanup;
	}
	client = cparms.mem_va;

	/* Register FID with the client */
	rc = tfp_get_fid(tfp, &client->fw_fid);
	if (rc)
		return rc;

	/* Build the Session Client ID by adding the fw_session_id */
	rc = tf_session_get_fw_session_id
			(tfp,
			&session_client_id.internal.fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Session Firmware id lookup failed, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	tfp_memcpy(client->ctrl_chan_name,
		   parms->ctrl_chan_name,
		   TF_SESSION_NAME_MAX);

	client->session_client_id.id = session_client_id.id;

	ll_insert(&session->client_ll, &client->ll_entry);

	session->ref_count++;

	/* Build the return value */
	parms->session_client_id->id = session_client_id.id;

 cleanup:
	/* TBD - Add code to unregister newly create client from fw */

	return rc;
}

/**
 * Destroys a Session Client on an existing Session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to the session client destroy parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 *   - (-ENOTFOUND) error, client not owned by the session.
 *   - (-ENOTSUPP) error, unable to destroy client as its the last
 *		 client. Please use the tf_session_close().
 */
static int
tf_session_client_destroy(struct tf *tfp,
			  struct tf_session_client_destroy_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_session_client *client;

	TF_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Check session owns this client and that we're not the last client */
	client = tf_session_get_session_client(tfs,
					       parms->session_client_id);
	if (client == NULL) {
		TFP_DRV_LOG(ERR,
			    "Client %d, not found within this session\n",
			    parms->session_client_id.id);
		return -EINVAL;
	}

	/* If last client the request is rejected and cleanup should
	 * be done by session close.
	 */
	if (tfs->ref_count == 1)
		return -EOPNOTSUPP;

	rc = tf_msg_session_client_unregister
			(tfp,
			tfs,
			parms->session_client_id.internal.fw_session_client_id);

	/* Log error, but continue. If FW fails we do not really have
	 * a way to fix this but the client would no longer be valid
	 * thus we remove from the session.
	 */
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Client destroy on FW Failed, rc:%s\n",
			    strerror(-rc));
	}

	ll_delete(&tfs->client_ll, &client->ll_entry);

	/* Decrement the session ref_count */
	tfs->ref_count--;

	tfp_free(client);

	return rc;
}

int
tf_session_open_session(struct tf *tfp,
			struct tf_session_open_session_parms *parms)
{
	int rc;
	struct tf_session_client_create_parms scparms;

	TF_CHECK_PARMS3(tfp, parms, parms->open_cfg->bp);

	tfp->bp = parms->open_cfg->bp;
	/* Decide if we're creating a new session or session client */
	if (tfp->session == NULL) {
		rc = tf_session_create(tfp, parms);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "Failed to create session, ctrl_chan_name:%s, rc:%s\n",
				    parms->open_cfg->ctrl_chan_name,
				    strerror(-rc));
			return rc;
		}

		TFP_DRV_LOG(INFO,
		       "Session created, session_client_id:%d,"
		       " session_id:0x%08x, fw_session_id:%d\n",
		       parms->open_cfg->session_client_id.id,
		       parms->open_cfg->session_id.id,
		       parms->open_cfg->session_id.internal.fw_session_id);
	} else {
		scparms.ctrl_chan_name = parms->open_cfg->ctrl_chan_name;
		scparms.session_client_id = &parms->open_cfg->session_client_id;

		/* Create the new client and get it associated with
		 * the session.
		 */
		rc = tf_session_client_create(tfp, &scparms);
		if (rc) {
			TFP_DRV_LOG(ERR,
			      "Failed to create client on session 0x%x, rc:%s\n",
			      parms->open_cfg->session_id.id,
			      strerror(-rc));
			return rc;
		}

		TFP_DRV_LOG(INFO,
			"Session Client:%d registered on session:0x%08x\n",
			scparms.session_client_id->internal.fw_session_client_id,
			tfp->session->session_id.id);
	}

	return 0;
}

int
tf_session_attach_session(struct tf *tfp __rte_unused,
			  struct tf_session_attach_session_parms *parms __rte_unused)
{
	int rc = -EOPNOTSUPP;

	TF_CHECK_PARMS2(tfp, parms);

	TFP_DRV_LOG(ERR,
		    "Attach not yet supported, rc:%s\n",
		    strerror(-rc));
	return rc;
}

int
tf_session_close_session(struct tf *tfp,
			 struct tf_session_close_session_parms *parms)
{
	int rc;
	struct tf_session *tfs = NULL;
	struct tf_session_client *client;
	struct tf_dev_info *tfd = NULL;
	struct tf_session_client_destroy_parms scdparms;
	uint16_t fid;
	uint8_t fw_session_id = 1;
	int mailbox = 0;

	TF_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Session lookup failed, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	if (tfs->session_id.id == TF_SESSION_ID_INVALID) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Invalid session id, unable to close, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Get the client, we need it independently of the closure
	 * type (client or session closure).
	 *
	 * We find the client by way of the fid. Thus one cannot close
	 * a client on behalf of someone else.
	 */
	rc = tfp_get_fid(tfp, &fid);
	if (rc)
		return rc;

	client = tf_session_find_session_client_by_fid(tfs,
						       fid);
	if (!client) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Client not part of the session, unable to close, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Record the session we're closing so the caller knows the
	 * details.
	 */
	*parms->session_id = tfs->session_id;

	/* In case multiple clients we chose to close those first */
	if (tfs->ref_count > 1) {
		/* Linaro gcc can't static init this structure */
		memset(&scdparms,
		       0,
		       sizeof(struct tf_session_client_destroy_parms));

		scdparms.session_client_id = client->session_client_id;
		/* Destroy requested client so its no longer
		 * registered with this session.
		 */
		rc = tf_session_client_destroy(tfp, &scdparms);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "Failed to unregister Client %d, rc:%s\n",
				    client->session_client_id.id,
				    strerror(-rc));
			return rc;
		}

		TFP_DRV_LOG(INFO,
			    "Closed session client, session_client_id:%d\n",
			    client->session_client_id.id);

		TFP_DRV_LOG(INFO,
			    "session_id:0x%08x, ref_count:%d\n",
			    tfs->session_id.id,
			    tfs->ref_count);

		return 0;
	}

	rc = tf_session_get_device(tfs, &tfd);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device lookup failed, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	mailbox = tfd->ops->tf_dev_get_mailbox();

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Unable to lookup FW id, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Unbind the device */
	rc = tf_dev_unbind(tfp, tfd);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Device unbind failed, rc:%s\n",
			    strerror(-rc));
	}

	rc = tf_msg_session_close(tfp, fw_session_id, mailbox);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "FW Session close failed, rc:%s\n",
			    strerror(-rc));
	}

	/* Final cleanup as we're last user of the session thus we
	 * also delete the last client.
	 */
	ll_delete(&tfs->client_ll, &client->ll_entry);
	tfp_free(client);

	tfs->ref_count--;

	TFP_DRV_LOG(INFO,
		    "Closed session, session_id:0x%08x, ref_count:%d\n",
		    tfs->session_id.id,
		    tfs->ref_count);

	tfs->dev_init = false;

	tfp_free(tfp->session->core_data);
	tfp_free(tfp->session);
	tfp->session = NULL;

	return 0;
}

bool
tf_session_is_fid_supported(struct tf_session *tfs,
			    uint16_t fid)
{
	struct ll_entry *c_entry;
	struct tf_session_client *client;

	for (c_entry = tfs->client_ll.head;
	     c_entry != NULL;
	     c_entry = c_entry->next) {
		client = (struct tf_session_client *)c_entry;
		if (client->fw_fid == fid)
			return true;
	}

	return false;
}

int
tf_session_get_session_internal(struct tf *tfp,
				struct tf_session **tfs)
{
	int rc = 0;

	/* Skip using the check macro as we want to control the error msg */
	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Session not created, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	*tfs = (struct tf_session *)(tfp->session->core_data);

	return rc;
}

int
tf_session_get_session(struct tf *tfp,
		       struct tf_session **tfs)
{
	int rc;
	uint16_t fw_fid;
	bool supported = false;

	rc = tf_session_get_session_internal(tfp,
					     tfs);
	/* Logging done by tf_session_get_session_internal */
	if (rc)
		return rc;

	/* As session sharing among functions aka 'individual clients'
	 * is supported we have to assure that the client is indeed
	 * registered before we get deep in the TruFlow api stack.
	 */
	rc = tfp_get_fid(tfp, &fw_fid);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Internal FID lookup\n, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	supported = tf_session_is_fid_supported(*tfs, fw_fid);
	if (!supported) {
		TFP_DRV_LOG
			(ERR,
			"Ctrl channel not registered with session\n, rc:%s\n",
			strerror(-rc));
		return -EINVAL;
	}

	return rc;
}

int tf_session_get(struct tf *tfp,
		   struct tf_session **tfs,
		   struct tf_dev_info **tfd)
{
	int rc;
	rc = tf_session_get_session_internal(tfp, tfs);

	/* Logging done by tf_session_get_session_internal */
	if (rc)
		return rc;

	rc = tf_session_get_device(*tfs, tfd);

	return rc;
}

struct tf_session_client *
tf_session_get_session_client(struct tf_session *tfs,
			      union tf_session_client_id session_client_id)
{
	struct ll_entry *c_entry;
	struct tf_session_client *client;

	/* Skip using the check macro as we just want to return */
	if (tfs == NULL)
		return NULL;

	for (c_entry = tfs->client_ll.head;
	     c_entry != NULL;
	     c_entry = c_entry->next) {
		client = (struct tf_session_client *)c_entry;
		if (client->session_client_id.id == session_client_id.id)
			return client;
	}

	return NULL;
}

struct tf_session_client *
tf_session_find_session_client_by_name(struct tf_session *tfs,
				       const char *ctrl_chan_name)
{
	struct ll_entry *c_entry;
	struct tf_session_client *client;

	/* Skip using the check macro as we just want to return */
	if (tfs == NULL || ctrl_chan_name == NULL)
		return NULL;

	for (c_entry = tfs->client_ll.head;
	     c_entry != NULL;
	     c_entry = c_entry->next) {
		client = (struct tf_session_client *)c_entry;
		if (strncmp(client->ctrl_chan_name,
			    ctrl_chan_name,
			    TF_SESSION_NAME_MAX) == 0)
			return client;
	}

	return NULL;
}

struct tf_session_client *
tf_session_find_session_client_by_fid(struct tf_session *tfs,
				      uint16_t fid)
{
	struct ll_entry *c_entry;
	struct tf_session_client *client;

	/* Skip using the check macro as we just want to return */
	if (tfs == NULL)
		return NULL;

	for (c_entry = tfs->client_ll.head;
	     c_entry != NULL;
	     c_entry = c_entry->next) {
		client = (struct tf_session_client *)c_entry;
		if (client->fw_fid == fid)
			return client;
	}

	return NULL;
}

int
tf_session_get_device(struct tf_session *tfs,
		      struct tf_dev_info **tfd)
{
	*tfd = &tfs->dev;

	return 0;
}

int
tf_session_get_fw_session_id(struct tf *tfp,
			     uint8_t *fw_session_id)
{
	int rc;
	struct tf_session *tfs = NULL;

	/* Skip using the check macro as we want to control the error msg */
	if (tfp->session == NULL) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Session not created, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	if (fw_session_id == NULL) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Invalid Argument(s), rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*fw_session_id = tfs->session_id.internal.fw_session_id;

	return 0;
}

int
tf_session_get_session_id(struct tf *tfp,
			  union tf_session_id *session_id)
{
	int rc;
	struct tf_session *tfs = NULL;

	if (tfp->session == NULL) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Session not created, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	if (session_id == NULL) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Invalid Argument(s), rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Using internal version as session client may not exist yet */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*session_id = tfs->session_id;

	return 0;
}

int
tf_session_get_em_ext_db(struct tf *tfp,
			 void **em_ext_db_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	*em_ext_db_handle = NULL;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*em_ext_db_handle = tfs->em_ext_db_handle;
	return rc;
}

int
tf_session_set_em_ext_db(struct tf *tfp,
			 void *em_ext_db_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tfs->em_ext_db_handle = em_ext_db_handle;
	return rc;
}

int
tf_session_get_db(struct tf *tfp,
		  enum tf_module_type type,
		  void **db_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	*db_handle = NULL;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	switch (type) {
	case TF_MODULE_TYPE_IDENTIFIER:
		if (tfs->id_db_handle)
			*db_handle = tfs->id_db_handle;
		else
			rc = -ENOMEM;
		break;
	case TF_MODULE_TYPE_TABLE:
		if (tfs->tbl_db_handle)
			*db_handle = tfs->tbl_db_handle;
		else
			rc = -ENOMEM;

		break;
	case TF_MODULE_TYPE_TCAM:
		if (tfs->tcam_db_handle)
			*db_handle = tfs->tcam_db_handle;
		else
			rc = -ENOMEM;
		break;
	case TF_MODULE_TYPE_EM:
		if (tfs->em_db_handle)
			*db_handle = tfs->em_db_handle;
		else
			rc = -ENOMEM;
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

int
tf_session_set_db(struct tf *tfp,
		  enum tf_module_type type,
		  void *db_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	switch (type) {
	case TF_MODULE_TYPE_IDENTIFIER:
		tfs->id_db_handle = db_handle;
		break;
	case TF_MODULE_TYPE_TABLE:
		tfs->tbl_db_handle = db_handle;
		break;
	case TF_MODULE_TYPE_TCAM:
		tfs->tcam_db_handle = db_handle;
		break;
	case TF_MODULE_TYPE_EM:
		tfs->em_db_handle = db_handle;
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

int
tf_session_get_tcam_shared_db(struct tf *tfp,
			      void **tcam_shared_db_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	*tcam_shared_db_handle = NULL;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*tcam_shared_db_handle = tfs->tcam_shared_db_handle;
	return rc;
}

int
tf_session_set_tcam_shared_db(struct tf *tfp,
			 void *tcam_shared_db_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tfs->tcam_shared_db_handle = tcam_shared_db_handle;
	return rc;
}

int
tf_session_get_sram_db(struct tf *tfp,
		       void **sram_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	*sram_handle = NULL;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*sram_handle = tfs->sram_handle;
	return rc;
}

int
tf_session_set_sram_db(struct tf *tfp,
		       void *sram_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tfs->sram_handle = sram_handle;
	return rc;
}

int
tf_session_get_global_db(struct tf *tfp,
			 void **global_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	*global_handle = NULL;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*global_handle = tfs->global_db_handle;
	return rc;
}

int
tf_session_set_global_db(struct tf *tfp,
			 void *global_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tfs->global_db_handle = global_handle;
	return rc;
}

int
tf_session_get_if_tbl_db(struct tf *tfp,
			 void **if_tbl_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	*if_tbl_handle = NULL;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*if_tbl_handle = tfs->if_tbl_db_handle;
	return rc;
}

int
tf_session_set_if_tbl_db(struct tf *tfp,
			 void *if_tbl_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	if (tfp == NULL)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tfs->if_tbl_db_handle = if_tbl_handle;
	return rc;
}

int
tf_session_set_hotup_state(struct tf *tfp,
			   struct tf_set_session_hotup_state_parms *parms)
{
	int rc = 0;
	struct tf_session *tfs = NULL;

	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Session lookup failed, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	if (!tf_session_is_shared_session(tfs)) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Only shared session able to set state, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_msg_session_set_hotup_state(tfp, parms->state);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Set session hot upgrade state failed, rc:%s\n",
			    strerror(-rc));
	}

	return rc;
}

int
tf_session_get_hotup_state(struct tf *tfp,
			   struct tf_get_session_hotup_state_parms *parms)
{
	int rc = 0;
	struct tf_session *tfs = NULL;

	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Session lookup failed, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	if (!tf_session_is_shared_session(tfs)) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Only shared session able to get state, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_msg_session_get_hotup_state(tfp, &parms->state, &parms->ref_cnt);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Get session hot upgrade state failed, rc:%s\n",
			    strerror(-rc));
	}

	return rc;
}
