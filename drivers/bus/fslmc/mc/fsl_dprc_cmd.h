/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2021 NXP
 *
 */

#ifndef _FSL_DPRC_CMD_H
#define _FSL_DPRC_CMD_H

/* Minimal supported DPRC Version */
#define DPRC_VER_MAJOR			6
#define DPRC_VER_MINOR			6

/* Command versioning */
#define DPRC_CMD_BASE_VERSION			1
#define DPRC_CMD_ID_OFFSET			4

#define DPRC_CMD(id)	((id << DPRC_CMD_ID_OFFSET) | DPRC_CMD_BASE_VERSION)

/* Command IDs */
#define DPRC_CMDID_CLOSE                        DPRC_CMD(0x800)
#define DPRC_CMDID_OPEN                         DPRC_CMD(0x805)
#define DPRC_CMDID_GET_CONNECTION               DPRC_CMD(0x16C)

#pragma pack(push, 1)
struct dprc_cmd_open {
	uint32_t container_id;
};

struct dprc_cmd_get_connection {
	uint32_t ep1_id;
	uint16_t ep1_interface_id;
	uint16_t pad;

	uint8_t ep1_type[16];
};

struct dprc_rsp_get_connection {
	uint64_t pad[3];
	uint32_t ep2_id;
	uint16_t ep2_interface_id;
	uint16_t pad1;
	uint8_t ep2_type[16];
	uint32_t state;
};
#pragma pack(pop)
#endif /* _FSL_DPRC_CMD_H */
