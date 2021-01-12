/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _ICE_DCF_ETHDEV_H_
#define _ICE_DCF_ETHDEV_H_

#include "base/ice_common.h"
#include "base/ice_adminq_cmd.h"

#include "ice_ethdev.h"
#include "ice_dcf.h"

#define ICE_DCF_MAX_RINGS  1

struct ice_dcf_queue {
	uint64_t dummy;
};

struct ice_dcf_adapter {
	struct ice_adapter parent; /* Must be first */
	struct ice_dcf_hw real_hw;
};

struct ice_dcf_vf_repr_param {
	struct ice_dcf_adapter *adapter;
	uint16_t switch_domain_id;
	uint16_t vf_id;
};

struct ice_dcf_vlan {
	uint16_t tpid;
	uint16_t vid;
};

struct ice_dcf_vf_repr {
	struct ice_dcf_adapter *dcf_adapter;
	struct rte_ether_addr mac_addr;
	uint16_t switch_domain_id;
	uint16_t vf_id;

	bool port_vlan_ena;
	struct ice_dcf_vlan port_vlan_info;
};

void ice_dcf_handle_pf_event_msg(struct ice_dcf_hw *dcf_hw,
				 uint8_t *msg, uint16_t msglen);
int ice_dcf_init_parent_adapter(struct rte_eth_dev *eth_dev);
void ice_dcf_uninit_parent_adapter(struct rte_eth_dev *eth_dev);

int ice_dcf_vf_repr_init(struct rte_eth_dev *ethdev, void *init_param);
int ice_dcf_vf_repr_uninit(struct rte_eth_dev *ethdev);

#endif /* _ICE_DCF_ETHDEV_H_ */
