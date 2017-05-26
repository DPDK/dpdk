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
#ifndef _FSL_DPNI_CMD_H
#define _FSL_DPNI_CMD_H

/* DPNI Version */
#define DPNI_VER_MAJOR				7
#define DPNI_VER_MINOR				0

/* Command IDs */
#define DPNI_CMDID_OPEN                                ((0x801 << 4) | (0x1))
#define DPNI_CMDID_CLOSE                               ((0x800 << 4) | (0x1))
#define DPNI_CMDID_CREATE                              ((0x901 << 4) | (0x1))
#define DPNI_CMDID_DESTROY                             ((0x981 << 4) | (0x1))
#define DPNI_CMDID_GET_API_VERSION                     ((0xa01 << 4) | (0x1))

#define DPNI_CMDID_ENABLE                              ((0x002 << 4) | (0x1))
#define DPNI_CMDID_DISABLE                             ((0x003 << 4) | (0x1))
#define DPNI_CMDID_GET_ATTR                            ((0x004 << 4) | (0x1))
#define DPNI_CMDID_RESET                               ((0x005 << 4) | (0x1))
#define DPNI_CMDID_IS_ENABLED                          ((0x006 << 4) | (0x1))

#define DPNI_CMDID_SET_POOLS                           ((0x200 << 4) | (0x1))
#define DPNI_CMDID_SET_ERRORS_BEHAVIOR                 ((0x20B << 4) | (0x1))

#define DPNI_CMDID_GET_QDID                            ((0x210 << 4) | (0x1))
#define DPNI_CMDID_GET_LINK_STATE                      ((0x215 << 4) | (0x1))
#define DPNI_CMDID_SET_MAX_FRAME_LENGTH                ((0x216 << 4) | (0x1))
#define DPNI_CMDID_GET_MAX_FRAME_LENGTH                ((0x217 << 4) | (0x1))
#define DPNI_CMDID_SET_LINK_CFG                        ((0x21a << 4) | (0x1))

#define DPNI_CMDID_SET_MCAST_PROMISC                   ((0x220 << 4) | (0x1))
#define DPNI_CMDID_GET_MCAST_PROMISC                   ((0x221 << 4) | (0x1))
#define DPNI_CMDID_SET_UNICAST_PROMISC                 ((0x222 << 4) | (0x1))
#define DPNI_CMDID_GET_UNICAST_PROMISC                 ((0x223 << 4) | (0x1))
#define DPNI_CMDID_SET_PRIM_MAC                        ((0x224 << 4) | (0x1))
#define DPNI_CMDID_GET_PRIM_MAC                        ((0x225 << 4) | (0x1))
#define DPNI_CMDID_ADD_MAC_ADDR                        ((0x226 << 4) | (0x1))
#define DPNI_CMDID_REMOVE_MAC_ADDR                     ((0x227 << 4) | (0x1))
#define DPNI_CMDID_CLR_MAC_FILTERS                     ((0x228 << 4) | (0x1))

#define DPNI_CMDID_ENABLE_VLAN_FILTER                  ((0x230 << 4) | (0x1))
#define DPNI_CMDID_ADD_VLAN_ID                         ((0x231 << 4) | (0x1))
#define DPNI_CMDID_REMOVE_VLAN_ID                      ((0x232 << 4) | (0x1))
#define DPNI_CMDID_CLR_VLAN_FILTERS                    ((0x233 << 4) | (0x1))

#define DPNI_CMDID_SET_RX_TC_DIST                      ((0x235 << 4) | (0x1))

#define DPNI_CMDID_GET_STATISTICS                      ((0x25D << 4) | (0x1))
#define DPNI_CMDID_RESET_STATISTICS                    ((0x25E << 4) | (0x1))
#define DPNI_CMDID_GET_QUEUE                           ((0x25F << 4) | (0x1))
#define DPNI_CMDID_SET_QUEUE                           ((0x260 << 4) | (0x1))
#define DPNI_CMDID_GET_TAILDROP                        ((0x261 << 4) | (0x1))
#define DPNI_CMDID_SET_TAILDROP                        ((0x262 << 4) | (0x1))

#define DPNI_CMDID_GET_PORT_MAC_ADDR                   ((0x263 << 4) | (0x1))

#define DPNI_CMDID_GET_BUFFER_LAYOUT                   ((0x264 << 4) | (0x1))
#define DPNI_CMDID_SET_BUFFER_LAYOUT                   ((0x265 << 4) | (0x1))

#define DPNI_CMDID_SET_CONGESTION_NOTIFICATION         ((0x267 << 4) | (0x1))
#define DPNI_CMDID_GET_CONGESTION_NOTIFICATION         ((0x268 << 4) | (0x1))
#define DPNI_CMDID_GET_OFFLOAD                         ((0x26B << 4) | (0x1))
#define DPNI_CMDID_SET_OFFLOAD                         ((0x26C << 4) | (0x1))
#define DPNI_CMDID_SET_TX_CONFIRMATION_MODE            ((0x266 << 4) | (0x1))
#define DPNI_CMDID_GET_TX_CONFIRMATION_MODE            ((0x26D << 4) | (0x1))

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_OPEN(cmd, dpni_id) \
	MC_CMD_OP(cmd,	 0,	0,	32,	int,	dpni_id)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_CREATE(cmd, cfg) \
do { \
	MC_CMD_OP(cmd, 0,  0, 32, uint32_t,  (cfg)->options); \
	MC_CMD_OP(cmd, 0, 32,  8,  uint8_t,  (cfg)->num_queues); \
	MC_CMD_OP(cmd, 0, 40,  8,  uint8_t,  (cfg)->num_tcs); \
	MC_CMD_OP(cmd, 0, 48,  8,  uint8_t,  (cfg)->mac_filter_entries); \
	MC_CMD_OP(cmd, 1,  0,  8,  uint8_t,  (cfg)->vlan_filter_entries); \
	MC_CMD_OP(cmd, 1, 16,  8,  uint8_t,  (cfg)->qos_entries); \
	MC_CMD_OP(cmd, 1, 32, 16, uint16_t,  (cfg)->fs_entries); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_SET_POOLS(cmd, cfg) \
do { \
	MC_CMD_OP(cmd, 0, 0,  8,  uint8_t,  cfg->num_dpbp); \
	MC_CMD_OP(cmd, 0, 8,  1,  int,      cfg->pools[0].backup_pool); \
	MC_CMD_OP(cmd, 0, 9,  1,  int,      cfg->pools[1].backup_pool); \
	MC_CMD_OP(cmd, 0, 10, 1,  int,      cfg->pools[2].backup_pool); \
	MC_CMD_OP(cmd, 0, 11, 1,  int,      cfg->pools[3].backup_pool); \
	MC_CMD_OP(cmd, 0, 12, 1,  int,      cfg->pools[4].backup_pool); \
	MC_CMD_OP(cmd, 0, 13, 1,  int,      cfg->pools[5].backup_pool); \
	MC_CMD_OP(cmd, 0, 14, 1,  int,      cfg->pools[6].backup_pool); \
	MC_CMD_OP(cmd, 0, 15, 1,  int,      cfg->pools[7].backup_pool); \
	MC_CMD_OP(cmd, 0, 32, 32, int,      cfg->pools[0].dpbp_id); \
	MC_CMD_OP(cmd, 4, 32, 16, uint16_t, cfg->pools[0].buffer_size);\
	MC_CMD_OP(cmd, 1, 0,  32, int,      cfg->pools[1].dpbp_id); \
	MC_CMD_OP(cmd, 4, 48, 16, uint16_t, cfg->pools[1].buffer_size);\
	MC_CMD_OP(cmd, 1, 32, 32, int,      cfg->pools[2].dpbp_id); \
	MC_CMD_OP(cmd, 5, 0,  16, uint16_t, cfg->pools[2].buffer_size);\
	MC_CMD_OP(cmd, 2, 0,  32, int,      cfg->pools[3].dpbp_id); \
	MC_CMD_OP(cmd, 5, 16, 16, uint16_t, cfg->pools[3].buffer_size);\
	MC_CMD_OP(cmd, 2, 32, 32, int,      cfg->pools[4].dpbp_id); \
	MC_CMD_OP(cmd, 5, 32, 16, uint16_t, cfg->pools[4].buffer_size);\
	MC_CMD_OP(cmd, 3, 0,  32, int,      cfg->pools[5].dpbp_id); \
	MC_CMD_OP(cmd, 5, 48, 16, uint16_t, cfg->pools[5].buffer_size);\
	MC_CMD_OP(cmd, 3, 32, 32, int,      cfg->pools[6].dpbp_id); \
	MC_CMD_OP(cmd, 6, 0,  16, uint16_t, cfg->pools[6].buffer_size);\
	MC_CMD_OP(cmd, 4, 0,  32, int,      cfg->pools[7].dpbp_id); \
	MC_CMD_OP(cmd, 6, 16, 16, uint16_t, cfg->pools[7].buffer_size);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_RSP_IS_ENABLED(cmd, en) \
	MC_RSP_OP(cmd, 0, 0,  1,  int,	    en)

/* DPNI_CMD_GET_ATTR is not used, no input parameters */

#define DPNI_RSP_GET_ATTR(cmd, attr) \
do { \
	MC_RSP_OP(cmd, 0,  0, 32, uint32_t, (attr)->options); \
	MC_RSP_OP(cmd, 0, 32,  8, uint8_t,  (attr)->num_queues); \
	MC_RSP_OP(cmd, 0, 40,  8, uint8_t,  (attr)->num_tcs); \
	MC_RSP_OP(cmd, 0, 48,  8, uint8_t,  (attr)->mac_filter_entries); \
	MC_RSP_OP(cmd, 1,  0,  8, uint8_t, (attr)->vlan_filter_entries); \
	MC_RSP_OP(cmd, 1, 16,  8, uint8_t,  (attr)->qos_entries); \
	MC_RSP_OP(cmd, 1, 32, 16, uint16_t, (attr)->fs_entries); \
	MC_RSP_OP(cmd, 2,  0,  8, uint8_t,  (attr)->qos_key_size); \
	MC_RSP_OP(cmd, 2,  8,  8, uint8_t,  (attr)->fs_key_size); \
	MC_RSP_OP(cmd, 2, 16, 16, uint16_t, (attr)->wriop_version); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_SET_ERRORS_BEHAVIOR(cmd, cfg) \
do { \
	MC_CMD_OP(cmd, 0, 0,  32, uint32_t, cfg->errors); \
	MC_CMD_OP(cmd, 0, 32, 4,  enum dpni_error_action, cfg->error_action); \
	MC_CMD_OP(cmd, 0, 36, 1,  int,      cfg->set_frame_annotation); \
} while (0)

#define DPNI_CMD_GET_BUFFER_LAYOUT(cmd, qtype) \
	MC_CMD_OP(cmd, 0,  0,  8, enum dpni_queue_type, qtype)

#define DPNI_RSP_GET_BUFFER_LAYOUT(cmd, layout) \
do { \
	MC_RSP_OP(cmd, 0, 48,  1, char, (layout)->pass_timestamp); \
	MC_RSP_OP(cmd, 0, 49,  1, char, (layout)->pass_parser_result); \
	MC_RSP_OP(cmd, 0, 50,  1, char, (layout)->pass_frame_status); \
	MC_RSP_OP(cmd, 1,  0, 16, uint16_t, (layout)->private_data_size); \
	MC_RSP_OP(cmd, 1, 16, 16, uint16_t, (layout)->data_align); \
	MC_RSP_OP(cmd, 1, 32, 16, uint16_t, (layout)->data_head_room); \
	MC_RSP_OP(cmd, 1, 48, 16, uint16_t, (layout)->data_tail_room); \
} while (0)

#define DPNI_CMD_SET_BUFFER_LAYOUT(cmd, qtype, layout) \
do { \
	MC_CMD_OP(cmd, 0,  0,  8, enum dpni_queue_type, qtype); \
	MC_CMD_OP(cmd, 0, 32, 16, uint16_t, (layout)->options); \
	MC_CMD_OP(cmd, 0, 48,  1, char, (layout)->pass_timestamp); \
	MC_CMD_OP(cmd, 0, 49,  1, char, (layout)->pass_parser_result); \
	MC_CMD_OP(cmd, 0, 50,  1, char, (layout)->pass_frame_status); \
	MC_CMD_OP(cmd, 1,  0, 16, uint16_t, (layout)->private_data_size); \
	MC_CMD_OP(cmd, 1, 16, 16, uint16_t, (layout)->data_align); \
	MC_CMD_OP(cmd, 1, 32, 16, uint16_t, (layout)->data_head_room); \
	MC_CMD_OP(cmd, 1, 48, 16, uint16_t, (layout)->data_tail_room); \
} while (0)

#define DPNI_CMD_SET_OFFLOAD(cmd, type, config) \
do { \
	MC_CMD_OP(cmd, 0, 24,  8, enum dpni_offload, type); \
	MC_CMD_OP(cmd, 0, 32, 32, uint32_t, config); \
} while (0)

#define DPNI_CMD_GET_OFFLOAD(cmd, type) \
	MC_CMD_OP(cmd, 0, 24,  8, enum dpni_offload, type)

#define DPNI_RSP_GET_OFFLOAD(cmd, config) \
	MC_RSP_OP(cmd, 0, 32, 32, uint32_t, config)

#define DPNI_CMD_GET_QDID(cmd, qtype) \
	MC_CMD_OP(cmd, 0,  0,  8, enum dpni_queue_type, qtype)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_RSP_GET_QDID(cmd, qdid) \
	MC_RSP_OP(cmd, 0, 0,  16, uint16_t, qdid)


/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_GET_STATISTICS(cmd, page) \
	MC_CMD_OP(cmd, 0, 0, 8, uint8_t, page)

#define DPNI_RSP_GET_STATISTICS(cmd, stat) \
do { \
	MC_RSP_OP(cmd, 0, 0, 64, uint64_t, (stat)->raw.counter[0]); \
	MC_RSP_OP(cmd, 1, 0, 64, uint64_t, (stat)->raw.counter[1]); \
	MC_RSP_OP(cmd, 2, 0, 64, uint64_t, (stat)->raw.counter[2]); \
	MC_RSP_OP(cmd, 3, 0, 64, uint64_t, (stat)->raw.counter[3]); \
	MC_RSP_OP(cmd, 4, 0, 64, uint64_t, (stat)->raw.counter[4]); \
	MC_RSP_OP(cmd, 5, 0, 64, uint64_t, (stat)->raw.counter[5]); \
	MC_RSP_OP(cmd, 6, 0, 64, uint64_t, (stat)->raw.counter[6]); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_SET_LINK_CFG(cmd, cfg) \
do { \
	MC_CMD_OP(cmd, 1, 0,  32, uint32_t, cfg->rate);\
	MC_CMD_OP(cmd, 2, 0,  64, uint64_t, cfg->options);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_RSP_GET_LINK_STATE(cmd, state) \
do { \
	MC_RSP_OP(cmd, 0, 32,  1, int,      state->up);\
	MC_RSP_OP(cmd, 1, 0,  32, uint32_t, state->rate);\
	MC_RSP_OP(cmd, 2, 0,  64, uint64_t, state->options);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_SET_MAX_FRAME_LENGTH(cmd, max_frame_length) \
	MC_CMD_OP(cmd, 0, 0,  16, uint16_t, max_frame_length)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_RSP_GET_MAX_FRAME_LENGTH(cmd, max_frame_length) \
	MC_RSP_OP(cmd, 0, 0,  16, uint16_t, max_frame_length)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_SET_MULTICAST_PROMISC(cmd, en) \
	MC_CMD_OP(cmd, 0, 0,  1,  int,      en)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_RSP_GET_MULTICAST_PROMISC(cmd, en) \
	MC_RSP_OP(cmd, 0, 0,  1,  int,	    en)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_SET_UNICAST_PROMISC(cmd, en) \
	MC_CMD_OP(cmd, 0, 0,  1,  int,      en)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_RSP_GET_UNICAST_PROMISC(cmd, en) \
	MC_RSP_OP(cmd, 0, 0,  1,  int,	    en)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_SET_PRIMARY_MAC_ADDR(cmd, mac_addr) \
do { \
	MC_CMD_OP(cmd, 0, 16, 8,  uint8_t,  mac_addr[5]); \
	MC_CMD_OP(cmd, 0, 24, 8,  uint8_t,  mac_addr[4]); \
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  mac_addr[3]); \
	MC_CMD_OP(cmd, 0, 40, 8,  uint8_t,  mac_addr[2]); \
	MC_CMD_OP(cmd, 0, 48, 8,  uint8_t,  mac_addr[1]); \
	MC_CMD_OP(cmd, 0, 56, 8,  uint8_t,  mac_addr[0]); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_RSP_GET_PRIMARY_MAC_ADDR(cmd, mac_addr) \
do { \
	MC_RSP_OP(cmd, 0, 16, 8,  uint8_t,  mac_addr[5]); \
	MC_RSP_OP(cmd, 0, 24, 8,  uint8_t,  mac_addr[4]); \
	MC_RSP_OP(cmd, 0, 32, 8,  uint8_t,  mac_addr[3]); \
	MC_RSP_OP(cmd, 0, 40, 8,  uint8_t,  mac_addr[2]); \
	MC_RSP_OP(cmd, 0, 48, 8,  uint8_t,  mac_addr[1]); \
	MC_RSP_OP(cmd, 0, 56, 8,  uint8_t,  mac_addr[0]); \
} while (0)

#define DPNI_RSP_GET_PORT_MAC_ADDR(cmd, mac_addr) \
do { \
	MC_RSP_OP(cmd, 0, 16, 8,  uint8_t,  mac_addr[5]); \
	MC_RSP_OP(cmd, 0, 24, 8,  uint8_t,  mac_addr[4]); \
	MC_RSP_OP(cmd, 0, 32, 8,  uint8_t,  mac_addr[3]); \
	MC_RSP_OP(cmd, 0, 40, 8,  uint8_t,  mac_addr[2]); \
	MC_RSP_OP(cmd, 0, 48, 8,  uint8_t,  mac_addr[1]); \
	MC_RSP_OP(cmd, 0, 56, 8,  uint8_t,  mac_addr[0]); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_ADD_MAC_ADDR(cmd, mac_addr) \
do { \
	MC_CMD_OP(cmd, 0, 16, 8,  uint8_t,  mac_addr[5]); \
	MC_CMD_OP(cmd, 0, 24, 8,  uint8_t,  mac_addr[4]); \
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  mac_addr[3]); \
	MC_CMD_OP(cmd, 0, 40, 8,  uint8_t,  mac_addr[2]); \
	MC_CMD_OP(cmd, 0, 48, 8,  uint8_t,  mac_addr[1]); \
	MC_CMD_OP(cmd, 0, 56, 8,  uint8_t,  mac_addr[0]); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_REMOVE_MAC_ADDR(cmd, mac_addr) \
do { \
	MC_CMD_OP(cmd, 0, 16, 8,  uint8_t,  mac_addr[5]); \
	MC_CMD_OP(cmd, 0, 24, 8,  uint8_t,  mac_addr[4]); \
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  mac_addr[3]); \
	MC_CMD_OP(cmd, 0, 40, 8,  uint8_t,  mac_addr[2]); \
	MC_CMD_OP(cmd, 0, 48, 8,  uint8_t,  mac_addr[1]); \
	MC_CMD_OP(cmd, 0, 56, 8,  uint8_t,  mac_addr[0]); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_CLEAR_MAC_FILTERS(cmd, unicast, multicast) \
do { \
	MC_CMD_OP(cmd, 0, 0,  1,  int,      unicast); \
	MC_CMD_OP(cmd, 0, 1,  1,  int,      multicast); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_ENABLE_VLAN_FILTER(cmd, en) \
	MC_CMD_OP(cmd, 0, 0,  1,  int,	    en)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_ADD_VLAN_ID(cmd, vlan_id) \
	MC_CMD_OP(cmd, 0, 32, 16, uint16_t, vlan_id)

/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_REMOVE_VLAN_ID(cmd, vlan_id) \
	MC_CMD_OP(cmd, 0, 32, 16, uint16_t, vlan_id)


/*                cmd, param, offset, width, type, arg_name */
#define DPNI_CMD_SET_RX_TC_DIST(cmd, tc_id, cfg) \
do { \
	MC_CMD_OP(cmd, 0, 0,  16, uint16_t,  cfg->dist_size); \
	MC_CMD_OP(cmd, 0, 16, 8,  uint8_t,  tc_id); \
	MC_CMD_OP(cmd, 0, 24, 4,  enum dpni_dist_mode, cfg->dist_mode); \
	MC_CMD_OP(cmd, 0, 28, 4,  enum dpni_fs_miss_action, \
						  cfg->fs_cfg.miss_action); \
	MC_CMD_OP(cmd, 0, 48, 16, uint16_t, cfg->fs_cfg.default_flow_id); \
	MC_CMD_OP(cmd, 6, 0,  64, uint64_t, cfg->key_cfg_iova); \
} while (0)

#define DPNI_CMD_GET_QUEUE(cmd, qtype, tc, index) \
do { \
	MC_CMD_OP(cmd, 0,  0,  8, enum dpni_queue_type, qtype); \
	MC_CMD_OP(cmd, 0,  8,  8,  uint8_t, tc); \
	MC_CMD_OP(cmd, 0, 16,  8,  uint8_t, index); \
} while (0)

#define DPNI_RSP_GET_QUEUE(cmd, queue, queue_id) \
do { \
	MC_RSP_OP(cmd, 1,  0, 32, uint32_t, (queue)->destination.id); \
	MC_RSP_OP(cmd, 1, 48,  8, uint8_t, (queue)->destination.priority); \
	MC_RSP_OP(cmd, 1, 56,  4, enum dpni_dest, (queue)->destination.type); \
	MC_RSP_OP(cmd, 1, 62,  1, char, (queue)->flc.stash_control); \
	MC_RSP_OP(cmd, 1, 63,  1, char, (queue)->destination.hold_active); \
	MC_RSP_OP(cmd, 2,  0, 64, uint64_t, (queue)->flc.value); \
	MC_RSP_OP(cmd, 3,  0, 64, uint64_t, (queue)->user_context); \
	MC_RSP_OP(cmd, 4,  0, 32, uint32_t, (queue_id)->fqid); \
	MC_RSP_OP(cmd, 4, 32, 16, uint16_t, (queue_id)->qdbin); \
} while (0)

#define DPNI_CMD_SET_QUEUE(cmd, qtype, tc, index, options, queue) \
do { \
	MC_CMD_OP(cmd, 0,  0,  8, enum dpni_queue_type, qtype); \
	MC_CMD_OP(cmd, 0,  8,  8,  uint8_t, tc); \
	MC_CMD_OP(cmd, 0, 16,  8,  uint8_t, index); \
	MC_CMD_OP(cmd, 0, 24,  8,  uint8_t, options); \
	MC_CMD_OP(cmd, 1,  0, 32, uint32_t, (queue)->destination.id); \
	MC_CMD_OP(cmd, 1, 48,  8, uint8_t, (queue)->destination.priority); \
	MC_CMD_OP(cmd, 1, 56,  4, enum dpni_dest, (queue)->destination.type); \
	MC_CMD_OP(cmd, 1, 62,  1, char, (queue)->flc.stash_control); \
	MC_CMD_OP(cmd, 1, 63,  1, char, (queue)->destination.hold_active); \
	MC_CMD_OP(cmd, 2,  0, 64, uint64_t, (queue)->flc.value); \
	MC_CMD_OP(cmd, 3,  0, 64, uint64_t, (queue)->user_context); \
} while (0)

/*                cmd, param, offset, width, type,      arg_name */
#define DPNI_RSP_GET_API_VERSION(cmd, major, minor) \
do { \
	MC_RSP_OP(cmd, 0, 0,  16, uint16_t, major);\
	MC_RSP_OP(cmd, 0, 16, 16, uint16_t, minor);\
} while (0)

#define DPNI_CMD_GET_TAILDROP(cmd, cp, q_type, tc, q_index) \
do { \
	MC_CMD_OP(cmd, 0,  0,  8, enum dpni_congestion_point, cp); \
	MC_CMD_OP(cmd, 0,  8,  8, enum dpni_queue_type, q_type); \
	MC_CMD_OP(cmd, 0, 16,  8, uint8_t, tc); \
	MC_CMD_OP(cmd, 0, 24,  8, uint8_t, q_index); \
} while (0)

#define DPNI_RSP_GET_TAILDROP(cmd, taildrop) \
do { \
	MC_RSP_OP(cmd, 1,  0,  1, char, (taildrop)->enable); \
	MC_RSP_OP(cmd, 1, 16,  8, enum dpni_congestion_unit, \
				(taildrop)->units); \
	MC_RSP_OP(cmd, 1, 32, 32, uint32_t, (taildrop)->threshold); \
} while (0)

#define DPNI_CMD_SET_TAILDROP(cmd, cp, q_type, tc, q_index, taildrop) \
do { \
	MC_CMD_OP(cmd, 0,  0,  8, enum dpni_congestion_point, cp); \
	MC_CMD_OP(cmd, 0,  8,  8, enum dpni_queue_type, q_type); \
	MC_CMD_OP(cmd, 0, 16,  8, uint8_t, tc); \
	MC_CMD_OP(cmd, 0, 24,  8, uint8_t, q_index); \
	MC_CMD_OP(cmd, 1,  0,  1, char, (taildrop)->enable); \
	MC_CMD_OP(cmd, 1, 16,  8, enum dpni_congestion_unit, \
				(taildrop)->units); \
	MC_CMD_OP(cmd, 1, 32, 32, uint32_t, (taildrop)->threshold); \
} while (0)

#define DPNI_CMD_SET_TX_CONFIRMATION_MODE(cmd, mode) \
	MC_CMD_OP(cmd, 0, 32, 8, enum dpni_confirmation_mode, mode)

#define DPNI_RSP_GET_TX_CONFIRMATION_MODE(cmd, mode) \
	MC_RSP_OP(cmd, 0, 32, 8, enum dpni_confirmation_mode, mode)

#define DPNI_CMD_SET_CONGESTION_NOTIFICATION(cmd, qtype, tc, cfg) \
do { \
	MC_CMD_OP(cmd, 0,  0,  8, enum dpni_queue_type, qtype); \
	MC_CMD_OP(cmd, 0,  8,  8, uint8_t, tc); \
	MC_CMD_OP(cmd, 1,  0, 32, uint32_t, (cfg)->dest_cfg.dest_id); \
	MC_CMD_OP(cmd, 1, 32, 16, uint16_t, (cfg)->notification_mode); \
	MC_CMD_OP(cmd, 1, 48,  8, uint8_t, (cfg)->dest_cfg.priority); \
	MC_CMD_OP(cmd, 1, 56,  4, enum dpni_dest, (cfg)->dest_cfg.dest_type); \
	MC_CMD_OP(cmd, 1, 60,  2, enum dpni_congestion_unit, (cfg)->units); \
	MC_CMD_OP(cmd, 2,  0, 64, uint64_t, (cfg)->message_iova); \
	MC_CMD_OP(cmd, 3,  0, 64, uint64_t, (cfg)->message_ctx); \
	MC_CMD_OP(cmd, 4,  0, 32, uint32_t, (cfg)->threshold_entry); \
	MC_CMD_OP(cmd, 4, 32, 32, uint32_t, (cfg)->threshold_exit); \
} while (0)

#define DPNI_CMD_GET_CONGESTION_NOTIFICATION(cmd, qtype, tc) \
do { \
	MC_CMD_OP(cmd, 0,  0,  8, enum dpni_queue_type, qtype); \
	MC_CMD_OP(cmd, 0,  8,  8, uint8_t, tc); \
} while (0)

#define DPNI_RSP_GET_CONGESTION_NOTIFICATION(cmd, cfg) \
do { \
	MC_RSP_OP(cmd, 1,  0, 32, uint32_t, (cfg)->dest_cfg.dest_id); \
	MC_RSP_OP(cmd, 1,  0, 16, uint16_t, (cfg)->notification_mode); \
	MC_RSP_OP(cmd, 1, 48,  8, uint8_t, (cfg)->dest_cfg.priority); \
	MC_RSP_OP(cmd, 1, 56,  4, enum dpni_dest, (cfg)->dest_cfg.dest_type); \
	MC_RSP_OP(cmd, 1, 60,  2, enum dpni_congestion_unit, (cfg)->units); \
	MC_RSP_OP(cmd, 2,  0, 64, uint64_t, (cfg)->message_iova); \
	MC_RSP_OP(cmd, 3,  0, 64, uint64_t, (cfg)->message_ctx); \
	MC_RSP_OP(cmd, 4,  0, 32, uint32_t, (cfg)->threshold_entry); \
	MC_RSP_OP(cmd, 4, 32, 32, uint32_t, (cfg)->threshold_exit); \
} while (0)

#endif /* _FSL_DPNI_CMD_H */
