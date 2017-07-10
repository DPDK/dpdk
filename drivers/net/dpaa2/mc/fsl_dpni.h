/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP.
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
#ifndef __FSL_DPNI_H
#define __FSL_DPNI_H

#include <fsl_dpkg.h>

struct fsl_mc_io;

/**
 * Data Path Network Interface API
 * Contains initialization APIs and runtime control APIs for DPNI
 */

/** General DPNI macros */

/**
 * Maximum number of traffic classes
 */
#define DPNI_MAX_TC				8
/**
 * Maximum number of buffer pools per DPNI
 */
#define DPNI_MAX_DPBP				8
/**
 * Maximum number of storage-profiles per DPNI
 */
#define DPNI_MAX_SP				2

/**
 * All traffic classes considered; see dpni_set_queue()
 */
#define DPNI_ALL_TCS				(uint8_t)(-1)
/**
 * All flows within traffic class considered; see dpni_set_queue()
 */
#define DPNI_ALL_TC_FLOWS			(uint16_t)(-1)

/**
 * Tx traffic is always released to a buffer pool on transmit, there are no
 * resources allocated to have the frames confirmed back to the source after
 * transmission.
 */
#define DPNI_OPT_TX_FRM_RELEASE			0x000001
/**
 * Disables support for MAC address filtering for addresses other than primary
 * MAC address. This affects both unicast and multicast. Promiscuous mode can
 * still be enabled/disabled for both unicast and multicast. If promiscuous mode
 * is disabled, only traffic matching the primary MAC address will be accepted.
 */
#define DPNI_OPT_NO_MAC_FILTER			0x000002
/**
 * Allocate policers for this DPNI. They can be used to rate-limit traffic per
 * traffic class (TC) basis.
 */
#define DPNI_OPT_HAS_POLICING			0x000004
/**
 * Congestion can be managed in several ways, allowing the buffer pool to
 * deplete on ingress, taildrop on each queue or use congestion groups for sets
 * of queues. If set, it configures a single congestion groups across all TCs.
 * If reset, a congestion group is allocated for each TC. Only relevant if the
 * DPNI has multiple traffic classes.
 */
#define DPNI_OPT_SHARED_CONGESTION		0x000008
/**
 * Enables TCAM for Flow Steering and QoS look-ups. If not specified, all
 * look-ups are exact match. Note that TCAM is not available on LS1088 and its
 * variants. Setting this bit on these SoCs will trigger an error.
 */
#define DPNI_OPT_HAS_KEY_MASKING		0x000010
/**
 * Disables the flow steering table.
 */
#define DPNI_OPT_NO_FS				0x000020

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
int dpni_open(struct fsl_mc_io	*mc_io,
	      uint32_t		cmd_flags,
	      int		dpni_id,
	      uint16_t		*token);

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
int dpni_close(struct fsl_mc_io	*mc_io,
	       uint32_t		cmd_flags,
	       uint16_t		token);

/**
 * struct dpni_cfg - Structure representing DPNI configuration
 * @mac_addr: Primary MAC address
 * @adv: Advanced parameters; default is all zeros;
 *		use this structure to change default settings
 */
struct dpni_cfg {
	/**
	 * @options: Any combination of the following options:
	 *		DPNI_OPT_TX_FRM_RELEASE
	 *		DPNI_OPT_NO_MAC_FILTER
	 *		DPNI_OPT_HAS_POLICING
	 *		DPNI_OPT_SHARED_CONGESTION
	 *		DPNI_OPT_HAS_KEY_MASKING
	 *		DPNI_OPT_NO_FS
	 * @fs_entries: Number of entries in the flow steering table.
	 *		This table is used to select the ingress queue for
	 *		ingress traffic, targeting a GPP core or another.
	 *		In addition it can be used to discard traffic that
	 *		matches the set rule. It is either an exact match table
	 *		or a TCAM table, depending on DPNI_OPT_ HAS_KEY_MASKING
	 *		bit in OPTIONS field. This field is ignored if
	 *		DPNI_OPT_NO_FS bit is set in OPTIONS field. Otherwise,
	 *		value 0 defaults to 64. Maximum supported value is 1024.
	 *		Note that the total number of entries is limited on the
	 *		SoC to as low as 512 entries if TCAM is used.
	 * @vlan_filter_entries: Number of entries in the VLAN address filtering
	 *		table. This is an exact match table used to filter
	 *		ingress traffic based on VLAN IDs. Value 0 disables VLAN
	 *		filtering. Maximum supported value is 16.
	 * @mac_filter_entries: Number of entries in the MAC address filtering
	 *		table. This is an exact match table and allows both
	 *		unicast and multicast entries. The primary MAC address
	 *		of the network interface is not part of this table,
	 *		this contains only entries in addition to it. This
	 *		field is ignored if DPNI_OPT_ NO_MAC_FILTER is set in
	 *		OPTIONS field. Otherwise, value 0 defaults to 80.
	 *		Maximum supported value is 80.
	 * @num_queues: Number of Tx and Rx queues used for traffic
	 *		distribution. This is orthogonal to QoS and is only
	 *		used to distribute traffic to multiple GPP cores.
	 *		This configuration affects the number of Tx queues
	 *		(logical FQs, all associated with a single CEETM queue),
	 *		Rx queues and Tx confirmation queues, if applicable.
	 *		Value 0 defaults to one queue. Maximum supported value
	 *		is 8.
	 * @num_tcs: Number of traffic classes (TCs), reserved for the DPNI.
	 *		TCs can have different priority levels for the purpose
	 *		of Tx scheduling (see DPNI_SET_TX_SELECTION), different
	 *		BPs (DPNI_ SET_POOLS), policers. There are dedicated QM
	 *		queues for traffic classes (including class queues on
	 *		Tx). Value 0 defaults to one TC. Maximum supported value
	 *		is 8.
	 * @qos_entries: Number of entries in the QoS classification table. This
	 *		table is used to select the TC for ingress traffic. It
	 *		is either an exact match or a TCAM table, depending on
	 *		DPNI_OPT_ HAS_KEY_MASKING bit in OPTIONS field. This
	 *		field is ignored if the DPNI has a single TC. Otherwise,
	 *		a value of 0 defaults to 64. Maximum supported value
	 *		is 64.
	 */
	uint32_t options;
	uint16_t fs_entries;
	uint8_t  vlan_filter_entries;
	uint8_t  mac_filter_entries;
	uint8_t  num_queues;
	uint8_t  num_tcs;
	uint8_t  qos_entries;
};

/**
 * dpni_create() - Create the DPNI object
 * @mc_io:	Pointer to MC portal's I/O object
 * @dprc_token:	Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @cfg:	Configuration structure
 * @obj_id: returned object id
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
int dpni_create(struct fsl_mc_io	*mc_io,
		uint16_t		dprc_token,
		uint32_t		cmd_flags,
		const struct dpni_cfg	*cfg,
		uint32_t		*obj_id);

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
int dpni_destroy(struct fsl_mc_io	*mc_io,
		 uint16_t		dprc_token,
		 uint32_t		cmd_flags,
		 uint32_t		object_id);

/**
 * struct dpni_pools_cfg - Structure representing buffer pools configuration
 * @num_dpbp: Number of DPBPs
 * @pools: Array of buffer pools parameters; The number of valid entries
 *	must match 'num_dpbp' value
 */
struct dpni_pools_cfg {
	uint8_t		num_dpbp;
	/**
	 * struct pools - Buffer pools parameters
	 * @dpbp_id: DPBP object ID
	 * @buffer_size: Buffer size
	 * @backup_pool: Backup pool
	 */
	struct {
		int		dpbp_id;
		uint16_t	buffer_size;
		int		backup_pool;
	} pools[DPNI_MAX_DPBP];
};

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
int dpni_set_pools(struct fsl_mc_io		*mc_io,
		   uint32_t			cmd_flags,
		   uint16_t			token,
		   const struct dpni_pools_cfg	*cfg);

/**
 * dpni_enable() - Enable the DPNI, allow sending and receiving frames.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPNI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_enable(struct fsl_mc_io	*mc_io,
		uint32_t		cmd_flags,
		uint16_t		token);

/**
 * dpni_disable() - Disable the DPNI, stop sending and receiving frames.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_disable(struct fsl_mc_io	*mc_io,
		 uint32_t		cmd_flags,
		 uint16_t		token);

/**
 * dpni_is_enabled() - Check if the DPNI is enabled.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Returns '1' if object is enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_is_enabled(struct fsl_mc_io	*mc_io,
		    uint32_t		cmd_flags,
		    uint16_t		token,
		    int			*en);

/**
 * dpni_reset() - Reset the DPNI, returns the object to initial state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_reset(struct fsl_mc_io	*mc_io,
	       uint32_t		cmd_flags,
	       uint16_t		token);

/**
 * struct dpni_attr - Structure representing DPNI attributes
 * @options: Any combination of the following options:
 *		DPNI_OPT_TX_FRM_RELEASE
 *		DPNI_OPT_NO_MAC_FILTER
 *		DPNI_OPT_HAS_POLICING
 *		DPNI_OPT_SHARED_CONGESTION
 *		DPNI_OPT_HAS_KEY_MASKING
 *		DPNI_OPT_NO_FS
 * @num_queues: Number of Tx and Rx queues used for traffic distribution.
 * @num_tcs: Number of traffic classes (TCs), reserved for the DPNI.
 * @mac_filter_entries: Number of entries in the MAC address filtering
 *		table.
 * @vlan_filter_entries: Number of entries in the VLAN address filtering
 *		table.
 * @qos_entries: Number of entries in the QoS classification table.
 * @fs_entries: Number of entries in the flow steering table.
 * @qos_key_size: Size, in bytes, of the QoS look-up key. Defining a key larger
 *			than this when adding QoS entries will result
 *			in an error.
 * @fs_key_size: Size, in bytes, of the flow steering look-up key. Defining a
 *			key larger than this when composing the hash + FS key
 *			will result in an error.
 * @wriop_version: Version of WRIOP HW block.
 *			The 3 version values are stored on 6, 5, 5 bits
 *			respectively.
 *			Values returned:
 *			- 0x400 - WRIOP version 1.0.0, used on LS2080 and
 *			variants,
 *			- 0x421 - WRIOP version 1.1.1, used on LS2088 and
 *			variants,
 *			- 0x422 - WRIOP version 1.1.2, used on LS1088 and
 *			variants.
 */
struct dpni_attr {
	uint32_t options;
	uint8_t  num_queues;
	uint8_t  num_tcs;
	uint8_t  mac_filter_entries;
	uint8_t  vlan_filter_entries;
	uint8_t  qos_entries;
	uint16_t fs_entries;
	uint8_t  qos_key_size;
	uint8_t  fs_key_size;
	uint16_t wriop_version;
};

/**
 * dpni_get_attributes() - Retrieve DPNI attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @attr:	Object's attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_attributes(struct fsl_mc_io	*mc_io,
			uint32_t		cmd_flags,
			uint16_t		token,
			struct dpni_attr	*attr);

/**
 * DPNI errors
 */

/**
 * Extract out of frame header error
 */
#define DPNI_ERROR_EOFHE	0x00020000
/**
 * Frame length error
 */
#define DPNI_ERROR_FLE		0x00002000
/**
 * Frame physical error
 */
#define DPNI_ERROR_FPE		0x00001000
/**
 * Parsing header error
 */
#define DPNI_ERROR_PHE		0x00000020
/**
 * Parser L3 checksum error
 */
#define DPNI_ERROR_L3CE		0x00000004
/**
 * Parser L3 checksum error
 */
#define DPNI_ERROR_L4CE		0x00000001

/**
 * enum dpni_error_action - Defines DPNI behavior for errors
 * @DPNI_ERROR_ACTION_DISCARD: Discard the frame
 * @DPNI_ERROR_ACTION_CONTINUE: Continue with the normal flow
 * @DPNI_ERROR_ACTION_SEND_TO_ERROR_QUEUE: Send the frame to the error queue
 */
enum dpni_error_action {
	DPNI_ERROR_ACTION_DISCARD = 0,
	DPNI_ERROR_ACTION_CONTINUE = 1,
	DPNI_ERROR_ACTION_SEND_TO_ERROR_QUEUE = 2
};

/**
 * struct dpni_error_cfg - Structure representing DPNI errors treatment
 * @errors: Errors mask; use 'DPNI_ERROR__<X>
 * @error_action: The desired action for the errors mask
 * @set_frame_annotation: Set to '1' to mark the errors in frame annotation
 *		status (FAS); relevant only for the non-discard action
 */
struct dpni_error_cfg {
	uint32_t		errors;
	enum dpni_error_action	error_action;
	int			set_frame_annotation;
};

/**
 * dpni_set_errors_behavior() - Set errors behavior
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	Errors configuration
 *
 * this function may be called numerous times with different
 * error masks
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_errors_behavior(struct fsl_mc_io		*mc_io,
			     uint32_t			cmd_flags,
			     uint16_t			token,
			     struct dpni_error_cfg	*cfg);

/**
 * DPNI buffer layout modification options
 */

/**
 * Select to modify the time-stamp setting
 */
#define DPNI_BUF_LAYOUT_OPT_TIMESTAMP		0x00000001
/**
 * Select to modify the parser-result setting; not applicable for Tx
 */
#define DPNI_BUF_LAYOUT_OPT_PARSER_RESULT	0x00000002
/**
 * Select to modify the frame-status setting
 */
#define DPNI_BUF_LAYOUT_OPT_FRAME_STATUS	0x00000004
/**
 * Select to modify the private-data-size setting
 */
#define DPNI_BUF_LAYOUT_OPT_PRIVATE_DATA_SIZE	0x00000008
/**
 * Select to modify the data-alignment setting
 */
#define DPNI_BUF_LAYOUT_OPT_DATA_ALIGN		0x00000010
/**
 * Select to modify the data-head-room setting
 */
#define DPNI_BUF_LAYOUT_OPT_DATA_HEAD_ROOM	0x00000020
/**
 * Select to modify the data-tail-room setting
 */
#define DPNI_BUF_LAYOUT_OPT_DATA_TAIL_ROOM	0x00000040

/**
 * struct dpni_buffer_layout - Structure representing DPNI buffer layout
 * @options: Flags representing the suggested modifications to the buffer
 *		layout; Use any combination of 'DPNI_BUF_LAYOUT_OPT_<X>' flags
 * @pass_timestamp: Pass timestamp value
 * @pass_parser_result: Pass parser results
 * @pass_frame_status: Pass frame status
 * @private_data_size: Size kept for private data (in bytes)
 * @data_align: Data alignment
 * @data_head_room: Data head room
 * @data_tail_room: Data tail room
 */
struct dpni_buffer_layout {
	uint32_t	options;
	int		pass_timestamp;
	int		pass_parser_result;
	int		pass_frame_status;
	uint16_t	private_data_size;
	uint16_t	data_align;
	uint16_t	data_head_room;
	uint16_t	data_tail_room;
};

/**
 * enum dpni_queue_type - Identifies a type of queue targeted by the command
 * @DPNI_QUEUE_RX: Rx queue
 * @DPNI_QUEUE_TX: Tx queue
 * @DPNI_QUEUE_TX_CONFIRM: Tx confirmation queue
 * @DPNI_QUEUE_RX_ERR: Rx error queue
 */enum dpni_queue_type {
	DPNI_QUEUE_RX,
	DPNI_QUEUE_TX,
	DPNI_QUEUE_TX_CONFIRM,
	DPNI_QUEUE_RX_ERR,
};

/**
 * dpni_get_buffer_layout() - Retrieve buffer layout attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue to get the layout from
 * @layout:	Returns buffer layout attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_buffer_layout(struct fsl_mc_io		*mc_io,
			   uint32_t			cmd_flags,
			   uint16_t			token,
			   enum dpni_queue_type		qtype,
			   struct dpni_buffer_layout	*layout);

/**
 * dpni_set_buffer_layout() - Set buffer layout configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue to set layout on
 * @layout:	Buffer layout configuration
 *
 * Return:	'0' on Success; Error code otherwise.
 *
 * @warning	Allowed only when DPNI is disabled
 */
int dpni_set_buffer_layout(struct fsl_mc_io		   *mc_io,
			   uint32_t			   cmd_flags,
			   uint16_t			   token,
			   enum dpni_queue_type		   qtype,
			   const struct dpni_buffer_layout *layout);

/**
 * enum dpni_offload - Identifies a type of offload targeted by the command
 * @DPNI_OFF_RX_L3_CSUM: Rx L3 checksum validation
 * @DPNI_OFF_RX_L4_CSUM: Rx L4 checksum validation
 * @DPNI_OFF_TX_L3_CSUM: Tx L3 checksum generation
 * @DPNI_OFF_TX_L4_CSUM: Tx L4 checksum generation
 */
enum dpni_offload {
	DPNI_OFF_RX_L3_CSUM,
	DPNI_OFF_RX_L4_CSUM,
	DPNI_OFF_TX_L3_CSUM,
	DPNI_OFF_TX_L4_CSUM,
};

/**
 * dpni_set_offload() - Set DPNI offload configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @type:	Type of DPNI offload
 * @config:	Offload configuration.
 *			For checksum offloads, non-zero value enables
 *			the offload.
 *
 * Return:	'0' on Success; Error code otherwise.
 *
 * @warning	Allowed only when DPNI is disabled
 */
int dpni_set_offload(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     enum dpni_offload type,
		     uint32_t config);

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
		     uint32_t *config);

/**
 * dpni_get_qdid() - Get the Queuing Destination ID (QDID) that should be used
 *			for enqueue operations
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue to get QDID for.  For applications lookig to
 *		transmit traffic this should be set to DPNI_QUEUE_TX
 * @qdid:	Returned virtual QDID value that should be used as an argument
 *			in all enqueue operations
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_qdid(struct fsl_mc_io	*mc_io,
		  uint32_t		cmd_flags,
		  uint16_t		token,
		  enum dpni_queue_type	qtype,
		  uint16_t		*qdid);

#define DPNI_STATISTICS_CNT		7

union dpni_statistics {
	/**
	 * struct page_0 - Page_0 statistics structure
	 * @ingress_all_frames: Ingress frame count
	 * @ingress_all_bytes: Ingress byte count
	 * @ingress_multicast_frames: Ingress multicast frame count
	 * @ingress_multicast_bytes: Ingress multicast byte count
	 * @ingress_broadcast_frames: Ingress broadcast frame count
	 * @ingress_broadcast_bytes: Ingress broadcast byte count
	 */
	struct {
		uint64_t ingress_all_frames;
		uint64_t ingress_all_bytes;
		uint64_t ingress_multicast_frames;
		uint64_t ingress_multicast_bytes;
		uint64_t ingress_broadcast_frames;
		uint64_t ingress_broadcast_bytes;
	} page_0;
	/**
	 * struct page_1 - Page_1 statistics structure
	 * @egress_all_frames: Egress frame count
	 * @egress_all_bytes: Egress byte count
	 * @egress_multicast_frames: Egress multicast frame count
	 * @egress_multicast_bytes: Egress multicast byte count
	 * @egress_broadcast_frames: Egress broadcast frame count
	 * @egress_broadcast_bytes: Egress broadcast byte count
	 */
	struct {
		uint64_t egress_all_frames;
		uint64_t egress_all_bytes;
		uint64_t egress_multicast_frames;
		uint64_t egress_multicast_bytes;
		uint64_t egress_broadcast_frames;
		uint64_t egress_broadcast_bytes;
	} page_1;
	/**
	 * struct page_2 - Page_2 statistics structure
	 * @ingress_filtered_frames: Ingress filtered frame count
	 * @ingress_discarded_frames: Ingress discarded frame count
	 * @ingress_nobuffer_discards: Ingress discarded frame count due to
	 *					lack of buffers
	 * @egress_discarded_frames: Egress discarded frame count
	 * @egress_confirmed_frames: Egress confirmed frame count
	 */
	struct {
		uint64_t ingress_filtered_frames;
		uint64_t ingress_discarded_frames;
		uint64_t ingress_nobuffer_discards;
		uint64_t egress_discarded_frames;
		uint64_t egress_confirmed_frames;
	} page_2;
	/**
	 * struct raw - raw statistics structure, used to index counters
	 */
	struct {
		uint64_t counter[DPNI_STATISTICS_CNT];
	} raw;
};

/**
 * Enable auto-negotiation
 */
#define DPNI_LINK_OPT_AUTONEG		0x0000000000000001ULL
/**
 * Enable half-duplex mode
 */
#define DPNI_LINK_OPT_HALF_DUPLEX	0x0000000000000002ULL
/**
 * Enable pause frames
 */
#define DPNI_LINK_OPT_PAUSE		0x0000000000000004ULL
/**
 * Enable a-symmetric pause frames
 */
#define DPNI_LINK_OPT_ASYM_PAUSE	0x0000000000000008ULL

/**
 * struct - Structure representing DPNI link configuration
 * @rate: Rate
 * @options: Mask of available options; use 'DPNI_LINK_OPT_<X>' values
 */
struct dpni_link_cfg {
	uint32_t rate;
	uint64_t options;
};

/**
 * dpni_set_link_cfg() - set the link configuration.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	Link configuration
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_link_cfg(struct fsl_mc_io			*mc_io,
		      uint32_t				cmd_flags,
		      uint16_t				token,
		      const struct dpni_link_cfg	*cfg);

/**
 * struct dpni_link_state - Structure representing DPNI link state
 * @rate: Rate
 * @options: Mask of available options; use 'DPNI_LINK_OPT_<X>' values
 * @up: Link state; '0' for down, '1' for up
 */
struct dpni_link_state {
	uint32_t	rate;
	uint64_t	options;
	int		up;
};

/**
 * dpni_get_link_state() - Return the link state (either up or down)
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @state:	Returned link state;
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_link_state(struct fsl_mc_io	*mc_io,
			uint32_t		cmd_flags,
			uint16_t		token,
			struct dpni_link_state	*state);

/**
 * dpni_set_max_frame_length() - Set the maximum received frame length.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @max_frame_length:	Maximum received frame length (in
 *				bytes); frame is discarded if its
 *				length exceeds this value
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_max_frame_length(struct fsl_mc_io	*mc_io,
			      uint32_t		cmd_flags,
			      uint16_t		token,
			      uint16_t		max_frame_length);

/**
 * dpni_get_max_frame_length() - Get the maximum received frame length.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @max_frame_length:	Maximum received frame length (in
 *				bytes); frame is discarded if its
 *				length exceeds this value
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_max_frame_length(struct fsl_mc_io	*mc_io,
			      uint32_t		cmd_flags,
			      uint16_t		token,
			      uint16_t		*max_frame_length);

/**
 * dpni_set_multicast_promisc() - Enable/disable multicast promiscuous mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Set to '1' to enable; '0' to disable
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_multicast_promisc(struct fsl_mc_io	*mc_io,
			       uint32_t		cmd_flags,
			       uint16_t		token,
			       int		en);

/**
 * dpni_get_multicast_promisc() - Get multicast promiscuous mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Returns '1' if enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_multicast_promisc(struct fsl_mc_io	*mc_io,
			       uint32_t		cmd_flags,
			       uint16_t		token,
			       int		*en);

/**
 * dpni_set_unicast_promisc() - Enable/disable unicast promiscuous mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Set to '1' to enable; '0' to disable
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_unicast_promisc(struct fsl_mc_io	*mc_io,
			     uint32_t		cmd_flags,
			     uint16_t		token,
			     int		en);

/**
 * dpni_get_unicast_promisc() - Get unicast promiscuous mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Returns '1' if enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_unicast_promisc(struct fsl_mc_io	*mc_io,
			     uint32_t		cmd_flags,
			     uint16_t		token,
			     int		*en);

/**
 * dpni_set_primary_mac_addr() - Set the primary MAC address
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mac_addr:	MAC address to set as primary address
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_primary_mac_addr(struct fsl_mc_io	*mc_io,
			      uint32_t		cmd_flags,
			      uint16_t		token,
			      const uint8_t	mac_addr[6]);

/**
 * dpni_get_primary_mac_addr() - Get the primary MAC address
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mac_addr:	Returned MAC address
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_primary_mac_addr(struct fsl_mc_io	*mc_io,
			      uint32_t		cmd_flags,
			      uint16_t		token,
			      uint8_t		mac_addr[6]);

/**
 * dpni_add_mac_addr() - Add MAC address filter
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mac_addr:	MAC address to add
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_add_mac_addr(struct fsl_mc_io	*mc_io,
		      uint32_t		cmd_flags,
		      uint16_t		token,
		      const uint8_t	mac_addr[6]);

/**
 * dpni_remove_mac_addr() - Remove MAC address filter
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mac_addr:	MAC address to remove
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_remove_mac_addr(struct fsl_mc_io	*mc_io,
			 uint32_t		cmd_flags,
			 uint16_t		token,
			 const uint8_t		mac_addr[6]);

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
int dpni_clear_mac_filters(struct fsl_mc_io	*mc_io,
			   uint32_t		cmd_flags,
			   uint16_t		token,
			   int			unicast,
			   int			multicast);

/**
 * dpni_get_port_mac_addr() - Retrieve MAC address associated to the physical
 *		port the DPNI is attached to
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @mac_addr:	MAC address of the physical port, if any, otherwise 0
 *
 * The primary MAC address is not modified by this operation.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_get_port_mac_addr(struct fsl_mc_io	*mc_io,
			   uint32_t		cmd_flags,
			   uint16_t		token,
			   uint8_t		mac_addr[6]);

/**
 * dpni_enable_vlan_filter() - Enable/disable VLAN filtering mode
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @en:		Set to '1' to enable; '0' to disable
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_enable_vlan_filter(struct fsl_mc_io	*mc_io,
			    uint32_t		cmd_flags,
			    uint16_t		token,
			    int			en);

/**
 * dpni_add_vlan_id() - Add VLAN ID filter
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @vlan_id:	VLAN ID to add
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_add_vlan_id(struct fsl_mc_io	*mc_io,
		     uint32_t		cmd_flags,
		     uint16_t		token,
		     uint16_t		vlan_id);

/**
 * dpni_remove_vlan_id() - Remove VLAN ID filter
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @vlan_id:	VLAN ID to remove
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_remove_vlan_id(struct fsl_mc_io	*mc_io,
			uint32_t		cmd_flags,
			uint16_t		token,
			uint16_t		vlan_id);

/**
 * dpni_clear_vlan_filters() - Clear all VLAN filters
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_clear_vlan_filters(struct fsl_mc_io	*mc_io,
			    uint32_t		cmd_flags,
			    uint16_t		token);

/**
 * enum dpni_dist_mode - DPNI distribution mode
 * @DPNI_DIST_MODE_NONE: No distribution
 * @DPNI_DIST_MODE_HASH: Use hash distribution; only relevant if
 *		the 'DPNI_OPT_DIST_HASH' option was set at DPNI creation
 * @DPNI_DIST_MODE_FS:  Use explicit flow steering; only relevant if
 *	 the 'DPNI_OPT_DIST_FS' option was set at DPNI creation
 */
enum dpni_dist_mode {
	DPNI_DIST_MODE_NONE = 0,
	DPNI_DIST_MODE_HASH = 1,
	DPNI_DIST_MODE_FS = 2
};

/**
 * enum dpni_fs_miss_action -   DPNI Flow Steering miss action
 * @DPNI_FS_MISS_DROP: In case of no-match, drop the frame
 * @DPNI_FS_MISS_EXPLICIT_FLOWID: In case of no-match, use explicit flow-id
 * @DPNI_FS_MISS_HASH: In case of no-match, distribute using hash
 */
enum dpni_fs_miss_action {
	DPNI_FS_MISS_DROP = 0,
	DPNI_FS_MISS_EXPLICIT_FLOWID = 1,
	DPNI_FS_MISS_HASH = 2
};

/**
 * struct dpni_fs_tbl_cfg - Flow Steering table configuration
 * @miss_action: Miss action selection
 * @default_flow_id: Used when 'miss_action = DPNI_FS_MISS_EXPLICIT_FLOWID'
 */
struct dpni_fs_tbl_cfg {
	enum dpni_fs_miss_action	miss_action;
	uint16_t			default_flow_id;
};

/**
 * dpni_prepare_key_cfg() - function prepare extract parameters
 * @cfg: defining a full Key Generation profile (rule)
 * @key_cfg_buf: Zeroed 256 bytes of memory before mapping it to DMA
 *
 * This function has to be called before the following functions:
 *	- dpni_set_rx_tc_dist()
 *	- dpni_set_qos_table()
 */
int dpni_prepare_key_cfg(const struct dpkg_profile_cfg	*cfg,
			 uint8_t			*key_cfg_buf);

/**
 * struct dpni_rx_tc_dist_cfg - Rx traffic class distribution configuration
 * @dist_size: Set the distribution size;
 *	supported values: 1,2,3,4,6,7,8,12,14,16,24,28,32,48,56,64,96,
 *	112,128,192,224,256,384,448,512,768,896,1024
 * @dist_mode: Distribution mode
 * @key_cfg_iova: I/O virtual address of 256 bytes DMA-able memory filled with
 *		the extractions to be used for the distribution key by calling
 *		dpni_prepare_key_cfg() relevant only when
 *		'dist_mode != DPNI_DIST_MODE_NONE', otherwise it can be '0'
 * @fs_cfg: Flow Steering table configuration; only relevant if
 *		'dist_mode = DPNI_DIST_MODE_FS'
 */
struct dpni_rx_tc_dist_cfg {
	uint16_t		dist_size;
	enum dpni_dist_mode	dist_mode;
	uint64_t		key_cfg_iova;
	struct dpni_fs_tbl_cfg	fs_cfg;
};

/**
 * dpni_set_rx_tc_dist() - Set Rx traffic class distribution configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @tc_id:	Traffic class selection (0-7)
 * @cfg:	Traffic class distribution configuration
 *
 * warning: if 'dist_mode != DPNI_DIST_MODE_NONE', call dpni_prepare_key_cfg()
 *			first to prepare the key_cfg_iova parameter
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_set_rx_tc_dist(struct fsl_mc_io			*mc_io,
			uint32_t				cmd_flags,
			uint16_t				token,
			uint8_t					tc_id,
			const struct dpni_rx_tc_dist_cfg	*cfg);
/**
 * enum dpni_congestion_unit - DPNI congestion units
 * @DPNI_CONGESTION_UNIT_BYTES: bytes units
 * @DPNI_CONGESTION_UNIT_FRAMES: frames units
 */
enum dpni_congestion_unit {
	DPNI_CONGESTION_UNIT_BYTES = 0,
	DPNI_CONGESTION_UNIT_FRAMES
};


/**
 * enum dpni_dest - DPNI destination types
 * @DPNI_DEST_NONE: Unassigned destination; The queue is set in parked mode and
 *		does not generate FQDAN notifications; user is expected to
 *		dequeue from the queue based on polling or other user-defined
 *		method
 * @DPNI_DEST_DPIO: The queue is set in schedule mode and generates FQDAN
 *		notifications to the specified DPIO; user is expected to dequeue
 *		from the queue only after notification is received
 * @DPNI_DEST_DPCON: The queue is set in schedule mode and does not generate
 *		FQDAN notifications, but is connected to the specified DPCON
 *		object; user is expected to dequeue from the DPCON channel
 */
enum dpni_dest {
	DPNI_DEST_NONE = 0,
	DPNI_DEST_DPIO = 1,
	DPNI_DEST_DPCON = 2
};

/**
 * struct dpni_dest_cfg - Structure representing DPNI destination parameters
 * @dest_type: Destination type
 * @dest_id: Either DPIO ID or DPCON ID, depending on the destination type
 * @priority: Priority selection within the DPIO or DPCON channel; valid values
 *		are 0-1 or 0-7, depending on the number of priorities in that
 *		channel; not relevant for 'DPNI_DEST_NONE' option
 */
struct dpni_dest_cfg {
	enum dpni_dest	dest_type;
	int		dest_id;
	uint8_t		priority;
};

/* DPNI congestion options */

/**
 * CSCN message is written to message_iova once entering a
 * congestion state (see 'threshold_entry')
 */
#define DPNI_CONG_OPT_WRITE_MEM_ON_ENTER	0x00000001
/**
 * CSCN message is written to message_iova once exiting a
 * congestion state (see 'threshold_exit')
 */
#define DPNI_CONG_OPT_WRITE_MEM_ON_EXIT		0x00000002
/**
 * CSCN write will attempt to allocate into a cache (coherent write);
 * valid only if 'DPNI_CONG_OPT_WRITE_MEM_<X>' is selected
 */
#define DPNI_CONG_OPT_COHERENT_WRITE		0x00000004
/**
 * if 'dest_cfg.dest_type != DPNI_DEST_NONE' CSCN message is sent to
 * DPIO/DPCON's WQ channel once entering a congestion state
 * (see 'threshold_entry')
 */
#define DPNI_CONG_OPT_NOTIFY_DEST_ON_ENTER	0x00000008
/**
 * if 'dest_cfg.dest_type != DPNI_DEST_NONE' CSCN message is sent to
 * DPIO/DPCON's WQ channel once exiting a congestion state
 * (see 'threshold_exit')
 */
#define DPNI_CONG_OPT_NOTIFY_DEST_ON_EXIT	0x00000010
/**
 * if 'dest_cfg.dest_type != DPNI_DEST_NONE' when the CSCN is written to the
 * sw-portal's DQRR, the DQRI interrupt is asserted immediately (if enabled)
 */
#define DPNI_CONG_OPT_INTR_COALESCING_DISABLED	0x00000020

/**
 * struct dpni_congestion_notification_cfg - congestion notification
 *		configuration
 * @units: units type
 * @threshold_entry: above this threshold we enter a congestion state.
 *	set it to '0' to disable it
 * @threshold_exit: below this threshold we exit the congestion state.
 * @message_ctx: The context that will be part of the CSCN message
 * @message_iova: I/O virtual address (must be in DMA-able memory),
 *	must be 16B aligned; valid only if 'DPNI_CONG_OPT_WRITE_MEM_<X>' is
 *	contained in 'options'
 * @dest_cfg: CSCN can be send to either DPIO or DPCON WQ channel
 * @notification_mode: Mask of available options; use 'DPNI_CONG_OPT_<X>' values
 */

struct dpni_congestion_notification_cfg {
	enum dpni_congestion_unit	units;
	uint32_t			threshold_entry;
	uint32_t			threshold_exit;
	uint64_t			message_ctx;
	uint64_t			message_iova;
	struct dpni_dest_cfg		dest_cfg;
	uint16_t			notification_mode;
};

/**
 * dpni_set_congestion_notification() - Set traffic class congestion
 *	notification configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue - Rx, Tx and Tx confirm types are supported
 * @tc_id:	Traffic class selection (0-7)
 * @cfg:	congestion notification configuration
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_set_congestion_notification(
			struct fsl_mc_io		*mc_io,
			uint32_t			cmd_flags,
			uint16_t			token,
			enum dpni_queue_type		qtype,
			uint8_t				tc_id,
			const struct dpni_congestion_notification_cfg *cfg);

/**
 * dpni_get_congestion_notification() - Get traffic class congestion
 *	notification configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @qtype:	Type of queue - Rx, Tx and Tx confirm types are supported
 * @tc_id:	Traffic class selection (0-7)
 * @cfg:	congestion notification configuration
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_get_congestion_notification(struct fsl_mc_io		*mc_io,
				     uint32_t			cmd_flags,
				     uint16_t			token,
				     enum dpni_queue_type	qtype,
				     uint8_t			tc_id,
				struct dpni_congestion_notification_cfg *cfg);


/**
 * struct dpni_queue - Queue structure
 * @user_context:	User data, presented to the user along with any frames
 *			from this queue. Not relevant for Tx queues.
 */
struct dpni_queue {
	/**
	 * struct destination - Destination structure
	 * @id:	ID of the destination, only relevant if DEST_TYPE is > 0.
	 *			Identifies either a DPIO or a DPCON object.
	 *			Not relevant for Tx queues.
	 * @type:	May be one of the following:
	 *			0 - No destination, queue can be manually
	 *				queried, but will not push traffic or
	 *				notifications to a DPIO;
	 *			1 - The destination is a DPIO. When traffic
	 *				becomes available in the queue a FQDAN
	 *				(FQ data available notification) will be
	 *				generated to selected DPIO;
	 *			2 - The destination is a DPCON. The queue is
	 *				associated with a DPCON object for the
	 *				purpose of scheduling between multiple
	 *				queues. The DPCON may be independently
	 *				configured to generate notifications.
	 *				Not relevant for Tx queues.
	 * @hold_active: Hold active, maintains a queue scheduled for longer
	 *		in a DPIO during dequeue to reduce spread of traffic.
	 *		Only relevant if queues are
	 *		not affined to a single DPIO.
	 */
	struct {
		uint16_t id;
		enum dpni_dest type;
		char hold_active;
		uint8_t priority;
	} destination;
	uint64_t user_context;
	/**
	 * struct flc - FD FLow Context structure
	 * @value:		FLC value to set
	 * @stash_control:	Boolean, indicates whether the 6 lowest
	 *			significant bits are used for stash control.
	 */
	struct {
		uint64_t value;
		char stash_control;
	} flc;
};

/**
 * struct dpni_queue_id - Queue identification, used for enqueue commands
 *				or queue control
 * @fqid:	FQID used for enqueueing to and/or configuration of this
 *			specific FQ
 * @qdbin:	Queueing bin, used to enqueue using QDID, DQBIN, QPRI.
 *			Only relevant for Tx queues.
 */
struct dpni_queue_id {
	uint32_t fqid;
	uint16_t qdbin;
};

/**
 * enum dpni_confirmation_mode - Defines DPNI options supported for Tx
 * confirmation
 * @DPNI_CONF_AFFINE: For each Tx queue set associated with a sender there is
 * an affine Tx Confirmation queue
 * @DPNI_CONF_SINGLE: All Tx queues are associated with a single Tx
 * confirmation queue
 * @DPNI_CONF_DISABLE: Tx frames are not confirmed.  This must be associated
 * with proper FD set-up to have buffers release to a Buffer Pool, otherwise
 * buffers will be leaked
 */
enum dpni_confirmation_mode {
	DPNI_CONF_AFFINE,
	DPNI_CONF_SINGLE,
	DPNI_CONF_DISABLE,
};

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
 * Tx confirmations to a shared Tx conf queue.  The ID of the queue when
 * calling dpni_set/get_queue is -1.
 * Tx confirmation mode can only be changed while the DPNI is disabled.
 * Executing this command while the DPNI is enabled will return an error.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_set_tx_confirmation_mode(struct fsl_mc_io		*mc_io,
				  uint32_t			cmd_flags,
				  uint16_t			token,
				  enum dpni_confirmation_mode	mode);

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
			 uint16_t *minor_ver);

/**
 * Set User Context
 */
#define DPNI_QUEUE_OPT_USER_CTX		0x00000001

/**
 * Set queue destination configuration
 */
#define DPNI_QUEUE_OPT_DEST		0x00000002

/**
 * Set FD[FLC] configuration for traffic on this queue.  Note that FLC values
 * set with dpni_add_fs_entry, if any, take precedence over values per queue.
 */
#define DPNI_QUEUE_OPT_FLC		0x00000004

/**
 * Set the queue to hold active mode.  This prevents the queue from being
 * rescheduled between DPIOs while it carries traffic and is active on one
 * DPNI.  Can help reduce reordering when servicing one queue on multiple
 * CPUs, but the queue is also less likely to push data to multiple CPUs
 * especially when congested.
 */
#define DPNI_QUEUE_OPT_HOLD_ACTIVE	0x00000008

/**
 * dpni_set_queue() - Set queue parameters
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPNI object
 * @qtype:		Type of queue - all queue types are supported, although
 *				the command is ignored for Tx
 * @tc:			Traffic class, in range 0 to NUM_TCS - 1
 * @index:		Selects the specific queue out of the set
 *				allocated for the same TC.Value must be in
 *				range 0 to NUM_QUEUES - 1
 * @options:		A combination of DPNI_QUEUE_OPT_ values that control
 *				what configuration options are set on the queue
 * @queue:		Queue configuration structure
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpni_set_queue(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   enum dpni_queue_type qtype,
		   uint8_t tc,
		   uint8_t index,
		   uint8_t options,
		   const struct dpni_queue *queue);

/**
 * dpni_get_queue() - Get queue parameters
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPNI object
 * @qtype:		Type of queue - all queue types are supported
 * @tc:			Traffic class, in range 0 to NUM_TCS - 1
 * @index:		Selects the specific queue out of the set allocated
 *				for the same TC. Value must be in range 0 to
 *				NUM_QUEUES - 1
 * @queue:		Queue configuration structure
 * @qid:		Queue identification
 *
 * This function returns current queue configuration which can be changed by
 * calling dpni_set_queue, and queue identification information.
 * Returned qid.fqid and/or qid.qdbin values can be used to:
 * - enqueue traffic for Tx queues,
 * - perform volatile dequeue for Rx and, if applicable, Tx confirmation
 *   clean-up,
 * - retrieve queue state.
 *
 * All these operations are supported through the DPIO run-time API.
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpni_get_queue(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   enum dpni_queue_type qtype,
		   uint8_t tc,
		   uint8_t index,
		   struct dpni_queue *queue,
		   struct dpni_queue_id *qid);

/**
 * dpni_get_statistics() - Get DPNI statistics
 * @mc_io:		Pointer to MC portal's I/O object
 * @cmd_flags:		Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:		Token of DPNI object
 * @page:		Selects the statistics page to retrieve, see
 *				DPNI_GET_STATISTICS output.
 *				Pages are numbered 0 to 2.
 * @stat:		Structure containing the statistics
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpni_get_statistics(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t page,
			union dpni_statistics *stat);

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
			  uint16_t token);

/**
 * enum dpni_congestion_point - Structure representing congestion point
 * @DPNI_CP_QUEUE:	Set taildrop per queue, identified by QUEUE_TYPE, TC and
 *				QUEUE_INDEX
 * @DPNI_CP_GROUP:	Set taildrop per queue group. Depending on options used
 *				to define the DPNI this can be either per
 *				TC (default) or per interface
 *				(DPNI_OPT_SHARED_CONGESTION set at DPNI create).
 *				QUEUE_INDEX is ignored if this type is used.
 */
enum dpni_congestion_point {
	DPNI_CP_QUEUE,
	DPNI_CP_GROUP,
};

/**
 * struct dpni_taildrop - Structure representing the taildrop
 * @enable:	Indicates whether the taildrop is active or not.
 * @units:	Indicates the unit of THRESHOLD. Queue taildrop only
 *			supports byte units, this field is ignored and
 *			assumed = 0 if CONGESTION_POINT is 0.
 * @threshold:	Threshold value, in units identified by UNITS field. Value 0
 *			cannot be used as a valid taildrop threshold,
 *			THRESHOLD must be > 0 if the taildrop is
 *			enabled.
 */
struct dpni_taildrop {
	char enable;
	enum dpni_congestion_unit units;
	uint32_t threshold;
};

/**
 * dpni_set_taildrop() - Set taildrop per queue or TC
 *
 * Setting a per-TC taildrop (cg_point = DPNI_CP_GROUP) will reset any current
 * congestion notification or early drop (WRED) configuration previously applied
 * to the same TC.
 *
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cg_point:	Congestion point.  DPNI_CP_QUEUE is only supported in
 *		combination with DPNI_QUEUE_RX.
 * @q_type:	Queue type, can be DPNI_QUEUE_RX or DPNI_QUEUE_TX.
 * @tc:		Traffic class to apply this taildrop to
 * @q_index:	Index of the queue if the DPNI supports multiple queues for
 *			traffic distribution.
 *			Ignored if CONGESTION_POINT is not DPNI_CP_QUEUE.
 * @taildrop:	Taildrop structure
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpni_set_taildrop(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      enum dpni_congestion_point cg_point,
		      enum dpni_queue_type q_type,
		      uint8_t tc,
		      uint8_t q_index,
		      struct dpni_taildrop *taildrop);

/**
 * dpni_get_taildrop() - Get taildrop information
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cg_point:	Congestion point
 * @q_type:
 * @tc:		Traffic class to apply this taildrop to
 * @q_index:	Index of the queue if the DPNI supports multiple queues for
 *			traffic distribution. Ignored if CONGESTION_POINT
 *			is not 0.
 * @taildrop:	Taildrop structure
 *
 * Return:  '0' on Success; Error code otherwise.
 */
int dpni_get_taildrop(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      enum dpni_congestion_point cg_point,
		      enum dpni_queue_type q_type,
		      uint8_t tc,
		      uint8_t q_index,
		      struct dpni_taildrop *taildrop);
#endif /* __FSL_DPNI_H */
