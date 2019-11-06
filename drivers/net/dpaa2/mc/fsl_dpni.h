/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2019 NXP
 *
 */
#ifndef __FSL_DPNI_H
#define __FSL_DPNI_H

#include <fsl_dpkg.h>
#include <fsl_dpopr.h>

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
 * Enable the Order Restoration support
 */
#define DPNI_OPT_HAS_OPR				0x000040

/**
 * Order Point Records are shared for the entire TC
 */
#define DPNI_OPT_OPR_PER_TC				0x000080
/**
 * All Tx traffic classes will use a single sender (ignore num_queueus for tx)
 */
#define DPNI_OPT_SINGLE_SENDER			0x000100
/**
 * Define a custom number of congestion groups
 */
#define DPNI_OPT_CUSTOM_CG				0x000200


/**
 * Software sequence maximum layout size
 */
#define DPNI_SW_SEQUENCE_LAYOUT_SIZE 33

int dpni_open(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      int dpni_id,
	      uint16_t *token);

int dpni_close(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token);

/**
 * struct dpni_cfg - Structure representing DPNI configuration
 * @options: Any combination of the following options:
 *		DPNI_OPT_TX_FRM_RELEASE
 *		DPNI_OPT_NO_MAC_FILTER
 *		DPNI_OPT_HAS_POLICING
 *		DPNI_OPT_SHARED_CONGESTION
 *		DPNI_OPT_HAS_KEY_MASKING
 *		DPNI_OPT_NO_FS
 *		DPNI_OPT_SINGLE_SENDER
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
 *		of Tx scheduling (see DPNI_SET_TX_PRIORITIES), different
 *		BPs (DPNI_ SET_POOLS), policers. There are dedicated QM
 *		queues for traffic classes (including class queues on
 *		Tx). Value 0 defaults to one TC. Maximum supported value
 *		is 16. There are maximum 16 TCs for Tx and 8 TCs for Rx.
 *		When num_tcs>8 Tx will use this value but Rx will have
 *		only 8 traffic classes.
 * @num_rx_tcs: if set to other value than zero represents number
 *		of TCs used for Rx. Maximum value is 8. If set to zero the
 *		number of Rx TCs will be initialized with the value provided
 *		in num_tcs parameter.
 * @qos_entries: Number of entries in the QoS classification table. This
 *		table is used to select the TC for ingress traffic. It
 *		is either an exact match or a TCAM table, depending on
 *		DPNI_OPT_ HAS_KEY_MASKING bit in OPTIONS field. This
 *		field is ignored if the DPNI has a single TC. Otherwise,
 *		a value of 0 defaults to 64. Maximum supported value
 *		is 64.
 */
struct dpni_cfg {
	uint32_t options;
	uint16_t fs_entries;
	uint8_t  vlan_filter_entries;
	uint8_t  mac_filter_entries;
	uint8_t  num_queues;
	uint8_t  num_tcs;
	uint8_t  num_rx_tcs;
	uint8_t  qos_entries;
	uint8_t  num_cgs;
};

int dpni_create(struct fsl_mc_io *mc_io,
		uint16_t dprc_token,
		uint32_t cmd_flags,
		const struct dpni_cfg *cfg,
		uint32_t *obj_id);

int dpni_destroy(struct fsl_mc_io *mc_io,
		 uint16_t dprc_token,
		 uint32_t cmd_flags,
		 uint32_t object_id);

/**
 * struct dpni_pools_cfg - Structure representing buffer pools configuration
 * @num_dpbp:	Number of DPBPs
 * @pool_options: Buffer assignment options
 *                This field is a combination of DPNI_POOL_ASSOC_flags
 * @pools:	Array of buffer pools parameters; The number of valid entries
 *		must match 'num_dpbp' value
 * @pools.dpbp_id:     DPBP object ID
 * @pools.priority:    Priority mask that indicates TC's used with this buffer.
 *		       I set to 0x00 MC will assume value 0xff.
 * @pools.buffer_size: Buffer size
 * @pools.backup_pool: Backup pool
 */

#define DPNI_POOL_ASSOC_QPRI	0
#define DPNI_POOL_ASSOC_QDBIN	1

struct dpni_pools_cfg {
	uint8_t num_dpbp;
	uint8_t pool_options;
	struct {
		int		dpbp_id;
		uint8_t		priority_mask;
		uint16_t	buffer_size;
		int		backup_pool;
	} pools[DPNI_MAX_DPBP];
};

int dpni_set_pools(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   const struct dpni_pools_cfg *cfg);

int dpni_enable(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token);

int dpni_disable(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token);

int dpni_is_enabled(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    int *en);

int dpni_reset(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token);

/**
 * DPNI IRQ Index and Events
 */

/**
 * IRQ index
 */
#define DPNI_IRQ_INDEX				0
/**
 * IRQ event - indicates a change in link state
 */
#define DPNI_IRQ_EVENT_LINK_CHANGED		0x00000001

int dpni_set_irq_enable(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t irq_index,
			uint8_t en);

int dpni_get_irq_enable(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t irq_index,
			uint8_t *en);

int dpni_set_irq_mask(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t irq_index,
		      uint32_t mask);

int dpni_get_irq_mask(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t irq_index,
		      uint32_t *mask);

int dpni_get_irq_status(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t irq_index,
			uint32_t *status);

int dpni_clear_irq_status(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  uint8_t irq_index,
			  uint32_t status);

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
 * @num_rx_tcs: Number of RX traffic classes (TCs), reserved for the DPNI.
 * @num_tx_tcs: Number of TX traffic classes (TCs), reserved for the DPNI.
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
 *			- 0xC00 - WRIOP version 3.0.0, used on LX2160 and
 *			variants.
 */
struct dpni_attr {
	uint32_t options;
	uint8_t  num_queues;
	uint8_t  num_rx_tcs;
	uint8_t  num_tx_tcs;
	uint8_t  mac_filter_entries;
	uint8_t  vlan_filter_entries;
	uint8_t  qos_entries;
	uint16_t fs_entries;
	uint8_t  qos_key_size;
	uint8_t  fs_key_size;
	uint16_t wriop_version;
	uint8_t  num_cgs;
};

int dpni_get_attributes(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpni_attr *attr);

/**
 * DPNI errors
 */

/**
 * Discard error. When set all discarded frames in wriop will be enqueued to
 * error queue. To be used in dpni_set_errors_behavior() only if error_action
 * parameter is set to DPNI_ERROR_ACTION_SEND_TO_ERROR_QUEUE.
 */
#define DPNI_ERROR_DISC		0x80000000

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
 * @errors:			Errors mask; use 'DPNI_ERROR__<X>
 * @error_action:		The desired action for the errors mask
 * @set_frame_annotation:	Set to '1' to mark the errors in frame
 *				annotation status (FAS); relevant only
 *				for the non-discard action
 */
struct dpni_error_cfg {
	uint32_t errors;
	enum dpni_error_action error_action;
	int set_frame_annotation;
};

int dpni_set_errors_behavior(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     struct dpni_error_cfg *cfg);

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
 * Select to modify the sw-opaque value setting
 */
#define DPNI_BUF_LAYOUT_OPT_SW_OPAQUE		0x00000080

/**
 * struct dpni_buffer_layout - Structure representing DPNI buffer layout
 * @options:		Flags representing the suggested modifications to the
 *			buffer layout;
 *			Use any combination of 'DPNI_BUF_LAYOUT_OPT_<X>' flags
 * @pass_timestamp:	Pass timestamp value
 * @pass_parser_result:	Pass parser results
 * @pass_frame_status:	Pass frame status
 * @private_data_size:	Size kept for private data (in bytes)
 * @data_align:		Data alignment
 * @data_head_room:	Data head room
 * @data_tail_room:	Data tail room
 */
struct dpni_buffer_layout {
	uint32_t options;
	int pass_timestamp;
	int pass_parser_result;
	int pass_frame_status;
	int pass_sw_opaque;
	uint16_t private_data_size;
	uint16_t data_align;
	uint16_t data_head_room;
	uint16_t data_tail_room;
};

/**
 * enum dpni_queue_type - Identifies a type of queue targeted by the command
 * @DPNI_QUEUE_RX: Rx queue
 * @DPNI_QUEUE_TX: Tx queue
 * @DPNI_QUEUE_TX_CONFIRM: Tx confirmation queue
 * @DPNI_QUEUE_RX_ERR: Rx error queue
 */
enum dpni_queue_type {
	DPNI_QUEUE_RX,
	DPNI_QUEUE_TX,
	DPNI_QUEUE_TX_CONFIRM,
	DPNI_QUEUE_RX_ERR,
};

int dpni_get_buffer_layout(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   enum dpni_queue_type qtype,
			   struct dpni_buffer_layout *layout);

int dpni_set_buffer_layout(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   enum dpni_queue_type qtype,
			   const struct dpni_buffer_layout *layout);

/**
 * enum dpni_offload - Identifies a type of offload targeted by the command
 * @DPNI_OFF_RX_L3_CSUM: Rx L3 checksum validation
 * @DPNI_OFF_RX_L4_CSUM: Rx L4 checksum validation
 * @DPNI_OFF_TX_L3_CSUM: Tx L3 checksum generation
 * @DPNI_OFF_TX_L4_CSUM: Tx L4 checksum generation
 * @DPNI_OPT_FLCTYPE_HASH: flow context will be generated by WRIOP for AIOP or
 *			   for CPU
 */
enum dpni_offload {
	DPNI_OFF_RX_L3_CSUM,
	DPNI_OFF_RX_L4_CSUM,
	DPNI_OFF_TX_L3_CSUM,
	DPNI_OFF_TX_L4_CSUM,
	DPNI_FLCTYPE_HASH,
};

int dpni_set_offload(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     enum dpni_offload type,
		     uint32_t config);

int dpni_get_offload(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     enum dpni_offload type,
		     uint32_t *config);

int dpni_get_qdid(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token,
		  enum dpni_queue_type qtype,
		  uint16_t *qdid);

int dpni_get_tx_data_offset(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token,
			    uint16_t *data_offset);

#define DPNI_STATISTICS_CNT		7

/**
 * union dpni_statistics - Union describing the DPNI statistics
 * @page_0: Page_0 statistics structure
 * @page_0.ingress_all_frames: Ingress frame count
 * @page_0.ingress_all_bytes: Ingress byte count
 * @page_0.ingress_multicast_frames: Ingress multicast frame count
 * @page_0.ingress_multicast_bytes: Ingress multicast byte count
 * @page_0.ingress_broadcast_frames: Ingress broadcast frame count
 * @page_0.ingress_broadcast_bytes: Ingress broadcast byte count
 * @page_1: Page_1 statistics structure
 * @page_1.egress_all_frames: Egress frame count
 * @page_1.egress_all_bytes: Egress byte count
 * @page_1.egress_multicast_frames: Egress multicast frame count
 * @page_1.egress_multicast_bytes: Egress multicast byte count
 * @page_1.egress_broadcast_frames: Egress broadcast frame count
 * @page_1.egress_broadcast_bytes: Egress broadcast byte count
 * @page_2: Page_2 statistics structure
 * @page_2.ingress_filtered_frames: Ingress filtered frame count
 * @page_2.ingress_discarded_frames: Ingress discarded frame count
 * @page_2.ingress_nobuffer_discards: Ingress discarded frame count due to
 *	lack of buffers
 * @page_2.egress_discarded_frames: Egress discarded frame count
 * @page_2.egress_confirmed_frames: Egress confirmed frame count
 * @page_3: Page_3 statistics structure with values for the selected TC
 * @page_3.ceetm_dequeue_bytes: Cumulative count of the number of bytes dequeued
 * @page_3.ceetm_dequeue_frames: Cumulative count of the number of frames
 *	dequeued
 * @page_3.ceetm_reject_bytes: Cumulative count of the number of bytes in all
 *	frames whose enqueue was rejected
 * @page_3.ceetm_reject_frames: Cumulative count of all frame enqueues rejected
 * @page_4: congestion point drops for seleted TC
 * @page_4.cgr_reject_frames: number of rejected frames due to congestion point
 * @page_4.cgr_reject_bytes: number of rejected bytes due to congestion point
 * @page_5: policer statistics per TC
 * @page_5.policer_cnt_red: NUmber of red colored frames
 * @page_5.policer_cnt_yellow: number of yellow colored frames
 * @page_5.policer_cnt_green: number of green colored frames
 * @page_5.policer_cnt_re_red: number of recolored red frames
 * @page_5.policer_cnt_re_yellow: number of recolored yellow frames
 * @page_6.tx_pending_frames_cnt: total number of frames pending in Tx queues
 * @raw: raw statistics structure, used to index counters
 */
union dpni_statistics {
	struct {
		uint64_t ingress_all_frames;
		uint64_t ingress_all_bytes;
		uint64_t ingress_multicast_frames;
		uint64_t ingress_multicast_bytes;
		uint64_t ingress_broadcast_frames;
		uint64_t ingress_broadcast_bytes;
	} page_0;
	struct {
		uint64_t egress_all_frames;
		uint64_t egress_all_bytes;
		uint64_t egress_multicast_frames;
		uint64_t egress_multicast_bytes;
		uint64_t egress_broadcast_frames;
		uint64_t egress_broadcast_bytes;
	} page_1;
	struct {
		uint64_t ingress_filtered_frames;
		uint64_t ingress_discarded_frames;
		uint64_t ingress_nobuffer_discards;
		uint64_t egress_discarded_frames;
		uint64_t egress_confirmed_frames;
	} page_2;
	struct {
		uint64_t ceetm_dequeue_bytes;
		uint64_t ceetm_dequeue_frames;
		uint64_t ceetm_reject_bytes;
		uint64_t ceetm_reject_frames;
	} page_3;
	struct {
		uint64_t cgr_reject_frames;
		uint64_t cgr_reject_bytes;
	} page_4;
	struct {
		uint64_t policer_cnt_red;
		uint64_t policer_cnt_yellow;
		uint64_t policer_cnt_green;
		uint64_t policer_cnt_re_red;
		uint64_t policer_cnt_re_yellow;
	} page_5;
	struct {
		uint64_t tx_pending_frames_cnt;
	} page_6;
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
 * Enable priority flow control pause frames
 */
#define DPNI_LINK_OPT_PFC_PAUSE	0x0000000000000010ULL

/**
 * Advertise 10MB full duplex
 */
#define DPNI_ADVERTISED_10BASET_FULL           0x0000000000000001ULL
/**
 * Advertise 100MB full duplex
 */
#define DPNI_ADVERTISED_100BASET_FULL          0x0000000000000002ULL
/**
 * Advertise 1GB full duplex
 */
#define DPNI_ADVERTISED_1000BASET_FULL         0x0000000000000004ULL
/**
 * Advertise auto-negotiation enable
 */
#define DPNI_ADVERTISED_AUTONEG                0x0000000000000008ULL
/**
 * Advertise 10GB full duplex
 */
#define DPNI_ADVERTISED_10000BASET_FULL        0x0000000000000010ULL
/**
 * Advertise 2.5GB full duplex
 */
#define DPNI_ADVERTISED_2500BASEX_FULL         0x0000000000000020ULL
/**
 * Advertise 5GB full duplex
 */
#define DPNI_ADVERTISED_5000BASET_FULL         0x0000000000000040ULL


/**
 * struct - Structure representing DPNI link configuration
 * @rate: Rate
 * @options: Mask of available options; use 'DPNI_LINK_OPT_<X>' values
 * @advertising: Speeds that are advertised for autoneg (bitmap)
 */
struct dpni_link_cfg {
	uint32_t rate;
	uint64_t options;
	uint64_t advertising;
};

int dpni_set_link_cfg(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      const struct dpni_link_cfg *cfg);

/**
 * struct dpni_link_state - Structure representing DPNI link state
 * @rate:	Rate
 * @options:	Mask of available options; use 'DPNI_LINK_OPT_<X>' values
 * @up:		Link state; '0' for down, '1' for up
 * @state_valid: Ignore/Update the state of the link
 * @supported: Speeds capability of the phy (bitmap)
 * @advertising: Speeds that are advertised for autoneg (bitmap)
 */
struct dpni_link_state {
	uint32_t rate;
	uint64_t options;
	int up;
	int state_valid;
	uint64_t supported;
	uint64_t advertising;
};

int dpni_get_link_state(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpni_link_state *state);

int dpni_set_max_frame_length(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      uint16_t max_frame_length);

int dpni_get_max_frame_length(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      uint16_t *max_frame_length);

int dpni_set_mtu(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token,
		 uint16_t mtu);

int dpni_get_mtu(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token,
		 uint16_t *mtu);

int dpni_set_multicast_promisc(struct fsl_mc_io *mc_io,
			       uint32_t cmd_flags,
			       uint16_t token,
			       int en);

int dpni_get_multicast_promisc(struct fsl_mc_io *mc_io,
			       uint32_t cmd_flags,
			       uint16_t token,
			       int *en);

int dpni_set_unicast_promisc(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     int en);

int dpni_get_unicast_promisc(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     int *en);

int dpni_set_primary_mac_addr(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      const uint8_t mac_addr[6]);

int dpni_get_primary_mac_addr(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      uint8_t mac_addr[6]);

int dpni_add_mac_addr(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      const uint8_t mac_addr[6],
			  uint8_t flags,
			  uint8_t tc_id,
			  uint8_t flow_id);

int dpni_remove_mac_addr(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 const uint8_t mac_addr[6]);

int dpni_clear_mac_filters(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   int unicast,
			   int multicast);

int dpni_get_port_mac_addr(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   uint8_t mac_addr[6]);

int dpni_enable_vlan_filter(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token,
			    int en);

/**
 * Set vlan filter queue action
 */
#define DPNI_VLAN_SET_QUEUE_ACTION 1

int dpni_add_vlan_id(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     uint16_t vlan_id,
			 uint8_t flags,
			 uint8_t tc_id,
			 uint8_t flow_id);

int dpni_remove_vlan_id(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint16_t vlan_id);

int dpni_clear_vlan_filters(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token);

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
 * @miss_action:	Miss action selection
 * @default_flow_id:	Used when 'miss_action = DPNI_FS_MISS_EXPLICIT_FLOWID'
 * @keep_hash_key: used only when miss_action is set to DPNI_FS_MISS_HASH. When
 *	set to one unclassified frames will be distributed according to previous
 *	used hash key. If set to zero hash key will be replaced with the key
 *	provided for flow steering.
 * @keep_entries: if set to one command will not delete the entries that already
 *	exist into FS table. Use this option with caution: if the table
 *	entries	are not compatible with the distribution key the packets
 *	will not be classified properly.
 */
struct dpni_fs_tbl_cfg {
	enum dpni_fs_miss_action miss_action;
	uint16_t default_flow_id;
	char keep_hash_key;
	uint8_t keep_entries;
};

/**
 * struct dpni_rx_tc_dist_cfg - Rx traffic class distribution configuration
 * @dist_size: Set the distribution size;
 *	supported values: 1,2,3,4,6,7,8,12,14,16,24,28,32,48,56,64,96,
 *	112,128,192,224,256,384,448,512,768,896,1024
 * @dist_mode: Distribution mode
 * @key_cfg_iova: I/O virtual address of 256 bytes DMA-able memory filled with
 *		the extractions to be used for the distribution key by calling
 *		dpkg_prepare_key_cfg() relevant only when
 *		'dist_mode != DPNI_DIST_MODE_NONE', otherwise it can be '0'
 * @fs_cfg: Flow Steering table configuration; only relevant if
 *		'dist_mode = DPNI_DIST_MODE_FS'
 */
struct dpni_rx_tc_dist_cfg {
	uint16_t dist_size;
	enum dpni_dist_mode dist_mode;
	uint64_t key_cfg_iova;
	struct dpni_fs_tbl_cfg fs_cfg;
};

int dpni_set_rx_tc_dist(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t tc_id,
			const struct dpni_rx_tc_dist_cfg *cfg);

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
	enum dpni_dest dest_type;
	int dest_id;
	uint8_t priority;
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
 * This congestion will trigger flow control or priority flow control. This
 * will have effect only if flow control is enabled with dpni_set_link_cfg()
 */
#define DPNI_CONG_OPT_FLOW_CONTROL	0x00000040

/**
 * enum dpni_congestion_point - Structure representing congestion point
 * @DPNI_CP_QUEUE:	Set congestion per queue, identified by QUEUE_TYPE, TC
 *			and QUEUE_INDEX
 * @DPNI_CP_GROUP:	Set congestion per queue group. Depending on options
 *			used to define the DPNI this can be either per
 *			TC (default) or per interface
 *			(DPNI_OPT_SHARED_CONGESTION set at DPNI create).
 *			QUEUE_INDEX is ignored if this type is used.
 * @DPNI_CP_CONGESTION_GROUP: Set per congestion group id. This will work
 *		only if the DPNI is created with  DPNI_OPT_CUSTOM_CG option
 */

enum dpni_congestion_point {
	DPNI_CP_QUEUE,
	DPNI_CP_GROUP,
	DPNI_CP_CONGESTION_GROUP,
};

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
 * @cg_point: Congestion point settings
 * @cgid: id of the congestion group. The index is relative to dpni.
 */

struct dpni_congestion_notification_cfg {
	enum dpni_congestion_unit units;
	uint32_t threshold_entry;
	uint32_t threshold_exit;
	uint64_t message_ctx;
	uint64_t message_iova;
	struct dpni_dest_cfg dest_cfg;
	uint16_t notification_mode;
	enum dpni_congestion_point cg_point;
	int cgid;
};

int dpni_set_congestion_notification(struct fsl_mc_io *mc_io,
				     uint32_t cmd_flags,
				     uint16_t token,
				     enum dpni_queue_type qtype,
				     uint8_t tc_id,
			const struct dpni_congestion_notification_cfg *cfg);

int dpni_get_congestion_notification(struct fsl_mc_io *mc_io,
				     uint32_t cmd_flags,
				     uint16_t token,
				     enum dpni_queue_type qtype,
				     uint8_t tc_id,
				struct dpni_congestion_notification_cfg *cfg);

/* DPNI FLC stash options */

/**
 * stashes the whole annotation area (up to 192 bytes)
 */
#define DPNI_FLC_STASH_FRAME_ANNOTATION	0x00000001

/**
 * struct dpni_queue - Queue structure
 * @destination - Destination structure
 * @destination.id: ID of the destination, only relevant if DEST_TYPE is > 0.
 *	Identifies either a DPIO or a DPCON object.
 *	Not relevant for Tx queues.
 * @destination.type:	May be one of the following:
 *	0 - No destination, queue can be manually
 *		queried, but will not push traffic or
 *		notifications to a DPIO;
 *	1 - The destination is a DPIO. When traffic
 *		becomes available in the queue a FQDAN
 *		(FQ data available notification) will be
 *		generated to selected DPIO;
 *	2 - The destination is a DPCON. The queue is
 *		associated with a DPCON object for the
 *		purpose of scheduling between multiple
 *		queues. The DPCON may be independently
 *		configured to generate notifications.
 *		Not relevant for Tx queues.
 * @destination.hold_active: Hold active, maintains a queue scheduled for longer
 *	in a DPIO during dequeue to reduce spread of traffic.
 *	Only relevant if queues are
 *	not affined to a single DPIO.
 * @user_context: User data, presented to the user along with any frames
 *	from this queue. Not relevant for Tx queues.
 * @flc: FD FLow Context structure
 * @flc.value: Default FLC value for traffic dequeued from
 *      this queue.  Please check description of FD
 *      structure for more information.
 *      Note that FLC values set using dpni_add_fs_entry,
 *      if any, take precedence over values per queue.
 * @flc.stash_control: Boolean, indicates whether the 6 lowest
 *      - significant bits are used for stash control.
 *      significant bits are used for stash control.  If set, the 6
 *      least significant bits in value are interpreted as follows:
 *      - bits 0-1: indicates the number of 64 byte units of context
 *      that are stashed.  FLC value is interpreted as a memory address
 *      in this case, excluding the 6 LS bits.
 *      - bits 2-3: indicates the number of 64 byte units of frame
 *      annotation to be stashed.  Annotation is placed at FD[ADDR].
 *      - bits 4-5: indicates the number of 64 byte units of frame
 *      data to be stashed.  Frame data is placed at FD[ADDR] +
 *      FD[OFFSET].
 *      For more details check the Frame Descriptor section in the
 *      hardware documentation.
 *@cgid :indicate the cgid to set relative to dpni
 */
struct dpni_queue {
	struct {
		uint16_t id;
		enum dpni_dest type;
		char hold_active;
		uint8_t priority;
	} destination;
	uint64_t user_context;
	struct {
		uint64_t value;
		char stash_control;
	} flc;
	int cgid;
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

int dpni_set_tx_confirmation_mode(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  enum dpni_confirmation_mode mode);

int dpni_get_tx_confirmation_mode(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  enum dpni_confirmation_mode *mode);

/**
 * struct dpni_qos_tbl_cfg - Structure representing QOS table configuration
 * @key_cfg_iova: I/O virtual address of 256 bytes DMA-able memory filled with
 *		key extractions to be used as the QoS criteria by calling
 *		dpkg_prepare_key_cfg()
 * @discard_on_miss: Set to '1' to discard frames in case of no match (miss);
 *		'0' to use the 'default_tc' in such cases
 * @keep_entries: if set to one will not delele existing table entries. This
 *		option will work properly only for dpni objects created with
 *		DPNI_OPT_HAS_KEY_MASKING option. All previous QoS entries must
 *		be compatible with new key composition rule.
 *		It is the caller's job to delete incompatible entries before
 *		executing this function.
 * @default_tc: Used in case of no-match and 'discard_on_miss'= 0
 */
struct dpni_qos_tbl_cfg {
	uint64_t key_cfg_iova;
	int discard_on_miss;
	int keep_entries;
	uint8_t default_tc;
};

int dpni_set_qos_table(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       uint16_t token,
		       const struct dpni_qos_tbl_cfg *cfg);

/**
 * struct dpni_rule_cfg - Rule configuration for table lookup
 * @key_iova: I/O virtual address of the key (must be in DMA-able memory)
 * @mask_iova: I/O virtual address of the mask (must be in DMA-able memory)
 * @key_size: key and mask size (in bytes)
 */
struct dpni_rule_cfg {
	uint64_t key_iova;
	uint64_t mask_iova;
	uint8_t key_size;
};

int dpni_add_qos_entry(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       uint16_t token,
		       const struct dpni_rule_cfg *cfg,
		       uint8_t tc_id,
		       uint16_t index,
			   uint8_t flags,
			   uint8_t flow_id);

int dpni_remove_qos_entry(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  const struct dpni_rule_cfg *cfg);

int dpni_clear_qos_table(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token);

/**
 * Discard matching traffic.  If set, this takes precedence over any other
 * configuration and matching traffic is always discarded.
 */
 #define DPNI_FS_OPT_DISCARD            0x1

/**
 * Set FLC value.  If set, flc member of truct dpni_fs_action_cfg is used to
 * override the FLC value set per queue.
 * For more details check the Frame Descriptor section in the hardware
 * documentation.
 */
#define DPNI_FS_OPT_SET_FLC            0x2

/*
 * Indicates whether the 6 lowest significant bits of FLC are used for stash
 * control.  If set, the 6 least significant bits in value are interpreted as
 * follows:
 *     - bits 0-1: indicates the number of 64 byte units of context that are
 *     stashed.  FLC value is interpreted as a memory address in this case,
 *     excluding the 6 LS bits.
 *     - bits 2-3: indicates the number of 64 byte units of frame annotation
 *     to be stashed.  Annotation is placed at FD[ADDR].
 *     - bits 4-5: indicates the number of 64 byte units of frame data to be
 *     stashed.  Frame data is placed at FD[ADDR] + FD[OFFSET].
 * This flag is ignored if DPNI_FS_OPT_SET_FLC is not specified.
 */
#define DPNI_FS_OPT_SET_STASH_CONTROL  0x4

/**
 * struct dpni_fs_action_cfg - Action configuration for table look-up
 * @flc: FLC value for traffic matching this rule.  Please check the Frame
 * Descriptor section in the hardware documentation for more information.
 * @flow_id: Identifies the Rx queue used for matching traffic.  Supported
 *     values are in range 0 to num_queue-1.
 * @options: Any combination of DPNI_FS_OPT_ values.
 */
struct dpni_fs_action_cfg {
	uint64_t flc;
	uint16_t flow_id;
	uint16_t options;
};

int dpni_add_fs_entry(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t tc_id,
		      uint16_t index,
		      const struct dpni_rule_cfg *cfg,
		      const struct dpni_fs_action_cfg *action);

int dpni_remove_fs_entry(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 uint8_t tc_id,
			 const struct dpni_rule_cfg *cfg);

int dpni_clear_fs_entries(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  uint8_t tc_id);

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

#define DPNI_QUEUE_OPT_SET_CGID				0x00000040
#define DPNI_QUEUE_OPT_CLEAR_CGID			0x00000080

int dpni_set_queue(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   enum dpni_queue_type qtype,
		   uint8_t tc,
		   uint8_t index,
		   uint8_t options,
		   const struct dpni_queue *queue);

int dpni_get_queue(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   enum dpni_queue_type qtype,
		   uint8_t tc,
		   uint8_t index,
		   struct dpni_queue *queue,
		   struct dpni_queue_id *qid);

int dpni_get_statistics(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t page,
			uint16_t param,
			union dpni_statistics *stat);

int dpni_reset_statistics(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token);

/**
 * struct dpni_taildrop - Structure representing the taildrop
 * @enable:	Indicates whether the taildrop is active or not.
 * @units:	Indicates the unit of THRESHOLD. Queue taildrop only
 *		supports byte units, this field is ignored and
 *		assumed = 0 if CONGESTION_POINT is 0.
 * @threshold:	Threshold value, in units identified by UNITS field. Value 0
 *		cannot be used as a valid taildrop threshold,
 *		THRESHOLD must be > 0 if the taildrop is
 *		enabled.
 * @oal	:	Overhead Accounting Length, a 12-bit, 2's complement value
 *		with range (-2048 to +2047) representing a fixed per-frame
 *		overhead to be added to the actual length of a frame when
 *		performing WRED and tail drop calculations and threshold
 *		comparisons.
 */
struct dpni_taildrop {
	char enable;
	enum dpni_congestion_unit units;
	uint32_t threshold;
	int16_t oal;
};

int dpni_set_taildrop(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      enum dpni_congestion_point cg_point,
		      enum dpni_queue_type q_type,
		      uint8_t tc,
		      uint8_t q_index,
		      struct dpni_taildrop *taildrop);

int dpni_get_taildrop(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      enum dpni_congestion_point cg_point,
		      enum dpni_queue_type q_type,
		      uint8_t tc,
		      uint8_t q_index,
		      struct dpni_taildrop *taildrop);

int dpni_set_opr(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token,
		 uint8_t tc,
		 uint8_t index,
		 uint8_t options,
		 struct opr_cfg *cfg);

int dpni_get_opr(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token,
		 uint8_t tc,
		 uint8_t index,
		 struct opr_cfg *cfg,
		 struct opr_qry *qry);

/**
 * When used for queue_idx in function dpni_set_rx_dist_default_queue will
 * signal to dpni to drop all unclassified frames
 */
#define DPNI_FS_MISS_DROP		((uint16_t)-1)

/**
 * struct dpni_rx_dist_cfg - distribution configuration
 * @dist_size:	distribution size; supported values: 1,2,3,4,6,7,8,
 *		12,14,16,24,28,32,48,56,64,96,112,128,192,224,256,384,448,
 *		512,768,896,1024
 * @key_cfg_iova: I/O virtual address of 256 bytes DMA-able memory filled with
 *		the extractions to be used for the distribution key by calling
 *		dpkg_prepare_key_cfg() relevant only when enable!=0 otherwise
 *		it can be '0'
 * @enable: enable/disable the distribution.
 * @tc: TC id for which distribution is set
 * @fs_miss_flow_id: when packet misses all rules from flow steering table and
 *		hash is disabled it will be put into this queue id; use
 *		DPNI_FS_MISS_DROP to drop frames. The value of this field is
 *		used only when flow steering distribution is enabled and hash
 *		distribution is disabled
 */
struct dpni_rx_dist_cfg {
	uint16_t dist_size;
	uint64_t key_cfg_iova;
	uint8_t enable;
	uint8_t tc;
	uint16_t fs_miss_flow_id;
};

int dpni_set_rx_fs_dist(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, const struct dpni_rx_dist_cfg *cfg);

int dpni_set_rx_hash_dist(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, const struct dpni_rx_dist_cfg *cfg);

int dpni_add_custom_tpid(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, uint16_t tpid);

int dpni_remove_custom_tpid(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, uint16_t tpid);

/**
 * struct dpni_custom_tpid_cfg - custom TPID configuration. Contains custom TPID
 *	values used in current dpni object to detect 802.1q frames.
 *	@tpid1: first tag. Not used if zero.
 *	@tpid2: second tag. Not used if zero.
 */
struct dpni_custom_tpid_cfg {
	uint16_t tpid1;
	uint16_t tpid2;
};

int dpni_get_custom_tpid(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, struct dpni_custom_tpid_cfg *tpid);

/**
 * enum dpni_soft_sequence_dest - Enumeration of WRIOP software sequence
 *				destinations
 * @DPNI_SS_INGRESS: Ingress parser
 * @DPNI_SS_EGRESS: Egress parser
 */
enum dpni_soft_sequence_dest {
	DPNI_SS_INGRESS = 0,
	DPNI_SS_EGRESS = 1,
};

/**
 * struct dpni_load_ss_cfg - Structure for Software Sequence load configuration
 * @dest:	Destination of the Software Sequence: ingress or egress parser
 * @ss_size: Size of the Software Sequence
 * @ss_offset:	The offset where to load the Software Sequence (0x20-0x7FD)
 * @ss_iova: I/O virtual address of the Software Sequence
 */
struct dpni_load_ss_cfg {
	enum dpni_soft_sequence_dest dest;
	uint16_t ss_size;
	uint16_t ss_offset;
	uint64_t ss_iova;
};

/**
 * struct dpni_enable_ss_cfg - Structure for software sequence enable
 *				configuration
 * @dest:	Destination of the Software Sequence: ingress or egress parser
 * @hxs: HXS to attach the software sequence to
 * @set_start: If the Software Sequence or HDR it is attached to is set as
 *		parser start
 *		If hxs=DUMMY_LAST_HXS the ss_offset is set directly as parser
 *			start else the hdr index code is set as parser start
 * @ss_offset: The offset of the Software Sequence to enable or set as parse
 *		start
 * @param_size: Size of the software sequence parameters
 * @param_offset: Offset in the parameter zone for the software sequence
 *			parameters
 * @param_iova: I/O virtual address of the parameters
 */
struct dpni_enable_ss_cfg {
	enum dpni_soft_sequence_dest dest;
	uint16_t hxs;
	uint8_t set_start;
	uint16_t ss_offset;
	uint8_t param_size;
	uint8_t param_offset;
	uint64_t param_iova;
};

/**
 * dpni_load_sw_sequence() - Loads a software sequence in parser memory.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	Software sequence load configuration
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_load_sw_sequence(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      uint16_t token,
		  struct dpni_load_ss_cfg *cfg);

/**
 * dpni_eanble_sw_sequence() - Enables a software sequence in the parser
 *				profile
 * corresponding to the ingress or egress of the DPNI.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @cfg:	Software sequence enable configuration
 * Return:	'0' on Success; Error code otherwise.
 */
int dpni_enable_sw_sequence(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token,
			    struct dpni_enable_ss_cfg *cfg);

/**
 * struct dpni_sw_sequence_layout - Structure for software sequence enable
 *				configuration
 * @num_ss:	Number of software sequences returned
 * @ss: Array of software sequence entries. The number of valid entries
 *			must match 'num_ss' value
 */
struct dpni_sw_sequence_layout {
	uint8_t num_ss;
	struct {
		uint16_t ss_offset;
		uint16_t ss_size;
		uint8_t param_offset;
		uint8_t param_size;
	} ss[DPNI_SW_SEQUENCE_LAYOUT_SIZE];
};

/**
 * dpni_get_sw_sequence_layout() - Get the soft sequence layout
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPNI object
 * @src:	Source of the layout (WRIOP Rx or Tx)
 * @ss_layout_iova:  I/O virtual address of 264 bytes DMA-able memory
 *
 * warning: After calling this function, call dpni_extract_sw_sequence_layout()
 *		to get the layout
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpni_get_sw_sequence_layout(struct fsl_mc_io *mc_io,
				uint32_t cmd_flags,
				uint16_t token,
				enum dpni_soft_sequence_dest src,
				uint64_t ss_layout_iova);

/**
 * dpni_extract_sw_sequence_layout() - extract the software sequence layout
 * @layout:		software sequence layout
 * @sw_sequence_layout_buf:	Zeroed 264 bytes of memory before mapping it
 *				to DMA
 *
 * This function has to be called after dpni_get_sw_sequence_layout
 *
 */
void dpni_extract_sw_sequence_layout(struct dpni_sw_sequence_layout *layout,
				     const uint8_t *sw_sequence_layout_buf);

#endif /* __FSL_DPNI_H */
