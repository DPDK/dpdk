/*******************************************************************************

Copyright (c) 2013 - 2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#ifndef _FM10K_PF_H_
#define _FM10K_PF_H_

#include "fm10k_type.h"
#include "fm10k_common.h"

bool fm10k_glort_valid_pf(struct fm10k_hw *hw, u16 glort);
u16 fm10k_queues_per_pool(struct fm10k_hw *hw);
u16 fm10k_vf_queue_index(struct fm10k_hw *hw, u16 vf_idx);

enum fm10k_pf_tlv_msg_id_v1 {
	FM10K_PF_MSG_ID_TEST			= 0x000, /* msg ID reserved */
	FM10K_PF_MSG_ID_XCAST_MODES		= 0x001,
	FM10K_PF_MSG_ID_UPDATE_MAC_FWD_RULE	= 0x002,
	FM10K_PF_MSG_ID_LPORT_MAP		= 0x100,
	FM10K_PF_MSG_ID_LPORT_CREATE		= 0x200,
	FM10K_PF_MSG_ID_LPORT_DELETE		= 0x201,
	FM10K_PF_MSG_ID_CONFIG			= 0x300,
	FM10K_PF_MSG_ID_UPDATE_PVID		= 0x400,
	FM10K_PF_MSG_ID_CREATE_FLOW_TABLE	= 0x501,
	FM10K_PF_MSG_ID_DELETE_FLOW_TABLE	= 0x502,
	FM10K_PF_MSG_ID_UPDATE_FLOW		= 0x503,
	FM10K_PF_MSG_ID_DELETE_FLOW		= 0x504,
	FM10K_PF_MSG_ID_SET_FLOW_STATE		= 0x505,
	FM10K_PF_MSG_ID_GET_1588_INFO		= 0x506,
	FM10K_PF_MSG_ID_1588_TIMESTAMP		= 0x701,
	FM10K_PF_MSG_ID_TX_TIMESTAMP_MODE	= 0x702,
};

enum fm10k_pf_tlv_attr_id_v1 {
	FM10K_PF_ATTR_ID_ERR			= 0x00,
	FM10K_PF_ATTR_ID_LPORT_MAP		= 0x01,
	FM10K_PF_ATTR_ID_XCAST_MODE		= 0x02,
	FM10K_PF_ATTR_ID_MAC_UPDATE		= 0x03,
	FM10K_PF_ATTR_ID_VLAN_UPDATE		= 0x04,
	FM10K_PF_ATTR_ID_CONFIG			= 0x05,
	FM10K_PF_ATTR_ID_CREATE_FLOW_TABLE	= 0x06,
	FM10K_PF_ATTR_ID_DELETE_FLOW_TABLE	= 0x07,
	FM10K_PF_ATTR_ID_UPDATE_FLOW		= 0x08,
	FM10K_PF_ATTR_ID_FLOW_STATE		= 0x09,
	FM10K_PF_ATTR_ID_FLOW_HANDLE		= 0x0A,
	FM10K_PF_ATTR_ID_DELETE_FLOW		= 0x0B,
	FM10K_PF_ATTR_ID_PORT			= 0x0C,
	FM10K_PF_ATTR_ID_UPDATE_PVID		= 0x0D,
	FM10K_PF_ATTR_ID_1588_TIMESTAMP		= 0x10,
	FM10K_PF_ATTR_ID_TIMESTAMP_MODE_REQ	= 0x11,
	FM10K_PF_ATTR_ID_TIMESTAMP_MODE_RESP	= 0x12,
};

#define FM10K_MSG_LPORT_MAP_GLORT_SHIFT	0
#define FM10K_MSG_LPORT_MAP_GLORT_SIZE	16
#define FM10K_MSG_LPORT_MAP_MASK_SHIFT	16
#define FM10K_MSG_LPORT_MAP_MASK_SIZE	16

#define FM10K_MSG_UPDATE_PVID_GLORT_SHIFT	0
#define FM10K_MSG_UPDATE_PVID_GLORT_SIZE	16
#define FM10K_MSG_UPDATE_PVID_PVID_SHIFT	16
#define FM10K_MSG_UPDATE_PVID_PVID_SIZE		16

struct fm10k_mac_update {
	__le32	mac_lower;
	__le16	mac_upper;
	__le16	vlan;
	__le16	glort;
	u8	flags;
	u8	action;
};

struct fm10k_global_table_data {
	__le32	used;
	__le32	avail;
};

struct fm10k_swapi_error {
	__le32				status;
	struct fm10k_global_table_data	mac;
	struct fm10k_global_table_data	nexthop;
	struct fm10k_global_table_data	ffu;
};

struct fm10k_swapi_1588_timestamp {
	__le64 egress;
	__le64 ingress;
	__le16 dglort;
	__le16 sglort;
};

#define FM10K_PF_MSG_LPORT_CREATE_HANDLER(func) \
	FM10K_MSG_HANDLER(FM10K_PF_MSG_ID_LPORT_CREATE, NULL, func)
#define FM10K_PF_MSG_LPORT_DELETE_HANDLER(func) \
	FM10K_MSG_HANDLER(FM10K_PF_MSG_ID_LPORT_DELETE, NULL, func)
s32 fm10k_msg_lport_map_pf(struct fm10k_hw *, u32 **, struct fm10k_mbx_info *);
extern const struct fm10k_tlv_attr fm10k_lport_map_msg_attr[];
#define FM10K_PF_MSG_LPORT_MAP_HANDLER(func) \
	FM10K_MSG_HANDLER(FM10K_PF_MSG_ID_LPORT_MAP, \
			  fm10k_lport_map_msg_attr, func)
s32 fm10k_msg_update_pvid_pf(struct fm10k_hw *, u32 **,
			     struct fm10k_mbx_info *);
extern const struct fm10k_tlv_attr fm10k_update_pvid_msg_attr[];
#define FM10K_PF_MSG_UPDATE_PVID_HANDLER(func) \
	FM10K_MSG_HANDLER(FM10K_PF_MSG_ID_UPDATE_PVID, \
			  fm10k_update_pvid_msg_attr, func)

s32 fm10k_msg_err_pf(struct fm10k_hw *, u32 **, struct fm10k_mbx_info *);
extern const struct fm10k_tlv_attr fm10k_err_msg_attr[];
#define FM10K_PF_MSG_ERR_HANDLER(msg, func) \
	FM10K_MSG_HANDLER(FM10K_PF_MSG_ID_##msg, fm10k_err_msg_attr, func)

extern const struct fm10k_tlv_attr fm10k_1588_timestamp_msg_attr[];
#define FM10K_PF_MSG_1588_TIMESTAMP_HANDLER(func) \
	FM10K_MSG_HANDLER(FM10K_PF_MSG_ID_1588_TIMESTAMP, \
			  fm10k_1588_timestamp_msg_attr, func)

s32 fm10k_iov_msg_msix_pf(struct fm10k_hw *, u32 **, struct fm10k_mbx_info *);
s32 fm10k_iov_msg_mac_vlan_pf(struct fm10k_hw *, u32 **,
			      struct fm10k_mbx_info *);
s32 fm10k_iov_msg_lport_state_pf(struct fm10k_hw *, u32 **,
				 struct fm10k_mbx_info *);
extern const struct fm10k_msg_data fm10k_iov_msg_data_pf[];

s32 fm10k_init_ops_pf(struct fm10k_hw *hw);
#endif /* _FM10K_PF_H */
