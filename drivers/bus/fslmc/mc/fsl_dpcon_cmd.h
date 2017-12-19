/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2017 NXP
 *
 */
#ifndef _FSL_DPCON_CMD_H
#define _FSL_DPCON_CMD_H

/* DPCON Version */
#define DPCON_VER_MAJOR			3
#define DPCON_VER_MINOR			3


/* Command versioning */
#define DPCON_CMD_BASE_VERSION		1
#define DPCON_CMD_ID_OFFSET		4

#define DPCON_CMD(id)	((id << DPCON_CMD_ID_OFFSET) | DPCON_CMD_BASE_VERSION)

/* Command IDs */
#define DPCON_CMDID_CLOSE		DPCON_CMD(0x800)
#define DPCON_CMDID_OPEN		DPCON_CMD(0x808)
#define DPCON_CMDID_CREATE		DPCON_CMD(0x908)
#define DPCON_CMDID_DESTROY		DPCON_CMD(0x988)
#define DPCON_CMDID_GET_API_VERSION	DPCON_CMD(0xa08)

#define DPCON_CMDID_ENABLE		DPCON_CMD(0x002)
#define DPCON_CMDID_DISABLE		DPCON_CMD(0x003)
#define DPCON_CMDID_GET_ATTR		DPCON_CMD(0x004)
#define DPCON_CMDID_RESET		DPCON_CMD(0x005)
#define DPCON_CMDID_IS_ENABLED		DPCON_CMD(0x006)

#define DPCON_CMDID_SET_NOTIFICATION	DPCON_CMD(0x100)

#pragma pack(push, 1)
struct dpcon_cmd_open {
	uint32_t dpcon_id;
};

struct dpcon_cmd_create {
	uint8_t num_priorities;
};

struct dpcon_cmd_destroy {
	uint32_t object_id;
};

#define DPCON_ENABLE			1

struct dpcon_rsp_is_enabled {
	uint8_t enabled;
};

struct dpcon_rsp_get_attr {
	uint32_t id;
	uint16_t qbman_ch_id;
	uint8_t num_priorities;
	uint8_t pad;
};

struct dpcon_cmd_set_notification {
	uint32_t dpio_id;
	uint8_t priority;
	uint8_t pad[3];
	uint64_t user_ctx;
};

struct dpcon_rsp_get_api_version {
	uint16_t major;
	uint16_t minor;
};

#pragma pack(pop)
#endif /* _FSL_DPCON_CMD_H */
