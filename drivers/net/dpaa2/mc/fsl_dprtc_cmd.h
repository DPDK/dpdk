/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright 2019-2021 NXP
 */
#include <fsl_mc_sys.h>
#ifndef _FSL_DPRTC_CMD_H
#define _FSL_DPRTC_CMD_H

/* DPRTC Version */
#define DPRTC_VER_MAJOR			2
#define DPRTC_VER_MINOR			3

/* Command versioning */
#define DPRTC_CMD_BASE_VERSION		1
#define DPRTC_CMD_VERSION_2		2
#define DPRTC_CMD_ID_OFFSET		4

#define DPRTC_CMD(id)	(((id) << DPRTC_CMD_ID_OFFSET) | DPRTC_CMD_BASE_VERSION)
#define DPRTC_CMD_V2(id) (((id) << DPRTC_CMD_ID_OFFSET) | DPRTC_CMD_VERSION_2)

/* Command IDs */
#define DPRTC_CMDID_CLOSE			DPRTC_CMD(0x800)
#define DPRTC_CMDID_OPEN			DPRTC_CMD(0x810)
#define DPRTC_CMDID_CREATE			DPRTC_CMD(0x910)
#define DPRTC_CMDID_DESTROY			DPRTC_CMD(0x990)
#define DPRTC_CMDID_GET_API_VERSION		DPRTC_CMD(0xa10)

#define DPRTC_CMDID_ENABLE			DPRTC_CMD(0x002)
#define DPRTC_CMDID_DISABLE			DPRTC_CMD(0x003)
#define DPRTC_CMDID_GET_ATTR			DPRTC_CMD(0x004)
#define DPRTC_CMDID_RESET			DPRTC_CMD(0x005)
#define DPRTC_CMDID_IS_ENABLED			DPRTC_CMD(0x006)

#define DPRTC_CMDID_SET_CLOCK_OFFSET		DPRTC_CMD(0x1d0)
#define DPRTC_CMDID_SET_FREQ_COMPENSATION	DPRTC_CMD(0x1d1)
#define DPRTC_CMDID_GET_FREQ_COMPENSATION	DPRTC_CMD(0x1d2)
#define DPRTC_CMDID_GET_TIME			DPRTC_CMD(0x1d3)
#define DPRTC_CMDID_SET_TIME			DPRTC_CMD(0x1d4)
#define DPRTC_CMDID_SET_ALARM			DPRTC_CMD(0x1d5)
#define DPRTC_CMDID_SET_PERIODIC_PULSE		DPRTC_CMD(0x1d6)
#define DPRTC_CMDID_CLEAR_PERIODIC_PULSE	DPRTC_CMD(0x1d7)
#define DPRTC_CMDID_SET_EXT_TRIGGER		DPRTC_CMD(0x1d8)
#define DPRTC_CMDID_CLEAR_EXT_TRIGGER		DPRTC_CMD(0x1d9)
#define DPRTC_CMDID_GET_EXT_TRIGGER_TIMESTAMP	DPRTC_CMD(0x1dA)
#define DPRTC_CMDID_SET_FIPER_LOOPBACK	DPRTC_CMD(0x1dB)

/* Macros for accessing command fields smaller than 1byte */
#define DPRTC_MASK(field)        \
	GENMASK(DPRTC_##field##_SHIFT + DPRTC_##field##_SIZE - 1, \
		DPRTC_##field##_SHIFT)
#define dprtc_get_field(var, field)      \
	(((var) & DPRTC_MASK(field)) >> DPRTC_##field##_SHIFT)

#pragma pack(push, 1)
struct dprtc_cmd_open {
	uint32_t dprtc_id;
};

struct dprtc_cmd_destroy {
	uint32_t object_id;
};

#define DPRTC_ENABLE_SHIFT	0
#define DPRTC_ENABLE_SIZE	1
#define DPRTC_ENDIANNESS_SHIFT 0
#define DPRTC_ENDIANNESS_SIZE  1

struct dprtc_rsp_is_enabled {
	uint8_t en;
};

struct dprtc_rsp_get_attributes {
	uint32_t paddr;
	uint32_t id;
	uint8_t little_endian;
};

struct dprtc_cmd_set_clock_offset {
	uint64_t offset;
};

struct dprtc_get_freq_compensation {
	uint32_t freq_compensation;
};

struct dprtc_time {
	uint64_t time;
};

struct dprtc_rsp_get_api_version {
	uint16_t major;
	uint16_t minor;
};

struct dprtc_cmd_ext_trigger_timestamp {
	uint32_t pad;
	uint8_t id;
};

struct dprtc_rsp_ext_trigger_timestamp {
	uint8_t unread_valid_timestamp;
	uint8_t pad1;
	uint16_t pad2;
	uint32_t pad3;
	uint64_t timestamp;
};

struct dprtc_ext_trigger_cfg {
	uint8_t id;
	uint8_t fiper_as_input;
};
#pragma pack(pop)
#endif /* _FSL_DPRTC_CMD_H */
