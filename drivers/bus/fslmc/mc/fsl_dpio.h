/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2017 NXP
 *
 */
#ifndef __FSL_DPIO_H
#define __FSL_DPIO_H

/* Data Path I/O Portal API
 * Contains initialization APIs and runtime control APIs for DPIO
 */

struct fsl_mc_io;

int dpio_open(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      int dpio_id,
	      uint16_t *token);

int dpio_close(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token);

/**
 * enum dpio_channel_mode - DPIO notification channel mode
 * @DPIO_NO_CHANNEL:	No support for notification channel
 * @DPIO_LOCAL_CHANNEL:	Notifications on data availability can be received by a
 *	dedicated channel in the DPIO; user should point the queue's
 *	destination in the relevant interface to this DPIO
 */
enum dpio_channel_mode {
	DPIO_NO_CHANNEL = 0,
	DPIO_LOCAL_CHANNEL = 1,
};

/**
 * struct dpio_cfg - Structure representing DPIO configuration
 * @channel_mode:	Notification channel mode
 * @num_priorities:	Number of priorities for the notification channel (1-8);
 *			relevant only if 'channel_mode = DPIO_LOCAL_CHANNEL'
 */
struct dpio_cfg {
	enum dpio_channel_mode channel_mode;
	uint8_t num_priorities;
};


int dpio_create(struct fsl_mc_io *mc_io,
		uint16_t dprc_token,
		uint32_t cmd_flags,
		const struct dpio_cfg *cfg,
		uint32_t *obj_id);

int dpio_destroy(struct fsl_mc_io *mc_io,
		 uint16_t dprc_token,
		 uint32_t cmd_flags,
		 uint32_t object_id);

int dpio_enable(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token);

int dpio_disable(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token);

int dpio_is_enabled(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    int *en);

int dpio_reset(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token);

int dpio_set_stashing_destination(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  uint8_t sdest);

int dpio_get_stashing_destination(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  uint8_t *sdest);

int dpio_add_static_dequeue_channel(struct fsl_mc_io *mc_io,
				    uint32_t cmd_flags,
				    uint16_t token,
				    int dpcon_id,
				    uint8_t *channel_index);

int dpio_remove_static_dequeue_channel(struct fsl_mc_io *mc_io,
				       uint32_t cmd_flags,
				       uint16_t token,
				       int dpcon_id);

/**
 * struct dpio_attr - Structure representing DPIO attributes
 * @id:				DPIO object ID
 * @qbman_portal_ce_offset:	Offset of the software portal cache-enabled area
 * @qbman_portal_ci_offset:	Offset of the software portal
 *				cache-inhibited area
 * @qbman_portal_id:		Software portal ID
 * @channel_mode:		Notification channel mode
 * @num_priorities:		Number of priorities for the notification
 *				channel (1-8); relevant only if
 *				'channel_mode = DPIO_LOCAL_CHANNEL'
 * @qbman_version:		QBMAN version
 */
struct dpio_attr {
	int id;
	uint64_t qbman_portal_ce_offset;
	uint64_t qbman_portal_ci_offset;
	uint16_t qbman_portal_id;
	enum dpio_channel_mode channel_mode;
	uint8_t num_priorities;
	uint32_t qbman_version;
	uint32_t clk;
};

int dpio_get_attributes(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpio_attr *attr);

int dpio_get_api_version(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t *major_ver,
			 uint16_t *minor_ver);

#endif /* __FSL_DPIO_H */
