/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018, 2020-2021 NXP
 */
#ifndef _FSL_DPDMAI_CMD_H
#define _FSL_DPDMAI_CMD_H

/* DPDMAI Version */
#define DPDMAI_VER_MAJOR		3
#define DPDMAI_VER_MINOR		4

/* Command versioning */
#define DPDMAI_CMD_BASE_VERSION		1
#define DPDMAI_CMD_VERSION_2		2
#define DPDMAI_CMD_VERSION_3		3
#define DPDMAI_CMD_ID_OFFSET		4

#define DPDMAI_CMD(id)	((id << DPDMAI_CMD_ID_OFFSET) | DPDMAI_CMD_BASE_VERSION)
#define DPDMAI_CMD_V2(id) ((id << DPDMAI_CMD_ID_OFFSET) | DPDMAI_CMD_VERSION_2)
#define DPDMAI_CMD_V3(id) ((id << DPDMAI_CMD_ID_OFFSET) | DPDMAI_CMD_VERSION_3)

/* Command IDs */
#define DPDMAI_CMDID_CLOSE		DPDMAI_CMD(0x800)
#define DPDMAI_CMDID_OPEN		DPDMAI_CMD(0x80E)
#define DPDMAI_CMDID_CREATE		DPDMAI_CMD_V3(0x90E)
#define DPDMAI_CMDID_DESTROY		DPDMAI_CMD(0x98E)
#define DPDMAI_CMDID_GET_API_VERSION	DPDMAI_CMD(0xa0E)

#define DPDMAI_CMDID_ENABLE		DPDMAI_CMD(0x002)
#define DPDMAI_CMDID_DISABLE		DPDMAI_CMD(0x003)
#define DPDMAI_CMDID_GET_ATTR		DPDMAI_CMD_V3(0x004)
#define DPDMAI_CMDID_RESET		DPDMAI_CMD(0x005)
#define DPDMAI_CMDID_IS_ENABLED		DPDMAI_CMD(0x006)

#define DPDMAI_CMDID_SET_RX_QUEUE	DPDMAI_CMD_V2(0x1A0)
#define DPDMAI_CMDID_GET_RX_QUEUE	DPDMAI_CMD_V2(0x1A1)
#define DPDMAI_CMDID_GET_TX_QUEUE	DPDMAI_CMD_V2(0x1A2)

/* Macros for accessing command fields smaller than 1byte */
#define DPDMAI_MASK(field)        \
	GENMASK(DPDMAI_##field##_SHIFT + DPDMAI_##field##_SIZE - 1, \
		DPDMAI_##field##_SHIFT)
#define dpdmai_set_field(var, field, val) \
	((var) |= (((val) << DPDMAI_##field##_SHIFT) & DPDMAI_MASK(field)))
#define dpdmai_get_field(var, field)      \
	(((var) & DPDMAI_MASK(field)) >> DPDMAI_##field##_SHIFT)

#pragma pack(push, 1)
struct dpdmai_cmd_open {
	uint32_t dpdmai_id;
};

struct dpdmai_cmd_create {
	uint8_t num_queues;
	uint8_t priorities[2];
	uint8_t pad;
	uint32_t options;
};

struct dpdmai_cmd_destroy {
	uint32_t dpdmai_id;
};

#define DPDMAI_ENABLE_SHIFT	0
#define DPDMAI_ENABLE_SIZE	1

struct dpdmai_rsp_is_enabled {
	/* only the LSB bit */
	uint8_t en;
};

struct dpdmai_rsp_get_attr {
	uint32_t id;
	uint8_t num_of_priorities;
	uint8_t num_of_queues;
	uint16_t pad;
	uint32_t options;
};

#define DPDMAI_DEST_TYPE_SHIFT	0
#define DPDMAI_DEST_TYPE_SIZE	4

struct dpdmai_cmd_set_rx_queue {
	uint32_t dest_id;
	uint8_t dest_priority;
	uint8_t priority;
	/* from LSB: dest_type:4 */
	uint8_t dest_type;
	uint8_t queue_idx;
	uint64_t user_ctx;
	uint32_t options;
};

struct dpdmai_cmd_get_queue {
	uint8_t pad[5];
	uint8_t priority;
	uint8_t queue_idx;
};

struct dpdmai_rsp_get_rx_queue {
	uint32_t dest_id;
	uint8_t dest_priority;
	uint8_t pad1;
	/* from LSB: dest_type:4 */
	uint8_t dest_type;
	uint8_t pad2;
	uint64_t user_ctx;
	uint32_t fqid;
};

struct dpdmai_rsp_get_tx_queue {
	uint64_t pad;
	uint32_t fqid;
};

#pragma pack(pop)
#endif /* _FSL_DPDMAI_CMD_H */
