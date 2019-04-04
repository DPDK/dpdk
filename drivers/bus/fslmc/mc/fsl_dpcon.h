/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 *
 */
#ifndef __FSL_DPCON_H
#define __FSL_DPCON_H

/* Data Path Concentrator API
 * Contains initialization APIs and runtime control APIs for DPCON
 */

struct fsl_mc_io;

/** General DPCON macros */

/**
 * Use it to disable notifications; see dpcon_set_notification()
 */
#define DPCON_INVALID_DPIO_ID		(int)(-1)

int dpcon_open(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       int dpcon_id,
	       uint16_t *token);

int dpcon_close(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token);

/**
 * struct dpcon_cfg - Structure representing DPCON configuration
 * @num_priorities: Number of priorities for the DPCON channel (1-8)
 */
struct dpcon_cfg {
	uint8_t num_priorities;
};

int dpcon_create(struct fsl_mc_io *mc_io,
		 uint16_t dprc_token,
		 uint32_t cmd_flags,
		 const struct dpcon_cfg *cfg,
		 uint32_t *obj_id);

int dpcon_destroy(struct fsl_mc_io *mc_io,
		  uint16_t dprc_token,
		  uint32_t cmd_flags,
		  uint32_t obj_id);

int dpcon_enable(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token);

int dpcon_disable(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token);

int dpcon_is_enabled(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     int *en);

int dpcon_reset(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token);

/**
 * struct dpcon_attr - Structure representing DPCON attributes
 * @id:			DPCON object ID
 * @qbman_ch_id:	Channel ID to be used by dequeue operation
 * @num_priorities:	Number of priorities for the DPCON channel (1-8)
 */
struct dpcon_attr {
	int id;
	uint16_t qbman_ch_id;
	uint8_t num_priorities;
};

int dpcon_get_attributes(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 struct dpcon_attr *attr);

int dpcon_get_api_version(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t *major_ver,
			  uint16_t *minor_ver);

#endif /* __FSL_DPCON_H */
