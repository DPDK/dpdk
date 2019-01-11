/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2018 NXP
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
 */
struct dpdmux_cfg {
	enum dpdmux_method method;
	enum dpdmux_manip manip;
	uint16_t num_ifs;
	struct {
		uint64_t options;
		uint16_t max_dmat_entries;
		uint16_t max_mc_groups;
		uint16_t max_vlan_ids;
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
 * struct dpdmux_attr - Structure representing DPDMUX attributes
 * @id: DPDMUX object ID
 * @options: Configuration options (bitmap)
 * @method: DPDMUX address table method
 * @manip: DPDMUX manipulation type
 * @num_ifs: Number of interfaces (excluding the uplink interface)
 * @mem_size: DPDMUX frame storage memory size
 */
struct dpdmux_attr {
	int id;
	uint64_t options;
	enum dpdmux_method method;
	enum dpdmux_manip manip;
	uint16_t num_ifs;
	uint16_t mem_size;
};

int dpdmux_get_attributes(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  struct dpdmux_attr *attr);

int dpdmux_set_max_frame_length(struct fsl_mc_io *mc_io,
				uint32_t cmd_flags,
				uint16_t token,
				uint16_t max_frame_length);

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
	DPDMUX_CNT_EGR_FRAME_DISCARD = 0xa
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
 * @mask_iova: DMA address of the mask used for TCAM classification
 * @key_size: size, in bytes, of the look-up value. This must match the size
 *	of the look-up key defined using dpdmux_set_custom_key, otherwise the
 *	entry will never be hit
 */
struct dpdmux_rule_cfg {
	uint64_t key_iova;
	uint64_t mask_iova;
	uint8_t key_size;
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

#endif /* __FSL_DPDMUX_H */
