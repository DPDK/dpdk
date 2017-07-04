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

/* VERSION info to exchange between VF and PF host. In case VF works with
 *  ND kernel driver, it reads VIRTCHNL_VERSION_MAJOR/MINOR. In
 *  case works with DPDK host, it reads version below. Then VF realize who it
 *  is talking to and use proper language to communicate.
 * */
#define I40E_DPDK_SIGNATURE     ('D' << 24 | 'P' << 16 | 'D' << 8 | 'K')
#define I40E_DPDK_VERSION_MAJOR I40E_DPDK_SIGNATURE
#define I40E_DPDK_VERSION_MINOR 0

/* Default setting on number of VSIs that VF can contain */
#define I40E_DEFAULT_VF_VSI_NUM 1

#define I40E_DPDK_OFFSET  0x100

/* DPDK pf driver specific command to VF */
enum virtchnl_ops_dpdk {
	/*
	 * Keep some gap between Linux PF commands and
	 * DPDK PF extended commands.
	 */
	I40E_VIRTCHNL_OP_CFG_VLAN_PVID = VIRTCHNL_OP_VERSION +
					 I40E_DPDK_OFFSET,
	VIRTCHNL_OP_CONFIG_VSI_QUEUES_EXT,
};

/* A structure to support extended info of a receive queue. */
struct virtchnl_rxq_ext_info {
	uint8_t crcstrip;
};

/*
 * A structure to support extended info of queue pairs, an additional field
 * is added, comparing to original 'struct virtchnl_queue_pair_info'.
 */
struct virtchnl_queue_pair_ext_info {
	/* vsi_id and queue_id should be identical for both rx and tx queues.*/
	struct virtchnl_txq_info txq;
	struct virtchnl_rxq_info rxq;
	struct virtchnl_rxq_ext_info rxq_ext;
};

/*
 * A structure to support extended info of VSI queue pairs,
 * 'struct virtchnl_queue_pair_ext_info' is used, see its original
 * of 'struct virtchnl_queue_pair_info'.
 */
struct virtchnl_vsi_queue_config_ext_info {
	uint16_t vsi_id;
	uint16_t num_queue_pairs;
	struct virtchnl_queue_pair_ext_info qpair[0];
};

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

/*
 * I40E_VIRTCHNL_OP_CFG_VLAN_PVID
 * VF sends this message to enable/disable pvid. If it's
 * enable op, needs to specify the pvid. PF returns status
 * code in retval.
 */
struct virtchnl_pvid_info {
	uint16_t vsi_id;
	struct i40e_vsi_vlan_pvid_info info;
};

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
