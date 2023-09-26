/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _ICE_ETHDEV_H_
#define _ICE_ETHDEV_H_

#include <rte_compat.h>
#include <rte_kvargs.h>
#include <rte_time.h>

#include <ethdev_driver.h>
#include <rte_tm_driver.h>

#include "base/ice_common.h"
#include "base/ice_adminq_cmd.h"
#include "base/ice_flow.h"
#include "base/ice_sched.h"

#define ICE_ADMINQ_LEN               32
#define ICE_SBIOQ_LEN                32
#define ICE_MAILBOXQ_LEN             32
#define ICE_SBQ_LEN                  64
#define ICE_ADMINQ_BUF_SZ            4096
#define ICE_SBIOQ_BUF_SZ             4096
#define ICE_MAILBOXQ_BUF_SZ          4096
/* Number of queues per TC should be one of 1, 2, 4, 8, 16, 32, 64, 128, 256 */
#define ICE_MAX_Q_PER_TC         256
#define ICE_NUM_DESC_DEFAULT     512
#define ICE_BUF_SIZE_MIN         1024
#define ICE_FRAME_SIZE_MAX       9728
#define ICE_QUEUE_BASE_ADDR_UNIT 128
/* number of VSIs and queue default setting */
#define ICE_MAX_QP_NUM_PER_VF    16
#define ICE_DEFAULT_QP_NUM_FDIR  1
#define ICE_UINT32_BIT_SIZE      (CHAR_BIT * sizeof(uint32_t))
#define ICE_VFTA_SIZE            (4096 / ICE_UINT32_BIT_SIZE)
/* Maximun number of MAC addresses */
#define ICE_NUM_MACADDR_MAX       64
/* Maximum number of VFs */
#define ICE_MAX_VF               128
#define ICE_MAX_INTR_QUEUE_NUM   256

#define ICE_MISC_VEC_ID          RTE_INTR_VEC_ZERO_OFFSET
#define ICE_RX_VEC_ID            RTE_INTR_VEC_RXTX_OFFSET

#define ICE_MAX_PKT_TYPE  1024

/* DDP package search path */
#define ICE_PKG_FILE_DEFAULT "/lib/firmware/intel/ice/ddp/ice.pkg"
#define ICE_PKG_FILE_UPDATES "/lib/firmware/updates/intel/ice/ddp/ice.pkg"
#define ICE_PKG_FILE_SEARCH_PATH_DEFAULT "/lib/firmware/intel/ice/ddp/"
#define ICE_PKG_FILE_SEARCH_PATH_UPDATES "/lib/firmware/updates/intel/ice/ddp/"
#define ICE_MAX_PKG_FILENAME_SIZE   256

#define MAX_ACL_NORMAL_ENTRIES    256

/**
 * vlan_id is a 12 bit number.
 * The VFTA array is actually a 4096 bit array, 128 of 32bit elements.
 * 2^5 = 32. The val of lower 5 bits specifies the bit in the 32bit element.
 * The higher 7 bit val specifies VFTA array index.
 */
#define ICE_VFTA_BIT(vlan_id)    (1 << ((vlan_id) & 0x1F))
#define ICE_VFTA_IDX(vlan_id)    ((vlan_id) >> 5)

/* Default TC traffic in case DCB is not enabled */
#define ICE_DEFAULT_TCMAP        0x1
#define ICE_FDIR_QUEUE_ID        0

/* Always assign pool 0 to main VSI, VMDQ will start from 1 */
#define ICE_VMDQ_POOL_BASE       1

#define ICE_DEFAULT_RX_FREE_THRESH  32
#define ICE_DEFAULT_RX_PTHRESH      8
#define ICE_DEFAULT_RX_HTHRESH      8
#define ICE_DEFAULT_RX_WTHRESH      0

#define ICE_DEFAULT_TX_FREE_THRESH  32
#define ICE_DEFAULT_TX_PTHRESH      32
#define ICE_DEFAULT_TX_HTHRESH      0
#define ICE_DEFAULT_TX_WTHRESH      0
#define ICE_DEFAULT_TX_RSBIT_THRESH 32

/* Bit shift and mask */
#define ICE_4_BIT_WIDTH  (CHAR_BIT / 2)
#define ICE_4_BIT_MASK   RTE_LEN2MASK(ICE_4_BIT_WIDTH, uint8_t)
#define ICE_8_BIT_WIDTH  CHAR_BIT
#define ICE_8_BIT_MASK   UINT8_MAX
#define ICE_16_BIT_WIDTH (CHAR_BIT * 2)
#define ICE_16_BIT_MASK  UINT16_MAX
#define ICE_32_BIT_WIDTH (CHAR_BIT * 4)
#define ICE_32_BIT_MASK  UINT32_MAX
#define ICE_40_BIT_WIDTH (CHAR_BIT * 5)
#define ICE_40_BIT_MASK  RTE_LEN2MASK(ICE_40_BIT_WIDTH, uint64_t)
#define ICE_48_BIT_WIDTH (CHAR_BIT * 6)
#define ICE_48_BIT_MASK  RTE_LEN2MASK(ICE_48_BIT_WIDTH, uint64_t)

#define ICE_FLAG_RSS                   BIT_ULL(0)
#define ICE_FLAG_DCB                   BIT_ULL(1)
#define ICE_FLAG_VMDQ                  BIT_ULL(2)
#define ICE_FLAG_SRIOV                 BIT_ULL(3)
#define ICE_FLAG_HEADER_SPLIT_DISABLED BIT_ULL(4)
#define ICE_FLAG_HEADER_SPLIT_ENABLED  BIT_ULL(5)
#define ICE_FLAG_FDIR                  BIT_ULL(6)
#define ICE_FLAG_VXLAN                 BIT_ULL(7)
#define ICE_FLAG_RSS_AQ_CAPABLE        BIT_ULL(8)
#define ICE_FLAG_VF_MAC_BY_PF          BIT_ULL(9)
#define ICE_FLAG_ALL  (ICE_FLAG_RSS | \
		       ICE_FLAG_DCB | \
		       ICE_FLAG_VMDQ | \
		       ICE_FLAG_SRIOV | \
		       ICE_FLAG_HEADER_SPLIT_DISABLED | \
		       ICE_FLAG_HEADER_SPLIT_ENABLED | \
		       ICE_FLAG_FDIR | \
		       ICE_FLAG_VXLAN | \
		       ICE_FLAG_RSS_AQ_CAPABLE | \
		       ICE_FLAG_VF_MAC_BY_PF)

#define ICE_RSS_OFFLOAD_ALL ( \
	RTE_ETH_RSS_IPV4 | \
	RTE_ETH_RSS_FRAG_IPV4 | \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
	RTE_ETH_RSS_NONFRAG_IPV4_OTHER | \
	RTE_ETH_RSS_IPV6 | \
	RTE_ETH_RSS_FRAG_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
	RTE_ETH_RSS_NONFRAG_IPV6_OTHER | \
	RTE_ETH_RSS_L2_PAYLOAD)

/**
 * The overhead from MTU to max frame size.
 * Considering QinQ packet, the VLAN tag needs to be counted twice.
 */
#define ICE_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + RTE_VLAN_HLEN * 2)
#define ICE_ETH_MAX_LEN (RTE_ETHER_MTU + ICE_ETH_OVERHEAD)

#define ICE_RXTX_BYTES_HIGH(bytes) ((bytes) & ~ICE_40_BIT_MASK)
#define ICE_RXTX_BYTES_LOW(bytes) ((bytes) & ICE_40_BIT_MASK)

/* Max number of flexible descriptor rxdid */
#define ICE_FLEX_DESC_RXDID_MAX_NUM 64

#define ICE_I2C_EEPROM_DEV_ADDR		0xA0
#define ICE_I2C_EEPROM_DEV_ADDR2	0xA2
#define ICE_MODULE_TYPE_SFP		0x03
#define ICE_MODULE_TYPE_QSFP_PLUS	0x0D
#define ICE_MODULE_TYPE_QSFP28		0x11
#define ICE_MODULE_SFF_ADDR_MODE	0x04
#define ICE_MODULE_SFF_DIAG_CAPAB	0x40
#define ICE_MODULE_REVISION_ADDR	0x01
#define ICE_MODULE_SFF_8472_COMP	0x5E
#define ICE_MODULE_SFF_8472_SWAP	0x5C
#define ICE_MODULE_QSFP_MAX_LEN		640

/* EEPROM Standards for plug in modules */
#define ICE_MODULE_SFF_8079		0x1
#define ICE_MODULE_SFF_8079_LEN		256
#define ICE_MODULE_SFF_8472		0x2
#define ICE_MODULE_SFF_8472_LEN		512
#define ICE_MODULE_SFF_8636		0x3
#define ICE_MODULE_SFF_8636_LEN		256
#define ICE_MODULE_SFF_8636_MAX_LEN     640
#define ICE_MODULE_SFF_8436		0x4
#define ICE_MODULE_SFF_8436_LEN		256
#define ICE_MODULE_SFF_8436_MAX_LEN     640


/* Per-channel register definitions */
#define GLTSYN_AUX_OUT(_chan, _idx)     (GLTSYN_AUX_OUT_0(_idx) + ((_chan) * 8))
#define GLTSYN_CLKO(_chan, _idx)        (GLTSYN_CLKO_0(_idx) + ((_chan) * 8))
#define GLTSYN_TGT_L(_chan, _idx)       (GLTSYN_TGT_L_0(_idx) + ((_chan) * 16))
#define GLTSYN_TGT_H(_chan, _idx)       (GLTSYN_TGT_H_0(_idx) + ((_chan) * 16))

/* DDP package type */
enum ice_pkg_type {
	ICE_PKG_TYPE_UNKNOWN,
	ICE_PKG_TYPE_OS_DEFAULT,
	ICE_PKG_TYPE_COMMS,
};

enum pps_type {
	PPS_NONE,
	PPS_PIN,
	PPS_MAX,
};

struct ice_adapter;

/**
 * MAC filter structure
 */
struct ice_mac_filter_info {
	struct rte_ether_addr mac_addr;
};

TAILQ_HEAD(ice_mac_filter_list, ice_mac_filter);

/* MAC filter list structure */
struct ice_mac_filter {
	TAILQ_ENTRY(ice_mac_filter) next;
	struct ice_mac_filter_info mac_info;
};

struct ice_vlan {
	uint16_t tpid;
	uint16_t vid;
};

#define ICE_VLAN(tpid, vid) \
	((struct ice_vlan){ tpid, vid })

/**
 * VLAN filter structure
 */
struct ice_vlan_filter_info {
	struct ice_vlan vlan;
};

TAILQ_HEAD(ice_vlan_filter_list, ice_vlan_filter);

/* VLAN filter list structure */
struct ice_vlan_filter {
	TAILQ_ENTRY(ice_vlan_filter) next;
	struct ice_vlan_filter_info vlan_info;
};

struct pool_entry {
	LIST_ENTRY(pool_entry) next;
	uint16_t base;
	uint16_t len;
};

LIST_HEAD(res_list, pool_entry);

struct ice_res_pool_info {
	uint32_t base;              /* Resource start index */
	uint32_t num_alloc;         /* Allocated resource number */
	uint32_t num_free;          /* Total available resource number */
	struct res_list alloc_list; /* Allocated resource list */
	struct res_list free_list;  /* Available resource list */
};

TAILQ_HEAD(ice_vsi_list_head, ice_vsi_list);

struct ice_vsi;

/* VSI list structure */
struct ice_vsi_list {
	TAILQ_ENTRY(ice_vsi_list) list;
	struct ice_vsi *vsi;
};

struct ice_rx_queue;
struct ice_tx_queue;

/**
 * Structure that defines a VSI, associated with a adapter.
 */
struct ice_vsi {
	struct ice_adapter *adapter; /* Backreference to associated adapter */
	struct ice_aqc_vsi_props info; /* VSI properties */
	/**
	 * When drivers loaded, only a default main VSI exists. In case new VSI
	 * needs to add, HW needs to know the layout that VSIs are organized.
	 * Besides that, VSI isan element and can't switch packets, which needs
	 * to add new component VEB to perform switching. So, a new VSI needs
	 * to specify the uplink VSI (Parent VSI) before created. The
	 * uplink VSI will check whether it had a VEB to switch packets. If no,
	 * it will try to create one. Then, uplink VSI will move the new VSI
	 * into its' sib_vsi_list to manage all the downlink VSI.
	 *  sib_vsi_list: the VSI list that shared the same uplink VSI.
	 *  parent_vsi  : the uplink VSI. It's NULL for main VSI.
	 *  veb         : the VEB associates with the VSI.
	 */
	struct ice_vsi_list sib_vsi_list; /* sibling vsi list */
	struct ice_vsi *parent_vsi;
	enum ice_vsi_type type; /* VSI types */
	uint16_t vlan_num;       /* Total VLAN number */
	uint16_t mac_num;        /* Total mac number */
	struct ice_mac_filter_list mac_list; /* macvlan filter list */
	struct ice_vlan_filter_list vlan_list; /* vlan filter list */
	uint16_t nb_qps;         /* Number of queue pairs VSI can occupy */
	uint16_t nb_used_qps;    /* Number of queue pairs VSI uses */
	uint16_t max_macaddrs;   /* Maximum number of MAC addresses */
	uint16_t base_queue;     /* The first queue index of this VSI */
	uint16_t vsi_id;         /* Hardware Id */
	uint16_t idx;            /* vsi_handle: SW index in hw->vsi_ctx */
	/* VF number to which the VSI connects, valid when VSI is VF type */
	uint8_t vf_num;
	uint16_t msix_intr; /* The MSIX interrupt binds to VSI */
	uint16_t nb_msix;   /* The max number of msix vector */
	uint8_t enabled_tc; /* The traffic class enabled */
	uint8_t vlan_anti_spoof_on; /* The VLAN anti-spoofing enabled */
	uint8_t vlan_filter_on; /* The VLAN filter enabled */
	/* information about rss configuration */
	u32 rss_key_size;
	u32 rss_lut_size;
	uint8_t *rss_lut;
	uint8_t *rss_key;
	struct ice_eth_stats eth_stats_offset;
	struct ice_eth_stats eth_stats;
	bool offset_loaded;
	uint64_t old_rx_bytes;
	uint64_t old_tx_bytes;
};

enum proto_xtr_type {
	PROTO_XTR_NONE,
	PROTO_XTR_VLAN,
	PROTO_XTR_IPV4,
	PROTO_XTR_IPV6,
	PROTO_XTR_IPV6_FLOW,
	PROTO_XTR_TCP,
	PROTO_XTR_IP_OFFSET,
	PROTO_XTR_MAX /* The last one */
};

enum ice_fdir_tunnel_type {
	ICE_FDIR_TUNNEL_TYPE_NONE = 0,
	ICE_FDIR_TUNNEL_TYPE_VXLAN,
	ICE_FDIR_TUNNEL_TYPE_GTPU,
	ICE_FDIR_TUNNEL_TYPE_GTPU_EH,
};

struct rte_flow;
TAILQ_HEAD(ice_flow_list, rte_flow);

struct ice_flow_parser_node;
TAILQ_HEAD(ice_parser_list, ice_flow_parser_node);

struct ice_fdir_filter_conf {
	struct ice_fdir_fltr input;
	enum ice_fdir_tunnel_type tunnel_type;

	struct ice_fdir_counter *counter; /* flow specific counter context */
	struct rte_flow_action_count act_count;

	uint64_t input_set_o; /* used for non-tunnel or tunnel outer fields */
	uint64_t input_set_i; /* only for tunnel inner fields */
	uint32_t mark_flag;

	struct ice_parser_profile *prof;
	bool parser_ena;
	u8 *pkt_buf;
	u8 pkt_len;
};

#define ICE_MAX_FDIR_FILTER_NUM		(1024 * 16)

struct ice_fdir_fltr_pattern {
	enum ice_fltr_ptype flow_type;

	union {
		struct ice_fdir_v4 v4;
		struct ice_fdir_v6 v6;
	} ip, mask;

	struct ice_fdir_udp_gtp gtpu_data;
	struct ice_fdir_udp_gtp gtpu_mask;

	struct ice_fdir_extra ext_data;
	struct ice_fdir_extra ext_mask;

	enum ice_fdir_tunnel_type tunnel_type;
};

#define ICE_FDIR_COUNTER_DEFAULT_POOL_SIZE	1
#define ICE_FDIR_COUNTER_MAX_POOL_SIZE		32
#define ICE_FDIR_COUNTERS_PER_BLOCK		256
#define ICE_FDIR_COUNTER_INDEX(base_idx) \
				((base_idx) * ICE_FDIR_COUNTERS_PER_BLOCK)
struct ice_fdir_counter_pool;

struct ice_fdir_counter {
	TAILQ_ENTRY(ice_fdir_counter) next;
	struct ice_fdir_counter_pool *pool;
	uint8_t shared;
	uint32_t ref_cnt;
	uint32_t id;
	uint64_t hits;
	uint64_t bytes;
	uint32_t hw_index;
};

TAILQ_HEAD(ice_fdir_counter_list, ice_fdir_counter);

struct ice_fdir_counter_pool {
	TAILQ_ENTRY(ice_fdir_counter_pool) next;
	struct ice_fdir_counter_list counter_list;
	struct ice_fdir_counter counters[0];
};

TAILQ_HEAD(ice_fdir_counter_pool_list, ice_fdir_counter_pool);

struct ice_fdir_counter_pool_container {
	struct ice_fdir_counter_pool_list pool_list;
	struct ice_fdir_counter_pool *pools[ICE_FDIR_COUNTER_MAX_POOL_SIZE];
	uint8_t index_free;
};

/**
 *  A structure used to define fields of a FDIR related info.
 */
struct ice_fdir_info {
	struct ice_vsi *fdir_vsi;     /* pointer to fdir VSI structure */
	struct ice_tx_queue *txq;
	struct ice_rx_queue *rxq;
	void *prg_pkt;                 /* memory for fdir program packet */
	uint64_t dma_addr;             /* physic address of packet memory*/
	const struct rte_memzone *mz;
	struct ice_fdir_filter_conf conf;

	struct ice_fdir_filter_conf **hash_map;
	struct rte_hash *hash_table;

	struct ice_fdir_counter_pool_container counter;
};

#define ICE_HASH_GTPU_CTX_EH_IP		0
#define ICE_HASH_GTPU_CTX_EH_IP_UDP	1
#define ICE_HASH_GTPU_CTX_EH_IP_TCP	2
#define ICE_HASH_GTPU_CTX_UP_IP		3
#define ICE_HASH_GTPU_CTX_UP_IP_UDP	4
#define ICE_HASH_GTPU_CTX_UP_IP_TCP	5
#define ICE_HASH_GTPU_CTX_DW_IP		6
#define ICE_HASH_GTPU_CTX_DW_IP_UDP	7
#define ICE_HASH_GTPU_CTX_DW_IP_TCP	8
#define ICE_HASH_GTPU_CTX_MAX		9

struct ice_hash_gtpu_ctx {
	struct ice_rss_hash_cfg ctx[ICE_HASH_GTPU_CTX_MAX];
};

struct ice_hash_ctx {
	struct ice_hash_gtpu_ctx gtpu4;
	struct ice_hash_gtpu_ctx gtpu6;
};

struct ice_acl_conf {
	struct ice_fdir_fltr input;
	uint64_t input_set;
};

/**
 * A structure used to define fields of ACL related info.
 */
struct ice_acl_info {
	struct ice_acl_conf conf;
	struct rte_bitmap *slots;
	uint64_t hw_entry_id[MAX_ACL_NORMAL_ENTRIES];
};

TAILQ_HEAD(ice_shaper_profile_list, ice_tm_shaper_profile);
TAILQ_HEAD(ice_tm_node_list, ice_tm_node);

struct ice_tm_shaper_profile {
	TAILQ_ENTRY(ice_tm_shaper_profile) node;
	uint32_t shaper_profile_id;
	uint32_t reference_count;
	struct rte_tm_shaper_params profile;
};

/* Struct to store Traffic Manager node configuration. */
struct ice_tm_node {
	TAILQ_ENTRY(ice_tm_node) node;
	uint32_t id;
	uint32_t tc;
	uint32_t priority;
	uint32_t weight;
	uint32_t reference_count;
	struct ice_tm_node *parent;
	struct ice_tm_node **children;
	struct ice_tm_shaper_profile *shaper_profile;
	struct rte_tm_node_params params;
};

/* node type of Traffic Manager */
enum ice_tm_node_type {
	ICE_TM_NODE_TYPE_PORT,
	ICE_TM_NODE_TYPE_TC,
	ICE_TM_NODE_TYPE_VSI,
	ICE_TM_NODE_TYPE_QGROUP,
	ICE_TM_NODE_TYPE_QUEUE,
	ICE_TM_NODE_TYPE_MAX,
};

/* Struct to store all the Traffic Manager configuration. */
struct ice_tm_conf {
	struct ice_shaper_profile_list shaper_profile_list;
	struct ice_tm_node *root; /* root node - port */
	struct ice_tm_node_list tc_list; /* node list for all the TCs */
	struct ice_tm_node_list vsi_list; /* node list for all the VSIs */
	struct ice_tm_node_list qgroup_list; /* node list for all the queue groups */
	struct ice_tm_node_list queue_list; /* node list for all the queues */
	uint32_t nb_tc_node;
	uint32_t nb_vsi_node;
	uint32_t nb_qgroup_node;
	uint32_t nb_queue_node;
	bool committed;
};

struct ice_pf {
	struct ice_adapter *adapter; /* The adapter this PF associate to */
	struct ice_vsi *main_vsi; /* pointer to main VSI structure */
	/* Used for next free software vsi idx.
	 * To save the effort, we don't recycle the index.
	 * Suppose the indexes are more than enough.
	 */
	uint16_t next_vsi_idx;
	uint16_t vsis_allocated;
	uint16_t vsis_unallocated;
	struct ice_res_pool_info qp_pool;    /*Queue pair pool */
	struct ice_res_pool_info msix_pool;  /* MSIX interrupt pool */
	struct rte_eth_dev_data *dev_data; /* Pointer to the device data */
	struct rte_ether_addr dev_addr; /* PF device mac address */
	uint64_t flags; /* PF feature flags */
	uint16_t hash_lut_size; /* The size of hash lookup table */
	uint16_t lan_nb_qp_max;
	uint16_t lan_nb_qps; /* The number of queue pairs of LAN */
	uint16_t base_queue; /* The base queue pairs index  in the device */
	uint8_t *proto_xtr; /* Protocol extraction type for all queues */
	uint16_t fdir_nb_qps; /* The number of queue pairs of Flow Director */
	uint16_t fdir_qp_offset;
	struct ice_fdir_info fdir; /* flow director info */
	struct ice_acl_info acl; /* ACL info */
	struct ice_hash_ctx hash_ctx;
	uint16_t hw_prof_cnt[ICE_FLTR_PTYPE_MAX][ICE_FD_HW_SEG_MAX];
	uint16_t fdir_fltr_cnt[ICE_FLTR_PTYPE_MAX][ICE_FD_HW_SEG_MAX];
	struct ice_hw_port_stats stats_offset;
	struct ice_hw_port_stats stats;
	/* internal packet statistics, it should be excluded from the total */
	struct ice_eth_stats internal_stats_offset;
	struct ice_eth_stats internal_stats;
	bool offset_loaded;
	bool adapter_stopped;
	struct ice_flow_list flow_list;
	rte_spinlock_t flow_ops_lock;
	bool init_link_up;
	uint64_t old_rx_bytes;
	uint64_t old_tx_bytes;
	uint64_t supported_rxdid; /* bitmap for supported RXDID */
	uint64_t rss_hf;
	struct ice_tm_conf tm_conf;
	uint16_t outer_ethertype;
};

#define ICE_MAX_QUEUE_NUM  2048
#define ICE_MAX_PIN_NUM   4

/**
 * Cache devargs parse result.
 */
struct ice_devargs {
	int rx_low_latency;
	int safe_mode_support;
	uint8_t proto_xtr_dflt;
	uint8_t default_mac_disable;
	uint8_t proto_xtr[ICE_MAX_QUEUE_NUM];
	uint8_t pin_idx;
	uint8_t pps_out_ena;
	int xtr_field_offs;
	uint8_t xtr_flag_offs[PROTO_XTR_MAX];
	/* Name of the field. */
	char xtr_field_name[RTE_MBUF_DYN_NAMESIZE];
};

/**
 * Structure to store fdir fv entry.
 */
struct ice_fdir_prof_info {
	struct ice_parser_profile prof;
	u64 fdir_actived_cnt;
};

/**
 * Structure to store rss fv entry.
 */
struct ice_rss_prof_info {
	struct ice_parser_profile prof;
	bool symm;
};

/**
 * Structure to store private data for each PF/VF instance.
 */
struct ice_adapter {
	/* Common for both PF and VF */
	struct ice_hw hw;
	struct ice_pf pf;
	bool rx_bulk_alloc_allowed;
	bool rx_vec_allowed;
	bool tx_vec_allowed;
	bool tx_simple_allowed;
	/* ptype mapping table */
	uint32_t ptype_tbl[ICE_MAX_PKT_TYPE] __rte_cache_min_aligned;
	bool is_safe_mode;
	struct ice_devargs devargs;
	enum ice_pkg_type active_pkg_type; /* loaded ddp package type */
	uint16_t fdir_ref_cnt;
	/* For PTP */
	struct rte_timecounter systime_tc;
	struct rte_timecounter rx_tstamp_tc;
	struct rte_timecounter tx_tstamp_tc;
	bool ptp_ena;
	uint64_t time_hw;
	struct ice_fdir_prof_info fdir_prof_info[ICE_MAX_PTGS];
	struct ice_rss_prof_info rss_prof_info[ICE_MAX_PTGS];
	/* True if DCF state of the associated PF is on */
	bool dcf_state_on;
	/* Set bit if the engine is disabled */
	unsigned long disabled_engine_mask;
	struct ice_parser *psr;
#ifdef RTE_ARCH_X86
	bool rx_use_avx2;
	bool rx_use_avx512;
	bool tx_use_avx2;
	bool tx_use_avx512;
	bool rx_vec_offload_support;
#endif
};

struct ice_vsi_vlan_pvid_info {
	uint16_t on;		/* Enable or disable pvid */
	union {
		uint16_t pvid;	/* Valid in case 'on' is set to set pvid */
		struct {
			/* Valid in case 'on' is cleared. 'tagged' will reject
			 * tagged packets, while 'untagged' will reject
			 * untagged packets.
			 */
			uint8_t tagged;
			uint8_t untagged;
		} reject;
	} config;
};

#define ICE_DEV_TO_PCI(eth_dev) \
	RTE_DEV_TO_PCI((eth_dev)->device)

/* ICE_DEV_PRIVATE_TO */
#define ICE_DEV_PRIVATE_TO_PF(adapter) \
	(&((struct ice_adapter *)adapter)->pf)
#define ICE_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct ice_adapter *)adapter)->hw)
#define ICE_DEV_PRIVATE_TO_ADAPTER(adapter) \
	((struct ice_adapter *)adapter)

/* ICE_VSI_TO */
#define ICE_VSI_TO_HW(vsi) \
	(&(((struct ice_vsi *)vsi)->adapter->hw))
#define ICE_VSI_TO_PF(vsi) \
	(&(((struct ice_vsi *)vsi)->adapter->pf))

/* ICE_PF_TO */
#define ICE_PF_TO_HW(pf) \
	(&(((struct ice_pf *)pf)->adapter->hw))
#define ICE_PF_TO_ADAPTER(pf) \
	((struct ice_adapter *)(pf)->adapter)
#define ICE_PF_TO_ETH_DEV(pf) \
	(((struct ice_pf *)pf)->adapter->eth_dev)

bool is_ice_supported(struct rte_eth_dev *dev);
int
ice_load_pkg(struct ice_adapter *adapter, bool use_dsn, uint64_t dsn);
struct ice_vsi *
ice_setup_vsi(struct ice_pf *pf, enum ice_vsi_type type);
int
ice_release_vsi(struct ice_vsi *vsi);
void ice_vsi_enable_queues_intr(struct ice_vsi *vsi);
void ice_vsi_disable_queues_intr(struct ice_vsi *vsi);
void ice_vsi_queues_bind_intr(struct ice_vsi *vsi);
int ice_add_rss_cfg_wrap(struct ice_pf *pf, uint16_t vsi_id,
			 struct ice_rss_hash_cfg *cfg);
int ice_rem_rss_cfg_wrap(struct ice_pf *pf, uint16_t vsi_id,
			 struct ice_rss_hash_cfg *cfg);
void ice_tm_conf_init(struct rte_eth_dev *dev);
void ice_tm_conf_uninit(struct rte_eth_dev *dev);
extern const struct rte_tm_ops ice_tm_ops;

static inline int
ice_align_floor(int n)
{
	if (n == 0)
		return 0;
	return 1 << (sizeof(n) * CHAR_BIT - 1 - rte_clz32(n));
}

#define ICE_PHY_TYPE_SUPPORT_50G(phy_type) \
	(((phy_type) & ICE_PHY_TYPE_LOW_50GBASE_CR2) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50GBASE_SR2) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50GBASE_LR2) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50GBASE_KR2) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50G_LAUI2_AOC_ACC) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50G_LAUI2) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50G_AUI2_AOC_ACC) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50G_AUI2) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50GBASE_CP) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50GBASE_SR) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50GBASE_FR) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50GBASE_LR) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50GBASE_KR_PAM4) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50G_AUI1_AOC_ACC) || \
	((phy_type) & ICE_PHY_TYPE_LOW_50G_AUI1))

#define ICE_PHY_TYPE_SUPPORT_100G_LOW(phy_type) \
	(((phy_type) & ICE_PHY_TYPE_LOW_100GBASE_CR4) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100GBASE_SR4) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100GBASE_LR4) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100GBASE_KR4) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100G_CAUI4_AOC_ACC) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100G_CAUI4) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100G_AUI4_AOC_ACC) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100G_AUI4) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100GBASE_CR_PAM4) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100GBASE_KR_PAM4) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100GBASE_CP2) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100GBASE_SR2) || \
	((phy_type) & ICE_PHY_TYPE_LOW_100GBASE_DR))

#define ICE_PHY_TYPE_SUPPORT_100G_HIGH(phy_type) \
	(((phy_type) & ICE_PHY_TYPE_HIGH_100GBASE_KR2_PAM4) || \
	((phy_type) & ICE_PHY_TYPE_HIGH_100G_CAUI2_AOC_ACC) || \
	((phy_type) & ICE_PHY_TYPE_HIGH_100G_CAUI2) || \
	((phy_type) & ICE_PHY_TYPE_HIGH_100G_AUI2_AOC_ACC) || \
	((phy_type) & ICE_PHY_TYPE_HIGH_100G_AUI2))

__rte_experimental
int rte_pmd_ice_dump_package(uint16_t port, uint8_t **buff, uint32_t *size);

__rte_experimental
int rte_pmd_ice_dump_switch(uint16_t port, uint8_t **buff, uint32_t *size);
#endif /* _ICE_ETHDEV_H_ */
