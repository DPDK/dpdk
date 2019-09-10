/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2019 NXP
 *
 */
#ifndef _FSL_DPIO_CMD_H
#define _FSL_DPIO_CMD_H

/* DPIO Version */
#define DPIO_VER_MAJOR			4
#define DPIO_VER_MINOR			3

#define DPIO_CMD_BASE_VERSION		1
#define DPIO_CMD_ID_OFFSET		4

#define DPIO_CMD(id)	(((id) << DPIO_CMD_ID_OFFSET) | DPIO_CMD_BASE_VERSION)

/* Command IDs */
#define DPIO_CMDID_CLOSE				DPIO_CMD(0x800)
#define DPIO_CMDID_OPEN					DPIO_CMD(0x803)
#define DPIO_CMDID_CREATE				DPIO_CMD(0x903)
#define DPIO_CMDID_DESTROY				DPIO_CMD(0x983)
#define DPIO_CMDID_GET_API_VERSION			DPIO_CMD(0xa03)

#define DPIO_CMDID_ENABLE				DPIO_CMD(0x002)
#define DPIO_CMDID_DISABLE				DPIO_CMD(0x003)
#define DPIO_CMDID_GET_ATTR				DPIO_CMD(0x004)
#define DPIO_CMDID_RESET				DPIO_CMD(0x005)
#define DPIO_CMDID_IS_ENABLED				DPIO_CMD(0x006)

#define DPIO_CMDID_SET_IRQ_ENABLE			DPIO_CMD(0x012)
#define DPIO_CMDID_GET_IRQ_ENABLE			DPIO_CMD(0x013)
#define DPIO_CMDID_SET_IRQ_MASK				DPIO_CMD(0x014)
#define DPIO_CMDID_GET_IRQ_MASK				DPIO_CMD(0x015)
#define DPIO_CMDID_GET_IRQ_STATUS			DPIO_CMD(0x016)
#define DPIO_CMDID_CLEAR_IRQ_STATUS			DPIO_CMD(0x017)

#define DPIO_CMDID_SET_STASHING_DEST			DPIO_CMD(0x120)
#define DPIO_CMDID_GET_STASHING_DEST			DPIO_CMD(0x121)
#define DPIO_CMDID_ADD_STATIC_DEQUEUE_CHANNEL		DPIO_CMD(0x122)
#define DPIO_CMDID_REMOVE_STATIC_DEQUEUE_CHANNEL	DPIO_CMD(0x123)

/* Macros for accessing command fields smaller than 1byte */
#define DPIO_MASK(field)        \
	GENMASK(DPIO_##field##_SHIFT + DPIO_##field##_SIZE - 1, \
		DPIO_##field##_SHIFT)
#define dpio_set_field(var, field, val) \
	((var) |= (((val) << DPIO_##field##_SHIFT) & DPIO_MASK(field)))
#define dpio_get_field(var, field)      \
	(((var) & DPIO_MASK(field)) >> DPIO_##field##_SHIFT)

#pragma pack(push, 1)
struct dpio_cmd_open {
	uint32_t dpio_id;
};

#define DPIO_CHANNEL_MODE_SHIFT		0
#define DPIO_CHANNEL_MODE_SIZE		2

struct dpio_cmd_create {
	uint16_t pad1;
	/* from LSB: channel_mode:2 */
	uint8_t channel_mode;
	uint8_t pad2;
	uint8_t num_priorities;
};

struct dpio_cmd_destroy {
	uint32_t dpio_id;
};

#define DPIO_ENABLE_SHIFT	0
#define DPIO_ENABLE_SIZE	1

struct dpio_rsp_is_enabled {
	/* only the LSB */
	uint8_t en;
};

#define DPIO_ATTR_CHANNEL_MODE_SHIFT	0
#define DPIO_ATTR_CHANNEL_MODE_SIZE	4

struct dpio_rsp_get_attr {
	uint32_t id;
	uint16_t qbman_portal_id;
	uint8_t num_priorities;
	/* from LSB: channel_mode:4 */
	uint8_t channel_mode;
	uint64_t qbman_portal_ce_offset;
	uint64_t qbman_portal_ci_offset;
	uint32_t qbman_version;
	uint32_t pad;
	uint32_t clk;
};

struct dpio_stashing_dest {
	uint8_t sdest;
};

struct dpio_cmd_static_dequeue_channel {
	uint32_t dpcon_id;
};

struct dpio_rsp_add_static_dequeue_channel {
	uint8_t channel_index;
};

struct dpio_rsp_get_api_version {
	uint16_t major;
	uint16_t minor;
};

#pragma pack(pop)
#endif /* _FSL_DPIO_CMD_H */
