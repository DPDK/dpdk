/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _ICE_DCF_ETHDEV_H_
#define _ICE_DCF_ETHDEV_H_

#include "base/ice_common.h"
#include "base/ice_adminq_cmd.h"
#include "base/ice_dcb.h"
#include "base/ice_sched.h"

#include "ice_ethdev.h"
#include "ice_dcf.h"

#define ICE_DCF_MAX_RINGS  1
#define DCF_NUM_MACADDR_MAX	64
#define ICE_DCF_FRAME_SIZE_MAX       9728
#define ICE_DCF_VLAN_TAG_SIZE               4
#define ICE_DCF_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + ICE_DCF_VLAN_TAG_SIZE * 2)
#define ICE_DCF_ETH_MAX_LEN (RTE_ETHER_MTU + ICE_DCF_ETH_OVERHEAD)

struct ice_dcf_queue {
	uint64_t dummy;
};

struct ice_dcf_repr_info {
	struct rte_eth_dev *vf_rep_eth_dev;
};

struct ice_dcf_adapter {
	struct ice_adapter parent; /* Must be first */
	struct ice_dcf_hw real_hw;

	bool promisc_unicast_enabled;
	bool promisc_multicast_enabled;
	uint32_t mc_addrs_num;
	struct rte_ether_addr mc_addrs[DCF_NUM_MACADDR_MAX];
	int num_reprs;
	struct ice_dcf_repr_info *repr_infos;
};

struct ice_dcf_vf_repr_param {
	struct rte_eth_dev *dcf_eth_dev;
	uint16_t switch_domain_id;
	uint16_t vf_id;
};

struct ice_dcf_vlan {
	bool port_vlan_ena;
	bool stripping_ena;

	uint16_t tpid;
	uint16_t vid;
};

struct ice_dcf_vf_repr {
	struct rte_eth_dev *dcf_eth_dev;
	struct rte_ether_addr mac_addr;
	uint16_t switch_domain_id;
	uint16_t vf_id;
	bool dcf_valid;

	struct ice_dcf_vlan outer_vlan_info; /* DCF always handle outer VLAN */
};

enum ice_dcf_devrarg {
	ICE_DCF_DEVARG_CAP,
	ICE_DCF_DEVARG_ACL,
};

extern const struct rte_tm_ops ice_dcf_tm_ops;
void ice_dcf_handle_pf_event_msg(struct ice_dcf_hw *dcf_hw,
				 uint8_t *msg, uint16_t msglen);
int ice_dcf_init_parent_adapter(struct rte_eth_dev *eth_dev);
void ice_dcf_uninit_parent_adapter(struct rte_eth_dev *eth_dev);

int ice_devargs_check(struct rte_devargs *devargs, enum ice_dcf_devrarg devarg_type);
int ice_dcf_vf_repr_init(struct rte_eth_dev *vf_rep_eth_dev, void *init_param);
int ice_dcf_vf_repr_uninit(struct rte_eth_dev *vf_rep_eth_dev);
int ice_dcf_vf_repr_init_vlan(struct rte_eth_dev *vf_rep_eth_dev);
void ice_dcf_vf_repr_stop_all(struct ice_dcf_adapter *dcf_adapter);
void ice_dcf_vf_repr_notify_all(struct ice_dcf_adapter *dcf_adapter, bool valid);
int ice_dcf_handle_vf_repr_close(struct ice_dcf_adapter *dcf_adapter, uint16_t vf_id);
bool ice_dcf_adminq_need_retry(struct ice_adapter *ad);

#endif /* _ICE_DCF_ETHDEV_H_ */
