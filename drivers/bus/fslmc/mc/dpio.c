/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2017 NXP
 *
 */
#include <fsl_mc_sys.h>
#include <fsl_mc_cmd.h>
#include <fsl_dpio.h>
#include <fsl_dpio_cmd.h>

/**
 * dpio_open() - Open a control session for the specified object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @dpio_id:	DPIO unique ID
 * @token:	Returned token; use in subsequent API calls
 *
 * This function can be used to open a control session for an
 * already created object; an object may have been declared in
 * the DPL or by calling the dpio_create() function.
 * This function returns a unique authentication token,
 * associated with the specific object ID and any MC portals
 * assigned to the parent container; this token must be used in
 * all subsequent commands for this specific object.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_open(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      int dpio_id,
	      uint16_t *token)
{
	struct dpio_cmd_open *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_OPEN,
					  cmd_flags,
					  0);
	cmd_params = (struct dpio_cmd_open *)cmd.params;
	cmd_params->dpio_id = cpu_to_le32(dpio_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}

/**
 * dpio_close() - Close the control session of the object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_close(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_CLOSE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpio_create() - Create the DPIO object.
 * @mc_io:	Pointer to MC portal's I/O object
 * @dprc_token:	Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @cfg:	Configuration structure
 * @obj_id:	Returned object id
 *
 * Create the DPIO object, allocate required resources and
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
int dpio_create(struct fsl_mc_io *mc_io,
		uint16_t dprc_token,
		uint32_t cmd_flags,
		const struct dpio_cfg *cfg,
		uint32_t *obj_id)
{
	struct dpio_cmd_create *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_CREATE,
					  cmd_flags,
					  dprc_token);
	cmd_params = (struct dpio_cmd_create *)cmd.params;
	cmd_params->num_priorities = cfg->num_priorities;
	dpio_set_field(cmd_params->channel_mode,
		       CHANNEL_MODE,
		       cfg->channel_mode);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*obj_id = mc_cmd_read_object_id(&cmd);

	return 0;
}

/**
 * dpio_destroy() - Destroy the DPIO object and release all its resources.
 * @mc_io:	Pointer to MC portal's I/O object
 * @dprc_token: Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @object_id:	The object id; it must be a valid id within the container that
 *		created this object;
 *
 * The function accepts the authentication token of the parent container that
 * created the object (not the one that currently owns the object). The object
 * is searched within parent using the provided 'object_id'.
 * All tokens to the object must be closed before calling destroy.
 *
 * Return:	'0' on Success; Error code otherwise
 */
int dpio_destroy(struct fsl_mc_io *mc_io,
		 uint16_t dprc_token,
		 uint32_t cmd_flags,
		 uint32_t object_id)
{
	struct dpio_cmd_destroy *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_DESTROY,
			cmd_flags,
			dprc_token);

	/* set object id to destroy */
	cmd_params = (struct dpio_cmd_destroy *)cmd.params;
	cmd_params->dpio_id = cpu_to_le32(object_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpio_enable() - Enable the DPIO, allow I/O portal operations.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 *
 * Return:	'0' on Success; Error code otherwise
 */
int dpio_enable(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_ENABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpio_disable() - Disable the DPIO, stop any I/O portal operation.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 *
 * Return:	'0' on Success; Error code otherwise
 */
int dpio_disable(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_DISABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpio_is_enabled() - Check if the DPIO is enabled.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 * @en:		Returns '1' if object is enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_is_enabled(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    int *en)
{
	struct dpio_rsp_is_enabled *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_IS_ENABLED, cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpio_rsp_is_enabled *)cmd.params;
	*en = dpio_get_field(rsp_params->en, ENABLE);

	return 0;
}

/**
 * dpio_reset() - Reset the DPIO, returns the object to initial state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_reset(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_RESET,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpio_get_attributes(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpio_attr *attr)
{
	struct dpio_rsp_get_attr *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpio_rsp_get_attr *)cmd.params;
	attr->id = le32_to_cpu(rsp_params->id);
	attr->qbman_portal_id = le16_to_cpu(rsp_params->qbman_portal_id);
	attr->num_priorities = rsp_params->num_priorities;
	attr->qbman_portal_ce_offset =
				le64_to_cpu(rsp_params->qbman_portal_ce_offset);
	attr->qbman_portal_ci_offset =
				le64_to_cpu(rsp_params->qbman_portal_ci_offset);
	attr->qbman_version = le32_to_cpu(rsp_params->qbman_version);
	attr->clk = le32_to_cpu(rsp_params->clk);
	attr->channel_mode = dpio_get_field(rsp_params->channel_mode,
					    ATTR_CHANNEL_MODE);

	return 0;
}

/**
 * dpio_set_stashing_destination() - Set the stashing destination.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 * @sdest:	Stashing destination value
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_set_stashing_destination(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  uint8_t sdest)
{
	struct dpio_stashing_dest *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_SET_STASHING_DEST,
					  cmd_flags,
					  token);
	cmd_params = (struct dpio_stashing_dest *)cmd.params;
	cmd_params->sdest = sdest;

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpio_get_stashing_destination() - Get the stashing destination..
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 * @sdest:	Returns the stashing destination value
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_get_stashing_destination(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  uint8_t *sdest)
{
	struct dpio_stashing_dest *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_GET_STASHING_DEST,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpio_stashing_dest *)cmd.params;
	*sdest = rsp_params->sdest;

	return 0;
}

/**
 * dpio_add_static_dequeue_channel() - Add a static dequeue channel.
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPIO object
 * @dpcon_id:		DPCON object ID
 * @channel_index:	Returned channel index to be used in qbman API
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_add_static_dequeue_channel(struct fsl_mc_io *mc_io,
				    uint32_t cmd_flags,
				    uint16_t token,
				    int dpcon_id,
				    uint8_t *channel_index)
{
	struct dpio_rsp_add_static_dequeue_channel *rsp_params;
	struct dpio_cmd_static_dequeue_channel *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPIO_CMDID_ADD_STATIC_DEQUEUE_CHANNEL,
					  cmd_flags,
					  token);
	cmd_params = (struct dpio_cmd_static_dequeue_channel *)cmd.params;
	cmd_params->dpcon_id = cpu_to_le32(dpcon_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpio_rsp_add_static_dequeue_channel *)cmd.params;
	*channel_index = rsp_params->channel_index;

	return 0;
}

/**
 * dpio_remove_static_dequeue_channel() - Remove a static dequeue channel.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 * @dpcon_id:	DPCON object ID
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_remove_static_dequeue_channel(struct fsl_mc_io *mc_io,
				       uint32_t cmd_flags,
				       uint16_t token,
				       int dpcon_id)
{
	struct dpio_cmd_static_dequeue_channel *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(
				DPIO_CMDID_REMOVE_STATIC_DEQUEUE_CHANNEL,
				cmd_flags,
				token);
	cmd_params = (struct dpio_cmd_static_dequeue_channel *)cmd.params;
	cmd_params->dpcon_id = cpu_to_le32(dpcon_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpio_get_api_version() - Get Data Path I/O API version
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @major_ver:	Major version of data path i/o API
 * @minor_ver:	Minor version of data path i/o API
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpio_get_api_version(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t *major_ver,
			 uint16_t *minor_ver)
{
	struct dpio_rsp_get_api_version *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPIO_CMDID_GET_API_VERSION,
					cmd_flags,
					0);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpio_rsp_get_api_version *)cmd.params;
	*major_ver = le16_to_cpu(rsp_params->major);
	*minor_ver = le16_to_cpu(rsp_params->minor);

	return 0;
}
