/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright 2019-2024 NXP
 */
#ifndef __FSL_DPRTC_H
#define __FSL_DPRTC_H

/** @addtogroup dprtc Data Path Real Time Counter API
 * Contains initialization APIs and runtime control APIs for RTC
 * @{
 */

struct fsl_mc_io;

/**
 * Number of irq's
 */
#define DPRTC_MAX_IRQ_NUM			1
#define DPRTC_IRQ_INDEX				0

/**
 * Interrupt event masks:
 */

/**
 * Interrupt event mask indicating alarm event had occurred
 */
#define DPRTC_EVENT_ALARM			0x40000000
/**
 * Interrupt event mask indicating periodic pulse 1 event had occurred
 */
#define DPRTC_EVENT_PPS				0x08000000
/**
 * Interrupt event mask indicating periodic pulse 2 event had occurred
 */
#define DPRTC_EVENT_PPS2            0x04000000
/**
 * Interrupt event mask indicating External trigger 1 new timestamp sample event had occurred
 */
#define DPRTC_EVENT_ETS1			0x00800000
/**
 * Interrupt event mask indicating External trigger 2 new timestamp sample event had occurred
 */
#define DPRTC_EVENT_ETS2			0x00400000


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

int dprtc_get_clock_offset(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   int64_t *offset);

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
