/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright (c) 2016 NXP.
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
#ifndef _FSL_DPIO_CMD_H
#define _FSL_DPIO_CMD_H

/* DPIO Version */
#define DPIO_VER_MAJOR				4
#define DPIO_VER_MINOR				2

/* Command IDs */
#define DPIO_CMDID_CLOSE                                0x8001
#define DPIO_CMDID_OPEN                                 0x8031
#define DPIO_CMDID_CREATE                               0x9031
#define DPIO_CMDID_DESTROY                              0x9831
#define DPIO_CMDID_GET_API_VERSION                      0xa031

#define DPIO_CMDID_ENABLE                               0x0021
#define DPIO_CMDID_DISABLE                              0x0031
#define DPIO_CMDID_GET_ATTR                             0x0041
#define DPIO_CMDID_RESET                                0x0051
#define DPIO_CMDID_IS_ENABLED                           0x0061

#define DPIO_CMDID_SET_STASHING_DEST                    0x1201
#define DPIO_CMDID_GET_STASHING_DEST                    0x1211
#define DPIO_CMDID_ADD_STATIC_DEQUEUE_CHANNEL           0x1221
#define DPIO_CMDID_REMOVE_STATIC_DEQUEUE_CHANNEL        0x1231

/*                cmd, param, offset, width, type, arg_name */
#define DPIO_CMD_OPEN(cmd, dpio_id) \
	MC_CMD_OP(cmd, 0, 0,  32, uint32_t,     dpio_id)

/*                cmd, param, offset, width, type, arg_name */
#define DPIO_CMD_CREATE(cmd, cfg) \
do { \
	MC_CMD_OP(cmd, 0, 16, 2,  enum dpio_channel_mode,	\
					   cfg->channel_mode);\
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t, cfg->num_priorities);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPIO_RSP_IS_ENABLED(cmd, en) \
	MC_RSP_OP(cmd, 0, 0,  1,  int,	    en)

/*                cmd, param, offset, width, type, arg_name */
#define DPIO_RSP_GET_ATTRIBUTES(cmd, attr) \
do { \
	MC_RSP_OP(cmd, 0,  0, 32, int,	    (attr)->id);\
	MC_RSP_OP(cmd, 0, 32, 16, uint16_t, (attr)->qbman_portal_id);\
	MC_RSP_OP(cmd, 0, 48,  8, uint8_t,  (attr)->num_priorities);\
	MC_RSP_OP(cmd, 0, 56,  4, enum dpio_channel_mode,\
			(attr)->channel_mode);\
	MC_RSP_OP(cmd, 1,  0, 64, uint64_t, (attr)->qbman_portal_ce_offset);\
	MC_RSP_OP(cmd, 2,  0, 64, uint64_t, (attr)->qbman_portal_ci_offset);\
	MC_RSP_OP(cmd, 3,  0, 32, uint32_t, (attr)->qbman_version);\
	MC_RSP_OP(cmd, 4,  0, 32, uint32_t, (attr)->clk);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPIO_CMD_SET_STASHING_DEST(cmd, sdest) \
	MC_CMD_OP(cmd, 0, 0,  8,  uint8_t,  sdest)

/*                cmd, param, offset, width, type, arg_name */
#define DPIO_RSP_GET_STASHING_DEST(cmd, sdest) \
	MC_RSP_OP(cmd, 0, 0,  8,  uint8_t,  sdest)

/*                cmd, param, offset, width, type, arg_name */
#define DPIO_CMD_ADD_STATIC_DEQUEUE_CHANNEL(cmd, dpcon_id) \
	MC_CMD_OP(cmd, 0, 0,  32, int,      dpcon_id)

/*                cmd, param, offset, width, type, arg_name */
#define DPIO_RSP_ADD_STATIC_DEQUEUE_CHANNEL(cmd, channel_index) \
	MC_RSP_OP(cmd, 0, 0,  8,  uint8_t,  channel_index)

/*                cmd, param, offset, width, type, arg_name */
#define DPIO_CMD_REMOVE_STATIC_DEQUEUE_CHANNEL(cmd, dpcon_id) \
	MC_CMD_OP(cmd, 0, 0,  32, int,      dpcon_id)

/*                cmd, param, offset, width, type,      arg_name */
#define DPIO_RSP_GET_API_VERSION(cmd, major, minor) \
do { \
	MC_RSP_OP(cmd, 0, 0,  16, uint16_t, major);\
	MC_RSP_OP(cmd, 0, 16, 16, uint16_t, minor);\
} while (0)

#endif /* _FSL_DPIO_CMD_H */
