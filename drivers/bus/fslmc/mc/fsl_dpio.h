/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright (c) 2016 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __FSL_DPIO_H
#define __FSL_DPIO_H

/* Data Path I/O Portal API
 * Contains initialization APIs and runtime control APIs for DPIO
 */

struct fsl_mc_io;

/**
 * dpio_open() - Open a control session for the specified object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @dpio_id:	DPIO unique ID
 * @token:	Returned token; use in subsequent API calls
 *
 * This function can be used to open a control session for an
 * already created object; an object may have been declared in
 * the DPL or by calling the dpio_create() function.
 * This function returns a unique authentication token,
 * associated with the specific object ID and any MC portals
 * assigned to the parent container; this token must be used in
 * all subsequent commands for this specific object.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_open(struct fsl_mc_io	*mc_io,
	      uint32_t		cmd_flags,
	      int		dpio_id,
	      uint16_t		*token);

/**
 * dpio_close() - Close the control session of the object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_close(struct fsl_mc_io	*mc_io,
	       uint32_t		cmd_flags,
	       uint16_t		token);

/**
 * enum dpio_channel_mode - DPIO notification channel mode
 * @DPIO_NO_CHANNEL: No support for notification channel
 * @DPIO_LOCAL_CHANNEL: Notifications on data availability can be received by a
 *	dedicated channel in the DPIO; user should point the queue's
 *	destination in the relevant interface to this DPIO
 */
enum dpio_channel_mode {
	DPIO_NO_CHANNEL = 0,
	DPIO_LOCAL_CHANNEL = 1,
};

/**
 * struct dpio_cfg - Structure representing DPIO configuration
 * @channel_mode: Notification channel mode
 * @num_priorities: Number of priorities for the notification channel (1-8);
 *			relevant only if 'channel_mode = DPIO_LOCAL_CHANNEL'
 */
struct dpio_cfg {
	enum dpio_channel_mode	channel_mode;
	uint8_t			num_priorities;
};

/**
 * dpio_create() - Create the DPIO object.
 * @mc_io:	Pointer to MC portal's I/O object
 * @dprc_token:	Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @cfg:	Configuration structure
 * @obj_id: returned object id
 *
 * Create the DPIO object, allocate required resources and
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
int dpio_create(struct fsl_mc_io	*mc_io,
		uint16_t		dprc_token,
		uint32_t		cmd_flags,
		const struct dpio_cfg	*cfg,
		uint32_t		*obj_id);

/**
 * dpio_destroy() - Destroy the DPIO object and release all its resources.
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
 * Return:	'0' on Success; Error code otherwise
 */
int dpio_destroy(struct fsl_mc_io	*mc_io,
		 uint16_t		dprc_token,
		uint32_t		cmd_flags,
		uint32_t		object_id);

/**
 * dpio_enable() - Enable the DPIO, allow I/O portal operations.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 *
 * Return:	'0' on Success; Error code otherwise
 */
int dpio_enable(struct fsl_mc_io	*mc_io,
		uint32_t		cmd_flags,
		uint16_t		token);

/**
 * dpio_disable() - Disable the DPIO, stop any I/O portal operation.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 *
 * Return:	'0' on Success; Error code otherwise
 */
int dpio_disable(struct fsl_mc_io	*mc_io,
		 uint32_t		cmd_flags,
		 uint16_t		token);

/**
 * dpio_is_enabled() - Check if the DPIO is enabled.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 * @en:	Returns '1' if object is enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_is_enabled(struct fsl_mc_io	*mc_io,
		    uint32_t		cmd_flags,
		    uint16_t		token,
		    int			*en);

/**
 * dpio_reset() - Reset the DPIO, returns the object to initial state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_reset(struct fsl_mc_io	*mc_io,
	       uint32_t		cmd_flags,
	       uint16_t		token);

/**
 * dpio_set_stashing_destination() - Set the stashing destination.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 * @sdest:	stashing destination value
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_set_stashing_destination(struct fsl_mc_io	*mc_io,
				  uint32_t		cmd_flags,
				  uint16_t		token,
				  uint8_t		sdest);

/**
 * dpio_get_stashing_destination() - Get the stashing destination..
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 * @sdest:	Returns the stashing destination value
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpio_get_stashing_destination(struct fsl_mc_io	*mc_io,
				  uint32_t		cmd_flags,
				  uint16_t		token,
				  uint8_t		*sdest);

/**
 * struct dpio_attr - Structure representing DPIO attributes
 * @id: DPIO object ID
 * @qbman_portal_ce_offset: offset of the software portal cache-enabled area
 * @qbman_portal_ci_offset: offset of the software portal cache-inhibited area
 * @qbman_portal_id: Software portal ID
 * @channel_mode: Notification channel mode
 * @num_priorities: Number of priorities for the notification channel (1-8);
 *			relevant only if 'channel_mode = DPIO_LOCAL_CHANNEL'
 * @qbman_version: QBMAN version
 */
struct dpio_attr {
	int			id;
	uint64_t		qbman_portal_ce_offset;
	uint64_t		qbman_portal_ci_offset;
	uint16_t		qbman_portal_id;
	enum dpio_channel_mode	channel_mode;
	uint8_t			num_priorities;
	uint32_t		qbman_version;
	uint32_t		clk;
};

/**
 * dpio_get_attributes() - Retrieve DPIO attributes
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPIO object
 * @attr:	Returned object's attributes
 *
 * Return:	'0' on Success; Error code otherwise
 */
int dpio_get_attributes(struct fsl_mc_io	*mc_io,
			uint32_t		cmd_flags,
			uint16_t		token,
			struct dpio_attr	*attr);

/**
 * dpio_get_api_version() - Get Data Path I/O API version
 * @mc_io:  Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @major_ver:	Major version of data path i/o API
 * @minor_ver:	Minor version of data path i/o API
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpio_get_api_version(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t *major_ver,
			 uint16_t *minor_ver);

#endif /* __FSL_DPIO_H */
