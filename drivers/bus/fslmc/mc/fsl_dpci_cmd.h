/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
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
#ifndef _FSL_DPCI_CMD_H
#define _FSL_DPCI_CMD_H

/* DPCI Version */
#define DPCI_VER_MAJOR				3
#define DPCI_VER_MINOR				3

/* Command IDs */
#define DPCI_CMDID_CLOSE                             0x8001
#define DPCI_CMDID_OPEN                              0x8071
#define DPCI_CMDID_CREATE                            0x9072
#define DPCI_CMDID_DESTROY                           0x9871
#define DPCI_CMDID_GET_API_VERSION                   0xa071

#define DPCI_CMDID_ENABLE                            0x0021
#define DPCI_CMDID_DISABLE                           0x0031
#define DPCI_CMDID_GET_ATTR                          0x0041
#define DPCI_CMDID_RESET                             0x0051
#define DPCI_CMDID_IS_ENABLED                        0x0061

#define DPCI_CMDID_SET_IRQ_ENABLE                    0x0121
#define DPCI_CMDID_GET_IRQ_ENABLE                    0x0131
#define DPCI_CMDID_SET_IRQ_MASK                      0x0141
#define DPCI_CMDID_GET_IRQ_MASK                      0x0151
#define DPCI_CMDID_GET_IRQ_STATUS                    0x0161
#define DPCI_CMDID_CLEAR_IRQ_STATUS                  0x0171

#define DPCI_CMDID_SET_RX_QUEUE                      0x0e01
#define DPCI_CMDID_GET_LINK_STATE                    0x0e11
#define DPCI_CMDID_GET_PEER_ATTR                     0x0e21
#define DPCI_CMDID_GET_RX_QUEUE                      0x0e31
#define DPCI_CMDID_GET_TX_QUEUE                      0x0e41
#define DPCI_CMDID_SET_OPR                           0x0e51
#define DPCI_CMDID_GET_OPR                           0x0e61

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_CMD_OPEN(cmd, dpci_id) \
	MC_CMD_OP(cmd, 0, 0, 32, int,	    dpci_id)

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_CMD_CREATE(cmd, cfg) \
do { \
	MC_CMD_OP(cmd, 0, 0,  8,  uint8_t,  cfg->num_of_priorities);\
	MC_CMD_OP(cmd, 2, 0,  32, uint32_t, cfg->options);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_RSP_IS_ENABLED(cmd, en) \
	MC_RSP_OP(cmd, 0, 0,  1,  int,	    en)

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_RSP_GET_ATTRIBUTES(cmd, attr) \
do { \
	MC_RSP_OP(cmd, 0,  0, 32, int,     (attr)->id);\
	MC_RSP_OP(cmd, 0, 48,  8, uint8_t, (attr)->num_of_priorities);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_RSP_GET_PEER_ATTR(cmd, attr) \
do { \
	MC_RSP_OP(cmd, 0, 0,  32, int,      attr->peer_id);\
	MC_RSP_OP(cmd, 1, 0,  8,  uint8_t,  attr->num_of_priorities);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_RSP_GET_LINK_STATE(cmd, up) \
	MC_RSP_OP(cmd, 0, 0,  1,  int,	    up)

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_CMD_SET_RX_QUEUE(cmd, priority, cfg) \
do { \
	MC_CMD_OP(cmd, 0, 0,  32, int,      cfg->dest_cfg.dest_id);\
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  cfg->dest_cfg.priority);\
	MC_CMD_OP(cmd, 0, 40, 8,  uint8_t,  priority);\
	MC_CMD_OP(cmd, 0, 48, 4,  enum dpci_dest, cfg->dest_cfg.dest_type);\
	MC_CMD_OP(cmd, 1, 0,  64, uint64_t, cfg->user_ctx);\
	MC_CMD_OP(cmd, 2, 0,  32, uint32_t, cfg->options);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_CMD_GET_RX_QUEUE(cmd, priority) \
	MC_CMD_OP(cmd, 0, 40, 8,  uint8_t,  priority)

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_RSP_GET_RX_QUEUE(cmd, attr) \
do { \
	MC_RSP_OP(cmd, 0, 0,  32, int,      attr->dest_cfg.dest_id);\
	MC_RSP_OP(cmd, 0, 32, 8,  uint8_t,  attr->dest_cfg.priority);\
	MC_RSP_OP(cmd, 0, 48, 4,  enum dpci_dest, attr->dest_cfg.dest_type);\
	MC_RSP_OP(cmd, 1, 0,  8,  uint64_t,  attr->user_ctx);\
	MC_RSP_OP(cmd, 2, 0,  32, uint32_t,  attr->fqid);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_CMD_GET_TX_QUEUE(cmd, priority) \
	MC_CMD_OP(cmd, 0, 40, 8,  uint8_t,  priority)

/*                cmd, param, offset, width, type, arg_name */
#define DPCI_RSP_GET_TX_QUEUE(cmd, attr) \
	MC_RSP_OP(cmd, 0, 32, 32, uint32_t,  attr->fqid)

/*                cmd, param, offset, width, type,      arg_name */
#define DPCI_RSP_GET_API_VERSION(cmd, major, minor) \
do { \
	MC_RSP_OP(cmd, 0, 0,  16, uint16_t, major);\
	MC_RSP_OP(cmd, 0, 16, 16, uint16_t, minor);\
} while (0)

#endif /* _FSL_DPCI_CMD_H */
