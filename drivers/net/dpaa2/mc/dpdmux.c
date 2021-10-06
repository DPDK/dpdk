/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2018-2021 NXP
 *
 */
#include <fsl_mc_sys.h>
#include <fsl_mc_cmd.h>
#include <fsl_dpdmux.h>
#include <fsl_dpdmux_cmd.h>

/** @addtogroup dpdmux
 * @{
 */

/**
 * dpdmux_open() - Open a control session for the specified object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @dpdmux_id:		DPDMUX unique ID
 * @token:		Returned token; use in subsequent API calls
 *
 * This function can be used to open a control session for an
 * already created object; an object may have been declared in
 * the DPL or by calling the dpdmux_create() function.
 * This function returns a unique authentication token,
 * associated with the specific object ID and the specific MC
 * portal; this token must be used in all subsequent commands for
 * this specific object.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_open(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		int dpdmux_id,
		uint16_t *token)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_open *cmd_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_OPEN,
					  cmd_flags,
					  0);
	cmd_params = (struct dpdmux_cmd_open *)cmd.params;
	cmd_params->dpdmux_id = cpu_to_le32(dpdmux_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}

/**
 * dpdmux_close() - Close the control session of the object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPDMUX object
 *
 * After this function is called, no further operations are
 * allowed on the object without opening a new control session.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_close(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_CLOSE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_create() - Create the DPDMUX object
 * @mc_io:	Pointer to MC portal's I/O object
 * @dprc_token:	Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @cfg:	Configuration structure
 * @obj_id: returned object id
 *
 * Create the DPDMUX object, allocate required resources and
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
int dpdmux_create(struct fsl_mc_io *mc_io,
		  uint16_t dprc_token,
		  uint32_t cmd_flags,
		  const struct dpdmux_cfg	*cfg,
		  uint32_t *obj_id)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_create *cmd_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_CREATE,
					  cmd_flags,
					  dprc_token);
	cmd_params = (struct dpdmux_cmd_create *)cmd.params;
	cmd_params->method = cfg->method;
	cmd_params->manip = cfg->manip;
	cmd_params->num_ifs = cpu_to_le16(cfg->num_ifs);
	cmd_params->default_if = cpu_to_le16(cfg->default_if);
	cmd_params->adv_max_dmat_entries =
			cpu_to_le16(cfg->adv.max_dmat_entries);
	cmd_params->adv_max_mc_groups = cpu_to_le16(cfg->adv.max_mc_groups);
	cmd_params->adv_max_vlan_ids = cpu_to_le16(cfg->adv.max_vlan_ids);
	cmd_params->mem_size = cpu_to_le16(cfg->adv.mem_size);
	cmd_params->options = cpu_to_le64(cfg->adv.options);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*obj_id = mc_cmd_read_object_id(&cmd);

	return 0;
}

/**
 * dpdmux_destroy() - Destroy the DPDMUX object and release all its resources.
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
int dpdmux_destroy(struct fsl_mc_io *mc_io,
		   uint16_t dprc_token,
		   uint32_t cmd_flags,
		   uint32_t object_id)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_destroy *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_DESTROY,
					  cmd_flags,
					  dprc_token);
	cmd_params = (struct dpdmux_cmd_destroy *)cmd.params;
	cmd_params->dpdmux_id = cpu_to_le32(object_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_enable() - Enable DPDMUX functionality
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_enable(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_ENABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_disable() - Disable DPDMUX functionality
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_disable(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_DISABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_is_enabled() - Check if the DPDMUX is enabled.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 * @en:		Returns '1' if object is enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_is_enabled(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      int *en)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_rsp_is_enabled *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IS_ENABLED,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpdmux_rsp_is_enabled *)cmd.params;
	*en = dpdmux_get_field(rsp_params->en, ENABLE);

	return 0;
}

/**
 * dpdmux_reset() - Reset the DPDMUX, returns the object to initial state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_reset(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_RESET,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_set_resetable() - Set overall resetable DPDMUX parameters.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 * @skip_reset_flags:	By default all are 0.
 *			By setting 1 will deactivate the reset.
 *	The flags are:
 *			DPDMUX_SKIP_DEFAULT_INTERFACE  0x01
 *			DPDMUX_SKIP_UNICAST_RULES      0x02
 *			DPDMUX_SKIP_MULTICAST_RULES    0x04
 *
 * For example, by default, through DPDMUX_RESET the default
 * interface will be restored with the one from create.
 * By setting DPDMUX_SKIP_DEFAULT_INTERFACE flag,
 * through DPDMUX_RESET the default interface will not be modified.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_set_resetable(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  uint8_t skip_reset_flags)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_set_skip_reset_flags *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_SET_RESETABLE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_set_skip_reset_flags *)cmd.params;
	dpdmux_set_field(cmd_params->skip_reset_flags,
			SKIP_RESET_FLAGS,
			skip_reset_flags);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_get_resetable() - Get overall resetable parameters.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 * @skip_reset_flags:	Get the reset flags.
 *
 *	The flags are:
 *			DPDMUX_SKIP_DEFAULT_INTERFACE  0x01
 *			DPDMUX_SKIP_UNICAST_RULES      0x02
 *			DPDMUX_SKIP_MULTICAST_RULES    0x04
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_get_resetable(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  uint8_t *skip_reset_flags)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_rsp_get_skip_reset_flags *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_GET_RESETABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpdmux_rsp_get_skip_reset_flags *)cmd.params;
	*skip_reset_flags = dpdmux_get_field(rsp_params->skip_reset_flags,
			SKIP_RESET_FLAGS);

	return 0;
}

/**
 * dpdmux_get_attributes() - Retrieve DPDMUX attributes
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 * @attr:	Returned object's attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_get_attributes(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  struct dpdmux_attr *attr)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_rsp_get_attr *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpdmux_rsp_get_attr *)cmd.params;
	attr->id = le32_to_cpu(rsp_params->id);
	attr->options = le64_to_cpu(rsp_params->options);
	attr->method = rsp_params->method;
	attr->manip = rsp_params->manip;
	attr->num_ifs = le16_to_cpu(rsp_params->num_ifs);
	attr->mem_size = le16_to_cpu(rsp_params->mem_size);
	attr->default_if = le16_to_cpu(rsp_params->default_if);

	return 0;
}

/**
 * dpdmux_if_enable() - Enable Interface
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 * @if_id:	Interface Identifier
 *
 * Return:	Completion status. '0' on Success; Error code otherwise.
 */
int dpdmux_if_enable(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     uint16_t if_id)
{
	struct dpdmux_cmd_if *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_ENABLE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_if *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_if_disable() - Disable Interface
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 * @if_id:	Interface Identifier
 *
 * Return:	Completion status. '0' on Success; Error code otherwise.
 */
int dpdmux_if_disable(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint16_t if_id)
{
	struct dpdmux_cmd_if *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_DISABLE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_if *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_set_max_frame_length() - Set the maximum frame length in DPDMUX
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPDMUX object
 * @max_frame_length:	The required maximum frame length
 *
 * Update the maximum frame length on all DMUX interfaces.
 * In case of VEPA, the maximum frame length on all dmux interfaces
 * will be updated with the minimum value of the mfls of the connected
 * dpnis and the actual value of dmux mfl.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_set_max_frame_length(struct fsl_mc_io *mc_io,
				uint32_t cmd_flags,
				uint16_t token,
				uint16_t max_frame_length)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_set_max_frame_length *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_SET_MAX_FRAME_LENGTH,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_set_max_frame_length *)cmd.params;
	cmd_params->max_frame_length = cpu_to_le16(max_frame_length);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_get_max_frame_length() - Return the maximum frame length for DPDMUX
 * interface
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPDMUX object
 * @if_id:		Interface id
 * @max_frame_length:	maximum frame length
 *
 * When dpdmux object is in VEPA mode this function will ignore if_id parameter
 * and will return maximum frame length for uplink interface (if_id==0).
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_get_max_frame_length(struct fsl_mc_io *mc_io,
				uint32_t cmd_flags,
				uint16_t token,
				uint16_t if_id,
				uint16_t *max_frame_length)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_get_max_frame_len *cmd_params;
	struct dpdmux_rsp_get_max_frame_len *rsp_params;
	int err = 0;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_GET_MAX_FRAME_LENGTH,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_get_max_frame_len *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpdmux_rsp_get_max_frame_len *)cmd.params;
	*max_frame_length = le16_to_cpu(rsp_params->max_len);

	/* send command to mc*/
	return err;
}

/**
 * dpdmux_ul_reset_counters() - Function resets the uplink counter
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_ul_reset_counters(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_UL_RESET_COUNTERS,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_if_set_accepted_frames() - Set the accepted frame types
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 * @if_id:	Interface ID (0 for uplink, or 1-num_ifs);
 * @cfg:	Frame types configuration
 *
 * if 'DPDMUX_ADMIT_ONLY_VLAN_TAGGED' is set - untagged frames or
 * priority-tagged frames are discarded.
 * if 'DPDMUX_ADMIT_ONLY_UNTAGGED' is set - untagged frames or
 * priority-tagged frames are accepted.
 * if 'DPDMUX_ADMIT_ALL' is set (default mode) - all VLAN tagged,
 * untagged and priority-tagged frame are accepted;
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_if_set_accepted_frames(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  uint16_t if_id,
				  const struct dpdmux_accepted_frames *cfg)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_if_set_accepted_frames *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_SET_ACCEPTED_FRAMES,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_if_set_accepted_frames *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	dpdmux_set_field(cmd_params->frames_options,
			 ACCEPTED_FRAMES_TYPE,
			 cfg->type);
	dpdmux_set_field(cmd_params->frames_options,
			 UNACCEPTED_FRAMES_ACTION,
			 cfg->unaccept_act);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_if_get_attributes() - Obtain DPDMUX interface attributes
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 * @if_id:	Interface ID (0 for uplink, or 1-num_ifs);
 * @attr:	Interface attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_if_get_attributes(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     uint16_t if_id,
			     struct dpdmux_if_attr *attr)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_if *cmd_params;
	struct dpdmux_rsp_if_get_attr *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_GET_ATTR,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_if *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpdmux_rsp_if_get_attr *)cmd.params;
	attr->rate = le32_to_cpu(rsp_params->rate);
	attr->enabled = dpdmux_get_field(rsp_params->enabled, ENABLE);
	attr->is_default = dpdmux_get_field(rsp_params->enabled, IS_DEFAULT);
	attr->accept_frame_type = dpdmux_get_field(
				  rsp_params->accepted_frames_type,
				  ACCEPTED_FRAMES_TYPE);

	return 0;
}

/**
 * dpdmux_if_remove_l2_rule() - Remove L2 rule from DPDMUX table
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 * @if_id:	Destination interface ID
 * @rule:	L2 rule
 *
 * Function removes a L2 rule from DPDMUX table
 * or adds an interface to an existing multicast address
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_if_remove_l2_rule(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     uint16_t if_id,
			     const struct dpdmux_l2_rule *rule)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_if_l2_rule *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_REMOVE_L2_RULE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_if_l2_rule *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	cmd_params->vlan_id = cpu_to_le16(rule->vlan_id);
	cmd_params->mac_addr5 = rule->mac_addr[5];
	cmd_params->mac_addr4 = rule->mac_addr[4];
	cmd_params->mac_addr3 = rule->mac_addr[3];
	cmd_params->mac_addr2 = rule->mac_addr[2];
	cmd_params->mac_addr1 = rule->mac_addr[1];
	cmd_params->mac_addr0 = rule->mac_addr[0];

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_if_add_l2_rule() - Add L2 rule into DPDMUX table
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMUX object
 * @if_id:	Destination interface ID
 * @rule:	L2 rule
 *
 * Function adds a L2 rule into DPDMUX table
 * or adds an interface to an existing multicast address
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_if_add_l2_rule(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  uint16_t if_id,
			  const struct dpdmux_l2_rule *rule)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_if_l2_rule *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_ADD_L2_RULE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_if_l2_rule *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	cmd_params->vlan_id = cpu_to_le16(rule->vlan_id);
	cmd_params->mac_addr5 = rule->mac_addr[5];
	cmd_params->mac_addr4 = rule->mac_addr[4];
	cmd_params->mac_addr3 = rule->mac_addr[3];
	cmd_params->mac_addr2 = rule->mac_addr[2];
	cmd_params->mac_addr1 = rule->mac_addr[1];
	cmd_params->mac_addr0 = rule->mac_addr[0];

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_if_get_counter() - Functions obtains specific counter of an interface
 * @mc_io: Pointer to MC portal's I/O object
 * @cmd_flags: Command flags; one or more of 'MC_CMD_FLAG_'
 * @token: Token of DPDMUX object
 * @if_id:  Interface Id
 * @counter_type: counter type
 * @counter: Returned specific counter information
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_if_get_counter(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  uint16_t if_id,
			  enum dpdmux_counter_type counter_type,
			  uint64_t *counter)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_if_get_counter *cmd_params;
	struct dpdmux_rsp_if_get_counter *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_GET_COUNTER,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_if_get_counter *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	cmd_params->counter_type = counter_type;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpdmux_rsp_if_get_counter *)cmd.params;
	*counter = le64_to_cpu(rsp_params->counter);

	return 0;
}

/**
 * dpdmux_if_set_link_cfg() - set the link configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token: Token of DPSW object
 * @if_id: interface id
 * @cfg: Link configuration
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_if_set_link_cfg(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   uint16_t if_id,
			   struct dpdmux_link_cfg *cfg)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_if_set_link_cfg *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_SET_LINK_CFG,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_if_set_link_cfg *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	cmd_params->rate = cpu_to_le32(cfg->rate);
	cmd_params->options = cpu_to_le64(cfg->options);
	cmd_params->advertising = cpu_to_le64(cfg->advertising);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_if_get_link_state - Return the link state
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token: Token of DPSW object
 * @if_id: interface id
 * @state: link state
 *
 * @returns	'0' on Success; Error code otherwise.
 */
int dpdmux_if_get_link_state(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     uint16_t if_id,
			     struct dpdmux_link_state *state)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_if_get_link_state *cmd_params;
	struct dpdmux_rsp_if_get_link_state *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_GET_LINK_STATE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_if_get_link_state *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpdmux_rsp_if_get_link_state *)cmd.params;
	state->rate = le32_to_cpu(rsp_params->rate);
	state->options = le64_to_cpu(rsp_params->options);
	state->up = dpdmux_get_field(rsp_params->up, UP);
	state->state_valid = dpdmux_get_field(rsp_params->up, STATE_VALID);
	state->supported = le64_to_cpu(rsp_params->supported);
	state->advertising = le64_to_cpu(rsp_params->advertising);

	return 0;
}

/**
 * dpdmux_if_set_default - Set default interface
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token: Token of DPSW object
 * @if_id: interface id
 *
 * @returns	'0' on Success; Error code otherwise.
 */
int dpdmux_if_set_default(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token,
		uint16_t if_id)
{
	struct dpdmux_cmd_if *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_SET_DEFAULT,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_if *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_if_get_default - Get default interface
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token: Token of DPSW object
 * @if_id: interface id
 *
 * @returns	'0' on Success; Error code otherwise.
 */
int dpdmux_if_get_default(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token,
		uint16_t *if_id)
{
	struct dpdmux_cmd_if *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_IF_GET_DEFAULT,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpdmux_cmd_if *)cmd.params;
	*if_id = le16_to_cpu(rsp_params->if_id);

	return 0;
}

/**
 * dpdmux_set_custom_key - Set a custom classification key.
 *
 * This API is only available for DPDMUX instance created with
 * DPDMUX_METHOD_CUSTOM.  This API must be called before populating the
 * classification table using dpdmux_add_custom_cls_entry.
 *
 * Calls to dpdmux_set_custom_key remove all existing classification entries
 * that may have been added previously using dpdmux_add_custom_cls_entry.
 *
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPSW object
 * @if_id:		Interface id
 * @key_cfg_iova:	DMA address of a configuration structure set up using
 *			dpkg_prepare_key_cfg. Maximum key size is 24 bytes
 *
 * @returns	'0' on Success; Error code otherwise.
 */
int dpdmux_set_custom_key(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint64_t key_cfg_iova)
{
	struct dpdmux_set_custom_key *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_SET_CUSTOM_KEY,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_set_custom_key *)cmd.params;
	cmd_params->key_cfg_iova = cpu_to_le64(key_cfg_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_add_custom_cls_entry - Adds a custom classification entry.
 *
 * This API is only available for DPDMUX instances created with
 * DPDMUX_METHOD_CUSTOM.  Before calling this function a classification key
 * composition rule must be set up using dpdmux_set_custom_key.
 *
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token: Token of DPSW object
 * @rule: Classification rule to insert.  Rules cannot be duplicated, if a
 *	matching rule already exists, the action will be replaced.
 * @action: Action to perform for matching traffic.
 *
 * @returns	'0' on Success; Error code otherwise.
 */
int dpdmux_add_custom_cls_entry(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token,
		struct dpdmux_rule_cfg *rule,
		struct dpdmux_cls_action *action)
{
	struct dpdmux_cmd_add_custom_cls_entry *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_ADD_CUSTOM_CLS_ENTRY,
					  cmd_flags,
					  token);

	cmd_params = (struct dpdmux_cmd_add_custom_cls_entry *)cmd.params;
	cmd_params->key_size = rule->key_size;
	cmd_params->entry_index = rule->entry_index;
	cmd_params->dest_if = cpu_to_le16(action->dest_if);
	cmd_params->key_iova = cpu_to_le64(rule->key_iova);
	cmd_params->mask_iova = cpu_to_le64(rule->mask_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_remove_custom_cls_entry - Removes a custom classification entry.
 *
 * This API is only available for DPDMUX instances created with
 * DPDMUX_METHOD_CUSTOM.  The API can be used to remove classification
 * entries previously inserted using dpdmux_add_custom_cls_entry.
 *
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token: Token of DPSW object
 * @rule: Classification rule to remove
 *
 * @returns	'0' on Success; Error code otherwise.
 */
int dpdmux_remove_custom_cls_entry(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token,
		struct dpdmux_rule_cfg *rule)
{
	struct dpdmux_cmd_remove_custom_cls_entry *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_REMOVE_CUSTOM_CLS_ENTRY,
					  cmd_flags,
					  token);
	cmd_params = (struct dpdmux_cmd_remove_custom_cls_entry *)cmd.params;
	cmd_params->key_size = rule->key_size;
	cmd_params->key_iova = cpu_to_le64(rule->key_iova);
	cmd_params->mask_iova = cpu_to_le64(rule->mask_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpdmux_get_api_version() - Get Data Path Demux API version
 * @mc_io:  Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @major_ver:	Major version of data path demux API
 * @minor_ver:	Minor version of data path demux API
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpdmux_get_api_version(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t *major_ver,
			   uint16_t *minor_ver)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_rsp_get_api_version *rsp_params;
	int err;

	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_GET_API_VERSION,
					cmd_flags,
					0);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpdmux_rsp_get_api_version *)cmd.params;
	*major_ver = le16_to_cpu(rsp_params->major);
	*minor_ver = le16_to_cpu(rsp_params->minor);

	return 0;
}

/**
 * dpdmux_if_set_errors_behavior() - Set errors behavior
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPSW object
 * @if_id:  Interface Identifier
 * @cfg:	Errors configuration
 *
 * Provides a set of frame errors that will be rejected or accepted by the
 * dpdmux interface. The frame with this errors will no longer be dropped by
 * the dpdmux interface. When frame has parsing error the distribution to
 * expected interface may fail. If the frame must be distributed using the
 * information from a header that was not parsed due errors the frame may
 * be discarded or end up on a default interface because needed data was not
 * parsed properly.
 * This function may be called numerous times with different error masks
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmux_if_set_errors_behavior(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, uint16_t if_id, struct dpdmux_error_cfg *cfg)
{
	struct mc_command cmd = { 0 };
	struct dpdmux_cmd_set_errors_behavior *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMUX_CMDID_SET_ERRORS_BEHAVIOR,
					cmd_flags,
					token);
	cmd_params = (struct dpdmux_cmd_set_errors_behavior *)cmd.params;
	cmd_params->errors = cpu_to_le32(cfg->errors);
	dpdmux_set_field(cmd_params->flags, ERROR_ACTION, cfg->error_action);
	cmd_params->if_id = cpu_to_le16(if_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
