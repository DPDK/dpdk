/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 *
 */
#ifndef _FSL_DPCI_CMD_H
#define _FSL_DPCI_CMD_H

/* DPCI Version */
#define DPCI_VER_MAJOR			3
#define DPCI_VER_MINOR			4

#define DPCI_CMD_BASE_VERSION		1
#define DPCI_CMD_BASE_VERSION_V2	2
#define DPCI_CMD_ID_OFFSET		4

#define DPCI_CMD_V1(id) ((id << DPCI_CMD_ID_OFFSET) | DPCI_CMD_BASE_VERSION)
#define DPCI_CMD_V2(id) ((id << DPCI_CMD_ID_OFFSET) | DPCI_CMD_BASE_VERSION_V2)

/* Command IDs */
#define DPCI_CMDID_CLOSE		DPCI_CMD_V1(0x800)
#define DPCI_CMDID_OPEN			DPCI_CMD_V1(0x807)
#define DPCI_CMDID_CREATE		DPCI_CMD_V2(0x907)
#define DPCI_CMDID_DESTROY		DPCI_CMD_V1(0x987)
#define DPCI_CMDID_GET_API_VERSION	DPCI_CMD_V1(0xa07)

#define DPCI_CMDID_ENABLE		DPCI_CMD_V1(0x002)
#define DPCI_CMDID_DISABLE		DPCI_CMD_V1(0x003)
#define DPCI_CMDID_GET_ATTR		DPCI_CMD_V1(0x004)
#define DPCI_CMDID_RESET		DPCI_CMD_V1(0x005)
#define DPCI_CMDID_IS_ENABLED		DPCI_CMD_V1(0x006)

#define DPCI_CMDID_SET_RX_QUEUE		DPCI_CMD_V1(0x0e0)
#define DPCI_CMDID_GET_LINK_STATE	DPCI_CMD_V1(0x0e1)
#define DPCI_CMDID_GET_PEER_ATTR	DPCI_CMD_V1(0x0e2)
#define DPCI_CMDID_GET_RX_QUEUE		DPCI_CMD_V1(0x0e3)
#define DPCI_CMDID_GET_TX_QUEUE		DPCI_CMD_V1(0x0e4)
#define DPCI_CMDID_SET_OPR		DPCI_CMD_V1(0x0e5)
#define DPCI_CMDID_GET_OPR		DPCI_CMD_V1(0x0e6)

/* Macros for accessing command fields smaller than 1byte */
#define DPCI_MASK(field)        \
	GENMASK(DPCI_##field##_SHIFT + DPCI_##field##_SIZE - 1, \
		DPCI_##field##_SHIFT)
#define dpci_set_field(var, field, val) \
	((var) |= (((val) << DPCI_##field##_SHIFT) & DPCI_MASK(field)))
#define dpci_get_field(var, field)      \
	(((var) & DPCI_MASK(field)) >> DPCI_##field##_SHIFT)

#pragma pack(push, 1)
struct dpci_cmd_open {
	uint32_t dpci_id;
};

struct dpci_cmd_create {
	uint8_t num_of_priorities;
	uint8_t pad[15];
	uint32_t options;
};

struct dpci_cmd_destroy {
	uint32_t dpci_id;
};

#define DPCI_ENABLE_SHIFT	0
#define DPCI_ENABLE_SIZE	1

struct dpci_rsp_is_enabled {
	/* only the LSB bit */
	uint8_t en;
};

struct dpci_rsp_get_attr {
	uint32_t id;
	uint16_t pad;
	uint8_t num_of_priorities;
};

struct dpci_rsp_get_peer_attr {
	uint32_t id;
	uint32_t pad;
	uint8_t num_of_priorities;
};

#define DPCI_UP_SHIFT	0
#define DPCI_UP_SIZE	1

struct dpci_rsp_get_link_state {
	/* only the LSB bit */
	uint8_t up;
};

#define DPCI_DEST_TYPE_SHIFT	0
#define DPCI_DEST_TYPE_SIZE	4
#define DPCI_ORDER_PRESERVATION_SHIFT	4
#define DPCI_ORDER_PRESERVATION_SIZE	1

struct dpci_cmd_set_rx_queue {
	uint32_t dest_id;
	uint8_t dest_priority;
	uint8_t priority;
	/* from LSB: dest_type:4 */
	uint8_t dest_type;
	uint8_t pad;
	uint64_t user_ctx;
	uint32_t options;
};

struct dpci_cmd_get_queue {
	uint8_t pad[5];
	uint8_t priority;
};

struct dpci_rsp_get_rx_queue {
	uint32_t dest_id;
	uint8_t dest_priority;
	uint8_t pad;
	/* from LSB: dest_type:4 */
	uint8_t dest_type;
	uint8_t pad1;
	uint64_t user_ctx;
	uint32_t fqid;
};

struct dpci_rsp_get_tx_queue {
	uint32_t pad;
	uint32_t fqid;
};

struct dpci_rsp_get_api_version {
	uint16_t major;
	uint16_t minor;
};

struct dpci_cmd_set_opr {
	uint16_t pad0;
	uint8_t index;
	uint8_t options;
	uint8_t pad1[7];
	uint8_t oloe;
	uint8_t oeane;
	uint8_t olws;
	uint8_t oa;
	uint8_t oprrws;
};

struct dpci_cmd_get_opr {
	uint16_t pad;
	uint8_t index;
};

#define DPCI_RIP_SHIFT		0
#define DPCI_RIP_SIZE		1
#define DPCI_OPR_ENABLE_SHIFT	1
#define DPCI_OPR_ENABLE_SIZE	1
#define DPCI_TSEQ_NLIS_SHIFT	0
#define DPCI_TSEQ_NLIS_SIZE	1
#define DPCI_HSEQ_NLIS_SHIFT	0
#define DPCI_HSEQ_NLIS_SIZE	1

struct dpci_rsp_get_opr {
	uint64_t pad0;
	/* from LSB: rip:1 enable:1 */
	uint8_t flags;
	uint16_t pad1;
	uint8_t oloe;
	uint8_t oeane;
	uint8_t olws;
	uint8_t oa;
	uint8_t oprrws;
	uint16_t nesn;
	uint16_t pad8;
	uint16_t ndsn;
	uint16_t pad2;
	uint16_t ea_tseq;
	/* only the LSB */
	uint8_t tseq_nlis;
	uint8_t pad3;
	uint16_t ea_hseq;
	/* only the LSB */
	uint8_t hseq_nlis;
	uint8_t pad4;
	uint16_t ea_hptr;
	uint16_t pad5;
	uint16_t ea_tptr;
	uint16_t pad6;
	uint16_t opr_vid;
	uint16_t pad7;
	uint16_t opr_id;
};
#pragma pack(pop)
#endif /* _FSL_DPCI_CMD_H */
