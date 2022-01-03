/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2021 NXP
 *
 */
#ifndef _FSL_DPRC_H
#define _FSL_DPRC_H

/** @addtogroup dprc Data Path Resource Container API
 * Contains DPRC API for managing and querying DPAA resources
 * @{
 */

struct fsl_mc_io;

int dprc_open(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      int container_id,
	      uint16_t *token);

int dprc_close(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token);

/**
 * struct dprc_endpoint - Endpoint description for link connect/disconnect
 *			operations
 * @type:	Endpoint object type: NULL terminated string
 * @id:		Endpoint object ID
 * @if_id:	Interface ID; should be set for endpoints with multiple
 *		interfaces ("dpsw", "dpdmux"); for others, always set to 0
 */
struct dprc_endpoint {
	char type[16];
	int id;
	uint16_t if_id;
};

int dprc_get_connection(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			const struct dprc_endpoint *endpoint1,
			struct dprc_endpoint *endpoint2,
			int *state);
#endif /* _FSL_DPRC_H */
