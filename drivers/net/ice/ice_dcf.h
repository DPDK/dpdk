/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _ICE_DCF_H_
#define _ICE_DCF_H_

#include <ethdev_driver.h>
#include <rte_tm_driver.h>

#include <iavf_prototype.h>
#include <iavf_adminq_cmd.h>
#include <iavf_type.h>

#include "base/ice_type.h"
#include "ice_logs.h"

/* ICE_DCF_DEV_PRIVATE_TO */
#define ICE_DCF_DEV_PRIVATE_TO_ADAPTER(adapter) \
	((struct ice_dcf_adapter *)adapter)
#define ICE_DCF_DEV_PRIVATE_TO_VF(adapter) \
	(&((struct ice_dcf_adapter *)adapter)->vf)

struct dcf_virtchnl_cmd {
	TAILQ_ENTRY(dcf_virtchnl_cmd) next;

	enum virtchnl_ops v_op;
	enum iavf_status v_ret;

	uint16_t req_msglen;
	uint8_t *req_msg;

	uint16_t rsp_msglen;
	uint16_t rsp_buflen;
	uint8_t *rsp_msgbuf;

	volatile int pending;
};

struct ice_dcf_tm_shaper_profile {
	TAILQ_ENTRY(ice_dcf_tm_shaper_profile) node;
	uint32_t shaper_profile_id;
	uint32_t reference_count;
	struct rte_tm_shaper_params profile;
};

TAILQ_HEAD(ice_dcf_shaper_profile_list, ice_dcf_tm_shaper_profile);

/* Struct to store Traffic Manager node configuration. */
struct ice_dcf_tm_node {
	TAILQ_ENTRY(ice_dcf_tm_node) node;
	uint32_t id;
	uint32_t tc;
	uint32_t priority;
	uint32_t weight;
	uint32_t reference_count;
	struct ice_dcf_tm_node *parent;
	struct ice_dcf_tm_shaper_profile *shaper_profile;
	struct rte_tm_node_params params;
};

TAILQ_HEAD(ice_dcf_tm_node_list, ice_dcf_tm_node);

/* node type of Traffic Manager */
enum ice_dcf_tm_node_type {
	ICE_DCF_TM_NODE_TYPE_PORT,
	ICE_DCF_TM_NODE_TYPE_TC,
	ICE_DCF_TM_NODE_TYPE_VSI,
	ICE_DCF_TM_NODE_TYPE_MAX,
};

/* Struct to store all the Traffic Manager configuration. */
struct ice_dcf_tm_conf {
	struct ice_dcf_shaper_profile_list shaper_profile_list;
	struct ice_dcf_tm_node *root; /* root node - port */
	struct ice_dcf_tm_node_list tc_list; /* node list for all the TCs */
	struct ice_dcf_tm_node_list vsi_list; /* node list for all the queues */
	uint32_t nb_tc_node;
	uint32_t nb_vsi_node;
	bool committed;
};

struct ice_dcf_eth_stats {
	u64 rx_bytes;			/* gorc */
	u64 rx_unicast;			/* uprc */
	u64 rx_multicast;		/* mprc */
	u64 rx_broadcast;		/* bprc */
	u64 rx_discards;		/* rdpc */
	u64 rx_unknown_protocol;	/* rupp */
	u64 tx_bytes;			/* gotc */
	u64 tx_unicast;			/* uptc */
	u64 tx_multicast;		/* mptc */
	u64 tx_broadcast;		/* bptc */
	u64 tx_discards;		/* tdpc */
	u64 tx_errors;			/* tepc */
	u64 rx_no_desc;			/* repc */
	u64 rx_errors;			/* repc */
};
struct ice_dcf_hw {
	struct iavf_hw avf;

	rte_spinlock_t vc_cmd_send_lock;
	rte_spinlock_t vc_cmd_queue_lock;
	TAILQ_HEAD(, dcf_virtchnl_cmd) vc_cmd_queue;
	void (*vc_event_msg_cb)(struct ice_dcf_hw *dcf_hw,
				uint8_t *msg, uint16_t msglen);

	int vsi_update_thread_num;

	uint8_t *arq_buf;

	uint16_t num_vfs;
	uint16_t *vf_vsi_map;
	uint16_t pf_vsi_id;

	struct ice_dcf_tm_conf tm_conf;
	struct virtchnl_dcf_bw_cfg_list **qos_bw_cfg;
	struct ice_aqc_port_ets_elem *ets_config;
	struct virtchnl_version_info virtchnl_version;
	struct virtchnl_vf_resource *vf_res; /* VF resource */
	struct virtchnl_vsi_resource *vsi_res; /* LAN VSI */
	uint16_t vsi_id;

	struct rte_eth_dev *eth_dev;
	uint8_t *rss_lut;
	uint8_t *rss_key;
	uint64_t supported_rxdid;
	uint16_t num_queue_pairs;

	uint16_t msix_base;
	uint16_t nb_msix;
	uint16_t rxq_map[16];
	struct virtchnl_eth_stats eth_stats_offset;
	struct virtchnl_vlan_caps vlan_v2_caps;

	/* Link status */
	bool link_up;
	uint32_t link_speed;

	bool resetting;
};

int ice_dcf_execute_virtchnl_cmd(struct ice_dcf_hw *hw,
				 struct dcf_virtchnl_cmd *cmd);
int ice_dcf_send_aq_cmd(void *dcf_hw, struct ice_aq_desc *desc,
			void *buf, uint16_t buf_size);
int ice_dcf_handle_vsi_update_event(struct ice_dcf_hw *hw);
int ice_dcf_init_hw(struct rte_eth_dev *eth_dev, struct ice_dcf_hw *hw);
void ice_dcf_uninit_hw(struct rte_eth_dev *eth_dev, struct ice_dcf_hw *hw);
int ice_dcf_configure_rss_key(struct ice_dcf_hw *hw);
int ice_dcf_configure_rss_lut(struct ice_dcf_hw *hw);
int ice_dcf_add_del_rss_cfg(struct ice_dcf_hw *hw,
		     struct virtchnl_rss_cfg *rss_cfg, bool add);
int ice_dcf_set_hena(struct ice_dcf_hw *hw, uint64_t hena);
int ice_dcf_rss_hash_set(struct ice_dcf_hw *hw, uint64_t rss_hf, bool add);
int ice_dcf_init_rss(struct ice_dcf_hw *hw);
int ice_dcf_configure_queues(struct ice_dcf_hw *hw);
int ice_dcf_config_irq_map(struct ice_dcf_hw *hw);
int ice_dcf_switch_queue(struct ice_dcf_hw *hw, uint16_t qid, bool rx, bool on);
int ice_dcf_disable_queues(struct ice_dcf_hw *hw);
int ice_dcf_query_stats(struct ice_dcf_hw *hw,
			struct virtchnl_eth_stats *pstats);
int ice_dcf_add_del_all_mac_addr(struct ice_dcf_hw *hw,
				 struct rte_ether_addr *addr, bool add,
				 uint8_t type);
int ice_dcf_link_update(struct rte_eth_dev *dev,
		    __rte_unused int wait_to_complete);
void ice_dcf_tm_conf_init(struct rte_eth_dev *dev);
void ice_dcf_tm_conf_uninit(struct rte_eth_dev *dev);
int ice_dcf_replay_vf_bw(struct ice_dcf_hw *hw, uint16_t vf_id);
int ice_dcf_clear_bw(struct ice_dcf_hw *hw);

#endif /* _ICE_DCF_H_ */
