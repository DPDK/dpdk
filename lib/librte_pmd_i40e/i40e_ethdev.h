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

#ifndef _I40E_ETHDEV_H_
#define _I40E_ETHDEV_H_

#include <rte_eth_ctrl.h>

#define I40E_AQ_LEN               32
#define I40E_AQ_BUF_SZ            4096
/* Number of queues per TC should be one of 1, 2, 4, 8, 16, 32, 64 */
#define I40E_MAX_Q_PER_TC         64
#define I40E_NUM_DESC_DEFAULT     512
#define I40E_NUM_DESC_ALIGN       32
#define I40E_BUF_SIZE_MIN         1024
#define I40E_FRAME_SIZE_MAX       9728
#define I40E_QUEUE_BASE_ADDR_UNIT 128
/* number of VSIs and queue default setting */
#define I40E_MAX_QP_NUM_PER_VF    16
#define I40E_DEFAULT_QP_NUM_FDIR  1
#define I40E_UINT32_BIT_SIZE      (CHAR_BIT * sizeof(uint32_t))
#define I40E_VFTA_SIZE            (4096 / I40E_UINT32_BIT_SIZE)
/*
 * vlan_id is a 12 bit number.
 * The VFTA array is actually a 4096 bit array, 128 of 32bit elements.
 * 2^5 = 32. The val of lower 5 bits specifies the bit in the 32bit element.
 * The higher 7 bit val specifies VFTA array index.
 */
#define I40E_VFTA_BIT(vlan_id)    (1 << ((vlan_id) & 0x1F))
#define I40E_VFTA_IDX(vlan_id)    ((vlan_id) >> 5)

/* Default TC traffic in case DCB is not enabled */
#define I40E_DEFAULT_TCMAP        0x1
#define I40E_FDIR_QUEUE_ID        0

/* Always assign pool 0 to main VSI, VMDQ will start from 1 */
#define I40E_VMDQ_POOL_BASE       1

#define I40E_DEFAULT_RX_FREE_THRESH  32
#define I40E_DEFAULT_RX_PTHRESH      8
#define I40E_DEFAULT_RX_HTHRESH      8
#define I40E_DEFAULT_RX_WTHRESH      0

#define I40E_DEFAULT_TX_FREE_THRESH  32
#define I40E_DEFAULT_TX_PTHRESH      32
#define I40E_DEFAULT_TX_HTHRESH      0
#define I40E_DEFAULT_TX_WTHRESH      0
#define I40E_DEFAULT_TX_RSBIT_THRESH 32

/* Bit shift and mask */
#define I40E_4_BIT_WIDTH  (CHAR_BIT / 2)
#define I40E_4_BIT_MASK   RTE_LEN2MASK(I40E_4_BIT_WIDTH, uint8_t)
#define I40E_8_BIT_WIDTH  CHAR_BIT
#define I40E_8_BIT_MASK   UINT8_MAX
#define I40E_16_BIT_WIDTH (CHAR_BIT * 2)
#define I40E_16_BIT_MASK  UINT16_MAX
#define I40E_32_BIT_WIDTH (CHAR_BIT * 4)
#define I40E_32_BIT_MASK  UINT32_MAX
#define I40E_48_BIT_WIDTH (CHAR_BIT * 6)
#define I40E_48_BIT_MASK  RTE_LEN2MASK(I40E_48_BIT_WIDTH, uint64_t)

/* index flex payload per layer */
enum i40e_flxpld_layer_idx {
	I40E_FLXPLD_L2_IDX    = 0,
	I40E_FLXPLD_L3_IDX    = 1,
	I40E_FLXPLD_L4_IDX    = 2,
	I40E_MAX_FLXPLD_LAYER = 3,
};
#define I40E_MAX_FLXPLD_FIED        3  /* max number of flex payload fields */
#define I40E_FDIR_BITMASK_NUM_WORD  2  /* max number of bitmask words */
#define I40E_FDIR_MAX_FLEXWORD_NUM  8  /* max number of flexpayload words */
#define I40E_FDIR_MAX_FLEX_LEN      16 /* len in bytes of flex payload */

/* i40e flags */
#define I40E_FLAG_RSS                   (1ULL << 0)
#define I40E_FLAG_DCB                   (1ULL << 1)
#define I40E_FLAG_VMDQ                  (1ULL << 2)
#define I40E_FLAG_SRIOV                 (1ULL << 3)
#define I40E_FLAG_HEADER_SPLIT_DISABLED (1ULL << 4)
#define I40E_FLAG_HEADER_SPLIT_ENABLED  (1ULL << 5)
#define I40E_FLAG_FDIR                  (1ULL << 6)
#define I40E_FLAG_VXLAN                 (1ULL << 7)
#define I40E_FLAG_ALL (I40E_FLAG_RSS | \
		       I40E_FLAG_DCB | \
		       I40E_FLAG_VMDQ | \
		       I40E_FLAG_SRIOV | \
		       I40E_FLAG_HEADER_SPLIT_DISABLED | \
		       I40E_FLAG_HEADER_SPLIT_ENABLED | \
		       I40E_FLAG_FDIR | \
		       I40E_FLAG_VXLAN)

#define I40E_RSS_OFFLOAD_ALL ( \
	ETH_RSS_FRAG_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_NONFRAG_IPV4_SCTP | \
	ETH_RSS_NONFRAG_IPV4_OTHER | \
	ETH_RSS_FRAG_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_NONFRAG_IPV6_SCTP | \
	ETH_RSS_NONFRAG_IPV6_OTHER | \
	ETH_RSS_L2_PAYLOAD)

/* All bits of RSS hash enable */
#define I40E_RSS_HENA_ALL ( \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) | \
	(1ULL << I40E_FILTER_PCTYPE_FRAG_IPV4) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER) | \
	(1ULL << I40E_FILTER_PCTYPE_FRAG_IPV6) | \
	(1ULL << I40E_FILTER_PCTYPE_FCOE_OX) | \
	(1ULL << I40E_FILTER_PCTYPE_FCOE_RX) | \
	(1ULL << I40E_FILTER_PCTYPE_FCOE_OTHER) | \
	(1ULL << I40E_FILTER_PCTYPE_L2_PAYLOAD))

struct i40e_adapter;

/**
 * MAC filter structure
 */
struct i40e_mac_filter_info {
	enum rte_mac_filter_type filter_type;
	struct ether_addr mac_addr;
};

TAILQ_HEAD(i40e_mac_filter_list, i40e_mac_filter);

/* MAC filter list structure */
struct i40e_mac_filter {
	TAILQ_ENTRY(i40e_mac_filter) next;
	struct i40e_mac_filter_info mac_info;
};

TAILQ_HEAD(i40e_vsi_list_head, i40e_vsi_list);

struct i40e_vsi;

/* VSI list structure */
struct i40e_vsi_list {
	TAILQ_ENTRY(i40e_vsi_list) list;
	struct i40e_vsi *vsi;
};

struct i40e_rx_queue;
struct i40e_tx_queue;

/* Structure that defines a VEB */
struct i40e_veb {
	struct i40e_vsi_list_head head;
	struct i40e_vsi *associate_vsi; /* Associate VSI who owns the VEB */
	uint16_t seid; /* The seid of VEB itself */
	uint16_t uplink_seid; /* The uplink seid of this VEB */
	uint16_t stats_idx;
	struct i40e_eth_stats stats;
};

/* i40e MACVLAN filter structure */
struct i40e_macvlan_filter {
	struct ether_addr macaddr;
	enum rte_mac_filter_type filter_type;
	uint16_t vlan_id;
};

/*
 * Structure that defines a VSI, associated with a adapter.
 */
struct i40e_vsi {
	struct i40e_adapter *adapter; /* Backreference to associated adapter */
	struct i40e_aqc_vsi_properties_data info; /* VSI properties */

	struct i40e_eth_stats eth_stats_offset;
	struct i40e_eth_stats eth_stats;
	/*
	 * When drivers loaded, only a default main VSI exists. In case new VSI
	 * needs to add, HW needs to know the layout that VSIs are organized.
	 * Besides that, VSI isan element and can't switch packets, which needs
	 * to add new component VEB to perform switching. So, a new VSI needs
	 * to specify the the uplink VSI (Parent VSI) before created. The
	 * uplink VSI will check whether it had a VEB to switch packets. If no,
	 * it will try to create one. Then, uplink VSI will move the new VSI
	 * into its' sib_vsi_list to manage all the downlink VSI.
	 *  sib_vsi_list: the VSI list that shared the same uplink VSI.
	 *  parent_vsi  : the uplink VSI. It's NULL for main VSI.
	 *  veb         : the VEB associates with the VSI.
	 */
	struct i40e_vsi_list sib_vsi_list; /* sibling vsi list */
	struct i40e_vsi *parent_vsi;
	struct i40e_veb *veb;    /* Associated veb, could be null */
	bool offset_loaded;
	enum i40e_vsi_type type; /* VSI types */
	uint16_t vlan_num;       /* Total VLAN number */
	uint16_t mac_num;        /* Total mac number */
	uint32_t vfta[I40E_VFTA_SIZE];        /* VLAN bitmap */
	struct i40e_mac_filter_list mac_list; /* macvlan filter list */
	/* specific VSI-defined parameters, SRIOV stored the vf_id */
	uint32_t user_param;
	uint16_t seid;           /* The seid of VSI itself */
	uint16_t uplink_seid;    /* The uplink seid of this VSI */
	uint16_t nb_qps;         /* Number of queue pairs VSI can occupy */
	uint16_t max_macaddrs;   /* Maximum number of MAC addresses */
	uint16_t base_queue;     /* The first queue index of this VSI */
	/*
	 * The offset to visit VSI related register, assigned by HW when
	 * creating VSI
	 */
	uint16_t vsi_id;
	uint16_t msix_intr; /* The MSIX interrupt binds to VSI */
	uint8_t enabled_tc; /* The traffic class enabled */
};

struct pool_entry {
	LIST_ENTRY(pool_entry) next;
	uint16_t base;
	uint16_t len;
};

LIST_HEAD(res_list, pool_entry);

struct i40e_res_pool_info {
	uint32_t base;              /* Resource start index */
	uint32_t num_alloc;         /* Allocated resource number */
	uint32_t num_free;          /* Total available resource number */
	struct res_list alloc_list; /* Allocated resource list */
	struct res_list free_list;  /* Available resource list */
};

enum I40E_VF_STATE {
	I40E_VF_INACTIVE = 0,
	I40E_VF_INRESET,
	I40E_VF_ININIT,
	I40E_VF_ACTIVE,
};

/*
 * Structure to store private data for PF host.
 */
struct i40e_pf_vf {
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	enum I40E_VF_STATE state; /* The number of queue pairs availiable */
	uint16_t vf_idx; /* VF index in pf->vfs */
	uint16_t lan_nb_qps; /* Actual queues allocated */
	uint16_t reset_cnt; /* Total vf reset times */
};

/*
 * Structure to store private data for VMDQ instance
 */
struct i40e_vmdq_info {
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
};

/*
 * Structure to store flex pit for flow diretor.
 */
struct i40e_fdir_flex_pit {
	uint8_t src_offset;    /* offset in words from the beginning of payload */
	uint8_t size;          /* size in words */
	uint8_t dst_offset;    /* offset in words of flexible payload */
};

struct i40e_fdir_flex_mask {
	uint8_t word_mask;  /**< Bit i enables word i of flexible payload */
	struct {
		uint8_t offset;
		uint16_t mask;
	} bitmask[I40E_FDIR_BITMASK_NUM_WORD];
};

#define I40E_FILTER_PCTYPE_MAX 64
/*
 *  A structure used to define fields of a FDIR related info.
 */
struct i40e_fdir_info {
	struct i40e_vsi *fdir_vsi;     /* pointer to fdir VSI structure */
	uint16_t match_counter_index;  /* Statistic counter index used for fdir*/
	struct i40e_tx_queue *txq;
	struct i40e_rx_queue *rxq;
	void *prg_pkt;                 /* memory for fdir program packet */
	uint64_t dma_addr;             /* physic address of packet memory*/
	/*
	 * the rule how bytes stream is extracted as flexible payload
	 * for each payload layer, the setting can up to three elements
	 */
	struct i40e_fdir_flex_pit flex_set[I40E_MAX_FLXPLD_LAYER * I40E_MAX_FLXPLD_FIED];
	struct i40e_fdir_flex_mask flex_mask[I40E_FILTER_PCTYPE_MAX];
};

/*
 * Structure to store private data specific for PF instance.
 */
struct i40e_pf {
	struct i40e_adapter *adapter; /* The adapter this PF associate to */
	struct i40e_vsi *main_vsi; /* pointer to main VSI structure */
	uint16_t mac_seid; /* The seid of the MAC of this PF */
	uint16_t main_vsi_seid; /* The seid of the main VSI */
	uint16_t max_num_vsi;
	struct i40e_res_pool_info qp_pool;    /*Queue pair pool */
	struct i40e_res_pool_info msix_pool;  /* MSIX interrupt pool */

	struct i40e_hw_port_stats stats_offset;
	struct i40e_hw_port_stats stats;
	bool offset_loaded;

	struct rte_eth_dev_data *dev_data; /* Pointer to the device data */
	struct ether_addr dev_addr; /* PF device mac address */
	uint64_t flags; /* PF featuer flags */
	/* All kinds of queue pair setting for different VSIs */
	struct i40e_pf_vf *vfs;
	uint16_t vf_num;
	/* Each of below queue pairs should be power of 2 since it's the
	   precondition after TC configuration applied */
	uint16_t lan_nb_qps; /* The number of queue pairs of LAN */
	uint16_t vmdq_nb_qps; /* The number of queue pairs of VMDq */
	uint16_t vf_nb_qps; /* The number of queue pairs of VF */
	uint16_t fdir_nb_qps; /* The number of queue pairs of Flow Director */
	uint16_t hash_lut_size; /* The size of hash lookup table */
	/* store VXLAN UDP ports */
	uint16_t vxlan_ports[I40E_MAX_PF_UDP_OFFLOAD_PORTS];
	uint16_t vxlan_bitmap; /* Vxlan bit mask */

	/* VMDQ related info */
	uint16_t max_nb_vmdq_vsi; /* Max number of VMDQ VSIs supported */
	uint16_t nb_cfg_vmdq_vsi; /* number of VMDQ VSIs configured */
	struct i40e_vmdq_info *vmdq;

	struct i40e_fdir_info fdir; /* flow director info */
};

enum pending_msg {
	PFMSG_LINK_CHANGE = 0x1,
	PFMSG_RESET_IMPENDING = 0x2,
	PFMSG_DRIVER_CLOSE = 0x4,
};

struct i40e_vsi_vlan_pvid_info {
	uint16_t on;            /* Enable or disable pvid */
	union {
		uint16_t pvid;  /* Valid in case 'on' is set to set pvid */
		struct {
		/*  Valid in case 'on' is cleared. 'tagged' will reject tagged packets,
		 *  while 'untagged' will reject untagged packets.
		 */
			uint8_t tagged;
			uint8_t untagged;
		} reject;
	} config;
};

struct i40e_vf_rx_queues {
	uint64_t rx_dma_addr;
	uint32_t rx_ring_len;
	uint32_t buff_size;
};

struct i40e_vf_tx_queues {
	uint64_t tx_dma_addr;
	uint32_t tx_ring_len;
};

/*
 * Structure to store private data specific for VF instance.
 */
struct i40e_vf {
	struct i40e_adapter *adapter; /* The adapter this VF associate to */
	struct rte_eth_dev_data *dev_data; /* Pointer to the device data */
	uint16_t num_queue_pairs;
	uint16_t max_pkt_len; /* Maximum packet length */
	bool promisc_unicast_enabled;
	bool promisc_multicast_enabled;

	uint32_t version_major; /* Major version number */
	uint32_t version_minor; /* Minor version number */
	uint16_t promisc_flags; /* Promiscuous setting */
	uint32_t vlan[I40E_VFTA_SIZE]; /* VLAN bit map */

	/* Event from pf */
	bool dev_closed;
	bool link_up;
	bool vf_reset;
	volatile uint32_t pend_cmd; /* pending command not finished yet */
	u16 pend_msg; /* flags indicates events from pf not handled yet */

	/* VSI info */
	struct i40e_virtchnl_vf_resource *vf_res; /* All VSIs */
	struct i40e_virtchnl_vsi_resource *vsi_res; /* LAN VSI */
	struct i40e_vsi vsi;
};

/*
 * Structure to store private data for each PF/VF instance.
 */
struct i40e_adapter {
	/* Common for both PF and VF */
	struct i40e_hw hw;
	struct rte_eth_dev *eth_dev;

	/* Specific for PF or VF */
	union {
		struct i40e_pf pf;
		struct i40e_vf vf;
	};
};

int i40e_dev_switch_queues(struct i40e_pf *pf, bool on);
int i40e_vsi_release(struct i40e_vsi *vsi);
struct i40e_vsi *i40e_vsi_setup(struct i40e_pf *pf,
				enum i40e_vsi_type type,
				struct i40e_vsi *uplink_vsi,
				uint16_t user_param);
int i40e_switch_rx_queue(struct i40e_hw *hw, uint16_t q_idx, bool on);
int i40e_switch_tx_queue(struct i40e_hw *hw, uint16_t q_idx, bool on);
int i40e_vsi_add_vlan(struct i40e_vsi *vsi, uint16_t vlan);
int i40e_vsi_delete_vlan(struct i40e_vsi *vsi, uint16_t vlan);
int i40e_vsi_add_mac(struct i40e_vsi *vsi, struct i40e_mac_filter_info *filter);
int i40e_vsi_delete_mac(struct i40e_vsi *vsi, struct ether_addr *addr);
void i40e_update_vsi_stats(struct i40e_vsi *vsi);
void i40e_pf_disable_irq0(struct i40e_hw *hw);
void i40e_pf_enable_irq0(struct i40e_hw *hw);
int i40e_dev_link_update(struct rte_eth_dev *dev,
			 __rte_unused int wait_to_complete);
void i40e_vsi_queues_bind_intr(struct i40e_vsi *vsi);
void i40e_vsi_queues_unbind_intr(struct i40e_vsi *vsi);
int i40e_vsi_vlan_pvid_set(struct i40e_vsi *vsi,
			   struct i40e_vsi_vlan_pvid_info *info);
int i40e_vsi_config_vlan_stripping(struct i40e_vsi *vsi, bool on);
uint64_t i40e_config_hena(uint64_t flags);
uint64_t i40e_parse_hena(uint64_t flags);
enum i40e_status_code i40e_fdir_setup_tx_resources(struct i40e_pf *pf);
enum i40e_status_code i40e_fdir_setup_rx_resources(struct i40e_pf *pf);
int i40e_fdir_setup(struct i40e_pf *pf);
const struct rte_memzone *i40e_memzone_reserve(const char *name,
					uint32_t len,
					int socket_id);
int i40e_fdir_configure(struct rte_eth_dev *dev);
void i40e_fdir_teardown(struct i40e_pf *pf);
enum i40e_filter_pctype i40e_flowtype_to_pctype(uint16_t flow_type);
uint16_t i40e_pctype_to_flowtype(enum i40e_filter_pctype pctype);
int i40e_fdir_ctrl_func(struct rte_eth_dev *dev,
			  enum rte_filter_op filter_op,
			  void *arg);

/* I40E_DEV_PRIVATE_TO */
#define I40E_DEV_PRIVATE_TO_PF(adapter) \
	(&((struct i40e_adapter *)adapter)->pf)
#define I40E_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct i40e_adapter *)adapter)->hw)
#define I40E_DEV_PRIVATE_TO_ADAPTER(adapter) \
	((struct i40e_adapter *)adapter)

/* I40EVF_DEV_PRIVATE_TO */
#define I40EVF_DEV_PRIVATE_TO_VF(adapter) \
	(&((struct i40e_adapter *)adapter)->vf)

static inline struct i40e_vsi *
i40e_get_vsi_from_adapter(struct i40e_adapter *adapter)
{
	struct i40e_hw *hw;

        if (!adapter)
                return NULL;

	hw = I40E_DEV_PRIVATE_TO_HW(adapter);
	if (hw->mac.type == I40E_MAC_VF) {
		struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(adapter);
		return &vf->vsi;
	} else {
		struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(adapter);
		return pf->main_vsi;
	}
}
#define I40E_DEV_PRIVATE_TO_MAIN_VSI(adapter) \
	i40e_get_vsi_from_adapter((struct i40e_adapter *)adapter)

/* I40E_VSI_TO */
#define I40E_VSI_TO_HW(vsi) \
	(&(((struct i40e_vsi *)vsi)->adapter->hw))
#define I40E_VSI_TO_PF(vsi) \
	(&(((struct i40e_vsi *)vsi)->adapter->pf))
#define I40E_VSI_TO_DEV_DATA(vsi) \
	(((struct i40e_vsi *)vsi)->adapter->pf.dev_data)
#define I40E_VSI_TO_ETH_DEV(vsi) \
	(((struct i40e_vsi *)vsi)->adapter->eth_dev)

/* I40E_PF_TO */
#define I40E_PF_TO_HW(pf) \
	(&(((struct i40e_pf *)pf)->adapter->hw))
#define I40E_PF_TO_ADAPTER(pf) \
	((struct i40e_adapter *)pf->adapter)

/* I40E_VF_TO */
#define I40E_VF_TO_HW(vf) \
	(&(((struct i40e_vf *)vf)->adapter->hw))

static inline void
i40e_init_adminq_parameter(struct i40e_hw *hw)
{
	hw->aq.num_arq_entries = I40E_AQ_LEN;
	hw->aq.num_asq_entries = I40E_AQ_LEN;
	hw->aq.arq_buf_size = I40E_AQ_BUF_SZ;
	hw->aq.asq_buf_size = I40E_AQ_BUF_SZ;
}

#define I40E_VALID_FLOW(flow_type) \
	((flow_type) == RTE_ETH_FLOW_FRAG_IPV4 || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV4_TCP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV4_UDP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV4_SCTP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV4_OTHER || \
	(flow_type) == RTE_ETH_FLOW_FRAG_IPV6 || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV6_TCP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV6_UDP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV6_SCTP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV6_OTHER || \
	(flow_type) == RTE_ETH_FLOW_L2_PAYLOAD)

#define I40E_VALID_PCTYPE(pctype) \
	((pctype) == I40E_FILTER_PCTYPE_FRAG_IPV4 || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_TCP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_UDP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_SCTP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER || \
	(pctype) == I40E_FILTER_PCTYPE_FRAG_IPV6 || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_UDP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_TCP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_SCTP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_OTHER || \
	(pctype) == I40E_FILTER_PCTYPE_L2_PAYLOAD)

#endif /* _I40E_ETHDEV_H_ */
