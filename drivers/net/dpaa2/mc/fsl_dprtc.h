/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright 2019-2021 NXP
 */
#ifndef __FSL_DPRTC_H
#define __FSL_DPRTC_H

/** @addtogroup dprtc Data Path Real Time Counter API
 * Contains initialization APIs and runtime control APIs for RTC
 * @{
 */

struct fsl_mc_io;

int dprtc_open(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       int dprtc_id,
	       uint16_t *token);

int dprtc_close(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token);

/**
 * struct dprtc_cfg - Structure representing DPRTC configuration
 * @options:	place holder
 */
struct dprtc_cfg {
	uint32_t options;
};

int dprtc_create(struct fsl_mc_io *mc_io,
		 uint16_t dprc_token,
		 uint32_t cmd_flags,
		 const struct dprtc_cfg *cfg,
		 uint32_t *obj_id);

int dprtc_destroy(struct fsl_mc_io *mc_io,
		  uint16_t dprc_token,
		  uint32_t cmd_flags,
		  uint32_t object_id);

int dprtc_enable(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token);

int dprtc_disable(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token);

int dprtc_is_enabled(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     int *en);

int dprtc_reset(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token);

int dprtc_set_clock_offset(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   int64_t offset);

int dprtc_set_freq_compensation(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token,
		  uint32_t freq_compensation);

int dprtc_get_freq_compensation(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token,
		  uint32_t *freq_compensation);

int dprtc_get_time(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   uint64_t *time);

int dprtc_set_time(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   uint64_t time);

int dprtc_set_alarm(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint64_t time);

struct dprtc_ext_trigger_status {
			uint64_t timestamp;
			uint8_t unread_valid_timestamp;
};

int dprtc_get_ext_trigger_timestamp(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t id,
			struct dprtc_ext_trigger_status *status);

int dprtc_set_fiper_loopback(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t id,
			uint8_t fiper_as_input);

/**
 * struct dprtc_attr - Structure representing DPRTC attributes
 * @id:		DPRTC object ID
 */
struct dprtc_attr {
	int id;
	int paddr;
	int little_endian;
};

int dprtc_get_attributes(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 struct dprtc_attr *attr);

int dprtc_get_api_version(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t *major_ver,
			  uint16_t *minor_ver);

#endif /* __FSL_DPRTC_H */
