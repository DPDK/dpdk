/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2017 NXP
 *
 */
#ifndef _FSL_DPBP_CMD_H
#define _FSL_DPBP_CMD_H

/* DPBP Version */
#define DPBP_VER_MAJOR				3
#define DPBP_VER_MINOR				4

/* Command versioning */
#define DPBP_CMD_BASE_VERSION			1
#define DPBP_CMD_VERSION_2			2
#define DPBP_CMD_ID_OFFSET			4

#define DPBP_CMD(id)	((id << DPBP_CMD_ID_OFFSET) | DPBP_CMD_BASE_VERSION)
#define DPBP_CMD_V2(id)	((id << DPBP_CMD_ID_OFFSET) | DPBP_CMD_VERSION_2)

/* Command IDs */
#define DPBP_CMDID_CLOSE		DPBP_CMD(0x800)
#define DPBP_CMDID_OPEN			DPBP_CMD(0x804)
#define DPBP_CMDID_CREATE		DPBP_CMD(0x904)
#define DPBP_CMDID_DESTROY		DPBP_CMD(0x984)
#define DPBP_CMDID_GET_API_VERSION	DPBP_CMD(0xa04)

#define DPBP_CMDID_ENABLE		DPBP_CMD(0x002)
#define DPBP_CMDID_DISABLE		DPBP_CMD(0x003)
#define DPBP_CMDID_GET_ATTR		DPBP_CMD(0x004)
#define DPBP_CMDID_RESET		DPBP_CMD(0x005)
#define DPBP_CMDID_IS_ENABLED		DPBP_CMD(0x006)

#define DPBP_CMDID_SET_IRQ_ENABLE	DPBP_CMD(0x012)
#define DPBP_CMDID_GET_IRQ_ENABLE	DPBP_CMD(0x013)
#define DPBP_CMDID_SET_IRQ_MASK		DPBP_CMD(0x014)
#define DPBP_CMDID_GET_IRQ_MASK		DPBP_CMD(0x015)
#define DPBP_CMDID_GET_IRQ_STATUS	DPBP_CMD(0x016)
#define DPBP_CMDID_CLEAR_IRQ_STATUS	DPBP_CMD(0x017)

#define DPBP_CMDID_SET_NOTIFICATIONS	DPBP_CMD_V2(0x1b0)
#define DPBP_CMDID_GET_NOTIFICATIONS	DPBP_CMD_V2(0x1b1)

#define DPBP_CMDID_GET_FREE_BUFFERS_NUM	DPBP_CMD(0x1b2)

#pragma pack(push, 1)
struct dpbp_cmd_open {
	uint32_t dpbp_id;
};

struct dpbp_cmd_destroy {
	uint32_t object_id;
};

#define DPBP_ENABLE			0x1

struct dpbp_rsp_is_enabled {
	uint8_t enabled;
};

struct dpbp_rsp_get_attributes {
	uint16_t pad;
	uint16_t bpid;
	uint32_t id;
};

struct dpbp_cmd_set_notifications {
	uint32_t depletion_entry;
	uint32_t depletion_exit;
	uint32_t surplus_entry;
	uint32_t surplus_exit;
	uint32_t options;
	uint16_t pad[2];
	uint64_t message_ctx;
	uint64_t message_iova;
};

struct dpbp_rsp_get_notifications {
	uint32_t depletion_entry;
	uint32_t depletion_exit;
	uint32_t surplus_entry;
	uint32_t surplus_exit;
	uint32_t options;
	uint16_t pad[2];
	uint64_t message_ctx;
	uint64_t message_iova;
};

struct dpbp_rsp_get_api_version {
	uint16_t major;
	uint16_t minor;
};

struct dpbp_rsp_get_num_free_bufs {
	uint32_t num_free_bufs;
};

#pragma pack(pop)
#endif /* _FSL_DPBP_CMD_H */
