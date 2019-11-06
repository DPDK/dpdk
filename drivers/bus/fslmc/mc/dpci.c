/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2017-2019 NXP
 *
 */
#include <fsl_mc_sys.h>
#include <fsl_mc_cmd.h>
#include <fsl_dpci.h>
#include <fsl_dpci_cmd.h>

/**
 * dpci_open() - Open a control session for the specified object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @dpci_id:	DPCI unique ID
 * @token:	Returned token; use in subsequent API calls
 *
 * This function can be used to open a control session for an
 * already created object; an object may have been declared in
 * the DPL or by calling the dpci_create() function.
 * This function returns a unique authentication token,
 * associated with the specific object ID and the specific MC
 * portal; this token must be used in all subsequent commands for
 * this specific object.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_open(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      int dpci_id,
	      uint16_t *token)
{
	struct dpci_cmd_open *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_OPEN,
					  cmd_flags,
					  0);
	cmd_params = (struct dpci_cmd_open *)cmd.params;
	cmd_params->dpci_id = cpu_to_le32(dpci_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}

/**
 * dpci_close() - Close the control session of the object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 *
 * After this function is called, no further operations are
 * allowed on the object without opening a new control session.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_close(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_CLOSE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpci_create() - Create the DPCI object.
 * @mc_io:	Pointer to MC portal's I/O object
 * @dprc_token:	Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @cfg:	Configuration structure
 * @obj_id:	Returned object id
 *
 * Create the DPCI object, allocate required resources and perform required
 * initialization.
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
int dpci_create(struct fsl_mc_io *mc_io,
		uint16_t dprc_token,
		uint32_t cmd_flags,
		const struct dpci_cfg *cfg,
		uint32_t *obj_id)
{
	struct dpci_cmd_create *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_CREATE,
					  cmd_flags,
					  dprc_token);
	cmd_params = (struct dpci_cmd_create *)cmd.params;
	cmd_params->num_of_priorities = cfg->num_of_priorities;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*obj_id = mc_cmd_read_object_id(&cmd);

	return 0;
}

/**
 * dpci_destroy() - Destroy the DPCI object and release all its resources.
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
int dpci_destroy(struct fsl_mc_io *mc_io,
		 uint16_t dprc_token,
		 uint32_t cmd_flags,
		 uint32_t object_id)
{
	struct dpci_cmd_destroy *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_DESTROY,
					  cmd_flags,
					  dprc_token);
	cmd_params = (struct dpci_cmd_destroy *)cmd.params;
	cmd_params->dpci_id = cpu_to_le32(object_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpci_enable() - Enable the DPCI, allow sending and receiving frames.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_enable(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_ENABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpci_disable() - Disable the DPCI, stop sending and receiving frames.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_disable(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_DISABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpci_is_enabled() - Check if the DPCI is enabled.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 * @en:		Returns '1' if object is enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_is_enabled(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    int *en)
{
	struct dpci_rsp_is_enabled *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_IS_ENABLED, cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpci_rsp_is_enabled *)cmd.params;
	*en = dpci_get_field(rsp_params->en, ENABLE);

	return 0;
}

/**
 * dpci_reset() - Reset the DPCI, returns the object to initial state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_reset(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_RESET,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpci_get_attributes() - Retrieve DPCI attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 * @attr:	Returned object's attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_get_attributes(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpci_attr *attr)
{
	struct dpci_rsp_get_attr *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpci_rsp_get_attr *)cmd.params;
	attr->id = le32_to_cpu(rsp_params->id);
	attr->num_of_priorities = rsp_params->num_of_priorities;

	return 0;
}

/**
 * dpci_set_rx_queue() - Set Rx queue configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 * @priority:	Select the queue relative to number of
 *			priorities configured at DPCI creation; use
 *			DPCI_ALL_QUEUES to configure all Rx queues
 *			identically.
 * @cfg:	Rx queue configuration
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_set_rx_queue(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t priority,
		      const struct dpci_rx_queue_cfg *cfg)
{
	struct dpci_cmd_set_rx_queue *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_SET_RX_QUEUE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpci_cmd_set_rx_queue *)cmd.params;
	cmd_params->dest_id = cpu_to_le32(cfg->dest_cfg.dest_id);
	cmd_params->dest_priority = cfg->dest_cfg.priority;
	cmd_params->priority = priority;
	cmd_params->user_ctx = cpu_to_le64(cfg->user_ctx);
	cmd_params->options = cpu_to_le32(cfg->options);
	dpci_set_field(cmd_params->dest_type,
		       DEST_TYPE,
		       cfg->dest_cfg.dest_type);
	dpci_set_field(cmd_params->dest_type,
		       ORDER_PRESERVATION,
		       cfg->order_preservation_en);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpci_get_rx_queue() - Retrieve Rx queue attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 * @priority:	Select the queue relative to number of
 *		priorities configured at DPCI creation
 * @attr:	Returned Rx queue attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_get_rx_queue(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t priority,
		      struct dpci_rx_queue_attr *attr)
{
	struct dpci_cmd_get_queue *cmd_params;
	struct dpci_rsp_get_rx_queue *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_GET_RX_QUEUE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpci_cmd_get_queue *)cmd.params;
	cmd_params->priority = priority;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpci_rsp_get_rx_queue *)cmd.params;
	attr->user_ctx = le64_to_cpu(rsp_params->user_ctx);
	attr->fqid = le32_to_cpu(rsp_params->fqid);
	attr->dest_cfg.dest_id = le32_to_cpu(rsp_params->dest_id);
	attr->dest_cfg.priority = rsp_params->dest_priority;
	attr->dest_cfg.dest_type = dpci_get_field(rsp_params->dest_type,
						  DEST_TYPE);

	return 0;
}

/**
 * dpci_get_tx_queue() - Retrieve Tx queue attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 * @priority:	Select the queue relative to number of
 *		priorities of the peer DPCI object
 * @attr:	Returned Tx queue attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_get_tx_queue(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t priority,
		      struct dpci_tx_queue_attr *attr)
{
	struct dpci_cmd_get_queue *cmd_params;
	struct dpci_rsp_get_tx_queue *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_GET_TX_QUEUE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpci_cmd_get_queue *)cmd.params;
	cmd_params->priority = priority;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpci_rsp_get_tx_queue *)cmd.params;
	attr->fqid = le32_to_cpu(rsp_params->fqid);

	return 0;
}

/**
 * dpci_get_api_version() - Get communication interface API version
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @major_ver:	Major version of data path communication interface API
 * @minor_ver:	Minor version of data path communication interface API
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpci_get_api_version(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t *major_ver,
			 uint16_t *minor_ver)
{
	struct dpci_rsp_get_api_version *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPCI_CMDID_GET_API_VERSION,
					cmd_flags,
					0);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpci_rsp_get_api_version *)cmd.params;
	*major_ver = le16_to_cpu(rsp_params->major);
	*minor_ver = le16_to_cpu(rsp_params->minor);

	return 0;
}

/**
 * dpci_set_opr() - Set Order Restoration configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 * @index:	The queue index
 * @options:	Configuration mode options
 *		can be OPR_OPT_CREATE or OPR_OPT_RETIRE
 * @cfg:	Configuration options for the OPR
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_set_opr(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token,
		 uint8_t index,
		 uint8_t options,
		 struct opr_cfg *cfg)
{
	struct dpci_cmd_set_opr *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_SET_OPR,
					  cmd_flags,
					  token);
	cmd_params = (struct dpci_cmd_set_opr *)cmd.params;
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
 * dpci_get_opr() - Retrieve Order Restoration config and query.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCI object
 * @index:	The queue index
 * @cfg:	Returned OPR configuration
 * @qry:	Returned OPR query
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpci_get_opr(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token,
		 uint8_t index,
		 struct opr_cfg *cfg,
		 struct opr_qry *qry)
{
	struct dpci_rsp_get_opr *rsp_params;
	struct dpci_cmd_get_opr *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCI_CMDID_GET_OPR,
					  cmd_flags,
					  token);
	cmd_params = (struct dpci_cmd_get_opr *)cmd.params;
	cmd_params->index = index;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpci_rsp_get_opr *)cmd.params;
	cfg->oloe = rsp_params->oloe;
	cfg->oeane = rsp_params->oeane;
	cfg->olws = rsp_params->olws;
	cfg->oa = rsp_params->oa;
	cfg->oprrws = rsp_params->oprrws;
	qry->rip = dpci_get_field(rsp_params->flags, RIP);
	qry->enable = dpci_get_field(rsp_params->flags, OPR_ENABLE);
	qry->nesn = le16_to_cpu(rsp_params->nesn);
	qry->ndsn = le16_to_cpu(rsp_params->ndsn);
	qry->ea_tseq = le16_to_cpu(rsp_params->ea_tseq);
	qry->tseq_nlis = dpci_get_field(rsp_params->tseq_nlis, TSEQ_NLIS);
	qry->ea_hseq = le16_to_cpu(rsp_params->ea_hseq);
	qry->hseq_nlis = dpci_get_field(rsp_params->hseq_nlis, HSEQ_NLIS);
	qry->ea_hptr = le16_to_cpu(rsp_params->ea_hptr);
	qry->ea_tptr = le16_to_cpu(rsp_params->ea_tptr);
	qry->opr_vid = le16_to_cpu(rsp_params->opr_vid);
	qry->opr_id = le16_to_cpu(rsp_params->opr_id);

	return 0;
}
