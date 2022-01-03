/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2018-2021 NXP
 *
 */
#ifndef __FSL_DPDMUX_H
#define __FSL_DPDMUX_H

#include <fsl_net.h>

struct fsl_mc_io;

/** @addtogroup dpdmux Data Path Demux API
 * Contains API for handling DPDMUX topology and functionality
 * @{
 */

int dpdmux_open(struct fsl_mc_io *mc_io,
		uint32_t  cmd_flags,
		int  dpdmux_id,
		uint16_t  *token);

int dpdmux_close(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token);

/**
 * DPDMUX general options
 */

/**
 * Enable bridging between internal interfaces
 */
#define DPDMUX_OPT_BRIDGE_EN	0x0000000000000002ULL

/**
 * Mask support for classification
 */
#define DPDMUX_OPT_CLS_MASK_SUPPORT		0x0000000000000020ULL

/**
 * Automatic max frame length - maximum frame length for dpdmux interface will
 * be changed automatically by connected dpni objects.
 */
#define DPDMUX_OPT_AUTO_MAX_FRAME_LEN	0x0000000000000040ULL

#define DPDMUX_IRQ_INDEX_IF	0x0000
#define DPDMUX_IRQ_INDEX	0x0001

/**
 * IRQ event - Indicates that the link state changed
 */
#define DPDMUX_IRQ_EVENT_LINK_CHANGED	0x0001

/**
 * enum dpdmux_manip - DPDMUX manipulation operations
 * @DPDMUX_MANIP_NONE:	No manipulation on frames
 * @DPDMUX_MANIP_ADD_REMOVE_S_VLAN: Add S-VLAN on egress, remove it on ingress
 */
enum dpdmux_manip {
	DPDMUX_MANIP_NONE = 0x0,
	DPDMUX_MANIP_ADD_REMOVE_S_VLAN = 0x1
};

/**
 * enum dpdmux_method - DPDMUX method options
 * @DPDMUX_METHOD_NONE: no DPDMUX method
 * @DPDMUX_METHOD_C_VLAN_MAC: DPDMUX based on C-VLAN and MAC address
 * @DPDMUX_METHOD_MAC: DPDMUX based on MAC address
 * @DPDMUX_METHOD_C_VLAN: DPDMUX based on C-VLAN
 * @DPDMUX_METHOD_S_VLAN: DPDMUX based on S-VLAN
 */
enum dpdmux_method {
	DPDMUX_METHOD_NONE = 0x0,
	DPDMUX_METHOD_C_VLAN_MAC = 0x1,
	DPDMUX_METHOD_MAC = 0x2,
	DPDMUX_METHOD_C_VLAN = 0x3,
	DPDMUX_METHOD_S_VLAN = 0x4,
	DPDMUX_METHOD_CUSTOM = 0x5,
};

/**
 * struct dpdmux_cfg - DPDMUX configuration parameters
 * @method: Defines the operation method for the DPDMUX address table
 * @manip: Required manipulation operation
 * @num_ifs: Number of interfaces (excluding the uplink interface)
 * @default_if: Default interface number (different from uplink,
	maximum value num_ifs)
 * @adv: Advanced parameters; default is all zeros;
 *	use this structure to change default settings
 * @adv.options: DPDMUX options - combination of 'DPDMUX_OPT_<X>' flags.
 * @adv.max_dmat_entries: Maximum entries in DPDMUX address table
 *	0 - indicates default: 64 entries per interface.
 * @adv.max_mc_groups: Number of multicast groups in DPDMUX table
 *	0 - indicates default: 32 multicast groups.
 * @adv.max_vlan_ids: Maximum vlan ids allowed in the system -
 *	relevant only case of working in mac+vlan method.
 *	0 - indicates default 16 vlan ids.
 * @adv.mem_size: Size of the memory used for internal buffers expressed as
 * number of 256byte buffers.
 */
struct dpdmux_cfg {
	enum dpdmux_method method;
	enum dpdmux_manip manip;
	uint16_t num_ifs;
	uint16_t default_if;
	struct {
		uint64_t options;
		uint16_t max_dmat_entries;
		uint16_t max_mc_groups;
		uint16_t max_vlan_ids;
		uint16_t mem_size;
	} adv;
};

int dpdmux_create(struct fsl_mc_io *mc_io,
		  uint16_t dprc_token,
		  uint32_t cmd_flags,
		  const struct dpdmux_cfg *cfg,
		  uint32_t *obj_id);

int dpdmux_destroy(struct fsl_mc_io *mc_io,
		   uint16_t dprc_token,
		   uint32_t cmd_flags,
		   uint32_t object_id);

int dpdmux_enable(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token);

int dpdmux_disable(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token);

int dpdmux_is_enabled(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      int *en);

int dpdmux_reset(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token);

/**
 *Setting 1 DPDMUX_RESET will not reset default interface
 */
#define DPDMUX_SKIP_DEFAULT_INTERFACE	0x01
/**
 *Setting 1 DPDMUX_RESET will not reset unicast rules
 */
#define DPDMUX_SKIP_UNICAST_RULES	0x02
/**
 *Setting 1 DPDMUX_RESET will not reset multicast rules
 */
#define DPDMUX_SKIP_MULTICAST_RULES	0x04

int dpdmux_set_resetable(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  uint8_t skip_reset_flags);

int dpdmux_get_resetable(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  uint8_t *skip_reset_flags);

/**
 * struct dpdmux_attr - Structure representing DPDMUX attributes
 * @id: DPDMUX object ID
 * @options: Configuration options (bitmap)
 * @method: DPDMUX address table method
 * @manip: DPDMUX manipulation type
 * @num_ifs: Number of interfaces (excluding the uplink interface)
 * @mem_size: DPDMUX frame storage memory size
 * @default_if: Default interface number (different from uplink,
	maximum value num_ifs)
 */
struct dpdmux_attr {
	int id;
	uint64_t options;
	enum dpdmux_method method;
	enum dpdmux_manip manip;
	uint16_t num_ifs;
	uint16_t mem_size;
	uint16_t default_if;
	uint16_t max_dmat_entries;
	uint16_t max_mc_groups;
	uint16_t max_vlan_ids;
};

int dpdmux_get_attributes(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  struct dpdmux_attr *attr);

int dpdmux_set_max_frame_length(struct fsl_mc_io *mc_io,
				uint32_t cmd_flags,
				uint16_t token,
				uint16_t max_frame_length);

int dpdmux_get_max_frame_length(struct fsl_mc_io *mc_io,
				uint32_t cmd_flags,
				uint16_t token,
				uint16_t if_id,
				uint16_t *max_frame_length);

/**
 * enum dpdmux_counter_type - Counter types
 * @DPDMUX_CNT_ING_FRAME: Counts ingress frames
 * @DPDMUX_CNT_ING_BYTE: Counts ingress bytes
 * @DPDMUX_CNT_ING_FLTR_FRAME: Counts filtered ingress frames
 * @DPDMUX_CNT_ING_FRAME_DISCARD: Counts discarded ingress frames
 * @DPDMUX_CNT_ING_MCAST_FRAME: Counts ingress multicast frames
 * @DPDMUX_CNT_ING_MCAST_BYTE: Counts ingress multicast bytes
 * @DPDMUX_CNT_ING_BCAST_FRAME: Counts ingress broadcast frames
 * @DPDMUX_CNT_ING_BCAST_BYTES: Counts ingress broadcast bytes
 * @DPDMUX_CNT_EGR_FRAME: Counts egress frames
 * @DPDMUX_CNT_EGR_BYTE: Counts egress bytes
 * @DPDMUX_CNT_EGR_FRAME_DISCARD: Counts discarded egress frames
 * @DPDMUX_CNT_ING_NO_BUFFER_DISCARD: Counts ingress no buffer discard frames
 */
enum dpdmux_counter_type {
	DPDMUX_CNT_ING_FRAME = 0x0,
	DPDMUX_CNT_ING_BYTE = 0x1,
	DPDMUX_CNT_ING_FLTR_FRAME = 0x2,
	DPDMUX_CNT_ING_FRAME_DISCARD = 0x3,
	DPDMUX_CNT_ING_MCAST_FRAME = 0x4,
	DPDMUX_CNT_ING_MCAST_BYTE = 0x5,
	DPDMUX_CNT_ING_BCAST_FRAME = 0x6,
	DPDMUX_CNT_ING_BCAST_BYTES = 0x7,
	DPDMUX_CNT_EGR_FRAME = 0x8,
	DPDMUX_CNT_EGR_BYTE = 0x9,
	DPDMUX_CNT_EGR_FRAME_DISCARD = 0xa,
	DPDMUX_CNT_ING_NO_BUFFER_DISCARD = 0xb,
};

/**
 * enum dpdmux_accepted_frames_type - DPDMUX frame types
 * @DPDMUX_ADMIT_ALL: The device accepts VLAN tagged, untagged and
 *			priority-tagged frames
 * @DPDMUX_ADMIT_ONLY_VLAN_TAGGED: The device discards untagged frames or
 *				priority-tagged frames that are received on this
 *				interface
 * @DPDMUX_ADMIT_ONLY_UNTAGGED: Untagged frames or priority-tagged frames
 *				received on this interface are accepted
 */
enum dpdmux_accepted_frames_type {
	DPDMUX_ADMIT_ALL = 0,
	DPDMUX_ADMIT_ONLY_VLAN_TAGGED = 1,
	DPDMUX_ADMIT_ONLY_UNTAGGED = 2
};

/**
 * enum dpdmux_action - DPDMUX action for un-accepted frames
 * @DPDMUX_ACTION_DROP: Drop un-accepted frames
 * @DPDMUX_ACTION_REDIRECT_TO_CTRL: Redirect un-accepted frames to the
 *					control interface
 */
enum dpdmux_action {
	DPDMUX_ACTION_DROP = 0,
	DPDMUX_ACTION_REDIRECT_TO_CTRL = 1
};

/**
 * struct dpdmux_accepted_frames - Frame types configuration
 * @type: Defines ingress accepted frames
 * @unaccept_act: Defines action on frames not accepted
 */
struct dpdmux_accepted_frames {
	enum dpdmux_accepted_frames_type type;
	enum dpdmux_action unaccept_act;
};

int dpdmux_if_set_accepted_frames(struct fsl_mc_io *mc_io,
				  uint32_t cmd_flags,
				  uint16_t token,
				  uint16_t if_id,
				  const struct dpdmux_accepted_frames *cfg);

/**
 * struct dpdmux_if_attr - Structure representing frame types configuration
 * @rate: Configured interface rate (in bits per second)
 * @enabled: Indicates if interface is enabled
 * @accept_frame_type: Indicates type of accepted frames for the interface
 */
struct dpdmux_if_attr {
	uint32_t rate;
	int enabled;
	int is_default;
	enum dpdmux_accepted_frames_type accept_frame_type;
};

int dpdmux_if_get_attributes(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     uint16_t if_id,
			     struct dpdmux_if_attr *attr);

int dpdmux_if_enable(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     uint16_t if_id);

int dpdmux_if_disable(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint16_t if_id);

/**
 * struct dpdmux_l2_rule - Structure representing L2 rule
 * @mac_addr: MAC address
 * @vlan_id: VLAN ID
 */
struct dpdmux_l2_rule {
	uint8_t mac_addr[6];
	uint16_t vlan_id;
};

int dpdmux_if_remove_l2_rule(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     uint16_t if_id,
			     const struct dpdmux_l2_rule *rule);

int dpdmux_if_add_l2_rule(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  uint16_t if_id,
			  const struct dpdmux_l2_rule *rule);

int dpdmux_if_get_counter(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  uint16_t if_id,
			  enum dpdmux_counter_type counter_type,
			  uint64_t *counter);

int dpdmux_ul_reset_counters(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token);

/**
 * Enable auto-negotiation
 */
#define DPDMUX_LINK_OPT_AUTONEG		0x0000000000000001ULL
/**
 * Enable half-duplex mode
 */
#define DPDMUX_LINK_OPT_HALF_DUPLEX	0x0000000000000002ULL
/**
 * Enable pause frames
 */
#define DPDMUX_LINK_OPT_PAUSE		0x0000000000000004ULL
/**
 * Enable a-symmetric pause frames
 */
#define DPDMUX_LINK_OPT_ASYM_PAUSE	0x0000000000000008ULL

/**
 * struct dpdmux_link_cfg - Structure representing DPDMUX link configuration
 * @rate: Rate
 * @options: Mask of available options; use 'DPDMUX_LINK_OPT_<X>' values
 */
struct dpdmux_link_cfg {
	uint32_t rate;
	uint64_t options;
	uint64_t advertising;
};

int dpdmux_if_set_link_cfg(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   uint16_t if_id,
			   struct dpdmux_link_cfg *cfg);
/**
 * struct dpdmux_link_state - Structure representing DPDMUX link state
 * @rate: Rate
 * @options: Mask of available options; use 'DPDMUX_LINK_OPT_<X>' values
 * @up: 0 - down, 1 - up
 * @state_valid: Ignore/Update the state of the link
 * @supported: Speeds capability of the phy (bitmap)
 * @advertising: Speeds that are advertised for autoneg (bitmap)
 */
struct dpdmux_link_state {
	uint32_t rate;
	uint64_t options;
	int      up;
	int      state_valid;
	uint64_t supported;
	uint64_t advertising;
};

int dpdmux_if_get_link_state(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     uint16_t if_id,
			     struct dpdmux_link_state *state);

int dpdmux_if_set_default(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token,
		uint16_t if_id);

int dpdmux_if_get_default(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token,
		uint16_t *if_id);

int dpdmux_set_custom_key(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint64_t key_cfg_iova);

/**
 * struct dpdmux_rule_cfg - Custom classification rule.
 *
 * @key_iova: DMA address of buffer storing the look-up value
 * @mask_iova: DMA address of the mask used for TCAM classification. This
 *  parameter is used only if dpdmux was created using option
 *  DPDMUX_OPT_CLS_MASK_SUPPORT.
 * @key_size: size, in bytes, of the look-up value. This must match the size
 *	of the look-up key defined using dpdmux_set_custom_key, otherwise the
 *	entry will never be hit
 * @entry_index: rule index into the table. This parameter is used only when
 *  dpdmux object was created using option DPDMUX_OPT_CLS_MASK_SUPPORT. In
 *  this case the rule is masking and the current frame may be a hit for
 *  multiple rules. This parameter determines the order in which the rules
 *  will be checked (smaller entry_index first).
 */
struct dpdmux_rule_cfg {
	uint64_t key_iova;
	uint64_t mask_iova;
	uint8_t key_size;
	uint16_t entry_index;
};

/**
 * struct dpdmux_cls_action - Action to execute for frames matching the
 *	classification entry
 *
 * @dest_if: Interface to forward the frames to. Port numbering is similar to
 *	the one used to connect interfaces:
 *	- 0 is the uplink port,
 *	- all others are downlink ports.
 */
struct dpdmux_cls_action {
	uint16_t dest_if;
};

int dpdmux_add_custom_cls_entry(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token,
		struct dpdmux_rule_cfg *rule,
		struct dpdmux_cls_action *action);

int dpdmux_remove_custom_cls_entry(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token,
		struct dpdmux_rule_cfg *rule);

int dpdmux_get_api_version(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t *major_ver,
			   uint16_t *minor_ver);

/**
 * Discard bit. This bit must be used together with other bits in
 * DPDMUX_ERROR_ACTION_CONTINUE to disable discarding of frames containing
 * errors
 */
#define DPDMUX_ERROR_DISC		0x80000000
/**
 * MACSEC is enabled
 */
#define DPDMUX_ERROR_MS			0x40000000
/**
 * PTP event frame
 */
#define DPDMUX_ERROR_PTP			0x08000000
/**
 * This is a multicast frame
 */
#define DPDMUX_ERROR_MC			0x04000000
/**
 * This is a broadcast frame
 */
#define DPDMUX_ERROR_BC			0x02000000
/**
 * Invalid Key composition or key size error
 */
#define DPDMUX_ERROR_KSE			0x00040000
/**
 * Extract out of frame header
 */
#define DPDMUX_ERROR_EOFHE		0x00020000
/**
 * Maximum number of chained lookups is reached
 */
#define DPDMUX_ERROR_MNLE			0x00010000
/**
 * Invalid table ID
 */
#define DPDMUX_ERROR_TIDE			0x00008000
/**
 * Policer initialization entry error
 */
#define DPDMUX_ERROR_PIEE			0x00004000
/**
 * Frame length error
 */
#define DPDMUX_ERROR_FLE			0x00002000
/**
 * Frame physical error
 */
#define DPDMUX_ERROR_FPE			0x00001000
/**
 * Cycle limit is exceeded and frame parsing is forced to terminate early
 */
#define DPDMUX_ERROR_PTE			0x00000080
/**
 * Invalid softparse instruction is encountered
 */
#define DPDMUX_ERROR_ISP			0x00000040
/**
 * Parsing header error
 */
#define DPDMUX_ERROR_PHE			0x00000020
/*
 * Block limit is exceeded. Maximum data that can be read and parsed is 256
 * bytes.
 * Parser will set this bit if it needs more that this limit to parse.
 */
#define DPDMUX_ERROR_BLE			0x00000010
/**
 * L3 checksum validation
 */
#define DPDMUX__ERROR_L3CV			0x00000008
/**
 * L3 checksum error
 */
#define DPDMUX__ERROR_L3CE			0x00000004
/**
 * L4 checksum validation
 */
#define DPDMUX__ERROR_L4CV			0x00000002
/**
 * L4 checksum error
 */
#define DPDMUX__ERROR_L4CE			0x00000001

enum dpdmux_error_action {
	DPDMUX_ERROR_ACTION_DISCARD = 0,
	DPDMUX_ERROR_ACTION_CONTINUE = 1
};

/**
 * Configure how dpdmux interface behaves on errors
 * @errors - or'ed combination of DPDMUX_ERROR_*
 * @action - set to DPDMUX_ERROR_ACTION_DISCARD or DPDMUX_ERROR_ACTION_CONTINUE
 */
struct dpdmux_error_cfg {
	uint32_t errors;
	enum dpdmux_error_action error_action;
};

int dpdmux_if_set_errors_behavior(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		uint16_t token, uint16_t if_id, struct dpdmux_error_cfg *cfg);

#endif /* __FSL_DPDMUX_H */
