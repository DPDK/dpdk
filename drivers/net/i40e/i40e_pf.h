/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _I40E_PF_H_
#define _I40E_PF_H_

/* Default setting on number of VSIs that VF can contain */
#define I40E_DEFAULT_VF_VSI_NUM 1

#define I40E_VIRTCHNL_OFFLOAD_CAPS ( \
	VIRTCHNL_VF_OFFLOAD_L2 | \
	VIRTCHNL_VF_OFFLOAD_VLAN | \
	VIRTCHNL_VF_OFFLOAD_RSS_PF | \
	VIRTCHNL_VF_OFFLOAD_RX_POLLING)

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
