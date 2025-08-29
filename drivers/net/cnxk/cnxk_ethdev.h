/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CNXK_ETHDEV_H__
#define __CNXK_ETHDEV_H__

#include <math.h>
#include <stdint.h>

#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_compat.h>
#include <rte_kvargs.h>
#include <rte_mbuf.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_mempool.h>
#include <rte_mtr_driver.h>
#include <rte_security.h>
#include <rte_security_driver.h>
#include <rte_tailq.h>
#include <rte_time.h>
#include <rte_tm_driver.h>

#include "roc_api.h"
#include <cnxk_ethdev_dp.h>

#define CNXK_ETH_DEV_PMD_VERSION "1.0"

/* Used for struct cnxk_eth_dev::flags */
#define CNXK_LINK_CFG_IN_PROGRESS_F BIT_ULL(0)

/* VLAN tag inserted by NIX_TX_VTAG_ACTION.
 * In Tx space is always reserved for this in FRS.
 */
#define CNXK_NIX_MAX_VTAG_INS	   2
#define CNXK_NIX_MAX_VTAG_ACT_SIZE (4 * CNXK_NIX_MAX_VTAG_INS)

/* ETH_HLEN+ETH_FCS+2*VLAN_HLEN */
#define CNXK_NIX_L2_OVERHEAD (RTE_ETHER_HDR_LEN + \
			      RTE_ETHER_CRC_LEN + \
			      CNXK_NIX_MAX_VTAG_ACT_SIZE)

#define CNXK_NIX_RX_MIN_DESC	    16
#define CNXK_NIX_RX_MIN_DESC_ALIGN  16
#define CNXK_NIX_RX_NB_SEG_MAX	    6
#define CNXK_NIX_RX_DEFAULT_RING_SZ 4096
/* Max supported SQB count */
#define CNXK_NIX_TX_MAX_SQB 512
/* LPB & SPB */
#define CNXK_NIX_NUM_POOLS_MAX 2

#define CNXK_NIX_DEF_SQ_COUNT 512

#define CNXK_NIX_RSS_L3_L4_SRC_DST                                                                 \
	(RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY | RTE_ETH_RSS_L4_SRC_ONLY |             \
	 RTE_ETH_RSS_L4_DST_ONLY)

#define CNXK_NIX_RSS_OFFLOAD                                                                       \
	(RTE_ETH_RSS_PORT | RTE_ETH_RSS_IP | RTE_ETH_RSS_UDP | RTE_ETH_RSS_TCP |                   \
	 RTE_ETH_RSS_SCTP | RTE_ETH_RSS_TUNNEL | RTE_ETH_RSS_L2_PAYLOAD |                          \
	 CNXK_NIX_RSS_L3_L4_SRC_DST | RTE_ETH_RSS_LEVEL_MASK | RTE_ETH_RSS_C_VLAN |                \
	 RTE_ETH_RSS_ESP | RTE_ETH_RSS_IB_BTH)

#define CNXK_NIX_TX_OFFLOAD_CAPA                                                                   \
	(RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE | RTE_ETH_TX_OFFLOAD_MT_LOCKFREE |                      \
	 RTE_ETH_TX_OFFLOAD_VLAN_INSERT | RTE_ETH_TX_OFFLOAD_QINQ_INSERT |                         \
	 RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM | RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM |                \
	 RTE_ETH_TX_OFFLOAD_TCP_CKSUM | RTE_ETH_TX_OFFLOAD_UDP_CKSUM |                             \
	 RTE_ETH_TX_OFFLOAD_SCTP_CKSUM | RTE_ETH_TX_OFFLOAD_TCP_TSO |                              \
	 RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO | RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |                    \
	 RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO | RTE_ETH_TX_OFFLOAD_MULTI_SEGS |                          \
	 RTE_ETH_TX_OFFLOAD_IPV4_CKSUM | RTE_ETH_TX_OFFLOAD_SECURITY)

#define CNXK_NIX_RX_OFFLOAD_CAPA                                                                   \
	(RTE_ETH_RX_OFFLOAD_CHECKSUM | RTE_ETH_RX_OFFLOAD_SCTP_CKSUM |                             \
	 RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM | RTE_ETH_RX_OFFLOAD_SCATTER |                        \
	 RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM | RTE_ETH_RX_OFFLOAD_RSS_HASH |                        \
	 RTE_ETH_RX_OFFLOAD_TIMESTAMP | RTE_ETH_RX_OFFLOAD_VLAN_STRIP |                            \
	 RTE_ETH_RX_OFFLOAD_SECURITY)

#define RSS_IPV4_ENABLE                                                                            \
	(RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_UDP |                 \
	 RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_NONFRAG_IPV4_SCTP)

#define RSS_IPV6_ENABLE                                                                            \
	(RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_UDP |                 \
	 RTE_ETH_RSS_NONFRAG_IPV6_TCP | RTE_ETH_RSS_NONFRAG_IPV6_SCTP)

#define RSS_IPV6_EX_ENABLE (RTE_ETH_RSS_IPV6_EX | RTE_ETH_RSS_IPV6_TCP_EX | RTE_ETH_RSS_IPV6_UDP_EX)

#define RSS_MAX_LEVELS 3

#define RSS_IPV4_INDEX 0
#define RSS_IPV6_INDEX 1
#define RSS_TCP_INDEX  2
#define RSS_UDP_INDEX  3
#define RSS_SCTP_INDEX 4
#define RSS_DMAC_INDEX 5

/* Default mark value used when none is provided. */
#define CNXK_NIX_MTR_COUNT_MAX	      73 /* 64(leaf) + 8(mid) + 1(top) */

/* Default cycle counter mask */
#define CNXK_CYCLECOUNTER_MASK     0xffffffffffffffffULL

/* Fastpath lookup */
#define CNXK_NIX_FASTPATH_LOOKUP_MEM "cnxk_nix_fastpath_lookup_mem"

#define CNXK_NIX_DMAC_IDX_INVALID -1

struct cnxk_fc_cfg {
	enum rte_eth_fc_mode mode;
	uint8_t rx_pause;
	uint8_t tx_pause;
};

struct cnxk_pfc_cfg {
	uint16_t class_en;
	uint16_t pause_time;
	uint16_t rx_pause_en;
	uint16_t tx_pause_en;
};

struct cnxk_eth_qconf {
	union {
		struct rte_eth_txconf tx;
		struct rte_eth_rxconf rx;
	} conf;
	struct rte_mempool *mp;
	uint16_t nb_desc;
	uint8_t valid;
};

struct cnxk_meter_node {
#define MAX_PRV_MTR_NODES 10
	TAILQ_ENTRY(cnxk_meter_node) next;
	/**< Pointer to the next flow meter structure. */
	uint32_t id; /**< Usr mtr id. */
	struct cnxk_mtr_profile_node *profile;
	struct cnxk_mtr_policy_node *policy;
	uint32_t bpf_id; /**< Hw mtr id. */
	uint32_t rq_num;
	uint32_t *rq_id;
	uint16_t level;
	uint32_t prev_id[MAX_PRV_MTR_NODES]; /**< Prev mtr id for chaining */
	uint32_t prev_cnt;
	uint32_t next_id; /**< Next mtr id for chaining */
	bool is_prev;
	bool is_next;
	struct rte_mtr_params params;
	struct roc_nix_bpf_objs profs;
	bool is_used;
	uint32_t ref_cnt;
};

struct action_rss {
	enum rte_eth_hash_function func;
	uint32_t level;
	uint64_t types;
	uint32_t key_len;
	uint32_t queue_num;
	uint8_t *key;
	uint16_t *queue;
};

struct policy_actions {
	uint32_t action_fate;
	union {
		uint16_t queue;
		uint32_t mtr_id;
		struct action_rss *rss_desc;
		bool skip_red;
	};
};

struct cnxk_mtr_policy_node {
	TAILQ_ENTRY(cnxk_mtr_policy_node) next;
	/**< Pointer to the next flow meter structure. */
	uint32_t id;	 /**< Policy id */
	uint32_t mtr_id; /** Meter id */
	struct rte_mtr_meter_policy_params policy;
	struct policy_actions actions[RTE_COLORS];
	uint32_t ref_cnt;
};

struct cnxk_mtr_profile_node {
	TAILQ_ENTRY(cnxk_mtr_profile_node) next;
	struct rte_mtr_meter_profile profile; /**< Profile detail. */
	uint32_t ref_cnt;		      /**< Use count. */
	uint32_t id;			      /**< Profile id. */
};

TAILQ_HEAD(cnxk_mtr_profiles, cnxk_mtr_profile_node);
TAILQ_HEAD(cnxk_mtr_policy, cnxk_mtr_policy_node);
TAILQ_HEAD(cnxk_mtr, cnxk_meter_node);

/* Security session private data */
struct cnxk_eth_sec_sess {
	/* List entry */
	TAILQ_ENTRY(cnxk_eth_sec_sess) entry;

	/* Inbound SA is from NIX_RX_IPSEC_SA_BASE or
	 * Outbound SA from roc_nix_inl_outb_sa_base_get()
	 */
	void *sa;

	/* SA index */
	uint32_t sa_idx;

	/* SPI */
	uint32_t spi;

	/* Back pointer to session */
	struct rte_security_session *sess;

	/* Inbound */
	bool inb;

	/* Inbound session on inl dev */
	bool inl_dev;

	/* Out-Of-Place processing */
	bool inb_oop;
};

TAILQ_HEAD(cnxk_eth_sec_sess_list, cnxk_eth_sec_sess);

/* Inbound security data */
struct cnxk_eth_dev_sec_inb {
	/* IPSec inbound min SPI */
	uint32_t min_spi;

	/* IPSec inbound max SPI */
	uint32_t max_spi;

	/* Using inbound with inline device */
	bool inl_dev;

	/* Device argument to disable inline device usage for inb */
	bool no_inl_dev;

	/* Active sessions */
	uint16_t nb_sess;

	/* List of sessions */
	struct cnxk_eth_sec_sess_list list;

	/* DPTR for WRITE_SA microcode op */
	void *sa_dptr;

	/* Number of oop sessions */
	uint16_t nb_oop;

	/* Reassembly enabled */
	bool reass_en;

	/* Lock to synchronize sa setup/release */
	rte_spinlock_t lock;

	/* Disable custom meta aura */
	bool custom_meta_aura_dis;

	/* Inline device CPT queue info */
	struct roc_nix_inl_dev_q *inl_dev_q;
};

/* Outbound security data */
struct cnxk_eth_dev_sec_outb {
	/* IPSec outbound max SA */
	uint16_t max_sa;

	/* Per CPT LF descriptor count */
	uint32_t nb_desc;

	/* SA Bitmap */
	struct plt_bitmap *sa_bmap;

	/* SA bitmap memory */
	void *sa_bmap_mem;

	/* SA base */
	uint64_t sa_base;

	/* CPT LF base */
	struct roc_cpt_lf *lf_base;

	/* Crypto queues => CPT lf count */
	uint16_t nb_crypto_qs;

	/* FC sw mem */
	uint64_t *fc_sw_mem;

	/* Active sessions */
	uint16_t nb_sess;

	/* List of sessions */
	struct cnxk_eth_sec_sess_list list;

	/* DPTR for WRITE_SA microcode op */
	void *sa_dptr;

	/* Lock to synchronize sa setup/release */
	rte_spinlock_t lock;

	/* Engine caps */
	uint64_t cpt_eng_caps;
};

/* MACsec session private data */
struct cnxk_macsec_sess {
	/* List entry */
	TAILQ_ENTRY(cnxk_macsec_sess) entry;

	/* Back pointer to session */
	struct rte_security_session *sess;
	enum mcs_direction dir;
	uint64_t sci;
	uint8_t secy_id;
	uint8_t sc_id;
	uint8_t flow_id;
};
TAILQ_HEAD(cnxk_macsec_sess_list, cnxk_macsec_sess);

struct cnxk_eth_dev {
	/* ROC NIX */
	struct roc_nix nix;

	/* ROC NPC */
	struct roc_npc npc;

	/* ROC RQs, SQs and CQs */
	struct roc_nix_rq *rqs;
	struct roc_nix_sq *sqs;
	struct roc_nix_cq *cqs;

	/* Configured queue count */
	uint16_t nb_rxq;
	uint16_t nb_txq;
	uint16_t nb_rxq_sso;
	uint8_t configured;

	/* Max macfilter entries */
	uint8_t dmac_filter_count;
	uint8_t max_mac_entries;
	bool dmac_filter_enable;
	int *dmac_idx_map;
	struct rte_ether_addr *dmac_addrs;

	uint16_t flags;
	uint8_t ptype_disable;
	bool scalar_ena;
	bool tx_compl_ena;
	bool tx_mark;
	bool ptp_en;

	/* Pointer back to rte */
	struct rte_eth_dev *eth_dev;

	/* HW capabilities / Limitations */
	union {
		struct {
			uint64_t cq_min_4k : 1;
			uint64_t ipsecd_drop_re_dis : 1;
			uint64_t vec_drop_re_dis : 1;
		};
		uint64_t hwcap;
	};

	/* Rx and Tx offload capabilities */
	uint64_t rx_offload_capa;
	uint64_t tx_offload_capa;
	uint32_t speed_capa;
	/* Configured Rx and Tx offloads */
	uint64_t rx_offloads;
	uint64_t tx_offloads;
	/* Platform specific offload flags */
	uint16_t rx_offload_flags;
	uint16_t tx_offload_flags;

	/* ETHDEV RSS HF bitmask */
	uint64_t ethdev_rss_hf;

	/* Saved qconf before lf realloc */
	struct cnxk_eth_qconf *tx_qconf;
	struct cnxk_eth_qconf *rx_qconf;

	/* Flow control configuration */
	struct cnxk_pfc_cfg pfc_cfg;
	struct cnxk_fc_cfg fc_cfg;

	/* PTP Counters */
	struct cnxk_timesync_info tstamp;
	struct rte_timecounter systime_tc;
	struct rte_timecounter rx_tstamp_tc;
	struct rte_timecounter tx_tstamp_tc;
	double clk_freq_mult;
	uint64_t clk_delta;

	/* Ingress policer */
	enum roc_nix_bpf_color precolor_tbl[ROC_NIX_BPF_PRECOLOR_TBL_SIZE_DSCP];
	enum rte_mtr_color_in_protocol proto;
	struct cnxk_mtr_profiles mtr_profiles;
	struct cnxk_mtr_policy mtr_policy;
	struct cnxk_mtr mtr;

	/* Congestion Management */
	struct rte_eth_cman_config cman_cfg;

	/* Rx burst for cleanup(Only Primary) */
	eth_rx_burst_t rx_pkt_burst_no_offload;

	/* Default mac address */
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];

	/* LSO Tunnel format indices */
	uint64_t lso_tun_fmt;

	/* Per queue statistics counters */
	uint32_t txq_stat_map[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	uint32_t rxq_stat_map[RTE_ETHDEV_QUEUE_STAT_CNTRS];

	/* Security data */
	struct cnxk_eth_dev_sec_inb inb;
	struct cnxk_eth_dev_sec_outb outb;

	/* Reassembly dynfield/flag offsets */
	int reass_dynfield_off;
	int reass_dynflag_bit;

	/* MCS device */
	struct cnxk_mcs_dev *mcs_dev;
	struct cnxk_macsec_sess_list mcs_list;

	/* Inject packets */
	struct cnxk_ethdev_inj_cfg inj_cfg;

	/* Eswitch domain ID */
	uint16_t switch_domain_id;

	/* SSO event dev */
	void *evdev_priv;

	/* SSO event dev ptp  */
	void (*cnxk_sso_ptp_tstamp_cb)
	     (uint16_t port_id, uint16_t flags, bool ptp_en);
};

struct cnxk_eth_rxq_sp {
	struct cnxk_eth_dev *dev;
	struct cnxk_eth_qconf qconf;
	uint16_t qid;
	uint8_t tx_pause;
	uint8_t tc;
} __plt_cache_aligned;

struct cnxk_eth_txq_sp {
	struct cnxk_eth_dev *dev;
	struct cnxk_eth_qconf qconf;
	uint16_t qid;
} __plt_cache_aligned;

static inline struct cnxk_eth_dev *
cnxk_eth_pmd_priv(const struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

static inline struct cnxk_eth_rxq_sp *
cnxk_eth_rxq_to_sp(void *__rxq)
{
	return ((struct cnxk_eth_rxq_sp *)__rxq) - 1;
}

static inline struct cnxk_eth_txq_sp *
cnxk_eth_txq_to_sp(void *__txq)
{
	return ((struct cnxk_eth_txq_sp *)__txq) - 1;
}

static inline int
cnxk_nix_tx_queue_count(uint64_t *mem, uint16_t sqes_per_sqb_log2)
{
	uint64_t val;

	val = rte_atomic_load_explicit((RTE_ATOMIC(uint64_t)*)mem, rte_memory_order_relaxed);
	val = (val << sqes_per_sqb_log2) - val;

	return (val & 0xFFFF);
}

static inline int
cnxk_nix_tx_queue_sec_count(uint64_t *mem, uint16_t sqes_per_sqb_log2, uint64_t *sec_fc)
{
	uint64_t sq_cnt, sec_cnt, val;

	sq_cnt = rte_atomic_load_explicit((RTE_ATOMIC(uint64_t)*)mem, rte_memory_order_relaxed);
	sq_cnt = (sq_cnt << sqes_per_sqb_log2) - sq_cnt;
	sec_cnt = rte_atomic_load_explicit((RTE_ATOMIC(uint64_t)*)sec_fc,
					   rte_memory_order_relaxed);
	val = RTE_MAX(sq_cnt, sec_cnt);

	return (val & 0xFFFF);
}

static inline int
cnxk_nix_inl_fc_check(uint64_t __rte_atomic *fc, int32_t *fc_sw, uint32_t nb_desc, uint16_t nb_inst)
{
	uint8_t retry_count = 32;
	int32_t val, newval;

	/* Check if there is any CPT instruction to submit */
	if (!nb_inst)
		return -EINVAL;

retry:
	val = rte_atomic_fetch_sub_explicit((RTE_ATOMIC(int32_t)*)fc_sw, nb_inst,
					    rte_memory_order_relaxed) - nb_inst;
	if (likely(val >= 0))
		return 0;

	newval = (int64_t)nb_desc - rte_atomic_load_explicit((RTE_ATOMIC(uint64_t)*)fc,
							     rte_memory_order_relaxed);
	newval -= nb_inst;

	if (!rte_atomic_compare_exchange_strong_explicit((RTE_ATOMIC(int32_t)*)fc_sw, &val, newval,
							 rte_memory_order_release,
							 rte_memory_order_relaxed)) {
		if (retry_count) {
			retry_count--;
			goto retry;
		} else {
			return -EAGAIN;
		}
	}
	if (unlikely(newval < 0))
		return -EAGAIN;

	return 0;
}

/* Common ethdev ops */
extern struct eth_dev_ops cnxk_eth_dev_ops;

/* Common flow ops */
extern struct rte_flow_ops cnxk_flow_ops;

/* Common security ops */
extern struct rte_security_ops cnxk_eth_sec_ops;

/* Common tm ops */
extern struct rte_tm_ops cnxk_tm_ops;

/* Platform specific rte pmd cnxk ops */
typedef uint16_t (*cnxk_inl_dev_submit_cb_t)(struct roc_nix_inl_dev_q *q, void *inst,
					     uint16_t nb_inst);

typedef void (*cnxk_ethdev_rx_offload_cb_t)(uint16_t port_id, uint64_t flags);

extern cnxk_ethdev_rx_offload_cb_t cnxk_ethdev_rx_offload_cb;

struct cnxk_ethdev_pmd_ops {
	cnxk_inl_dev_submit_cb_t inl_dev_submit;
};
extern struct cnxk_ethdev_pmd_ops cnxk_pmd_ops;

/* Ops */
int cnxk_nix_probe(struct rte_pci_driver *pci_drv,
		   struct rte_pci_device *pci_dev);
int cnxk_nix_remove(struct rte_pci_device *pci_dev);
int cnxk_nix_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu);
int cnxk_nix_sq_flush(struct rte_eth_dev *eth_dev);
int cnxk_nix_mc_addr_list_configure(struct rte_eth_dev *eth_dev,
				    struct rte_ether_addr *mc_addr_set,
				    uint32_t nb_mc_addr);
int cnxk_nix_mac_addr_add(struct rte_eth_dev *eth_dev,
			  struct rte_ether_addr *addr, uint32_t index,
			  uint32_t pool);
void cnxk_nix_mac_addr_del(struct rte_eth_dev *eth_dev, uint32_t index);
int cnxk_nix_mac_addr_set(struct rte_eth_dev *eth_dev,
			  struct rte_ether_addr *addr);
int cnxk_nix_promisc_enable(struct rte_eth_dev *eth_dev);
int cnxk_nix_promisc_disable(struct rte_eth_dev *eth_dev);
int cnxk_nix_allmulticast_enable(struct rte_eth_dev *eth_dev);
int cnxk_nix_allmulticast_disable(struct rte_eth_dev *eth_dev);
int cnxk_nix_info_get(struct rte_eth_dev *eth_dev,
		      struct rte_eth_dev_info *dev_info);
int cnxk_nix_rx_burst_mode_get(struct rte_eth_dev *eth_dev, uint16_t queue_id,
			       struct rte_eth_burst_mode *mode);
int cnxk_nix_tx_burst_mode_get(struct rte_eth_dev *eth_dev, uint16_t queue_id,
			       struct rte_eth_burst_mode *mode);
int cnxk_nix_flow_ctrl_set(struct rte_eth_dev *eth_dev,
			   struct rte_eth_fc_conf *fc_conf);
int cnxk_nix_flow_ctrl_get(struct rte_eth_dev *eth_dev,
			   struct rte_eth_fc_conf *fc_conf);
int cnxk_nix_priority_flow_ctrl_queue_config(struct rte_eth_dev *eth_dev,
					     struct rte_eth_pfc_queue_conf *pfc_conf);
int cnxk_nix_priority_flow_ctrl_queue_info_get(struct rte_eth_dev *eth_dev,
					       struct rte_eth_pfc_queue_info *pfc_info);
int cnxk_nix_set_link_up(struct rte_eth_dev *eth_dev);
int cnxk_nix_set_link_down(struct rte_eth_dev *eth_dev);
int cnxk_nix_get_module_info(struct rte_eth_dev *eth_dev,
			     struct rte_eth_dev_module_info *modinfo);
int cnxk_nix_get_module_eeprom(struct rte_eth_dev *eth_dev,
			       struct rte_dev_eeprom_info *info);
int cnxk_nix_rx_queue_intr_enable(struct rte_eth_dev *eth_dev,
				  uint16_t rx_queue_id);
int cnxk_nix_rx_queue_intr_disable(struct rte_eth_dev *eth_dev,
				   uint16_t rx_queue_id);
int cnxk_nix_pool_ops_supported(struct rte_eth_dev *eth_dev, const char *pool);
int cnxk_nix_tx_done_cleanup(void *txq, uint32_t free_cnt);
int cnxk_nix_flow_ops_get(struct rte_eth_dev *eth_dev,
			  const struct rte_flow_ops **ops);
int cnxk_nix_configure(struct rte_eth_dev *eth_dev);
int cnxk_nix_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			    uint16_t nb_desc, uint16_t fp_tx_q_sz,
			    const struct rte_eth_txconf *tx_conf);
int cnxk_nix_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			    uint32_t nb_desc, uint16_t fp_rx_q_sz,
			    const struct rte_eth_rxconf *rx_conf,
			    struct rte_mempool *mp);
int cnxk_nix_tx_queue_start(struct rte_eth_dev *eth_dev, uint16_t qid);
void cnxk_nix_tx_queue_release(struct rte_eth_dev *eth_dev, uint16_t qid);
int cnxk_nix_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qid);
int cnxk_nix_dev_start(struct rte_eth_dev *eth_dev);
int cnxk_nix_timesync_enable(struct rte_eth_dev *eth_dev);
int cnxk_nix_timesync_disable(struct rte_eth_dev *eth_dev);
int cnxk_nix_timesync_read_rx_timestamp(struct rte_eth_dev *eth_dev,
					struct timespec *timestamp,
					uint32_t flags);
int cnxk_nix_timesync_read_tx_timestamp(struct rte_eth_dev *eth_dev,
					struct timespec *timestamp);
int cnxk_nix_timesync_read_time(struct rte_eth_dev *eth_dev,
				struct timespec *ts);
int cnxk_nix_timesync_write_time(struct rte_eth_dev *eth_dev,
				 const struct timespec *ts);
int cnxk_nix_timesync_adjust_time(struct rte_eth_dev *eth_dev, int64_t delta);
int cnxk_nix_tsc_convert(struct cnxk_eth_dev *dev);
int cnxk_nix_read_clock(struct rte_eth_dev *eth_dev, uint64_t *clock);

uint64_t cnxk_nix_rxq_mbuf_setup(struct cnxk_eth_dev *dev);
int cnxk_nix_tm_ops_get(struct rte_eth_dev *eth_dev, void *ops);
int cnxk_nix_tm_set_queue_rate_limit(struct rte_eth_dev *eth_dev,
				     uint16_t queue_idx, uint32_t tx_rate);
int cnxk_nix_tm_mark_vlan_dei(struct rte_eth_dev *eth_dev, int mark_green,
			      int mark_yellow, int mark_red,
			      struct rte_tm_error *error);
int cnxk_nix_tm_mark_ip_ecn(struct rte_eth_dev *eth_dev, int mark_green,
			    int mark_yellow, int mark_red,
			    struct rte_tm_error *error);
int cnxk_nix_tm_mark_ip_dscp(struct rte_eth_dev *eth_dev, int mark_green,
			     int mark_yellow, int mark_red,
			     struct rte_tm_error *error);
int cnxk_nix_tx_descriptor_dump(const struct rte_eth_dev *eth_dev, uint16_t qid, uint16_t offset,
				uint16_t num, FILE *file);

/* MTR */
int cnxk_nix_mtr_ops_get(struct rte_eth_dev *dev, void *ops);

/* RSS */
uint32_t cnxk_rss_ethdev_to_nix(struct cnxk_eth_dev *dev, uint64_t ethdev_rss,
				uint8_t rss_level);
int cnxk_nix_reta_update(struct rte_eth_dev *eth_dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size);
int cnxk_nix_reta_query(struct rte_eth_dev *eth_dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size);
int cnxk_nix_rss_hash_update(struct rte_eth_dev *eth_dev,
			     struct rte_eth_rss_conf *rss_conf);
int cnxk_nix_rss_hash_conf_get(struct rte_eth_dev *eth_dev,
			       struct rte_eth_rss_conf *rss_conf);
int cnxk_nix_eth_dev_priv_dump(struct rte_eth_dev *eth_dev, FILE *file);

/* Link */
void cnxk_nix_toggle_flag_link_cfg(struct cnxk_eth_dev *dev, bool set);
void cnxk_eth_dev_link_status_cb(struct roc_nix *nix,
				 struct roc_nix_link_info *link);
void cnxk_eth_dev_link_status_get_cb(struct roc_nix *nix,
				     struct roc_nix_link_info *link);
void cnxk_eth_dev_q_err_cb(struct roc_nix *nix, void *data);
int cnxk_nix_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete);
int cnxk_nix_queue_stats_mapping(struct rte_eth_dev *dev, uint16_t queue_id,
				 uint8_t stat_idx, uint8_t is_rx);
int cnxk_nix_stats_reset(struct rte_eth_dev *dev);
int cnxk_nix_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
int cnxk_nix_xstats_get(struct rte_eth_dev *eth_dev,
			struct rte_eth_xstat *xstats, unsigned int n);
int cnxk_nix_xstats_get_names(struct rte_eth_dev *eth_dev,
			      struct rte_eth_xstat_name *xstats_names,
			      unsigned int limit);
int cnxk_nix_xstats_get_names_by_id(struct rte_eth_dev *eth_dev,
				    const uint64_t *ids,
				    struct rte_eth_xstat_name *xstats_names,
				    unsigned int limit);
int cnxk_nix_xstats_get_by_id(struct rte_eth_dev *eth_dev, const uint64_t *ids,
			      uint64_t *values, unsigned int n);
int cnxk_nix_xstats_reset(struct rte_eth_dev *eth_dev);
int cnxk_nix_fw_version_get(struct rte_eth_dev *eth_dev, char *fw_version,
			    size_t fw_size);
void cnxk_nix_rxq_info_get(struct rte_eth_dev *eth_dev, uint16_t qid,
			   struct rte_eth_rxq_info *qinfo);
void cnxk_nix_txq_info_get(struct rte_eth_dev *eth_dev, uint16_t qid,
			   struct rte_eth_txq_info *qinfo);

/* Queue status */
int cnxk_nix_rx_descriptor_status(void *rxq, uint16_t offset);
int cnxk_nix_tx_descriptor_status(void *txq, uint16_t offset);
int cnxk_nix_rx_queue_count(void *rxq);

/* Lookup configuration */
const uint32_t *cnxk_nix_supported_ptypes_get(struct rte_eth_dev *eth_dev,
					      size_t *no_of_elements);
void *cnxk_nix_fastpath_lookup_mem_get(void);

/* Devargs */
int cnxk_ethdev_parse_devargs(struct rte_devargs *devargs,
			      struct cnxk_eth_dev *dev);

/* Debug */
int cnxk_nix_dev_get_reg(struct rte_eth_dev *eth_dev,
			 struct rte_dev_reg_info *regs);
/* Security */
int cnxk_eth_outb_sa_idx_get(struct cnxk_eth_dev *dev, uint32_t *idx_p,
			     uint32_t spi);
int cnxk_eth_outb_sa_idx_put(struct cnxk_eth_dev *dev, uint32_t idx);
int cnxk_nix_lookup_mem_sa_base_set(struct cnxk_eth_dev *dev);
int cnxk_nix_lookup_mem_sa_base_clear(struct cnxk_eth_dev *dev);
int cnxk_nix_lookup_mem_metapool_set(struct cnxk_eth_dev *dev);
int cnxk_nix_lookup_mem_metapool_clear(struct cnxk_eth_dev *dev);
int cnxk_nix_lookup_mem_bufsize_set(struct cnxk_eth_dev *dev, uint64_t size);
int cnxk_nix_lookup_mem_bufsize_clear(struct cnxk_eth_dev *dev);
__rte_internal
int cnxk_nix_inb_mode_set(struct cnxk_eth_dev *dev, bool use_inl_dev);
__rte_internal
void cnxk_ethdev_rx_offload_cb_register(cnxk_ethdev_rx_offload_cb_t cb);

struct cnxk_eth_sec_sess *cnxk_eth_sec_sess_get_by_sa_idx(struct cnxk_eth_dev *dev,
							  uint32_t sa_idx, bool inb);
struct cnxk_eth_sec_sess *
cnxk_eth_sec_sess_get_by_sess(struct cnxk_eth_dev *dev,
			      struct rte_security_session *sess);
int cnxk_nix_inl_meta_pool_cb(uint64_t *aura_handle, uintptr_t *mpool, uint32_t buf_sz,
			      uint32_t nb_bufs, bool destroy, const char *mempool_name);
int cnxk_nix_inl_custom_meta_pool_cb(uintptr_t pmpool, uintptr_t *mpool, const char *mempool_name,
				     uint64_t *aura_handle, uint32_t buf_sz, uint32_t nb_bufs,
				     bool destroy);

/* Congestion Management */
int cnxk_nix_cman_info_get(struct rte_eth_dev *dev, struct rte_eth_cman_info *info);

int cnxk_nix_cman_config_init(struct rte_eth_dev *dev, struct rte_eth_cman_config *config);

int cnxk_nix_cman_config_set(struct rte_eth_dev *dev, const struct rte_eth_cman_config *config);

int cnxk_nix_cman_config_get(struct rte_eth_dev *dev, struct rte_eth_cman_config *config);

int cnxk_mcs_dev_init(struct cnxk_eth_dev *dev, uint8_t mcs_idx);
void cnxk_mcs_dev_fini(struct cnxk_eth_dev *dev);

struct cnxk_macsec_sess *cnxk_eth_macsec_sess_get_by_sess(struct cnxk_eth_dev *dev,
							  const struct rte_security_session *sess);
int cnxk_mcs_flow_configure(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[], struct rte_flow_error *error,
			     void **mcs_flow);
int cnxk_mcs_flow_destroy(struct cnxk_eth_dev *eth_dev, void *mcs_flow);

/* Other private functions */
int nix_recalc_mtu(struct rte_eth_dev *eth_dev);
int nix_mtr_validate(struct rte_eth_dev *dev, uint32_t id);
int nix_mtr_policy_act_get(struct rte_eth_dev *eth_dev, uint32_t id,
			   struct cnxk_mtr_policy_node **policy);
int nix_mtr_rq_update(struct rte_eth_dev *eth_dev, uint32_t id,
		      uint32_t queue_num, const uint16_t *queue);
int nix_mtr_chain_update(struct rte_eth_dev *eth_dev, uint32_t cur_id,
			 uint32_t prev_id, uint32_t next_id);
int nix_mtr_chain_reset(struct rte_eth_dev *eth_dev, uint32_t cur_id);
struct cnxk_meter_node *nix_get_mtr(struct rte_eth_dev *eth_dev,
				    uint32_t cur_id);
int nix_mtr_level_update(struct rte_eth_dev *eth_dev, uint32_t id,
			 uint32_t level);
int nix_mtr_capabilities_init(struct rte_eth_dev *eth_dev);
int nix_mtr_configure(struct rte_eth_dev *eth_dev, uint32_t id);
int nix_mtr_connect(struct rte_eth_dev *eth_dev, uint32_t id);
int nix_mtr_destroy(struct rte_eth_dev *eth_dev, uint32_t id,
		    struct rte_mtr_error *error);
int nix_mtr_color_action_validate(struct rte_eth_dev *eth_dev, uint32_t id,
				  uint32_t *prev_id, uint32_t *next_id,
				  struct cnxk_mtr_policy_node *policy,
				  int *tree_level);
int nix_priority_flow_ctrl_rq_conf(struct rte_eth_dev *eth_dev, uint16_t qid,
				   uint8_t tx_pause, uint8_t tc);
int nix_priority_flow_ctrl_sq_conf(struct rte_eth_dev *eth_dev, uint16_t qid,
				   uint8_t rx_pause, uint8_t tc);

#endif /* __CNXK_ETHDEV_H__ */
