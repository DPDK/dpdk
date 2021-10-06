/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright 2019-2021 NXP
 */
#include <fsl_mc_sys.h>
#include <fsl_mc_cmd.h>
#include <fsl_dprtc.h>
#include <fsl_dprtc_cmd.h>

/** @addtogroup dprtc
 * @{
 */

/**
 * dprtc_open() - Open a control session for the specified object.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @dprtc_id:	DPRTC unique ID
 * @token:	Returned token; use in subsequent API calls
 *
 * This function can be used to open a control session for an
 * already created object; an object may have been declared in
 * the DPL or by calling the dprtc_create function.
 * This function returns a unique authentication token,
 * associated with the specific object ID and the specific MC
 * portal; this token must be used in all subsequent commands for
 * this specific object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_open(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       int dprtc_id,
	       uint16_t *token)
{
	struct dprtc_cmd_open *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_OPEN,
					  cmd_flags,
					  0);
	cmd_params = (struct dprtc_cmd_open *)cmd.params;
	cmd_params->dprtc_id = cpu_to_le32(dprtc_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = mc_cmd_hdr_read_token(&cmd);

	return err;
}

/**
 * dprtc_close() - Close the control session of the object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 *
 * After this function is called, no further operations are
 * allowed on the object without opening a new control session.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_close(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_CLOSE, cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dprtc_create() - Create the DPRTC object.
 * @mc_io:	Pointer to MC portal's I/O object
 * @dprc_token:	Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @cfg:	Configuration structure
 * @obj_id:	Returned object id
 *
 * Create the DPRTC object, allocate required resources and
 * perform required initialization.
 *
 * The function accepts an authentication token of a parent
 * container that this object should be assigned to. The token
 * can be '0' so the object will be assigned to the default container.
 * The newly created object can be opened with the returned
 * object id and using the container's associated tokens and MC portals.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_create(struct fsl_mc_io *mc_io,
		 uint16_t dprc_token,
		 uint32_t cmd_flags,
		 const struct dprtc_cfg *cfg,
		 uint32_t *obj_id)
{
	struct mc_command cmd = { 0 };
	int err;

	(void)(cfg); /* unused */

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_CREATE,
					  cmd_flags,
					  dprc_token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*obj_id = mc_cmd_read_object_id(&cmd);

	return 0;
}

/**
 * dprtc_destroy() - Destroy the DPRTC object and release all its resources.
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
int dprtc_destroy(struct fsl_mc_io *mc_io,
		  uint16_t dprc_token,
		  uint32_t cmd_flags,
		  uint32_t object_id)
{
	struct dprtc_cmd_destroy *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_DESTROY,
					  cmd_flags,
					  dprc_token);
	cmd_params = (struct dprtc_cmd_destroy *)cmd.params;
	cmd_params->object_id = cpu_to_le32(object_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dprtc_enable() - Enable the DPRTC.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_enable(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_ENABLE, cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dprtc_disable() - Disable the DPRTC.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_disable(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_DISABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dprtc_is_enabled() - Check if the DPRTC is enabled.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 * @en:		Returns '1' if object is enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_is_enabled(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     int *en)
{
	struct dprtc_rsp_is_enabled *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_IS_ENABLED, cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dprtc_rsp_is_enabled *)cmd.params;
	*en = dprtc_get_field(rsp_params->en, ENABLE);

	return 0;
}

/**
 * dprtc_reset() - Reset the DPRTC, returns the object to initial state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_reset(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_RESET,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dprtc_get_attributes - Retrieve DPRTC attributes.
 *
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 * @attr:	Returned object's attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_get_attributes(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 struct dprtc_attr *attr)
{
	struct dprtc_rsp_get_attributes *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dprtc_rsp_get_attributes *)cmd.params;
	attr->id = le32_to_cpu(rsp_params->id);
	attr->paddr = le32_to_cpu(rsp_params->paddr);
	attr->little_endian =
		dprtc_get_field(rsp_params->little_endian, ENDIANNESS);
	return 0;
}

/**
 * dprtc_set_clock_offset() - Sets the clock's offset
 * (usually relative to another clock).
 *
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 * @offset:	New clock offset (in nanoseconds).
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_set_clock_offset(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   int64_t offset)
{
	struct dprtc_cmd_set_clock_offset *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_SET_CLOCK_OFFSET,
					  cmd_flags,
					  token);
	cmd_params = (struct dprtc_cmd_set_clock_offset *)cmd.params;
	cmd_params->offset = cpu_to_le64(offset);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dprtc_set_freq_compensation() - Sets a new frequency compensation value.
 *
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPRTC object
 * @freq_compensation:	The new frequency compensation value to set.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_set_freq_compensation(struct fsl_mc_io *mc_io,
				uint32_t cmd_flags,
				uint16_t token,
				uint32_t freq_compensation)
{
	struct dprtc_get_freq_compensation *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_SET_FREQ_COMPENSATION,
					  cmd_flags,
					  token);
	cmd_params = (struct dprtc_get_freq_compensation *)cmd.params;
	cmd_params->freq_compensation = cpu_to_le32(freq_compensation);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dprtc_get_freq_compensation() - Retrieves the frequency compensation value
 *
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPRTC object
 * @freq_compensation:	Frequency compensation value
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_get_freq_compensation(struct fsl_mc_io *mc_io,
				uint32_t cmd_flags,
				uint16_t token,
				uint32_t *freq_compensation)
{
	struct dprtc_get_freq_compensation *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_GET_FREQ_COMPENSATION,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dprtc_get_freq_compensation *)cmd.params;
	*freq_compensation = le32_to_cpu(rsp_params->freq_compensation);

	return 0;
}

/**
 * dprtc_get_time() - Returns the current RTC time.
 *
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 * @time:	Current RTC time.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_get_time(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   uint64_t *time)
{
	struct dprtc_time *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_GET_TIME,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dprtc_time *)cmd.params;
	*time = le64_to_cpu(rsp_params->time);

	return 0;
}

/**
 * dprtc_set_time() - Updates current RTC time.
 *
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 * @time:	New RTC time.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_set_time(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   uint64_t time)
{
	struct dprtc_time *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_SET_TIME,
					  cmd_flags,
					  token);
	cmd_params = (struct dprtc_time *)cmd.params;
	cmd_params->time = cpu_to_le64(time);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dprtc_set_alarm() - Defines and sets alarm.
 *
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 * @time:	In nanoseconds, the time when the alarm
 *			should go off - must be a multiple of
 *			1 microsecond
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_set_alarm(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token, uint64_t time)
{
	struct dprtc_time *cmd_params;
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_SET_ALARM,
					  cmd_flags,
					  token);
	cmd_params = (struct dprtc_time *)cmd.params;
	cmd_params->time = cpu_to_le64(time);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dprtc_get_api_version() - Get Data Path Real Time Counter API version
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @major_ver:	Major version of data path real time counter API
 * @minor_ver:	Minor version of data path real time counter API
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dprtc_get_api_version(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t *major_ver,
			  uint16_t *minor_ver)
{
	struct dprtc_rsp_get_api_version *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_GET_API_VERSION,
					cmd_flags,
					0);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dprtc_rsp_get_api_version *)cmd.params;
	*major_ver = le16_to_cpu(rsp_params->major);
	*minor_ver = le16_to_cpu(rsp_params->minor);

	return 0;
}

/**
 * dprtc_get_ext_trigger_timestamp - Retrieve the Ext trigger timestamp status
 * (timestamp + flag for unread timestamp in FIFO).
 *
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 * @id:     External trigger id.
 * @status:	Returned object's external trigger status
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprtc_get_ext_trigger_timestamp(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token,
		uint8_t id,
		struct dprtc_ext_trigger_status *status)
{
	struct dprtc_rsp_ext_trigger_timestamp *rsp_params;
	struct dprtc_cmd_ext_trigger_timestamp *cmd_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_GET_EXT_TRIGGER_TIMESTAMP,
					  cmd_flags,
					  token);

	cmd_params = (struct dprtc_cmd_ext_trigger_timestamp *)cmd.params;
	cmd_params->id = id;
	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dprtc_rsp_ext_trigger_timestamp *)cmd.params;
	status->timestamp = le64_to_cpu(rsp_params->timestamp);
	status->unread_valid_timestamp = rsp_params->unread_valid_timestamp;

	return 0;
}

/**
 * dprtc_set_fiper_loopback() - Set the fiper pulse as source of interrupt for
 * External Trigger stamps
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRTC object
 * @id:     External trigger id.
 * @fiper_as_input:	Bit used to control interrupt signal source:
 *                  0 = Normal operation, interrupt external source
 *                  1 = Fiper pulse is looped back into Trigger input
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dprtc_set_fiper_loopback(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token,
		uint8_t id,
		uint8_t fiper_as_input)
{
	struct  dprtc_ext_trigger_cfg *cmd_params;
	struct mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPRTC_CMDID_SET_FIPER_LOOPBACK,
					cmd_flags,
					token);

	cmd_params = (struct dprtc_ext_trigger_cfg *)cmd.params;
	cmd_params->id = id;
	cmd_params->fiper_as_input = fiper_as_input;

	return mc_send_command(mc_io, &cmd);
}
