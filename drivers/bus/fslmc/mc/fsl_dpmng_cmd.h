/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#ifndef __FSL_DPMNG_CMD_H
#define __FSL_DPMNG_CMD_H

/* Command versioning */
#define DPMNG_CMD_BASE_VERSION		1
#define DPMNG_CMD_ID_OFFSET		4

#define DPMNG_CMD(id)	((id << DPMNG_CMD_ID_OFFSET) | DPMNG_CMD_BASE_VERSION)

/* Command IDs */
#define DPMNG_CMDID_GET_VERSION		DPMNG_CMD(0x831)
#define DPMNG_CMDID_GET_SOC_VERSION	DPMNG_CMD(0x832)

#pragma pack(push, 1)
struct dpmng_rsp_get_version {
	uint32_t revision;
	uint32_t version_major;
	uint32_t version_minor;
};

struct dpmng_rsp_get_soc_version {
	uint32_t svr;
	uint32_t pvr;
};

#pragma pack(pop)

#endif /* __FSL_DPMNG_CMD_H */
