/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2017 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
