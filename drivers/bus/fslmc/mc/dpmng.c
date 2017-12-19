/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2015 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */
#include <fsl_mc_sys.h>
#include <fsl_mc_cmd.h>
#include <fsl_dpmng.h>
#include <fsl_dpmng_cmd.h>

/**
 * mc_get_version() - Retrieves the Management Complex firmware
 *			version information
 * @mc_io:		Pointer to opaque I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @mc_ver_info:	Returned version information structure
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int mc_get_version(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   struct mc_version *mc_ver_info)
{
	struct mc_command cmd = { 0 };
	struct dpmng_rsp_get_version *rsp_params;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPMNG_CMDID_GET_VERSION,
					  cmd_flags,
					  0);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpmng_rsp_get_version *)cmd.params;
	mc_ver_info->revision = le32_to_cpu(rsp_params->revision);
	mc_ver_info->major = le32_to_cpu(rsp_params->version_major);
	mc_ver_info->minor = le32_to_cpu(rsp_params->version_minor);

	return 0;
}

/**
 * mc_get_soc_version() - Retrieves the Management Complex firmware
 *                     version information
 * @mc_io		Pointer to opaque I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @mc_platform_info:	Returned version information structure. The structure
 *			contains the values of SVR and PVR registers.
 *			Please consult platform specific reference manual
 *			for detailed information.
 *
 * Return:     '0' on Success; Error code otherwise.
 */
int mc_get_soc_version(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       struct mc_soc_version *mc_platform_info)
{
	struct dpmng_rsp_get_soc_version *rsp_params;
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPMNG_CMDID_GET_SOC_VERSION,
					  cmd_flags,
					  0);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpmng_rsp_get_soc_version *)cmd.params;
	mc_platform_info->svr = le32_to_cpu(rsp_params->svr);
	mc_platform_info->pvr = le32_to_cpu(rsp_params->pvr);

	return 0;
}
