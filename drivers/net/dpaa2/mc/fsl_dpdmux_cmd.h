/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2018 NXP
 *
 */
#ifndef _FSL_DPDMUX_CMD_H
#define _FSL_DPDMUX_CMD_H

/* DPDMUX Version */
#define DPDMUX_VER_MAJOR		6
#define DPDMUX_VER_MINOR		3

#define DPDMUX_CMD_BASE_VERSION		1
#define DPDMUX_CMD_VERSION_2		2
#define DPDMUX_CMD_ID_OFFSET		4

#define DPDMUX_CMD(id)	(((id) << DPDMUX_CMD_ID_OFFSET) |\
				DPDMUX_CMD_BASE_VERSION)
#define DPDMUX_CMD_V2(id) (((id) << DPDMUX_CMD_ID_OFFSET) | \
				DPDMUX_CMD_VERSION_2)

/* Command IDs */
#define DPDMUX_CMDID_CLOSE			DPDMUX_CMD(0x800)
#define DPDMUX_CMDID_OPEN			DPDMUX_CMD(0x806)
#define DPDMUX_CMDID_CREATE			DPDMUX_CMD(0x906)
#define DPDMUX_CMDID_DESTROY			DPDMUX_CMD(0x986)
#define DPDMUX_CMDID_GET_API_VERSION		DPDMUX_CMD(0xa06)

#define DPDMUX_CMDID_ENABLE			DPDMUX_CMD(0x002)
#define DPDMUX_CMDID_DISABLE			DPDMUX_CMD(0x003)
#define DPDMUX_CMDID_GET_ATTR			DPDMUX_CMD(0x004)
#define DPDMUX_CMDID_RESET			DPDMUX_CMD(0x005)
#define DPDMUX_CMDID_IS_ENABLED			DPDMUX_CMD(0x006)

#define DPDMUX_CMDID_SET_MAX_FRAME_LENGTH	DPDMUX_CMD(0x0a1)

#define DPDMUX_CMDID_UL_RESET_COUNTERS		DPDMUX_CMD(0x0a3)

#define DPDMUX_CMDID_IF_SET_ACCEPTED_FRAMES	DPDMUX_CMD(0x0a7)
#define DPDMUX_CMDID_IF_GET_ATTR		DPDMUX_CMD(0x0a8)
#define DPDMUX_CMDID_IF_ENABLE			DPDMUX_CMD(0x0a9)
#define DPDMUX_CMDID_IF_DISABLE			DPDMUX_CMD(0x0aa)

#define DPDMUX_CMDID_IF_ADD_L2_RULE		DPDMUX_CMD(0x0b0)
#define DPDMUX_CMDID_IF_REMOVE_L2_RULE		DPDMUX_CMD(0x0b1)
#define DPDMUX_CMDID_IF_GET_COUNTER		DPDMUX_CMD(0x0b2)
#define DPDMUX_CMDID_IF_SET_LINK_CFG		DPDMUX_CMD_V2(0x0b3)
#define DPDMUX_CMDID_IF_GET_LINK_STATE		DPDMUX_CMD_V2(0x0b4)

#define DPDMUX_CMDID_SET_CUSTOM_KEY		DPDMUX_CMD(0x0b5)
#define DPDMUX_CMDID_ADD_CUSTOM_CLS_ENTRY	DPDMUX_CMD(0x0b6)
#define DPDMUX_CMDID_REMOVE_CUSTOM_CLS_ENTRY	DPDMUX_CMD(0x0b7)

#define DPDMUX_CMDID_IF_SET_DEFAULT		DPDMUX_CMD(0x0b8)
#define DPDMUX_CMDID_IF_GET_DEFAULT		DPDMUX_CMD(0x0b9)

#define DPDMUX_MASK(field)        \
	GENMASK(DPDMUX_##field##_SHIFT + DPDMUX_##field##_SIZE - 1, \
		DPDMUX_##field##_SHIFT)
#define dpdmux_set_field(var, field, val) \
	((var) |= (((val) << DPDMUX_##field##_SHIFT) & DPDMUX_MASK(field)))
#define dpdmux_get_field(var, field)      \
	(((var) & DPDMUX_MASK(field)) >> DPDMUX_##field##_SHIFT)

#pragma pack(push, 1)
struct dpdmux_cmd_open {
	uint32_t dpdmux_id;
};

struct dpdmux_cmd_create {
	uint8_t method;
	uint8_t manip;
	uint16_t num_ifs;
	uint32_t pad;

	uint16_t adv_max_dmat_entries;
	uint16_t adv_max_mc_groups;
	uint16_t adv_max_vlan_ids;
	uint16_t pad1;

	uint64_t options;
};

struct dpdmux_cmd_destroy {
	uint32_t dpdmux_id;
};

#define DPDMUX_ENABLE_SHIFT	0
#define DPDMUX_ENABLE_SIZE	1
#define DPDMUX_IS_DEFAULT_SHIFT		1
#define DPDMUX_IS_DEFAULT_SIZE		1

struct dpdmux_rsp_is_enabled {
	uint8_t en;
};

struct dpdmux_rsp_get_attr {
	uint8_t method;
	uint8_t manip;
	uint16_t num_ifs;
	uint16_t mem_size;
	uint16_t pad;

	uint64_t pad1;

	uint32_t id;
	uint32_t pad2;

	uint64_t options;
};

struct dpdmux_cmd_set_max_frame_length {
	uint16_t max_frame_length;
};

#define DPDMUX_ACCEPTED_FRAMES_TYPE_SHIFT	0
#define DPDMUX_ACCEPTED_FRAMES_TYPE_SIZE	4
#define DPDMUX_UNACCEPTED_FRAMES_ACTION_SHIFT	4
#define DPDMUX_UNACCEPTED_FRAMES_ACTION_SIZE	4

struct dpdmux_cmd_if_set_accepted_frames {
	uint16_t if_id;
	uint8_t frames_options;
};

struct dpdmux_cmd_if {
	uint16_t if_id;
};

struct dpdmux_rsp_if_get_attr {
	uint8_t pad[3];
	uint8_t enabled;
	uint8_t pad1[3];
	uint8_t accepted_frames_type;
	uint32_t rate;
};

struct dpdmux_cmd_if_l2_rule {
	uint16_t if_id;
	uint8_t mac_addr5;
	uint8_t mac_addr4;
	uint8_t mac_addr3;
	uint8_t mac_addr2;
	uint8_t mac_addr1;
	uint8_t mac_addr0;

	uint32_t pad;
	uint16_t vlan_id;
};

struct dpdmux_cmd_if_get_counter {
	uint16_t if_id;
	uint8_t counter_type;
};

struct dpdmux_rsp_if_get_counter {
	uint64_t pad;
	uint64_t counter;
};

struct dpdmux_cmd_if_set_link_cfg {
	uint16_t if_id;
	uint16_t pad[3];

	uint32_t rate;
	uint32_t pad1;

	uint64_t options;
	uint64_t advertising;
};

struct dpdmux_cmd_if_get_link_state {
	uint16_t if_id;
};

#define DPDMUX_UP_SHIFT				0
#define DPDMUX_UP_SIZE				1
#define DPDMUX_STATE_VALID_SHIFT	1
#define DPDMUX_STATE_VALID_SIZE		1
struct dpdmux_rsp_if_get_link_state {
	uint32_t pad;
	uint8_t up;
	uint8_t pad1[3];

	uint32_t rate;
	uint32_t pad2;

	uint64_t options;
	uint64_t supported;
	uint64_t advertising;
};

struct dpdmux_rsp_get_api_version {
	uint16_t major;
	uint16_t minor;
};

struct dpdmux_set_custom_key {
	uint64_t pad[6];
	uint64_t key_cfg_iova;
};

struct dpdmux_cmd_add_custom_cls_entry {
	uint8_t pad[3];
	uint8_t key_size;
	uint16_t pad1;
	uint16_t dest_if;
	uint64_t key_iova;
	uint64_t mask_iova;
};

struct dpdmux_cmd_remove_custom_cls_entry {
	uint8_t pad[3];
	uint8_t key_size;
	uint32_t pad1;
	uint64_t key_iova;
	uint64_t mask_iova;
};
#pragma pack(pop)
#endif /* _FSL_DPDMUX_CMD_H */
