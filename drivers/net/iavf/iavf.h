/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _IAVF_ETHDEV_H_
#define _IAVF_ETHDEV_H_

#include <rte_kvargs.h>
#include <iavf_prototype.h>
#include <iavf_adminq_cmd.h>
#include <iavf_type.h>

#include "iavf_log.h"

#define IAVF_AQ_LEN               32
#define IAVF_AQ_BUF_SZ            4096
#define IAVF_RESET_WAIT_CNT       50
#define IAVF_BUF_SIZE_MIN         1024
#define IAVF_FRAME_SIZE_MAX       9728
#define IAVF_QUEUE_BASE_ADDR_UNIT 128

#define IAVF_MAX_NUM_QUEUES_DFLT	 16
#define IAVF_MAX_NUM_QUEUES_LV		 256
#define IAVF_CFG_Q_NUM_PER_BUF		 32
#define IAVF_IRQ_MAP_NUM_PER_BUF	 128
#define IAVF_RXTX_QUEUE_CHUNKS_NUM	 2

#define IAVF_NUM_MACADDR_MAX      64

#define IAVF_DEFAULT_RX_PTHRESH      8
#define IAVF_DEFAULT_RX_HTHRESH      8
#define IAVF_DEFAULT_RX_WTHRESH      0

#define IAVF_DEFAULT_RX_FREE_THRESH  32

#define IAVF_DEFAULT_TX_PTHRESH      32
#define IAVF_DEFAULT_TX_HTHRESH      0
#define IAVF_DEFAULT_TX_WTHRESH      0

#define IAVF_DEFAULT_TX_FREE_THRESH  32
#define IAVF_DEFAULT_TX_RS_THRESH 32

#define IAVF_BASIC_OFFLOAD_CAPS  ( \
	VF_BASE_MODE_OFFLOADS | \
	VIRTCHNL_VF_OFFLOAD_WB_ON_ITR | \
	VIRTCHNL_VF_OFFLOAD_RX_POLLING)

#define IAVF_RSS_OFFLOAD_ALL ( \
	ETH_RSS_FRAG_IPV4 |         \
	ETH_RSS_NONFRAG_IPV4_TCP |  \
	ETH_RSS_NONFRAG_IPV4_UDP |  \
	ETH_RSS_NONFRAG_IPV4_SCTP | \
	ETH_RSS_NONFRAG_IPV4_OTHER)

#define IAVF_MISC_VEC_ID                RTE_INTR_VEC_ZERO_OFFSET
#define IAVF_RX_VEC_START               RTE_INTR_VEC_RXTX_OFFSET

/* Default queue interrupt throttling time in microseconds */
#define IAVF_ITR_INDEX_DEFAULT          0
#define IAVF_QUEUE_ITR_INTERVAL_DEFAULT 32 /* 32 us */
#define IAVF_QUEUE_ITR_INTERVAL_MAX     8160 /* 8160 us */

/* The overhead from MTU to max frame size.
 * Considering QinQ packet, the VLAN tag needs to be counted twice.
 */
#define IAVF_VLAN_TAG_SIZE               4
#define IAVF_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + IAVF_VLAN_TAG_SIZE * 2)
#define IAVF_ETH_MAX_LEN (RTE_ETHER_MTU + IAVF_ETH_OVERHEAD)

#define IAVF_32_BIT_WIDTH (CHAR_BIT * 4)
#define IAVF_48_BIT_WIDTH (CHAR_BIT * 6)
#define IAVF_48_BIT_MASK  RTE_LEN2MASK(IAVF_48_BIT_WIDTH, uint64_t)

#define IAVF_RX_DESC_EXT_STATUS_FLEXBH_MASK  0x03
#define IAVF_RX_DESC_EXT_STATUS_FLEXBH_FD_ID 0x01

struct iavf_adapter;
struct iavf_rx_queue;
struct iavf_tx_queue;

/* Structure that defines a VSI, associated with a adapter. */
struct iavf_vsi {
	struct iavf_adapter *adapter; /* Backreference to associated adapter */
	uint16_t vsi_id;
	uint16_t nb_qps;         /* Number of queue pairs VSI can occupy */
	uint16_t nb_used_qps;    /* Number of queue pairs VSI uses */
	uint16_t max_macaddrs;   /* Maximum number of MAC addresses */
	uint16_t base_vector;
	uint16_t msix_intr;      /* The MSIX interrupt binds to VSI */
	struct virtchnl_eth_stats eth_stats_offset;
};

struct rte_flow;
TAILQ_HEAD(iavf_flow_list, rte_flow);

struct iavf_flow_parser_node;
TAILQ_HEAD(iavf_parser_list, iavf_flow_parser_node);

struct iavf_fdir_conf {
	struct virtchnl_fdir_add add_fltr;
	struct virtchnl_fdir_del del_fltr;
	uint64_t input_set;
	uint32_t flow_id;
	uint32_t mark_flag;
};

struct iavf_fdir_info {
	struct iavf_fdir_conf conf;
};

struct iavf_qv_map {
	uint16_t queue_id;
	uint16_t vector_id;
};

/* Message type read in admin queue from PF */
enum iavf_aq_result {
	IAVF_MSG_ERR = -1, /* Meet error when accessing admin queue */
	IAVF_MSG_NON,      /* Read nothing from admin queue */
	IAVF_MSG_SYS,      /* Read system msg from admin queue */
	IAVF_MSG_CMD,      /* Read async command result */
};

/* Structure to store private data specific for VF instance. */
struct iavf_info {
	uint16_t num_queue_pairs;
	uint16_t max_pkt_len; /* Maximum packet length */
	uint16_t mac_num;     /* Number of MAC addresses */
	bool promisc_unicast_enabled;
	bool promisc_multicast_enabled;

	struct virtchnl_version_info virtchnl_version;
	struct virtchnl_vf_resource *vf_res; /* VF resource */
	struct virtchnl_vsi_resource *vsi_res; /* LAN VSI */
	uint64_t supported_rxdid;
	uint8_t *proto_xtr; /* proto xtr type for all queues */
	volatile enum virtchnl_ops pend_cmd; /* pending command not finished */
	int cmd_retval; /* return value of the cmd response from PF */
	uint8_t *aq_resp; /* buffer to store the adminq response from PF */

	/* Event from pf */
	bool dev_closed;
	bool link_up;
	uint32_t link_speed;

	/* Multicast addrs */
	struct rte_ether_addr mc_addrs[IAVF_NUM_MACADDR_MAX];
	uint16_t mc_addrs_num;   /* Multicast mac addresses number */

	struct iavf_vsi vsi;
	bool vf_reset;	/* true for VF reset pending, false for no VF reset */
	uint64_t flags;

	uint8_t *rss_lut;
	uint8_t *rss_key;
	uint16_t nb_msix;   /* number of MSI-X interrupts on Rx */
	uint16_t msix_base; /* msix vector base from */
	uint16_t max_rss_qregion; /* max RSS queue region supported by PF */
	struct iavf_qv_map *qv_map; /* queue vector mapping */
	struct iavf_flow_list flow_list;
	rte_spinlock_t flow_ops_lock;
	struct iavf_parser_list rss_parser_list;
	struct iavf_parser_list dist_parser_list;

	struct iavf_fdir_info fdir; /* flow director info */
	/* indicate large VF support enabled or not */
	bool lv_enabled;

	struct rte_eth_dev *eth_dev;
};

#define IAVF_MAX_PKT_TYPE 1024

#define IAVF_MAX_QUEUE_NUM  2048

enum iavf_proto_xtr_type {
	IAVF_PROTO_XTR_NONE,
	IAVF_PROTO_XTR_VLAN,
	IAVF_PROTO_XTR_IPV4,
	IAVF_PROTO_XTR_IPV6,
	IAVF_PROTO_XTR_IPV6_FLOW,
	IAVF_PROTO_XTR_TCP,
	IAVF_PROTO_XTR_IP_OFFSET,
	IAVF_PROTO_XTR_MAX,
};

/**
 * Cache devargs parse result.
 */
struct iavf_devargs {
	uint8_t proto_xtr_dflt;
	uint8_t proto_xtr[IAVF_MAX_QUEUE_NUM];
};

/* Structure to store private data for each VF instance. */
struct iavf_adapter {
	struct iavf_hw hw;
	struct rte_eth_dev_data *dev_data;
	struct iavf_info vf;

	bool rx_bulk_alloc_allowed;
	/* For vector PMD */
	bool rx_vec_allowed;
	bool tx_vec_allowed;
	uint32_t ptype_tbl[IAVF_MAX_PKT_TYPE] __rte_cache_min_aligned;
	bool stopped;
	uint16_t fdir_ref_cnt;
	struct iavf_devargs devargs;
};

/* IAVF_DEV_PRIVATE_TO */
#define IAVF_DEV_PRIVATE_TO_ADAPTER(adapter) \
	((struct iavf_adapter *)adapter)
#define IAVF_DEV_PRIVATE_TO_VF(adapter) \
	(&((struct iavf_adapter *)adapter)->vf)
#define IAVF_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct iavf_adapter *)adapter)->hw)

/* IAVF_VSI_TO */
#define IAVF_VSI_TO_HW(vsi) \
	(&(((struct iavf_vsi *)vsi)->adapter->hw))
#define IAVF_VSI_TO_VF(vsi) \
	(&(((struct iavf_vsi *)vsi)->adapter->vf))

static inline void
iavf_init_adminq_parameter(struct iavf_hw *hw)
{
	hw->aq.num_arq_entries = IAVF_AQ_LEN;
	hw->aq.num_asq_entries = IAVF_AQ_LEN;
	hw->aq.arq_buf_size = IAVF_AQ_BUF_SZ;
	hw->aq.asq_buf_size = IAVF_AQ_BUF_SZ;
}

static inline uint16_t
iavf_calc_itr_interval(int16_t interval)
{
	if (interval < 0 || interval > IAVF_QUEUE_ITR_INTERVAL_MAX)
		interval = IAVF_QUEUE_ITR_INTERVAL_DEFAULT;

	/* Convert to hardware count, as writing each 1 represents 2 us */
	return interval / 2;
}

/* structure used for sending and checking response of virtchnl ops */
struct iavf_cmd_info {
	enum virtchnl_ops ops;
	uint8_t *in_args;       /* buffer for sending */
	uint32_t in_args_size;  /* buffer size for sending */
	uint8_t *out_buffer;    /* buffer for response */
	uint32_t out_size;      /* buffer size for response */
};

/* notify current command done. Only call in case execute
 * _atomic_set_cmd successfully.
 */
static inline void
_notify_cmd(struct iavf_info *vf, int msg_ret)
{
	vf->cmd_retval = msg_ret;
	rte_wmb();
	vf->pend_cmd = VIRTCHNL_OP_UNKNOWN;
}

/* clear current command. Only call in case execute
 * _atomic_set_cmd successfully.
 */
static inline void
_clear_cmd(struct iavf_info *vf)
{
	rte_wmb();
	vf->pend_cmd = VIRTCHNL_OP_UNKNOWN;
	vf->cmd_retval = VIRTCHNL_STATUS_SUCCESS;
}

/* Check there is pending cmd in execution. If none, set new command. */
static inline int
_atomic_set_cmd(struct iavf_info *vf, enum virtchnl_ops ops)
{
	int ret = rte_atomic32_cmpset(&vf->pend_cmd, VIRTCHNL_OP_UNKNOWN, ops);

	if (!ret)
		PMD_DRV_LOG(ERR, "There is incomplete cmd %d", vf->pend_cmd);

	return !ret;
}

int iavf_check_api_version(struct iavf_adapter *adapter);
int iavf_get_vf_resource(struct iavf_adapter *adapter);
void iavf_handle_virtchnl_msg(struct rte_eth_dev *dev);
int iavf_enable_vlan_strip(struct iavf_adapter *adapter);
int iavf_disable_vlan_strip(struct iavf_adapter *adapter);
int iavf_switch_queue(struct iavf_adapter *adapter, uint16_t qid,
		     bool rx, bool on);
int iavf_switch_queue_lv(struct iavf_adapter *adapter, uint16_t qid,
		     bool rx, bool on);
int iavf_enable_queues(struct iavf_adapter *adapter);
int iavf_enable_queues_lv(struct iavf_adapter *adapter);
int iavf_disable_queues(struct iavf_adapter *adapter);
int iavf_disable_queues_lv(struct iavf_adapter *adapter);
int iavf_configure_rss_lut(struct iavf_adapter *adapter);
int iavf_configure_rss_key(struct iavf_adapter *adapter);
int iavf_configure_queues(struct iavf_adapter *adapter,
			uint16_t num_queue_pairs, uint16_t index);
int iavf_get_supported_rxdid(struct iavf_adapter *adapter);
int iavf_config_irq_map(struct iavf_adapter *adapter);
int iavf_config_irq_map_lv(struct iavf_adapter *adapter, uint16_t num,
			uint16_t index);
void iavf_add_del_all_mac_addr(struct iavf_adapter *adapter, bool add);
int iavf_dev_link_update(struct rte_eth_dev *dev,
			__rte_unused int wait_to_complete);
int iavf_query_stats(struct iavf_adapter *adapter,
		    struct virtchnl_eth_stats **pstats);
int iavf_config_promisc(struct iavf_adapter *adapter, bool enable_unicast,
		       bool enable_multicast);
int iavf_add_del_eth_addr(struct iavf_adapter *adapter,
			 struct rte_ether_addr *addr, bool add, uint8_t type);
int iavf_add_del_vlan(struct iavf_adapter *adapter, uint16_t vlanid, bool add);
int iavf_fdir_add(struct iavf_adapter *adapter, struct iavf_fdir_conf *filter);
int iavf_fdir_del(struct iavf_adapter *adapter, struct iavf_fdir_conf *filter);
int iavf_fdir_check(struct iavf_adapter *adapter,
		struct iavf_fdir_conf *filter);
int iavf_add_del_rss_cfg(struct iavf_adapter *adapter,
			 struct virtchnl_rss_cfg *rss_cfg, bool add);
int iavf_add_del_mc_addr_list(struct iavf_adapter *adapter,
			struct rte_ether_addr *mc_addrs,
			uint32_t mc_addrs_num, bool add);
int iavf_request_queues(struct rte_eth_dev *dev, uint16_t num);
int iavf_get_max_rss_queue_region(struct iavf_adapter *adapter);
#endif /* _IAVF_ETHDEV_H_ */
