/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _I40E_PF_H_
#define _I40E_PF_H_

/* VERSION info to exchange between VF and PF host. In case VF works with
 *  ND kernel driver, it reads I40E_VIRTCHNL_VERSION_MAJOR/MINOR. In
 *  case works with DPDK host, it reads version below. Then VF realize who it
 *  is talking to and use proper language to communicate.
 * */
#define I40E_DPDK_SIGNATURE     ('D' << 24 | 'P' << 16 | 'D' << 8 | 'K')
#define I40E_DPDK_VERSION_MAJOR I40E_DPDK_SIGNATURE
#define I40E_DPDK_VERSION_MINOR 0

/* Default setting on number of VSIs that VF can contain */
#define I40E_DEFAULT_VF_VSI_NUM 1

enum i40e_pf_vfr_state {
	I40E_PF_VFR_INPROGRESS = 0,
	I40E_PF_VFR_COMPLETED = 1,
};

/* DPDK pf driver specific command to VF */
enum i40e_virtchnl_ops_DPDK {
	/* Keep some gap between Linu PF commands and DPDK PF specific commands */
	I40E_VIRTCHNL_OP_GET_LINK_STAT = I40E_VIRTCHNL_OP_EVENT + 0x100,
	I40E_VIRTCHNL_OP_CFG_VLAN_OFFLOAD,
	I40E_VIRTCHNL_OP_CFG_VLAN_PVID,
};
struct i40e_virtchnl_vlan_offload_info {
	uint16_t vsi_id;
	uint8_t enable_vlan_strip;
	uint8_t reserved;
};

/* I40E_VIRTCHNL_OP_CFG_VLAN_PVID
 * VF sends this message to enable/disable pvid. If it's enable op, needs to specify the
 * pvid.
 * PF returns status code in retval.
 */
struct i40e_virtchnl_pvid_info {
	uint16_t vsi_id;
	struct i40e_vsi_vlan_pvid_info info;
};

int i40e_pf_host_vf_reset(struct i40e_pf_vf *vf, bool do_hw_reset);
void i40e_pf_host_handle_vf_msg(struct rte_eth_dev *dev,
				uint16_t abs_vf_id, uint32_t opcode,
				__rte_unused uint32_t retval,
				uint8_t *msg, uint16_t msglen);
int i40e_pf_host_init(struct rte_eth_dev *dev);

#endif /* _I40E_PF_H_ */
