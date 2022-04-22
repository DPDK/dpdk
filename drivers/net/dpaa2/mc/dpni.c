/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2022 NXP
 *
 */
#include <fsl_mc_sys.h>
#include <fsl_mc_cmd.h>
#include <fsl_dpni.h>
#include <fsl_dpni_cmd.h>

/**
 * dpni_open() - Open a control session for the specified object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @dpni_id:	DPNI unique ID
 * @token:	Returned token; use in subsequent API calls
 *
 * This function can be used to open a control session for an
 * already created object; an object may have been declared in
 * the DPL or by calling the dpni_create() function.
 * This function returns a unique authentication token,
 * associated with the specific object ID and the specific MC
 * portal; this token must be used in all subsequent commands for
 * this specific object.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_open(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      int dpni_id,
	      uint16_t *token)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_open *cmd_params;

	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_OPEN,
					  cmd_flags,
					  0);
	cmd_params = (struct dpni_cmd_open *)cmd.params;
	cmd_params->dpni_id = cpu_to_le32(dpni_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}

/**
 * dpni_close() - Close the control session of the object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 *
 * After this function is called, no further operations are
 * allowed on the object without opening a new control session.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_close(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CLOSE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_create() - Create the DPNI object
 * @mc_io:	Pointer to MC portal's I/O object
 * @dprc_token:	Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @cfg:	Configuration structure
 * @obj_id:	Returned object id
 *
 * Create the DPNI object, allocate required resources and
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
int dpni_create(struct fsl_mc_io *mc_io,
		uint16_t dprc_token,
		uint32_t cmd_flags,
		const struct dpni_cfg *cfg,
		uint32_t *obj_id)
{
	struct dpni_cmd_create *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CREATE,
					  cmd_flags,
					  dprc_token);
	cmd_params = (struct dpni_cmd_create *)cmd.params;
	cmd_params->options = cpu_to_le32(cfg->options);
	cmd_params->num_queues = cfg->num_queues;
	cmd_params->num_tcs = cfg->num_tcs;
	cmd_params->mac_filter_entries = cfg->mac_filter_entries;
	cmd_params->num_rx_tcs = cfg->num_rx_tcs;
	cmd_params->vlan_filter_entries =  cfg->vlan_filter_entries;
	cmd_params->qos_entries = cfg->qos_entries;
	cmd_params->fs_entries = cpu_to_le16(cfg->fs_entries);
	cmd_params->num_cgs = cfg->num_cgs;
	cmd_params->num_opr = cfg->num_opr;
	cmd_params->dist_key_size = cfg->dist_key_size;
	cmd_params->num_channels = cfg->num_channels;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*obj_id = mc_cmd_read_object_id(&cmd);

	return 0;
}

/**
 * dpni_destroy() - Destroy the DPNI object and release all its resources.
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
int dpni_destroy(struct fsl_mc_io *mc_io,
		 uint16_t dprc_token,
		 uint32_t cmd_flags,
		 uint32_t object_id)
{
	struct dpni_cmd_destroy *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_DESTROY,
					  cmd_flags,
					  dprc_token);
	/* set object id to destroy */
	cmd_params = (struct dpni_cmd_destroy *)cmd.params;
	cmd_params->dpsw_id = cpu_to_le32(object_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_pools() - Set buffer pools configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	Buffer pools configuration
 *
 * mandatory for DPNI operation
 * warning:Allowed only when DPNI is disabled
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_pools(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   const struct dpni_pools_cfg *cfg)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_pools *cmd_params;
	int i;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_POOLS,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_pools *)cmd.params;
	cmd_params->num_dpbp = cfg->num_dpbp;
	cmd_params->pool_options = cfg->pool_options;
	for (i = 0; i < DPNI_MAX_DPBP; i++) {
		cmd_params->pool[i].dpbp_id =
			cpu_to_le16(cfg->pools[i].dpbp_id);
		cmd_params->pool[i].priority_mask =
			cfg->pools[i].priority_mask;
		cmd_params->buffer_size[i] =
			cpu_to_le16(cfg->pools[i].buffer_size);
		cmd_params->backup_pool_mask |=
			DPNI_BACKUP_POOL(cfg->pools[i].backup_pool, i);
	}

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_enable() - Enable the DPNI, allow sending and receiving frames.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_enable(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ENABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_disable() - Disable the DPNI, stop sending and receiving frames.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_disable(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_DISABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_is_enabled() - Check if the DPNI is enabled.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Returns '1' if object is enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_is_enabled(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    int *en)
{
	struct mc_command cmd = { 0 };
	struct dpni_rsp_is_enabled *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_IS_ENABLED,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_is_enabled *)cmd.params;
	*en = dpni_get_field(rsp_params->enabled, ENABLE);

	return 0;
}

/**
 * dpni_reset() - Reset the DPNI, returns the object to initial state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_reset(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_RESET,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_irq_enable() - Set overall interrupt state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @irq_index:	The interrupt index to configure
 * @en:		Interrupt state: - enable = 1, disable = 0
 *
 * Allows GPP software to control when interrupts are generated.
 * Each interrupt can have up to 32 causes.  The enable/disable control's the
 * overall interrupt state. if the interrupt is disabled no causes will cause
 * an interrupt.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_irq_enable(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t irq_index,
			uint8_t en)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_irq_enable *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_IRQ_ENABLE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_irq_enable *)cmd.params;
	dpni_set_field(cmd_params->enable, ENABLE, en);
	cmd_params->irq_index = irq_index;

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_irq_enable() - Get overall interrupt state
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @irq_index:	The interrupt index to configure
 * @en:		Returned interrupt state - enable = 1, disable = 0
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_irq_enable(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t irq_index,
			uint8_t *en)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_get_irq_enable *cmd_params;
	struct dpni_rsp_get_irq_enable *rsp_params;

	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_IRQ_ENABLE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_irq_enable *)cmd.params;
	cmd_params->irq_index = irq_index;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_irq_enable *)cmd.params;
	*en = dpni_get_field(rsp_params->enabled, ENABLE);

	return 0;
}

/**
 * dpni_set_irq_mask() - Set interrupt mask.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @irq_index:	The interrupt index to configure
 * @mask:	Event mask to trigger interrupt;
 *		each bit:
 *			0 = ignore event
 *			1 = consider event for asserting IRQ
 *
 * Every interrupt can have up to 32 causes and the interrupt model supports
 * masking/unmasking each cause independently
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_irq_mask(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t irq_index,
		      uint32_t mask)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_irq_mask *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_IRQ_MASK,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_irq_mask *)cmd.params;
	cmd_params->mask = cpu_to_le32(mask);
	cmd_params->irq_index = irq_index;

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_irq_mask() - Get interrupt mask.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @irq_index:	The interrupt index to configure
 * @mask:	Returned event mask to trigger interrupt
 *
 * Every interrupt can have up to 32 causes and the interrupt model supports
 * masking/unmasking each cause independently
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_irq_mask(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t irq_index,
		      uint32_t *mask)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_get_irq_mask *cmd_params;
	struct dpni_rsp_get_irq_mask *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_IRQ_MASK,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_irq_mask *)cmd.params;
	cmd_params->irq_index = irq_index;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_irq_mask *)cmd.params;
	*mask = le32_to_cpu(rsp_params->mask);

	return 0;
}

/**
 * dpni_get_irq_status() - Get the current status of any pending interrupts.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @irq_index:	The interrupt index to configure
 * @status:	Returned interrupts status - one bit per cause:
 *			0 = no interrupt pending
 *			1 = interrupt pending
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_irq_status(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t irq_index,
			uint32_t *status)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_get_irq_status *cmd_params;
	struct dpni_rsp_get_irq_status *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_IRQ_STATUS,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_irq_status *)cmd.params;
	cmd_params->status = cpu_to_le32(*status);
	cmd_params->irq_index = irq_index;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_irq_status *)cmd.params;
	*status = le32_to_cpu(rsp_params->status);

	return 0;
}

/**
 * dpni_clear_irq_status() - Clear a pending interrupt's status
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @irq_index:	The interrupt index to configure
 * @status:	bits to clear (W1C) - one bit per cause:
 *			0 = don't change
 *			1 = clear status bit
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_clear_irq_status(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  uint8_t irq_index,
			  uint32_t status)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_clear_irq_status *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CLEAR_IRQ_STATUS,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_clear_irq_status *)cmd.params;
	cmd_params->irq_index = irq_index;
	cmd_params->status = cpu_to_le32(status);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_attributes() - Retrieve DPNI attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @attr:	Object's attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_attributes(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpni_attr *attr)
{
	struct mc_command cmd = { 0 };
	struct dpni_rsp_get_attr *rsp_params;

	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_attr *)cmd.params;
	attr->options = le32_to_cpu(rsp_params->options);
	attr->num_queues = rsp_params->num_queues;
	attr->num_rx_tcs = rsp_params->num_rx_tcs;
	attr->num_tx_tcs = rsp_params->num_tx_tcs;
	attr->mac_filter_entries = rsp_params->mac_filter_entries;
	attr->vlan_filter_entries = rsp_params->vlan_filter_entries;
	attr->num_channels = rsp_params->num_channels;
	attr->qos_entries = rsp_params->qos_entries;
	attr->fs_entries = le16_to_cpu(rsp_params->fs_entries);
	attr->num_opr = le16_to_cpu(rsp_params->num_opr);
	attr->qos_key_size = rsp_params->qos_key_size;
	attr->fs_key_size = rsp_params->fs_key_size;
	attr->wriop_version = le16_to_cpu(rsp_params->wriop_version);
	attr->num_cgs = rsp_params->num_cgs;

	return 0;
}

/**
 * dpni_set_errors_behavior() - Set errors behavior
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	Errors configuration
 *
 * This function may be called numerous times with different
 * error masks
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_errors_behavior(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     struct dpni_error_cfg *cfg)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_errors_behavior *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_ERRORS_BEHAVIOR,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_errors_behavior *)cmd.params;
	cmd_params->errors = cpu_to_le32(cfg->errors);
	dpni_set_field(cmd_params->flags, ERROR_ACTION, cfg->error_action);
	dpni_set_field(cmd_params->flags, FRAME_ANN, cfg->set_frame_annotation);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_buffer_layout() - Retrieve buffer layout attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue to retrieve configuration for
 * @layout:	Returns buffer layout attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_buffer_layout(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   enum dpni_queue_type qtype,
			   struct dpni_buffer_layout *layout)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_get_buffer_layout *cmd_params;
	struct dpni_rsp_get_buffer_layout *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_BUFFER_LAYOUT,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_buffer_layout *)cmd.params;
	cmd_params->qtype = qtype;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_buffer_layout *)cmd.params;
	layout->pass_timestamp =
				(int)dpni_get_field(rsp_params->flags, PASS_TS);
	layout->pass_parser_result =
				(int)dpni_get_field(rsp_params->flags, PASS_PR);
	layout->pass_frame_status =
				(int)dpni_get_field(rsp_params->flags, PASS_FS);
	layout->pass_sw_opaque =
			(int)dpni_get_field(rsp_params->flags, PASS_SWO);
	layout->private_data_size = le16_to_cpu(rsp_params->private_data_size);
	layout->data_align = le16_to_cpu(rsp_params->data_align);
	layout->data_head_room = le16_to_cpu(rsp_params->head_room);
	layout->data_tail_room = le16_to_cpu(rsp_params->tail_room);

	return 0;
}

/**
 * dpni_set_buffer_layout() - Set buffer layout configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue this configuration applies to
 * @layout:	Buffer layout configuration
 *
 * Return:	'0' on Success; Error code otherwise.
 *
 * @warning	Allowed only when DPNI is disabled
 */
int dpni_set_buffer_layout(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   enum dpni_queue_type qtype,
			   const struct dpni_buffer_layout *layout)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_buffer_layout *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_BUFFER_LAYOUT,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_buffer_layout *)cmd.params;
	cmd_params->qtype = qtype;
	cmd_params->options = cpu_to_le16((uint16_t)layout->options);
	dpni_set_field(cmd_params->flags, PASS_TS, layout->pass_timestamp);
	dpni_set_field(cmd_params->flags, PASS_PR, layout->pass_parser_result);
	dpni_set_field(cmd_params->flags, PASS_FS, layout->pass_frame_status);
	dpni_set_field(cmd_params->flags, PASS_SWO, layout->pass_sw_opaque);
	cmd_params->private_data_size = cpu_to_le16(layout->private_data_size);
	cmd_params->data_align = cpu_to_le16(layout->data_align);
	cmd_params->head_room = cpu_to_le16(layout->data_head_room);
	cmd_params->tail_room = cpu_to_le16(layout->data_tail_room);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_offload() - Set DPNI offload configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @type:	Type of DPNI offload
 * @config:	Offload configuration.
 *		For checksum offloads, non-zero value enables the offload
 *
 * Return:     '0' on Success; Error code otherwise.
 *
 * @warning    Allowed only when DPNI is disabled
 */

int dpni_set_offload(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     enum dpni_offload type,
		     uint32_t config)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_offload *cmd_params;

	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_OFFLOAD,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_offload *)cmd.params;
	cmd_params->dpni_offload = type;
	cmd_params->config = cpu_to_le32(config);

	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_offload() - Get DPNI offload configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @type:	Type of DPNI offload
 * @config:	Offload configuration.
 *			For checksum offloads, a value of 1 indicates that the
 *			offload is enabled.
 *
 * Return:	'0' on Success; Error code otherwise.
 *
 * @warning	Allowed only when DPNI is disabled
 */
int dpni_get_offload(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     enum dpni_offload type,
		     uint32_t *config)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_get_offload *cmd_params;
	struct dpni_rsp_get_offload *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_OFFLOAD,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_offload *)cmd.params;
	cmd_params->dpni_offload = type;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_offload *)cmd.params;
	*config = le32_to_cpu(rsp_params->config);

	return 0;
}

/**
 * dpni_get_qdid() - Get the Queuing Destination ID (QDID) that should be used
 *			for enqueue operations
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue to receive QDID for
 * @qdid:	Returned virtual QDID value that should be used as an argument
 *			in all enqueue operations
 *
 * Return:	'0' on Success; Error code otherwise.
 *
 * If dpni object is created using multiple Tc channels this function will return
 * qdid value for the first channel
 */
int dpni_get_qdid(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token,
		  enum dpni_queue_type qtype,
		  uint16_t *qdid)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_get_qdid *cmd_params;
	struct dpni_rsp_get_qdid *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_QDID,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_qdid *)cmd.params;
	cmd_params->qtype = qtype;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_qdid *)cmd.params;
	*qdid = le16_to_cpu(rsp_params->qdid);

	return 0;
}

/**
 * dpni_get_tx_data_offset() - Get the Tx data offset (from start of buffer)
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @data_offset: Tx data offset (from start of buffer)
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_tx_data_offset(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token,
			    uint16_t *data_offset)
{
	struct mc_command cmd = { 0 };
	struct dpni_rsp_get_tx_data_offset *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_TX_DATA_OFFSET,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_tx_data_offset *)cmd.params;
	*data_offset = le16_to_cpu(rsp_params->data_offset);

	return 0;
}

/**
 * dpni_set_link_cfg() - set the link configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	Link configuration
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_link_cfg(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      const struct dpni_link_cfg *cfg)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_link_cfg *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_LINK_CFG,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_link_cfg *)cmd.params;
	cmd_params->rate = cpu_to_le32(cfg->rate);
	cmd_params->options = cpu_to_le64(cfg->options);
	cmd_params->advertising = cpu_to_le64(cfg->advertising);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_link_cfg() - return the link configuration configured by
 *			dpni_set_link_cfg().
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	Link configuration from dpni object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_link_cfg(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      struct dpni_link_cfg *cfg)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_link_cfg *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_LINK_CFG,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_cmd_set_link_cfg *)cmd.params;
	cfg->advertising	= le64_to_cpu(rsp_params->advertising);
	cfg->options		= le64_to_cpu(rsp_params->options);
	cfg->rate		= le32_to_cpu(rsp_params->rate);

	return err;
}

/**
 * dpni_get_link_state() - Return the link state (either up or down)
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @state:	Returned link state;
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_link_state(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpni_link_state *state)
{
	struct mc_command cmd = { 0 };
	struct dpni_rsp_get_link_state *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_LINK_STATE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_link_state *)cmd.params;
	state->up = dpni_get_field(rsp_params->flags, LINK_STATE);
	state->state_valid = dpni_get_field(rsp_params->flags, STATE_VALID);
	state->rate = le32_to_cpu(rsp_params->rate);
	state->options = le64_to_cpu(rsp_params->options);
	state->supported = le64_to_cpu(rsp_params->supported);
	state->advertising = le64_to_cpu(rsp_params->advertising);

	return 0;
}

/**
 * dpni_set_tx_shaping() - Set the transmit shaping
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPNI object
 * @tx_cr_shaper:	TX committed rate shaping configuration
 * @tx_er_shaper:	TX excess rate shaping configuration
 * @param:		Special parameters
 *			bit0: Committed and excess rates are coupled
 *			bit1: 1 modify LNI shaper, 0 modify channel shaper
 *			bit8-15: Tx channel to be shaped. Used only if bit1 is set to zero
 *			bits16-26: OAL (Overhead accounting length 11bit value). Used only
 *			when bit1 is set.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_tx_shaping(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			const struct dpni_tx_shaping_cfg *tx_cr_shaper,
			const struct dpni_tx_shaping_cfg *tx_er_shaper,
			uint32_t param)
{
	struct dpni_cmd_set_tx_shaping *cmd_params;
	struct mc_command cmd = { 0 };
	int coupled, lni_shaper;
	uint8_t channel_id;
	uint16_t oal;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_TX_SHAPING,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_tx_shaping *)cmd.params;
	cmd_params->tx_cr_max_burst_size =
		cpu_to_le16(tx_cr_shaper->max_burst_size);
	cmd_params->tx_er_max_burst_size =
		cpu_to_le16(tx_er_shaper->max_burst_size);
	cmd_params->tx_cr_rate_limit =
		cpu_to_le32(tx_cr_shaper->rate_limit);
	cmd_params->tx_er_rate_limit =
		cpu_to_le32(tx_er_shaper->rate_limit);

	coupled = !!(param & 0x01);
	dpni_set_field(cmd_params->options, COUPLED, coupled);

	lni_shaper = !!((param >> 1) & 0x01);
	dpni_set_field(cmd_params->options, LNI_SHAPER, lni_shaper);

	channel_id = (param >> 8) & 0xff;
	cmd_params->channel_id = channel_id;

	oal = (param >> 16) & 0x7FF;
	cmd_params->oal = cpu_to_le16(oal);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_max_frame_length() - Set the maximum received frame length.
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPNI object
 * @max_frame_length:	Maximum received frame length (in bytes);
 *			frame is discarded if its length exceeds this value
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_max_frame_length(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      uint16_t max_frame_length)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_max_frame_length *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_MAX_FRAME_LENGTH,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_max_frame_length *)cmd.params;
	cmd_params->max_frame_length = cpu_to_le16(max_frame_length);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_max_frame_length() - Get the maximum received frame length.
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPNI object
 * @max_frame_length:	Maximum received frame length (in bytes);
 *			frame is discarded if its length exceeds this value
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_max_frame_length(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      uint16_t *max_frame_length)
{
	struct mc_command cmd = { 0 };
	struct dpni_rsp_get_max_frame_length *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_MAX_FRAME_LENGTH,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_max_frame_length *)cmd.params;
	*max_frame_length = le16_to_cpu(rsp_params->max_frame_length);

	return 0;
}

/**
 * dpni_set_multicast_promisc() - Enable/disable multicast promiscuous mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Set to '1' to enable; '0' to disable
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_multicast_promisc(struct fsl_mc_io *mc_io,
			       uint32_t cmd_flags,
			       uint16_t token,
			       int en)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_multicast_promisc *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_MCAST_PROMISC,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_multicast_promisc *)cmd.params;
	dpni_set_field(cmd_params->enable, ENABLE, en);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_multicast_promisc() - Get multicast promiscuous mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Returns '1' if enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_multicast_promisc(struct fsl_mc_io *mc_io,
			       uint32_t cmd_flags,
			       uint16_t token,
			       int *en)
{
	struct mc_command cmd = { 0 };
	struct dpni_rsp_get_multicast_promisc *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_MCAST_PROMISC,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_multicast_promisc *)cmd.params;
	*en = dpni_get_field(rsp_params->enabled, ENABLE);

	return 0;
}

/**
 * dpni_set_unicast_promisc() - Enable/disable unicast promiscuous mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Set to '1' to enable; '0' to disable
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_unicast_promisc(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     int en)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_unicast_promisc *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_UNICAST_PROMISC,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_unicast_promisc *)cmd.params;
	dpni_set_field(cmd_params->enable, ENABLE, en);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_unicast_promisc() - Get unicast promiscuous mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Returns '1' if enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_unicast_promisc(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     int *en)
{
	struct mc_command cmd = { 0 };
	struct dpni_rsp_get_unicast_promisc *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_UNICAST_PROMISC,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_unicast_promisc *)cmd.params;
	*en = dpni_get_field(rsp_params->enabled, ENABLE);

	return 0;
}

/**
 * dpni_set_primary_mac_addr() - Set the primary MAC address
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mac_addr:	MAC address to set as primary address
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_primary_mac_addr(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      const uint8_t mac_addr[6])
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_primary_mac_addr *cmd_params;
	int i;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_PRIM_MAC,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_primary_mac_addr *)cmd.params;
	for (i = 0; i < 6; i++)
		cmd_params->mac_addr[i] = mac_addr[5 - i];

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_primary_mac_addr() - Get the primary MAC address
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mac_addr:	Returned MAC address
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_primary_mac_addr(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      uint8_t mac_addr[6])
{
	struct mc_command cmd = { 0 };
	struct dpni_rsp_get_primary_mac_addr *rsp_params;
	int i, err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_PRIM_MAC,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_primary_mac_addr *)cmd.params;
	for (i = 0; i < 6; i++)
		mac_addr[5 - i] = rsp_params->mac_addr[i];

	return 0;
}

/**
 * dpni_add_mac_addr() - Add MAC address filter
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mac_addr:	MAC address to add
 * @flags :	0 - tc_id and flow_id will be ignored.
 *		  Pkt with this mac_id will be passed to the next
 *		  classification stages
 *		DPNI_MAC_SET_QUEUE_ACTION
 *		  Pkt with this mac will be forward directly to
 *		  queue defined by the tc_id and flow_id
 * @tc_id : Traffic class selection (0-7)
 * @flow_id : Selects the specific queue out of the set allocated for the
 *            same as tc_id. Value must be in range 0 to NUM_QUEUES - 1
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_add_mac_addr(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      const uint8_t mac_addr[6],
			  uint8_t flags,
			  uint8_t tc_id,
			  uint8_t flow_id)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_add_mac_addr *cmd_params;
	int i;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ADD_MAC_ADDR,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_add_mac_addr *)cmd.params;
	cmd_params->flags = flags;
	cmd_params->tc_id = tc_id;
	cmd_params->fq_id = flow_id;

	for (i = 0; i < 6; i++)
		cmd_params->mac_addr[i] = mac_addr[5 - i];

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_remove_mac_addr() - Remove MAC address filter
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mac_addr:	MAC address to remove
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_remove_mac_addr(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 const uint8_t mac_addr[6])
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_remove_mac_addr *cmd_params;
	int i;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_REMOVE_MAC_ADDR,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_remove_mac_addr *)cmd.params;
	for (i = 0; i < 6; i++)
		cmd_params->mac_addr[i] = mac_addr[5 - i];

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_clear_mac_filters() - Clear all unicast and/or multicast MAC filters
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @unicast:	Set to '1' to clear unicast addresses
 * @multicast:	Set to '1' to clear multicast addresses
 *
 * The primary MAC address is not cleared by this operation.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_clear_mac_filters(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   int unicast,
			   int multicast)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_clear_mac_filters *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CLR_MAC_FILTERS,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_clear_mac_filters *)cmd.params;
	dpni_set_field(cmd_params->flags, UNICAST_FILTERS, unicast);
	dpni_set_field(cmd_params->flags, MULTICAST_FILTERS, multicast);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_port_mac_addr() - Retrieve MAC address associated to the physical
 *			port the DPNI is attached to
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mac_addr:	MAC address of the physical port, if any, otherwise 0
 *
 * The primary MAC address is not cleared by this operation.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_port_mac_addr(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   uint8_t mac_addr[6])
{
	struct mc_command cmd = { 0 };
	struct dpni_rsp_get_port_mac_addr *rsp_params;
	int i, err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_PORT_MAC_ADDR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_port_mac_addr *)cmd.params;
	for (i = 0; i < 6; i++)
		mac_addr[5 - i] = rsp_params->mac_addr[i];

	return 0;
}

/**
 * dpni_enable_vlan_filter() - Enable/disable VLAN filtering mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Set to '1' to enable; '0' to disable
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_enable_vlan_filter(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token,
			    int en)
{
	struct dpni_cmd_enable_vlan_filter *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ENABLE_VLAN_FILTER,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_enable_vlan_filter *)cmd.params;
	dpni_set_field(cmd_params->en, ENABLE, en);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_add_vlan_id() - Add VLAN ID filter
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @vlan_id:	VLAN ID to add
 * @flags:	0 - tc_id and flow_id will be ignored.
 *		  Pkt with this vlan_id will be passed to the next
 *		  classification stages
 *		DPNI_VLAN_SET_QUEUE_ACTION
 *		  Pkt with this vlan_id will be forward directly to
 *		  queue defined by the tc_id and flow_id
 *
 * @tc_id: Traffic class selection (0-7)
 * @flow_id: Selects the specific queue out of the set allocated for the
 *           same as tc_id. Value must be in range 0 to NUM_QUEUES - 1
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_add_vlan_id(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     uint16_t vlan_id,
			 uint8_t flags,
			 uint8_t tc_id,
			 uint8_t flow_id)
{
	struct dpni_cmd_vlan_id *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ADD_VLAN_ID,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_vlan_id *)cmd.params;
	cmd_params->flags = flags;
	cmd_params->tc_id = tc_id;
	cmd_params->flow_id =  flow_id;
	cmd_params->vlan_id = cpu_to_le16(vlan_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_remove_vlan_id() - Remove VLAN ID filter
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @vlan_id:	VLAN ID to remove
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_remove_vlan_id(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint16_t vlan_id)
{
	struct dpni_cmd_vlan_id *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_REMOVE_VLAN_ID,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_vlan_id *)cmd.params;
	cmd_params->vlan_id = cpu_to_le16(vlan_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_clear_vlan_filters() - Clear all VLAN filters
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_clear_vlan_filters(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CLR_VLAN_FILTERS,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_tx_priorities() - Set transmission TC priority configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	Transmission selection configuration
 *
 * warning:	Allowed only when DPNI is disabled
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_tx_priorities(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   const struct dpni_tx_priorities_cfg *cfg)
{
	struct dpni_cmd_set_tx_priorities *cmd_params;
	struct mc_command cmd = { 0 };
	int i;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_TX_PRIORITIES,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_tx_priorities *)cmd.params;
	cmd_params->channel_idx = cfg->channel_idx;
	dpni_set_field(cmd_params->flags,
				SEPARATE_GRP,
				cfg->separate_groups);
	cmd_params->prio_group_A = cfg->prio_group_A;
	cmd_params->prio_group_B = cfg->prio_group_B;

	for (i = 0; i + 1 < DPNI_MAX_TC; i = i + 2) {
		dpni_set_field(cmd_params->modes[i / 2],
			       MODE_1,
			       cfg->tc_sched[i].mode);
		dpni_set_field(cmd_params->modes[i / 2],
				   MODE_2,
				   cfg->tc_sched[i + 1].mode);
	}

	for (i = 0; i < DPNI_MAX_TC; i++) {
		cmd_params->delta_bandwidth[i] =
				cpu_to_le16(cfg->tc_sched[i].delta_bandwidth);
	}

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_rx_tc_dist() - Set Rx traffic class distribution configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tc_id:	Traffic class selection (0-7)
 * @cfg:	Traffic class distribution configuration
 *
 * warning: if 'dist_mode != DPNI_DIST_MODE_NONE', call dpkg_prepare_key_cfg()
 *			first to prepare the key_cfg_iova parameter
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_set_rx_tc_dist(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t tc_id,
			const struct dpni_rx_tc_dist_cfg *cfg)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_rx_tc_dist *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_RX_TC_DIST,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_rx_tc_dist *)cmd.params;
	cmd_params->dist_size = cpu_to_le16(cfg->dist_size);
	cmd_params->tc_id = tc_id;
	cmd_params->default_flow_id = cpu_to_le16(cfg->fs_cfg.default_flow_id);
	cmd_params->key_cfg_iova = cpu_to_le64(cfg->key_cfg_iova);
	dpni_set_field(cmd_params->flags,
		       DIST_MODE,
		       cfg->dist_mode);
	dpni_set_field(cmd_params->flags,
		       MISS_ACTION,
		       cfg->fs_cfg.miss_action);
	dpni_set_field(cmd_params->keep_hash_key,
		       KEEP_HASH_KEY,
		       cfg->fs_cfg.keep_hash_key);
	dpni_set_field(cmd_params->keep_hash_key,
		       KEEP_ENTRIES,
		       cfg->fs_cfg.keep_entries);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_tx_confirmation_mode() - Tx confirmation mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mode:	Tx confirmation mode
 *
 * This function is useful only when 'DPNI_OPT_TX_CONF_DISABLED' is not
 * selected at DPNI creation.
 * Calling this function with 'mode' set to DPNI_CONF_DISABLE disables all
 * transmit confirmation (including the private confirmation queues), regardless
 * of previous settings; Note that in this case, Tx error frames are still
 * enqueued to the general transmit errors queue.
 * Calling this function with 'mode' set to DPNI_CONF_SINGLE switches all
 * Tx confirmations to a shared Tx conf queue. 'index' field in dpni_get_queue
 * command will be ignored.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_tx_confirmation_mode(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  enum dpni_confirmation_mode mode)
{
	struct dpni_tx_confirmation_mode *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_TX_CONFIRMATION_MODE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_tx_confirmation_mode *)cmd.params;
	cmd_params->confirmation_mode = mode;

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_tx_confirmation_mode() - Get Tx confirmation mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mode:	Tx confirmation mode
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpni_get_tx_confirmation_mode(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  enum dpni_confirmation_mode *mode)
{
	struct dpni_tx_confirmation_mode *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_TX_CONFIRMATION_MODE,
					cmd_flags,
					token);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpni_tx_confirmation_mode *)cmd.params;
	*mode =  rsp_params->confirmation_mode;

	return 0;
}

/**
 * dpni_set_qos_table() - Set QoS mapping table
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	QoS table configuration
 *
 * This function and all QoS-related functions require that
 *'max_tcs > 1' was set at DPNI creation.
 *
 * warning: Before calling this function, call dpkg_prepare_key_cfg() to
 *			prepare the key_cfg_iova parameter
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_qos_table(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       uint16_t token,
		       const struct dpni_qos_tbl_cfg *cfg)
{
	struct dpni_cmd_set_qos_table *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_QOS_TBL,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_qos_table *)cmd.params;
	cmd_params->default_tc = cfg->default_tc;
	cmd_params->key_cfg_iova = cpu_to_le64(cfg->key_cfg_iova);
	dpni_set_field(cmd_params->discard_on_miss,
		       ENABLE,
		       cfg->discard_on_miss);
	dpni_set_field(cmd_params->discard_on_miss,
					KEEP_QOS_ENTRIES,
			       cfg->keep_entries);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_add_qos_entry() - Add QoS mapping entry (to select a traffic class)
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	QoS rule to add
 * @tc_id:	Traffic class selection (0-7)
 * @index:	Location in the QoS table where to insert the entry.
 *		Only relevant if MASKING is enabled for QoS classification on
 *		this DPNI, it is ignored for exact match.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_add_qos_entry(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       uint16_t token,
		       const struct dpni_rule_cfg *cfg,
		       uint8_t tc_id,
		       uint16_t index,
			   uint8_t flags,
			   uint8_t flow_id)
{
	struct dpni_cmd_add_qos_entry *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ADD_QOS_ENT,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_add_qos_entry *)cmd.params;
	cmd_params->flags = flags;
	cmd_params->flow_id = flow_id;
	cmd_params->tc_id = tc_id;
	cmd_params->key_size = cfg->key_size;
	cmd_params->index = cpu_to_le16(index);
	cmd_params->key_iova = cpu_to_le64(cfg->key_iova);
	cmd_params->mask_iova = cpu_to_le64(cfg->mask_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_remove_qos_entry() - Remove QoS mapping entry
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	QoS rule to remove
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_remove_qos_entry(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  const struct dpni_rule_cfg *cfg)
{
	struct dpni_cmd_remove_qos_entry *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_REMOVE_QOS_ENT,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_remove_qos_entry *)cmd.params;
	cmd_params->key_size = cfg->key_size;
	cmd_params->key_iova = cpu_to_le64(cfg->key_iova);
	cmd_params->mask_iova = cpu_to_le64(cfg->mask_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_clear_qos_table() - Clear all QoS mapping entries
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 *
 * Following this function call, all frames are directed to
 * the default traffic class (0)
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_clear_qos_table(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CLR_QOS_TBL,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_add_fs_entry() - Add Flow Steering entry for a specific traffic class
 *			(to select a flow ID)
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tc_id:	Traffic class selection (0-7)
 * @index:	Location in the QoS table where to insert the entry.
 *		Only relevant if MASKING is enabled for QoS classification
 *		on this DPNI, it is ignored for exact match.
 * @cfg:	Flow steering rule to add
 * @action:	Action to be taken as result of a classification hit
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_add_fs_entry(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t tc_id,
		      uint16_t index,
		      const struct dpni_rule_cfg *cfg,
		      const struct dpni_fs_action_cfg *action)
{
	struct dpni_cmd_add_fs_entry *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ADD_FS_ENT,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_add_fs_entry *)cmd.params;
	cmd_params->tc_id = tc_id;
	cmd_params->key_size = cfg->key_size;
	cmd_params->index = cpu_to_le16(index);
	cmd_params->key_iova = cpu_to_le64(cfg->key_iova);
	cmd_params->mask_iova = cpu_to_le64(cfg->mask_iova);
	cmd_params->options = cpu_to_le16(action->options);
	cmd_params->flow_id = cpu_to_le16(action->flow_id);
	cmd_params->flc = cpu_to_le64(action->flc);
	cmd_params->redir_token = cpu_to_le16(action->redirect_obj_token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_remove_fs_entry() - Remove Flow Steering entry from a specific
 *			traffic class
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tc_id:	Traffic class selection (0-7)
 * @cfg:	Flow steering rule to remove
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_remove_fs_entry(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 uint8_t tc_id,
			 const struct dpni_rule_cfg *cfg)
{
	struct dpni_cmd_remove_fs_entry *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_REMOVE_FS_ENT,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_remove_fs_entry *)cmd.params;
	cmd_params->tc_id = tc_id;
	cmd_params->key_size = cfg->key_size;
	cmd_params->key_iova = cpu_to_le64(cfg->key_iova);
	cmd_params->mask_iova = cpu_to_le64(cfg->mask_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_clear_fs_entries() - Clear all Flow Steering entries of a specific
 *			traffic class
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tc_id:	Traffic class selection (0-7)
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_clear_fs_entries(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  uint8_t tc_id)
{
	struct dpni_cmd_clear_fs_entries *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CLR_FS_ENT,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_clear_fs_entries *)cmd.params;
	cmd_params->tc_id = tc_id;

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_rx_tc_policing() - Set Rx traffic class policing configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tc_id:	Traffic class selection (0-7)
 * @cfg:	Traffic class policing configuration
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_set_rx_tc_policing(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token,
			    uint8_t tc_id,
			    const struct dpni_rx_tc_policing_cfg *cfg)
{
	struct dpni_cmd_set_rx_tc_policing *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_RX_TC_POLICING,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_rx_tc_policing *)cmd.params;
	dpni_set_field(cmd_params->mode_color, COLOR, cfg->default_color);
	dpni_set_field(cmd_params->mode_color, MODE, cfg->mode);
	dpni_set_field(cmd_params->units, UNITS, cfg->units);
	cmd_params->options = cpu_to_le32(cfg->options);
	cmd_params->cir = cpu_to_le32(cfg->cir);
	cmd_params->cbs = cpu_to_le32(cfg->cbs);
	cmd_params->eir = cpu_to_le32(cfg->eir);
	cmd_params->ebs = cpu_to_le32(cfg->ebs);
	cmd_params->tc_id = tc_id;

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_rx_tc_policing() - Get Rx traffic class policing configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tc_id:	Traffic class selection (0-7)
 * @cfg:	Traffic class policing configuration
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_get_rx_tc_policing(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token,
			    uint8_t tc_id,
			    struct dpni_rx_tc_policing_cfg *cfg)
{
	struct dpni_rsp_get_rx_tc_policing *rsp_params;
	struct dpni_cmd_get_rx_tc_policing *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_RX_TC_POLICING,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_rx_tc_policing *)cmd.params;
	cmd_params->tc_id = tc_id;


	/* send command to mc*/
	err =  mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params =  (struct dpni_rsp_get_rx_tc_policing *)cmd.params;
	cfg->options = le32_to_cpu(rsp_params->options);
	cfg->cir = le32_to_cpu(rsp_params->cir);
	cfg->cbs = le32_to_cpu(rsp_params->cbs);
	cfg->eir = le32_to_cpu(rsp_params->eir);
	cfg->ebs = le32_to_cpu(rsp_params->ebs);
	cfg->units = dpni_get_field(rsp_params->units, UNITS);
	cfg->mode = dpni_get_field(rsp_params->mode_color, MODE);
	cfg->default_color = dpni_get_field(rsp_params->mode_color, COLOR);

	return 0;
}

/**
 * dpni_prepare_early_drop() - prepare an early drop.
 * @cfg:		Early-drop configuration
 * @early_drop_buf:	Zeroed 256 bytes of memory before mapping it to DMA
 *
 * This function has to be called before dpni_set_rx_tc_early_drop or
 * dpni_set_tx_tc_early_drop
 *
 */
void dpni_prepare_early_drop(const struct dpni_early_drop_cfg *cfg,
			     uint8_t *early_drop_buf)
{
	struct dpni_early_drop *ext_params;

	ext_params = (struct dpni_early_drop *)early_drop_buf;

	dpni_set_field(ext_params->flags, DROP_ENABLE, cfg->enable);
	dpni_set_field(ext_params->flags, DROP_UNITS, cfg->units);
	ext_params->green_drop_probability = cfg->green.drop_probability;
	ext_params->green_max_threshold = cpu_to_le64(cfg->green.max_threshold);
	ext_params->green_min_threshold = cpu_to_le64(cfg->green.min_threshold);
	ext_params->yellow_drop_probability = cfg->yellow.drop_probability;
	ext_params->yellow_max_threshold =
		cpu_to_le64(cfg->yellow.max_threshold);
	ext_params->yellow_min_threshold =
		cpu_to_le64(cfg->yellow.min_threshold);
	ext_params->red_drop_probability = cfg->red.drop_probability;
	ext_params->red_max_threshold = cpu_to_le64(cfg->red.max_threshold);
	ext_params->red_min_threshold = cpu_to_le64(cfg->red.min_threshold);
}

/**
 * dpni_extract_early_drop() - extract the early drop configuration.
 * @cfg:		Early-drop configuration
 * @early_drop_buf:	Zeroed 256 bytes of memory before mapping it to DMA
 *
 * This function has to be called after dpni_get_rx_tc_early_drop or
 * dpni_get_tx_tc_early_drop
 *
 */
void dpni_extract_early_drop(struct dpni_early_drop_cfg *cfg,
			     const uint8_t *early_drop_buf)
{
	const struct dpni_early_drop *ext_params;

	ext_params = (const struct dpni_early_drop *)early_drop_buf;

	cfg->enable = dpni_get_field(ext_params->flags, DROP_ENABLE);
	cfg->units = dpni_get_field(ext_params->flags, DROP_UNITS);
	cfg->green.drop_probability = ext_params->green_drop_probability;
	cfg->green.max_threshold = le64_to_cpu(ext_params->green_max_threshold);
	cfg->green.min_threshold = le64_to_cpu(ext_params->green_min_threshold);
	cfg->yellow.drop_probability = ext_params->yellow_drop_probability;
	cfg->yellow.max_threshold =
		le64_to_cpu(ext_params->yellow_max_threshold);
	cfg->yellow.min_threshold =
		le64_to_cpu(ext_params->yellow_min_threshold);
	cfg->red.drop_probability = ext_params->red_drop_probability;
	cfg->red.max_threshold = le64_to_cpu(ext_params->red_max_threshold);
	cfg->red.min_threshold = le64_to_cpu(ext_params->red_min_threshold);
}

/**
 * dpni_set_early_drop() - Set traffic class early-drop configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue - only Rx and Tx types are supported
 * @param:	Traffic class and channel ID.
 *		MSB - channel id; used only for DPNI_QUEUE_TX and DPNI_QUEUE_TX_CONFIRM,
 *		ignored for the rest
 *		LSB - traffic class
 *		Use macro DPNI_BUILD_PARAM() to build correct value.
 *		If dpni uses a single channel (uses only channel zero) the parameter can receive
 *		traffic class directly.
 * @early_drop_iova:  I/O virtual address of 256 bytes DMA-able memory filled
 *	with the early-drop configuration by calling dpni_prepare_early_drop()
 *
 * warning: Before calling this function, call dpni_prepare_early_drop() to
 *			prepare the early_drop_iova parameter
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_set_early_drop(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			enum dpni_queue_type qtype,
			uint16_t param,
			uint64_t early_drop_iova)
{
	struct dpni_cmd_early_drop *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_EARLY_DROP,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_early_drop *)cmd.params;
	cmd_params->qtype = qtype;
	cmd_params->tc = (uint8_t)(param & 0xff);
	cmd_params->channel_id = (uint8_t)((param >> 8) & 0xff);
	cmd_params->early_drop_iova = cpu_to_le64(early_drop_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_early_drop() - Get Rx traffic class early-drop configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue - only Rx and Tx types are supported
 * @param:	Traffic class and channel ID.
 *		MSB - channel id; used only for DPNI_QUEUE_TX and DPNI_QUEUE_TX_CONFIRM,
 *		ignored for the rest
 *		LSB - traffic class
 *		Use macro DPNI_BUILD_PARAM() to build correct value.
 *		If dpni uses a single channel (uses only channel zero) the parameter can receive
 *		traffic class directly.
 * @early_drop_iova:  I/O virtual address of 256 bytes DMA-able memory
 *
 * warning: After calling this function, call dpni_extract_early_drop() to
 *	get the early drop configuration
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_get_early_drop(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			enum dpni_queue_type qtype,
			uint16_t param,
			uint64_t early_drop_iova)
{
	struct dpni_cmd_early_drop *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_EARLY_DROP,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_early_drop *)cmd.params;
	cmd_params->qtype = qtype;
	cmd_params->tc = (uint8_t)(param & 0xff);
	cmd_params->channel_id = (uint8_t)((param >> 8) & 0xff);
	cmd_params->early_drop_iova = cpu_to_le64(early_drop_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_congestion_notification() - Set traffic class congestion
 *	notification configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue - Rx, Tx and Tx confirm types are supported
 * @param:	Traffic class and channel. Bits[0-7] contain traaffic class,
 *		bite[8-15] contains channel id
 * @cfg:	congestion notification configuration
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_set_congestion_notification(struct fsl_mc_io *mc_io,
				     uint32_t cmd_flags,
				     uint16_t token,
				     enum dpni_queue_type qtype,
				     uint16_t param,
				     const struct dpni_congestion_notification_cfg *cfg)
{
	struct dpni_cmd_set_congestion_notification *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(
					DPNI_CMDID_SET_CONGESTION_NOTIFICATION,
					cmd_flags,
					token);
	cmd_params = (struct dpni_cmd_set_congestion_notification *)cmd.params;
	cmd_params->qtype = qtype;
	cmd_params->tc = (uint8_t)(param & 0xff);
	cmd_params->channel_id = (uint8_t)((param >> 8) & 0xff);
	cmd_params->congestion_point = cfg->cg_point;
	cmd_params->cgid = (uint8_t)cfg->cgid;
	cmd_params->dest_id = cpu_to_le32(cfg->dest_cfg.dest_id);
	cmd_params->notification_mode = cpu_to_le16(cfg->notification_mode);
	cmd_params->dest_priority = cfg->dest_cfg.priority;
	cmd_params->message_iova = cpu_to_le64(cfg->message_iova);
	cmd_params->message_ctx = cpu_to_le64(cfg->message_ctx);
	cmd_params->threshold_entry = cpu_to_le32(cfg->threshold_entry);
	cmd_params->threshold_exit = cpu_to_le32(cfg->threshold_exit);
	dpni_set_field(cmd_params->type_units,
		       DEST_TYPE,
		       cfg->dest_cfg.dest_type);
	dpni_set_field(cmd_params->type_units,
		       CONG_UNITS,
		       cfg->units);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_congestion_notification() - Get traffic class congestion
 *	notification configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue - Rx, Tx and Tx confirm types are supported
 * @param:	Traffic class and channel. Bits[0-7] contain traaffic class,
 *		byte[8-15] contains channel id
 * @cfg:	congestion notification configuration
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_get_congestion_notification(struct fsl_mc_io *mc_io,
				     uint32_t cmd_flags,
				     uint16_t token,
				     enum dpni_queue_type qtype,
				     uint16_t param,
				     struct dpni_congestion_notification_cfg *cfg)
{
	struct dpni_rsp_get_congestion_notification *rsp_params;
	struct dpni_cmd_get_congestion_notification *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(
					DPNI_CMDID_GET_CONGESTION_NOTIFICATION,
					cmd_flags,
					token);
	cmd_params = (struct dpni_cmd_get_congestion_notification *)cmd.params;
	cmd_params->qtype = qtype;
	cmd_params->tc = (uint8_t)(param & 0xff);
	cmd_params->channel_id = (uint8_t)((param >> 8) & 0xff);
	cmd_params->congestion_point = cfg->cg_point;
	cmd_params->cgid = cfg->cgid;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpni_rsp_get_congestion_notification *)cmd.params;
	cfg->units = dpni_get_field(rsp_params->type_units, CONG_UNITS);
	cfg->threshold_entry = le32_to_cpu(rsp_params->threshold_entry);
	cfg->threshold_exit = le32_to_cpu(rsp_params->threshold_exit);
	cfg->message_ctx = le64_to_cpu(rsp_params->message_ctx);
	cfg->message_iova = le64_to_cpu(rsp_params->message_iova);
	cfg->notification_mode = le16_to_cpu(rsp_params->notification_mode);
	cfg->dest_cfg.dest_id = le32_to_cpu(rsp_params->dest_id);
	cfg->dest_cfg.priority = rsp_params->dest_priority;
	cfg->dest_cfg.dest_type = dpni_get_field(rsp_params->type_units,
						 DEST_TYPE);

	return 0;
}

/**
 * dpni_get_api_version() - Get Data Path Network Interface API version
 * @mc_io:  Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @major_ver:	Major version of data path network interface API
 * @minor_ver:	Minor version of data path network interface API
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpni_get_api_version(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t *major_ver,
			 uint16_t *minor_ver)
{
	struct dpni_rsp_get_api_version *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_API_VERSION,
					cmd_flags,
					0);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpni_rsp_get_api_version *)cmd.params;
	*major_ver = le16_to_cpu(rsp_params->major);
	*minor_ver = le16_to_cpu(rsp_params->minor);

	return 0;
}

/**
 * dpni_set_queue() - Set queue parameters
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue - all queue types are supported, although
 *		the command is ignored for Tx
 * @tc:		Traffic class, in range 0 to NUM_TCS - 1
 * @index:	Selects the specific queue out of the set allocated for the
 *		same TC. Value must be in range 0 to NUM_QUEUES - 1
 * @options:	A combination of DPNI_QUEUE_OPT_ values that control what
 *		configuration options are set on the queue
 * @queue:	Queue structure
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_queue(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   enum dpni_queue_type qtype,
		   uint16_t param,
		   uint8_t index,
		   uint8_t options,
		   const struct dpni_queue *queue)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_queue *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_QUEUE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_queue *)cmd.params;
	cmd_params->qtype = qtype;
	cmd_params->tc = (uint8_t)(param & 0xff);
	cmd_params->channel_id = (uint8_t)((param >> 8) & 0xff);
	cmd_params->index = index;
	cmd_params->options = options;
	cmd_params->dest_id = cpu_to_le32(queue->destination.id);
	cmd_params->dest_prio = queue->destination.priority;
	dpni_set_field(cmd_params->flags, DEST_TYPE, queue->destination.type);
	dpni_set_field(cmd_params->flags, STASH_CTRL, queue->flc.stash_control);
	dpni_set_field(cmd_params->flags, HOLD_ACTIVE,
		       queue->destination.hold_active);
	cmd_params->flc = cpu_to_le64(queue->flc.value);
	cmd_params->user_context = cpu_to_le64(queue->user_context);
	cmd_params->cgid = queue->cgid;

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_queue() - Get queue parameters
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue - all queue types are supported
 * @param:	Traffic class and channel ID.
 *		MSB - channel id; used only for DPNI_QUEUE_TX and DPNI_QUEUE_TX_CONFIRM,
 *		ignored for the rest
 *		LSB - traffic class
 *		Use macro DPNI_BUILD_PARAM() to build correct value.
 *		If dpni uses a single channel (uses only channel zero) the parameter can receive
 *		traffic class directly.
 * @index:	Selects the specific queue out of the set allocated for the
 *		same TC. Value must be in range 0 to NUM_QUEUES - 1
 * @queue:	Queue configuration structure
 * @qid:	Queue identification
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_queue(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   enum dpni_queue_type qtype,
		   uint16_t param,
		   uint8_t index,
		   struct dpni_queue *queue,
		   struct dpni_queue_id *qid)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_get_queue *cmd_params;
	struct dpni_rsp_get_queue *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_QUEUE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_queue *)cmd.params;
	cmd_params->qtype = qtype;
	cmd_params->tc = (uint8_t)(param & 0xff);
	cmd_params->index = index;
	cmd_params->channel_id = (uint8_t)((param >> 8) & 0xff);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_queue *)cmd.params;
	queue->destination.id = le32_to_cpu(rsp_params->dest_id);
	queue->destination.priority = rsp_params->dest_prio;
	queue->destination.type = dpni_get_field(rsp_params->flags,
						     DEST_TYPE);
	queue->flc.stash_control = dpni_get_field(rsp_params->flags,
						  STASH_CTRL);
	queue->destination.hold_active = dpni_get_field(rsp_params->flags,
							HOLD_ACTIVE);
	queue->flc.value = le64_to_cpu(rsp_params->flc);
	queue->user_context = le64_to_cpu(rsp_params->user_context);
	qid->fqid = le32_to_cpu(rsp_params->fqid);
	qid->qdbin = le16_to_cpu(rsp_params->qdbin);
	if (dpni_get_field(rsp_params->flags, CGID_VALID))
		queue->cgid = rsp_params->cgid;
	else
		queue->cgid = -1;

	return 0;
}

/**
 * dpni_get_statistics() - Get DPNI statistics
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @page:	Selects the statistics page to retrieve, see
 *		DPNI_GET_STATISTICS output. Pages are numbered 0 to 6.
 * @param:  Custom parameter for some pages used to select
 *		 a certain statistic source, for example the TC.
 *		 - page_0: not used
 *		 - page_1: not used
 *		 - page_2: not used
 *		 - page_3: high_byte - channel_id, low_byte - traffic class
 *		 - page_4: high_byte - queue_index have meaning only if dpni is
 *		 created using option DPNI_OPT_CUSTOM_CG, low_byte - traffic class
 *		 - page_5: not used
 *		 - page_6: not used
 * @stat:	Structure containing the statistics
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_statistics(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t page,
			uint16_t param,
			union dpni_statistics *stat)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_get_statistics *cmd_params;
	struct dpni_rsp_get_statistics *rsp_params;
	int i, err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_STATISTICS,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_statistics *)cmd.params;
	cmd_params->page_number = page;
	cmd_params->param = param;

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_statistics *)cmd.params;
	for (i = 0; i < DPNI_STATISTICS_CNT; i++)
		stat->raw.counter[i] = le64_to_cpu(rsp_params->counter[i]);

	return 0;
}

/**
 * dpni_reset_statistics() - Clears DPNI statistics
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPNI object
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpni_reset_statistics(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
		     uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_RESET_STATISTICS,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_taildrop() - Set taildrop per congestion group
 *
 * Setting a per-TC taildrop (cg_point = DPNI_CP_GROUP) will reset any current
 * congestion notification or early drop (WRED) configuration previously applied
 * to the same TC.
 *
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cg_point:	Congestion group identifier DPNI_CP_QUEUE is only supported in
 *		combination with DPNI_QUEUE_RX.
 * @q_type:	Queue type, can be DPNI_QUEUE_RX or DPNI_QUEUE_TX.
 * @tc:		Traffic class to apply this taildrop to
 * @index/cgid:	Index of the queue if the DPNI supports multiple queues for
 *		traffic distribution.
 *		If CONGESTION_POINT is DPNI_CP_CONGESTION_GROUP then it
 *		represent the cgid of the congestion point
 * @taildrop:	Taildrop structure
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_taildrop(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      enum dpni_congestion_point cg_point,
		      enum dpni_queue_type qtype,
		      uint16_t param,
		      uint8_t index,
		      struct dpni_taildrop *taildrop)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_set_taildrop *cmd_params;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_TAILDROP,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_taildrop *)cmd.params;
	cmd_params->congestion_point = cg_point;
	cmd_params->qtype = qtype;
	cmd_params->tc = (uint8_t)(param & 0xff);
	cmd_params->channel_id = (uint8_t)((param >> 8) & 0xff);
	cmd_params->index = index;
	cmd_params->units = taildrop->units;
	cmd_params->threshold = cpu_to_le32(taildrop->threshold);
	dpni_set_field(cmd_params->enable_oal_lo, ENABLE, taildrop->enable);
	dpni_set_field(cmd_params->enable_oal_lo, OAL_LO, taildrop->oal);
	dpni_set_field(cmd_params->oal_hi,
		       OAL_HI,
		       taildrop->oal >> DPNI_OAL_LO_SIZE);

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_taildrop() - Get taildrop information
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cg_point:	Congestion point
 * @q_type:	Queue type on which the taildrop is configured.
 *		Only Rx queues are supported for now
 * @tc:		Traffic class to apply this taildrop to
 * @q_index:	Index of the queue if the DPNI supports multiple queues for
 *		traffic distribution. Ignored if CONGESTION_POINT is not 0.
 * @taildrop:	Taildrop structure
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_taildrop(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      enum dpni_congestion_point cg_point,
		      enum dpni_queue_type qtype,
		      uint8_t tc,
		      uint8_t index,
		      struct dpni_taildrop *taildrop)
{
	struct mc_command cmd = { 0 };
	struct dpni_cmd_get_taildrop *cmd_params;
	struct dpni_rsp_get_taildrop *rsp_params;
	uint8_t oal_lo, oal_hi;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_TAILDROP,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_taildrop *)cmd.params;
	cmd_params->congestion_point = cg_point;
	cmd_params->qtype = qtype;
	cmd_params->tc = tc;
	cmd_params->index = index;

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_taildrop *)cmd.params;
	taildrop->enable = dpni_get_field(rsp_params->enable_oal_lo, ENABLE);
	taildrop->units = rsp_params->units;
	taildrop->threshold = le32_to_cpu(rsp_params->threshold);
	oal_lo = dpni_get_field(rsp_params->enable_oal_lo, OAL_LO);
	oal_hi = dpni_get_field(rsp_params->oal_hi, OAL_HI);
	taildrop->oal = oal_hi << DPNI_OAL_LO_SIZE | oal_lo;

	/* Fill the first 4 bits, 'oal' is a 2's complement value of 12 bits */
	if (taildrop->oal >= 0x0800)
		taildrop->oal |= 0xF000;

	return 0;
}

/**
 * dpni_set_opr() - Set Order Restoration configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tc:		Traffic class, in range 0 to NUM_TCS - 1
 * @index:	Selects the specific queue out of the set allocated
 *			for the same TC. Value must be in range 0 to
 *			NUM_QUEUES - 1
 * @options:	Configuration mode options
 *			can be OPR_OPT_CREATE or OPR_OPT_RETIRE
 * @cfg:	Configuration options for the OPR
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_opr(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token,
		 uint8_t tc,
		 uint8_t index,
		 uint8_t options,
		 struct opr_cfg *cfg,
		 uint8_t opr_id)
{
	struct dpni_cmd_set_opr *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(
			DPNI_CMDID_SET_OPR,
			cmd_flags,
			token);
	cmd_params = (struct dpni_cmd_set_opr *)cmd.params;
	cmd_params->tc_id = tc;
	cmd_params->index = index;
	cmd_params->options = options;
	cmd_params->opr_id = opr_id;
	cmd_params->oloe = cfg->oloe;
	cmd_params->oeane = cfg->oeane;
	cmd_params->olws = cfg->olws;
	cmd_params->oa = cfg->oa;
	cmd_params->oprrws = cfg->oprrws;

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_opr() - Retrieve Order Restoration config and query.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tc:		Traffic class, in range 0 to NUM_TCS - 1
 * @index:	Selects the specific queue out of the set allocated
 *			for the same TC. Value must be in range 0 to
 *			NUM_QUEUES - 1
 * @cfg:	Returned OPR configuration
 * @qry:	Returned OPR query
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_opr(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token,
		 uint8_t tc,
		 uint8_t index,
		 struct opr_cfg *cfg,
		 struct opr_qry *qry,
		 uint8_t flags,
		 uint8_t opr_id)
{
	struct dpni_rsp_get_opr *rsp_params;
	struct dpni_cmd_get_opr *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_OPR,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_get_opr *)cmd.params;
	cmd_params->index = index;
	cmd_params->tc_id = tc;
	cmd_params->flags = flags;
	cmd_params->opr_id = opr_id;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpni_rsp_get_opr *)cmd.params;
	cfg->oloe = rsp_params->oloe;
	cfg->oeane = rsp_params->oeane;
	cfg->olws = rsp_params->olws;
	cfg->oa = rsp_params->oa;
	cfg->oprrws = rsp_params->oprrws;
	qry->rip = dpni_get_field(rsp_params->flags, RIP);
	qry->enable = dpni_get_field(rsp_params->flags, OPR_ENABLE);
	qry->nesn = le16_to_cpu(rsp_params->nesn);
	qry->ndsn = le16_to_cpu(rsp_params->ndsn);
	qry->ea_tseq = le16_to_cpu(rsp_params->ea_tseq);
	qry->tseq_nlis = dpni_get_field(rsp_params->tseq_nlis, TSEQ_NLIS);
	qry->ea_hseq = le16_to_cpu(rsp_params->ea_hseq);
	qry->hseq_nlis = dpni_get_field(rsp_params->hseq_nlis, HSEQ_NLIS);
	qry->ea_hptr = le16_to_cpu(rsp_params->ea_hptr);
	qry->ea_tptr = le16_to_cpu(rsp_params->ea_tptr);
	qry->opr_vid = le16_to_cpu(rsp_params->opr_vid);
	qry->opr_id = le16_to_cpu(rsp_params->opr_id);

	return 0;
}

int dpni_load_sw_sequence(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      uint16_t token,
		  struct dpni_load_ss_cfg *cfg)
{
	struct dpni_load_sw_sequence *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_LOAD_SW_SEQUENCE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_load_sw_sequence *)cmd.params;
	cmd_params->dest = cfg->dest;
	cmd_params->ss_offset = cpu_to_le16(cfg->ss_offset);
	cmd_params->ss_size = cpu_to_le16(cfg->ss_size);
	cmd_params->ss_iova = cpu_to_le64(cfg->ss_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_enable_sw_sequence(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      uint16_t token,
		  struct dpni_enable_ss_cfg *cfg)
{
	struct dpni_enable_sw_sequence *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ENABLE_SW_SEQUENCE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_enable_sw_sequence *)cmd.params;
	cmd_params->dest = cfg->dest;
	cmd_params->set_start = cfg->set_start;
	cmd_params->hxs = cpu_to_le16(cfg->hxs);
	cmd_params->ss_offset = cpu_to_le16(cfg->ss_offset);
	cmd_params->param_offset = cfg->param_offset;
	cmd_params->param_size = cfg->param_size;
	cmd_params->param_iova = cpu_to_le64(cfg->param_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_sw_sequence_layout() - Get the soft sequence layout
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @src:	Source of the layout (WRIOP Rx or Tx)
 * @ss_layout_iova:  I/O virtual address of 264 bytes DMA-able memory
 *
 * warning: After calling this function, call dpni_extract_sw_sequence_layout()
 *		to get the layout.
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_get_sw_sequence_layout(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      uint16_t token,
		  enum dpni_soft_sequence_dest src,
		  uint64_t ss_layout_iova)
{
	struct dpni_get_sw_sequence_layout *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_SW_SEQUENCE_LAYOUT,
					  cmd_flags,
					  token);

	cmd_params = (struct dpni_get_sw_sequence_layout *)cmd.params;
	cmd_params->src = src;
	cmd_params->layout_iova = cpu_to_le64(ss_layout_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_extract_sw_sequence_layout() - extract the software sequence layout
 * @layout:		software sequence layout
 * @sw_sequence_layout_buf:	Zeroed 264 bytes of memory before mapping it
 *				to DMA
 *
 * This function has to be called after dpni_get_sw_sequence_layout
 *
 */
void dpni_extract_sw_sequence_layout(struct dpni_sw_sequence_layout *layout,
			     const uint8_t *sw_sequence_layout_buf)
{
	const struct dpni_sw_sequence_layout_entry *ext_params;
	int i;
	uint16_t ss_size, ss_offset;

	ext_params = (const struct dpni_sw_sequence_layout_entry *)
						sw_sequence_layout_buf;

	for (i = 0; i < DPNI_SW_SEQUENCE_LAYOUT_SIZE; i++) {
		ss_offset = le16_to_cpu(ext_params[i].ss_offset);
		ss_size = le16_to_cpu(ext_params[i].ss_size);

		if (ss_offset == 0 && ss_size == 0) {
			layout->num_ss = i;
			return;
		}

		layout->ss[i].ss_offset = ss_offset;
		layout->ss[i].ss_size = ss_size;
		layout->ss[i].param_offset = ext_params[i].param_offset;
		layout->ss[i].param_size = ext_params[i].param_size;
	}
}
/**
 * dpni_set_rx_fs_dist() - Set Rx traffic class FS distribution
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg: Distribution configuration
 * If the FS is already enabled with a previous call the classification
 *		key will be changed but all the table rules are kept. If the
 *		existing rules do not match the key the results will not be
 *		predictable. It is the user responsibility to keep keyintegrity.
 * If cfg.enable is set to 1 the command will create a flow steering table
 *		and will classify packets according to this table. The packets
 *		that miss all the table rules will be classified according to
 *		settings made in dpni_set_rx_hash_dist()
 * If cfg.enable is set to 0 the command will clear flow steering table. The
 *		packets will be classified according to settings made in
 *		dpni_set_rx_hash_dist()
 */
int dpni_set_rx_fs_dist(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, const struct dpni_rx_dist_cfg *cfg)
{
	struct dpni_cmd_set_rx_fs_dist *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_RX_FS_DIST,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_rx_fs_dist *)cmd.params;
	cmd_params->dist_size	= cpu_to_le16(cfg->dist_size);
	dpni_set_field(cmd_params->enable, RX_FS_DIST_ENABLE, cfg->enable);
	cmd_params->tc			= cfg->tc;
	cmd_params->miss_flow_id = cpu_to_le16(cfg->fs_miss_flow_id);
	cmd_params->key_cfg_iova = cpu_to_le64(cfg->key_cfg_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_set_rx_hash_dist() - Set Rx traffic class HASH distribution
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg: Distribution configuration
 * If cfg.enable is set to 1 the packets will be classified using a hash
 *	function based on the key received in cfg.key_cfg_iova parameter.
 * If cfg.enable is set to 0 the packets will be sent to the queue configured in
 *	dpni_set_rx_dist_default_queue() call
 */
int dpni_set_rx_hash_dist(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, const struct dpni_rx_dist_cfg *cfg)
{
	struct dpni_cmd_set_rx_hash_dist *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_RX_HASH_DIST,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_set_rx_hash_dist *)cmd.params;
	cmd_params->dist_size	= cpu_to_le16(cfg->dist_size);
	dpni_set_field(cmd_params->enable, RX_FS_DIST_ENABLE, cfg->enable);
	cmd_params->tc_id		= cfg->tc;
	cmd_params->key_cfg_iova = cpu_to_le64(cfg->key_cfg_iova);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_add_custom_tpid() - Configures a distinct Ethertype value (or TPID
 *		value) to indicate VLAN tag in adition to the common TPID values
 *		0x81000 and 0x88A8
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tpid:	New value for TPID
 *
 * Only two custom values are accepted. If the function is called for the third
 * time it will return error.
 * To replace an existing value use dpni_remove_custom_tpid() to remove a
 * previous TPID and after that use again the function.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_add_custom_tpid(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, uint16_t tpid)
{
	struct dpni_cmd_add_custom_tpid *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ADD_CUSTOM_TPID,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_add_custom_tpid *)cmd.params;
	cmd_params->tpid = cpu_to_le16(tpid);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_remove_custom_tpid() - Removes a distinct Ethertype value added
 * previously with dpni_add_custom_tpid()
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tpid:	New value for TPID
 *
 * Use this function when a TPID value added with dpni_add_custom_tpid() needs
 * to be replaced.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_remove_custom_tpid(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, uint16_t tpid)
{
	struct dpni_cmd_remove_custom_tpid *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_REMOVE_CUSTOM_TPID,
					  cmd_flags,
					  token);
	cmd_params = (struct dpni_cmd_remove_custom_tpid *)cmd.params;
	cmd_params->tpid = cpu_to_le16(tpid);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dpni_get_custom_tpid() - Returns custom TPID (vlan tags) values configured to
 * detect 802.1q frames
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tpid:	TPID values. Only nonzero members of the structure are valid.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_custom_tpid(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, struct dpni_custom_tpid_cfg *tpid)
{
	struct dpni_rsp_get_custom_tpid *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_CUSTOM_TPID,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* read command response */
	rsp_params = (struct dpni_rsp_get_custom_tpid *)cmd.params;
	tpid->tpid1 = le16_to_cpu(rsp_params->tpid1);
	tpid->tpid2 = le16_to_cpu(rsp_params->tpid2);

	return err;
}

/**
 * dpni_set_port_cfg() - performs configurations at physical port connected on
 *		this dpni. The command have effect only if dpni is connected to
 *		another dpni object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @flags:	Valid fields from port_cfg structure
 * @port_cfg: Configuration data; one or more of DPNI_PORT_CFG_
 * The command can be called only when dpni is connected to a dpmac object. If
 * the dpni is unconnected or the endpoint is not a dpni it will return error.
 * If dpmac endpoint is disconnected the settings will be lost
 */
int dpni_set_port_cfg(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, uint32_t flags, struct dpni_port_cfg *port_cfg)
{
	struct dpni_cmd_set_port_cfg *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_PORT_CFG,
			cmd_flags, token);

	cmd_params = (struct dpni_cmd_set_port_cfg *)cmd.params;
	cmd_params->flags = cpu_to_le32(flags);
	dpni_set_field(cmd_params->bit_params,	PORT_LOOPBACK_EN,
			!!port_cfg->loopback_en);

	/* send command to MC */
	return mc_send_command(mc_io, &cmd);
}
