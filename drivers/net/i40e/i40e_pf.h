/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
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

/* Default setting on number of VSIs that VF can contain */
#define I40E_DEFAULT_VF_VSI_NUM 1

struct virtchnl_vlan_offload_info {
	uint16_t vsi_id;
	uint8_t enable_vlan_strip;
	uint8_t reserved;
};

/*
 * Macro to calculate the memory size for configuring VSI queues
 * via virtual channel.
 */
#define I40E_VIRTCHNL_CONFIG_VSI_QUEUES_SIZE(x, n) \
	(sizeof(*(x)) + sizeof((x)->qpair[0]) * (n))

int i40e_pf_host_vf_reset(struct i40e_pf_vf *vf, bool do_hw_reset);
void i40e_pf_host_handle_vf_msg(struct rte_eth_dev *dev,
				uint16_t abs_vf_id, uint32_t opcode,
				uint32_t retval,
				uint8_t *msg, uint16_t msglen);
int i40e_pf_host_init(struct rte_eth_dev *dev);
int i40e_pf_host_uninit(struct rte_eth_dev *dev);
void i40e_notify_vf_link_status(struct rte_eth_dev *dev,
				struct i40e_pf_vf *vf);

#endif /* _I40E_PF_H_ */
