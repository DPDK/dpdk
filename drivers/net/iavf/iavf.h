/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _IAVF_ETHDEV_H_
#define _IAVF_ETHDEV_H_

#include <rte_kvargs.h>
#include "base/iavf_type.h"

#define IAVF_AQ_LEN               32
#define IAVF_AQ_BUF_SZ            4096
#define IAVF_RESET_WAIT_CNT       50
#define IAVF_BUF_SIZE_MIN         1024
#define IAVF_FRAME_SIZE_MAX       9728
#define IAVF_QUEUE_BASE_ADDR_UNIT 128

#define IAVF_MAX_NUM_QUEUES       16

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
	(ETHER_HDR_LEN + ETHER_CRC_LEN + IAVF_VLAN_TAG_SIZE * 2)

#define IAVF_32_BIT_WIDTH (CHAR_BIT * 4)
#define IAVF_48_BIT_WIDTH (CHAR_BIT * 6)
#define IAVF_48_BIT_MASK  RTE_LEN2MASK(IAVF_48_BIT_WIDTH, uint64_t)

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

/* TODO: is that correct to assume the max number to be 16 ?*/
#define IAVF_MAX_MSIX_VECTORS   16

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

	volatile enum virtchnl_ops pend_cmd; /* pending command not finished */
	uint32_t cmd_retval; /* return value of the cmd response from PF */
	uint8_t *aq_resp; /* buffer to store the adminq response from PF */

	/* Event from pf */
	bool dev_closed;
	bool link_up;
	uint32_t link_speed;

	struct iavf_vsi vsi;
	bool vf_reset;
	uint64_t flags;

	uint8_t *rss_lut;
	uint8_t *rss_key;
	uint16_t nb_msix;   /* number of MSI-X interrupts on Rx */
	uint16_t msix_base; /* msix vector base from */
	/* queue bitmask for each vector */
	uint16_t rxq_map[IAVF_MAX_MSIX_VECTORS];
};

#define IAVF_MAX_PKT_TYPE 256

/* Structure to store private data for each VF instance. */
struct iavf_adapter {
	struct iavf_hw hw;
	struct rte_eth_dev *eth_dev;
	struct iavf_info vf;

	bool rx_bulk_alloc_allowed;
	/* For vector PMD */
	bool rx_vec_allowed;
	bool tx_vec_allowed;
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
#define IAVF_VSI_TO_ETH_DEV(vsi) \
	(((struct iavf_vsi *)vsi)->adapter->eth_dev)

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
int iavf_enable_queues(struct iavf_adapter *adapter);
int iavf_disable_queues(struct iavf_adapter *adapter);
int iavf_configure_rss_lut(struct iavf_adapter *adapter);
int iavf_configure_rss_key(struct iavf_adapter *adapter);
int iavf_configure_queues(struct iavf_adapter *adapter);
int iavf_config_irq_map(struct iavf_adapter *adapter);
void iavf_add_del_all_mac_addr(struct iavf_adapter *adapter, bool add);
int iavf_dev_link_update(struct rte_eth_dev *dev,
			__rte_unused int wait_to_complete);
int iavf_query_stats(struct iavf_adapter *adapter,
		    struct virtchnl_eth_stats **pstats);
int iavf_config_promisc(struct iavf_adapter *adapter, bool enable_unicast,
		       bool enable_multicast);
int iavf_add_del_eth_addr(struct iavf_adapter *adapter,
			 struct ether_addr *addr, bool add);
int iavf_add_del_vlan(struct iavf_adapter *adapter, uint16_t vlanid, bool add);
#endif /* _IAVF_ETHDEV_H_ */
