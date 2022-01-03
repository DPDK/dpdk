/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2021 NXP
 *
 */
#include <fsl_mc_sys.h>
#include <fsl_mc_cmd.h>
#include <fsl_dprc.h>
#include <fsl_dprc_cmd.h>

/** @addtogroup dprc
 * @{
 */

/**
 * dprc_open() - Open DPRC object for use
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @container_id:	Container ID to open
 * @token:		Returned token of DPRC object
 *
 * Return:	'0' on Success; Error code otherwise.
 *
 * @warning	Required before any operation on the object.
 */
int dprc_open(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      int container_id,
	      uint16_t *token)
{
	struct mc_command cmd = { 0 };
	struct dprc_cmd_open *cmd_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_OPEN, cmd_flags,
					  0);
	cmd_params = (struct dprc_cmd_open *)cmd.params;
	cmd_params->container_id = cpu_to_le32(container_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}

/**
 * dprc_close() - Close the control session of the object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRC object
 *
 * After this function is called, no further operations are
 * allowed on the object without opening a new control session.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dprc_close(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_CLOSE, cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

/**
 * dprc_get_connection() - Get connected endpoint and link status if connection
 *			exists.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPRC object
 * @endpoint1:	Endpoint 1 configuration parameters
 * @endpoint2:	Returned endpoint 2 configuration parameters
 * @state:	Returned link state:
 *		1 - link is up;
 *		0 - link is down;
 *		-1 - no connection (endpoint2 information is irrelevant)
 *
 * Return:     '0' on Success; -ENAVAIL if connection does not exist.
 */
int dprc_get_connection(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			const struct dprc_endpoint *endpoint1,
			struct dprc_endpoint *endpoint2,
			int *state)
{
	struct mc_command cmd = { 0 };
	struct dprc_cmd_get_connection *cmd_params;
	struct dprc_rsp_get_connection *rsp_params;
	int err, i;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_CONNECTION,
					  cmd_flags,
					  token);
	cmd_params = (struct dprc_cmd_get_connection *)cmd.params;
	cmd_params->ep1_id = cpu_to_le32(endpoint1->id);
	cmd_params->ep1_interface_id = cpu_to_le16(endpoint1->if_id);
	for (i = 0; i < 16; i++)
		cmd_params->ep1_type[i] = endpoint1->type[i];

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dprc_rsp_get_connection *)cmd.params;
	endpoint2->id = le32_to_cpu(rsp_params->ep2_id);
	endpoint2->if_id = le16_to_cpu(rsp_params->ep2_interface_id);
	*state = le32_to_cpu(rsp_params->state);
	for (i = 0; i < 16; i++)
		endpoint2->type[i] = rsp_params->ep2_type[i];

	return 0;
}
