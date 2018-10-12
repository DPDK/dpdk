/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP
 *
 */
#include <fsl_mc_sys.h>
#include <fsl_mc_cmd.h>
#include <fsl_dpopr.h>
#include <fsl_dpseci.h>
#include <fsl_dpseci_cmd.h>

/**
 * dpseci_open() - Open a control session for the specified object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @dpseci_id:	DPSECI unique ID
 * @token:	Returned token; use in subsequent API calls
 *
 * This function can be used to open a control session for an
 * already created object; an object may have been declared in
 * the DPL or by calling the dpseci_create() function.
 * This function returns a unique authentication token,
 * associated with the specific object ID and the specific MC
 * portal; this token must be used in all subsequent commands for
 * this specific object.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_open(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		int dpseci_id,
		uint16_t *token)
{
	struct dpseci_cmd_open *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_OPEN,
					  cmd_flags,
					  0);
	cmd_params = (struct dpseci_cmd_open *)cmd.params;
	cmd_params->dpseci_id = cpu_to_le32(dpseci_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}

/**
 * dpseci_close() - Close the control session of the object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 *
 * After this function is called, no further operations are
 * allowed on the object without opening a new control session.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_close(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_CLOSE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpseci_create() - Create the DPSECI object
 * @mc_io:	Pointer to MC portal's I/O object
 * @dprc_token:	Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @cfg:	Configuration structure
 * @obj_id:	Returned object id
 *
 * Create the DPSECI object, allocate required resources and
 * perform required initialization.
 *
 * The object can be created either by declaring it in the
 * DPL file, or by calling this function.
 *
 * The function accepts an authentication token of a parent
 * container that this object should be assigned to. The token
 * can be '0' so the object will be assigned to the default container.
 * The newly created object can be opened with the returned
 * object id and using the container's associated tokens and MC portals.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_create(struct fsl_mc_io *mc_io,
		  uint16_t dprc_token,
		  uint32_t cmd_flags,
		  const struct dpseci_cfg *cfg,
		  uint32_t *obj_id)
{
	struct dpseci_cmd_create *cmd_params;
	struct mc_command cmd = { 0 };
	int err, i;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_CREATE,
					  cmd_flags,
					  dprc_token);
	cmd_params = (struct dpseci_cmd_create *)cmd.params;
	for (i = 0; i < 8; i++)
		cmd_params->priorities[i] = cfg->priorities[i];
	for (i = 0; i < 8; i++)
		cmd_params->priorities2[i] = cfg->priorities[8 + i];
	cmd_params->num_tx_queues = cfg->num_tx_queues;
	cmd_params->num_rx_queues = cfg->num_rx_queues;
	cmd_params->options = cpu_to_le32(cfg->options);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*obj_id = mc_cmd_read_object_id(&cmd);

	return 0;
}

/**
 * dpseci_destroy() - Destroy the DPSECI object and release all its resources.
 * @mc_io:	Pointer to MC portal's I/O object
 * @dprc_token: Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @object_id:	The object id; it must be a valid id within the container that
 * created this object;
 *
 * The function accepts the authentication token of the parent container that
 * created the object (not the one that currently owns the object). The object
 * is searched within parent using the provided 'object_id'.
 * All tokens to the object must be closed before calling destroy.
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpseci_destroy(struct fsl_mc_io *mc_io,
		   uint16_t dprc_token,
		   uint32_t cmd_flags,
		   uint32_t object_id)
{
	struct dpseci_cmd_destroy *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_DESTROY,
					  cmd_flags,
					  dprc_token);
	cmd_params = (struct dpseci_cmd_destroy *)cmd.params;
	cmd_params->dpseci_id = cpu_to_le32(object_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpseci_enable() - Enable the DPSECI, allow sending and receiving frames.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_enable(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_ENABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpseci_disable() - Disable the DPSECI, stop sending and receiving frames.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_disable(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_DISABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpseci_is_enabled() - Check if the DPSECI is enabled.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @en:		Returns '1' if object is enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_is_enabled(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      int *en)
{
	struct dpseci_rsp_is_enabled *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_IS_ENABLED,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpseci_rsp_is_enabled *)cmd.params;
	*en = dpseci_get_field(rsp_params->en, ENABLE);

	return 0;
}

/**
 * dpseci_reset() - Reset the DPSECI, returns the object to initial state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_reset(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_RESET,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpseci_get_attributes() - Retrieve DPSECI attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @attr:	Returned object's attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_get_attributes(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  struct dpseci_attr *attr)
{
	struct dpseci_rsp_get_attr *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpseci_rsp_get_attr *)cmd.params;
	attr->id = le32_to_cpu(rsp_params->id);
	attr->options = le32_to_cpu(rsp_params->options);
	attr->num_tx_queues = rsp_params->num_tx_queues;
	attr->num_rx_queues = rsp_params->num_rx_queues;

	return 0;
}

/**
 * dpseci_set_rx_queue() - Set Rx queue configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @queue:	Select the queue relative to number of
 *		priorities configured at DPSECI creation; use
 *		DPSECI_ALL_QUEUES to configure all Rx queues identically.
 * @cfg:	Rx queue configuration
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_set_rx_queue(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t queue,
			const struct dpseci_rx_queue_cfg *cfg)
{
	struct dpseci_cmd_set_rx_queue *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_SET_RX_QUEUE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpseci_cmd_set_rx_queue *)cmd.params;
	cmd_params->dest_id = cpu_to_le32(cfg->dest_cfg.dest_id);
	cmd_params->dest_priority = cfg->dest_cfg.priority;
	cmd_params->queue = queue;
	cmd_params->user_ctx = cpu_to_le64(cfg->user_ctx);
	cmd_params->options = cpu_to_le32(cfg->options);
	dpseci_set_field(cmd_params->dest_type,
			 DEST_TYPE,
			 cfg->dest_cfg.dest_type);
	dpseci_set_field(cmd_params->order_preservation_en,
			 ORDER_PRESERVATION,
			 cfg->order_preservation_en);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpseci_get_rx_queue() - Retrieve Rx queue attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @queue:	Select the queue relative to number of
 *				priorities configured at DPSECI creation
 * @attr:	Returned Rx queue attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_get_rx_queue(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t queue,
			struct dpseci_rx_queue_attr *attr)
{
	struct dpseci_rsp_get_rx_queue *rsp_params;
	struct dpseci_cmd_get_queue *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_RX_QUEUE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpseci_cmd_get_queue *)cmd.params;
	cmd_params->queue = queue;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpseci_rsp_get_rx_queue *)cmd.params;
	attr->user_ctx = le64_to_cpu(rsp_params->user_ctx);
	attr->fqid = le32_to_cpu(rsp_params->fqid);
	attr->dest_cfg.dest_id = le32_to_cpu(rsp_params->dest_id);
	attr->dest_cfg.priority = rsp_params->dest_priority;
	attr->dest_cfg.dest_type =
		dpseci_get_field(rsp_params->dest_type,
				 DEST_TYPE);
	attr->order_preservation_en =
		dpseci_get_field(rsp_params->order_preservation_en,
				 ORDER_PRESERVATION);

	return 0;
}

/**
 * dpseci_get_tx_queue() - Retrieve Tx queue attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @queue:	Select the queue relative to number of
 *		priorities configured at DPSECI creation
 * @attr:	Returned Tx queue attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_get_tx_queue(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t queue,
			struct dpseci_tx_queue_attr *attr)
{
	struct dpseci_rsp_get_tx_queue *rsp_params;
	struct dpseci_cmd_get_queue *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_TX_QUEUE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpseci_cmd_get_queue *)cmd.params;
	cmd_params->queue = queue;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpseci_rsp_get_tx_queue *)cmd.params;
	attr->fqid = le32_to_cpu(rsp_params->fqid);
	attr->priority = rsp_params->priority;

	return 0;
}

/**
 * dpseci_get_sec_attr() - Retrieve SEC accelerator attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @attr:	Returned SEC attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_get_sec_attr(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpseci_sec_attr *attr)
{
	struct dpseci_rsp_get_sec_attr *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_SEC_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpseci_rsp_get_sec_attr *)cmd.params;
	attr->ip_id = le16_to_cpu(rsp_params->ip_id);
	attr->major_rev = rsp_params->major_rev;
	attr->minor_rev = rsp_params->minor_rev;
	attr->era = rsp_params->era;
	attr->deco_num = rsp_params->deco_num;
	attr->zuc_auth_acc_num = rsp_params->zuc_auth_acc_num;
	attr->zuc_enc_acc_num = rsp_params->zuc_enc_acc_num;
	attr->snow_f8_acc_num = rsp_params->snow_f8_acc_num;
	attr->snow_f9_acc_num = rsp_params->snow_f9_acc_num;
	attr->crc_acc_num = rsp_params->crc_acc_num;
	attr->pk_acc_num = rsp_params->pk_acc_num;
	attr->kasumi_acc_num = rsp_params->kasumi_acc_num;
	attr->rng_acc_num = rsp_params->rng_acc_num;
	attr->md_acc_num = rsp_params->md_acc_num;
	attr->arc4_acc_num = rsp_params->arc4_acc_num;
	attr->des_acc_num = rsp_params->des_acc_num;
	attr->aes_acc_num = rsp_params->aes_acc_num;
	attr->ccha_acc_num = rsp_params->ccha_acc_num;
	attr->ptha_acc_num = rsp_params->ptha_acc_num;

	return 0;
}

/**
 * dpseci_get_sec_counters() - Retrieve SEC accelerator counters.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @counters:	Returned SEC counters
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_get_sec_counters(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token,
			    struct dpseci_sec_counters *counters)
{
	struct dpseci_rsp_get_sec_counters *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_SEC_COUNTERS,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpseci_rsp_get_sec_counters *)cmd.params;
	counters->dequeued_requests =
				le64_to_cpu(rsp_params->dequeued_requests);
	counters->ob_enc_requests = le64_to_cpu(rsp_params->ob_enc_requests);
	counters->ib_dec_requests = le64_to_cpu(rsp_params->ib_dec_requests);
	counters->ob_enc_bytes = le64_to_cpu(rsp_params->ob_enc_bytes);
	counters->ob_prot_bytes = le64_to_cpu(rsp_params->ob_prot_bytes);
	counters->ib_dec_bytes = le64_to_cpu(rsp_params->ib_dec_bytes);
	counters->ib_valid_bytes = le64_to_cpu(rsp_params->ib_valid_bytes);

	return 0;
}

/**
 * dpseci_get_api_version() - Get Data Path SEC Interface API version
 * @mc_io:  Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @major_ver:	Major version of data path sec API
 * @minor_ver:	Minor version of data path sec API
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpseci_get_api_version(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t *major_ver,
			   uint16_t *minor_ver)
{
	struct dpseci_rsp_get_api_version *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_API_VERSION,
					cmd_flags,
					0);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpseci_rsp_get_api_version *)cmd.params;
	*major_ver = le16_to_cpu(rsp_params->major);
	*minor_ver = le16_to_cpu(rsp_params->minor);

	return 0;
}

/**
 * dpseci_set_opr() - Set Order Restoration configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @index:	The queue index
 * @options:	Configuration mode options
 *			can be OPR_OPT_CREATE or OPR_OPT_RETIRE
 * @cfg:	Configuration options for the OPR
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpseci_set_opr(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   uint8_t index,
		   uint8_t options,
		   struct opr_cfg *cfg)
{
	struct dpseci_cmd_set_opr *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_SET_OPR,
					  cmd_flags,
					  token);
	cmd_params = (struct dpseci_cmd_set_opr *)cmd.params;
	cmd_params->index = index;
	cmd_params->options = options;
	cmd_params->oloe = cfg->oloe;
	cmd_params->oeane = cfg->oeane;
	cmd_params->olws = cfg->olws;
	cmd_params->oa = cfg->oa;
	cmd_params->oprrws = cfg->oprrws;

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpseci_get_opr() - Retrieve Order Restoration config and query.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @index:	The queue index
 * @cfg:	Returned OPR configuration
 * @qry:	Returned OPR query
 *
 * Return:     '0' on Success; Error code otherwise.
 */
int dpseci_get_opr(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   uint8_t index,
		   struct opr_cfg *cfg,
		   struct opr_qry *qry)
{
	struct dpseci_rsp_get_opr *rsp_params;
	struct dpseci_cmd_get_opr *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_OPR,
					  cmd_flags,
					  token);
	cmd_params = (struct dpseci_cmd_get_opr *)cmd.params;
	cmd_params->index = index;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpseci_rsp_get_opr *)cmd.params;
	cfg->oloe = rsp_params->oloe;
	cfg->oeane = rsp_params->oeane;
	cfg->olws = rsp_params->olws;
	cfg->oa = rsp_params->oa;
	cfg->oprrws = rsp_params->oprrws;
	qry->rip = dpseci_get_field(rsp_params->flags, RIP);
	qry->enable = dpseci_get_field(rsp_params->flags, OPR_ENABLE);
	qry->nesn = le16_to_cpu(rsp_params->nesn);
	qry->ndsn = le16_to_cpu(rsp_params->ndsn);
	qry->ea_tseq = le16_to_cpu(rsp_params->ea_tseq);
	qry->tseq_nlis = dpseci_get_field(rsp_params->tseq_nlis, TSEQ_NLIS);
	qry->ea_hseq = le16_to_cpu(rsp_params->ea_hseq);
	qry->hseq_nlis = dpseci_get_field(rsp_params->hseq_nlis, HSEQ_NLIS);
	qry->ea_hptr = le16_to_cpu(rsp_params->ea_hptr);
	qry->ea_tptr = le16_to_cpu(rsp_params->ea_tptr);
	qry->opr_vid = le16_to_cpu(rsp_params->opr_vid);
	qry->opr_id = le16_to_cpu(rsp_params->opr_id);

	return 0;
}

/**
 * dpseci_set_congestion_notification() - Set congestion group
 *	notification configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @cfg:	congestion notification configuration
 *
 * Return:	'0' on success, error code otherwise
 */
int dpseci_set_congestion_notification(
			struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			const struct dpseci_congestion_notification_cfg *cfg)
{
	struct dpseci_cmd_set_congestion_notification *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(
			DPSECI_CMDID_SET_CONGESTION_NOTIFICATION,
			cmd_flags,
			token);

	cmd_params =
		(struct dpseci_cmd_set_congestion_notification *)cmd.params;
	cmd_params->dest_id = cfg->dest_cfg.dest_id;
	cmd_params->dest_priority = cfg->dest_cfg.priority;
	cmd_params->message_ctx = cfg->message_ctx;
	cmd_params->message_iova = cfg->message_iova;
	cmd_params->notification_mode = cfg->notification_mode;
	cmd_params->threshold_entry = cfg->threshold_entry;
	cmd_params->threshold_exit = cfg->threshold_exit;
	dpseci_set_field(cmd_params->type_units,
			 DEST_TYPE,
			 cfg->dest_cfg.dest_type);
	dpseci_set_field(cmd_params->type_units,
			 CG_UNITS,
			 cfg->units);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpseci_get_congestion_notification() - Get congestion group
 *	notification configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSECI object
 * @cfg:	congestion notification configuration
 *
 * Return:	'0' on success, error code otherwise
 */
int dpseci_get_congestion_notification(
				struct fsl_mc_io *mc_io,
				uint32_t cmd_flags,
				uint16_t token,
				struct dpseci_congestion_notification_cfg *cfg)
{
	struct dpseci_cmd_set_congestion_notification *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(
			DPSECI_CMDID_GET_CONGESTION_NOTIFICATION,
			cmd_flags,
			token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params =
		(struct dpseci_cmd_set_congestion_notification *)cmd.params;

	cfg->dest_cfg.dest_id = le32_to_cpu(rsp_params->dest_id);
	cfg->dest_cfg.priority = rsp_params->dest_priority;
	cfg->notification_mode = le16_to_cpu(rsp_params->notification_mode);
	cfg->message_ctx = le64_to_cpu(rsp_params->message_ctx);
	cfg->message_iova = le64_to_cpu(rsp_params->message_iova);
	cfg->threshold_entry = le32_to_cpu(rsp_params->threshold_entry);
	cfg->threshold_exit = le32_to_cpu(rsp_params->threshold_exit);
	cfg->units = dpseci_get_field(rsp_params->type_units, CG_UNITS);
	cfg->dest_cfg.dest_type = dpseci_get_field(rsp_params->type_units,
						DEST_TYPE);

	return 0;
}
