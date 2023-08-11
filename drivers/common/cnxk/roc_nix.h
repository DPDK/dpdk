/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_NIX_H_
#define _ROC_NIX_H_

/* Constants */
#define ROC_NIX_BPF_PER_PFFUNC	      64
#define ROC_NIX_BPF_ID_INVALID	      0xFFFF
#define ROC_NIX_BPF_LEVEL_IDX_INVALID 0xFF
#define ROC_NIX_BPF_LEVEL_MAX	      3
#define ROC_NIX_BPF_STATS_MAX	      12
#define ROC_NIX_MTR_ID_INVALID	      UINT32_MAX
#define ROC_NIX_PFC_CLASS_INVALID     UINT8_MAX
#define ROC_NIX_SQB_THRESH	      30U
#define ROC_NIX_SQB_SLACK	      12U
#define ROC_NIX_AURA_THRESH	      95U

/* Reserved interface types for BPID allocation */
#define ROC_NIX_INTF_TYPE_CGX  0
#define ROC_NIX_INTF_TYPE_LBK  1
#define ROC_NIX_INTF_TYPE_SDP  2
#define ROC_NIX_INTF_TYPE_CPT  3
#define ROC_NIX_INTF_TYPE_RSVD 4

/* Application based types for BPID allocation, start from end (255 unused rsvd) */
#define ROC_NIX_INTF_TYPE_CPT_NIX 254
#define ROC_NIX_INTF_TYPE_SSO     253

enum roc_nix_rss_reta_sz {
	ROC_NIX_RSS_RETA_SZ_64 = 64,
	ROC_NIX_RSS_RETA_SZ_128 = 128,
	ROC_NIX_RSS_RETA_SZ_256 = 256,
};

enum roc_nix_sq_max_sqe_sz {
	roc_nix_maxsqesz_w16 = NIX_MAXSQESZ_W16,
	roc_nix_maxsqesz_w8 = NIX_MAXSQESZ_W8,
};

enum roc_nix_fc_mode {
	ROC_NIX_FC_NONE = 0,
	ROC_NIX_FC_RX,
	ROC_NIX_FC_TX,
	ROC_NIX_FC_FULL
};

enum roc_nix_vlan_type {
	ROC_NIX_VLAN_TYPE_INNER = 0x01,
	ROC_NIX_VLAN_TYPE_OUTER = 0x02,
};

enum roc_nix_bpf_level_flag {
	ROC_NIX_BPF_LEVEL_F_LEAF = BIT(0),
	ROC_NIX_BPF_LEVEL_F_MID = BIT(1),
	ROC_NIX_BPF_LEVEL_F_TOP = BIT(2),
};

enum roc_nix_bpf_precolor_tbl_size {
	ROC_NIX_BPF_PRECOLOR_TBL_SIZE_GEN = 16,
	ROC_NIX_BPF_PRECOLOR_TBL_SIZE_VLAN = 16,
	ROC_NIX_BPF_PRECOLOR_TBL_SIZE_DSCP = 64,
};

enum roc_nix_bpf_pc_mode {
	ROC_NIX_BPF_PC_MODE_VLAN_INNER,
	ROC_NIX_BPF_PC_MODE_VLAN_OUTER,
	ROC_NIX_BPF_PC_MODE_DSCP_INNER,
	ROC_NIX_BPF_PC_MODE_DSCP_OUTER,
	ROC_NIX_BPF_PC_MODE_GEN_INNER,
	ROC_NIX_BPF_PC_MODE_GEN_OUTER
};

enum roc_nix_bpf_color {
	ROC_NIX_BPF_COLOR_GREEN,
	ROC_NIX_BPF_COLOR_YELLOW,
	ROC_NIX_BPF_COLOR_RED,
	ROC_NIX_BPF_COLOR_MAX
};

enum roc_nix_bpf_algo {
	ROC_NIX_BPF_ALGO_NONE,
	ROC_NIX_BPF_ALGO_2698,
	ROC_NIX_BPF_ALGO_4115,
	ROC_NIX_BPF_ALGO_2697
};

enum roc_nix_bpf_lmode { ROC_NIX_BPF_LMODE_BYTE, ROC_NIX_BPF_LMODE_PACKET };

enum roc_nix_bpf_action {
	ROC_NIX_BPF_ACTION_PASS,
	ROC_NIX_BPF_ACTION_DROP,
	ROC_NIX_BPF_ACTION_RED
};

enum roc_nix_bpf_stats {
	ROC_NIX_BPF_GREEN_PKT_F_PASS = BIT_ULL(0),
	ROC_NIX_BPF_GREEN_OCTS_F_PASS = BIT_ULL(1),
	ROC_NIX_BPF_GREEN_PKT_F_DROP = BIT_ULL(2),
	ROC_NIX_BPF_GREEN_OCTS_F_DROP = BIT_ULL(3),
	ROC_NIX_BPF_YELLOW_PKT_F_PASS = BIT_ULL(4),
	ROC_NIX_BPF_YELLOW_OCTS_F_PASS = BIT_ULL(5),
	ROC_NIX_BPF_YELLOW_PKT_F_DROP = BIT_ULL(6),
	ROC_NIX_BPF_YELLOW_OCTS_F_DROP = BIT_ULL(7),
	ROC_NIX_BPF_RED_PKT_F_PASS = BIT_ULL(8),
	ROC_NIX_BPF_RED_OCTS_F_PASS = BIT_ULL(9),
	ROC_NIX_BPF_RED_PKT_F_DROP = BIT_ULL(10),
	ROC_NIX_BPF_RED_OCTS_F_DROP = BIT_ULL(11),
};

struct roc_nix_bpf_cfg {
	enum roc_nix_bpf_algo alg;
	enum roc_nix_bpf_lmode lmode;
	enum roc_nix_bpf_color icolor;
	enum roc_nix_bpf_pc_mode pc_mode;
	bool tnl_ena;
	union {
		/* Valid when *alg* is set to ROC_NIX_BPF_ALGO_2697. */
		struct {
			uint64_t cir;
			uint64_t cbs;
			uint64_t ebs;
		} algo2697;

		/* Valid when *alg* is set to ROC_NIX_BPF_ALGO_2698. */
		struct {
			uint64_t cir;
			uint64_t pir;
			uint64_t cbs;
			uint64_t pbs;
		} algo2698;

		/* Valid when *alg* is set to ROC_NIX_BPF_ALGO_4115. */
		struct {
			uint64_t cir;
			uint64_t eir;
			uint64_t cbs;
			uint64_t ebs;
		} algo4115;
	};

	enum roc_nix_bpf_action action[ROC_NIX_BPF_COLOR_MAX];

	/* Reserved for future config*/
	uint32_t rsvd[3];
};

struct roc_nix_bpf_objs {
	uint16_t level;
	uint16_t count;
	uint16_t ids[ROC_NIX_BPF_PER_PFFUNC];
};

struct roc_nix_bpf_precolor {
#define ROC_NIX_BPF_PRE_COLOR_MAX 64
	uint8_t count;
	enum roc_nix_bpf_pc_mode mode;
	enum roc_nix_bpf_color color[ROC_NIX_BPF_PRE_COLOR_MAX];
};

struct roc_nix_vlan_config {
	uint32_t type;
	union {
		struct {
			uint32_t vtag_inner;
			uint32_t vtag_outer;
		} vlan;

		struct {
			int idx_inner;
			int idx_outer;
		} mcam;
	};
};

struct roc_nix_fc_cfg {
#define ROC_NIX_FC_RXCHAN_CFG 0
#define ROC_NIX_FC_CQ_CFG     1
#define ROC_NIX_FC_TM_CFG     2
#define ROC_NIX_FC_RQ_CFG     3
	uint8_t type;
	union {
		struct {
			bool enable;
		} rxchan_cfg;

		struct {
			uint32_t rq;
			uint16_t tc;
			uint16_t cq_drop;
			bool enable;
		} cq_cfg;

		struct {
			uint32_t rq;
			uint16_t tc;
			uint16_t cq_drop;
			bool enable;
			uint64_t pool;
			uint64_t spb_pool;
			uint64_t pool_drop_pct;
		} rq_cfg;

		struct {
			uint32_t sq;
			uint16_t tc;
			bool enable;
		} tm_cfg;
	};
};

struct roc_nix_pfc_cfg {
	enum roc_nix_fc_mode mode;
	/* For SET, tc must be [0, 15].
	 * For GET, TC will represent bitmap
	 */
	uint16_t tc;
};

struct roc_nix_eeprom_info {
#define ROC_NIX_EEPROM_SIZE 256
	uint16_t sff_id;
	uint8_t buf[ROC_NIX_EEPROM_SIZE];
};

/* Range to adjust PTP frequency. Valid range is
 * (-ROC_NIX_PTP_FREQ_ADJUST, ROC_NIX_PTP_FREQ_ADJUST)
 */
#define ROC_NIX_PTP_FREQ_ADJUST (1 << 9)

/* NIX LF RX offload configuration flags.
 * These are input flags to roc_nix_lf_alloc:rx_cfg
 */
#define ROC_NIX_LF_RX_CFG_DROP_RE     BIT_ULL(32)
#define ROC_NIX_LF_RX_CFG_L2_LEN_ERR  BIT_ULL(33)
#define ROC_NIX_LF_RX_CFG_IP6_UDP_OPT BIT_ULL(34)
#define ROC_NIX_LF_RX_CFG_DIS_APAD    BIT_ULL(35)
#define ROC_NIX_LF_RX_CFG_CSUM_IL4    BIT_ULL(36)
#define ROC_NIX_LF_RX_CFG_CSUM_OL4    BIT_ULL(37)
#define ROC_NIX_LF_RX_CFG_LEN_IL4     BIT_ULL(38)
#define ROC_NIX_LF_RX_CFG_LEN_IL3     BIT_ULL(39)
#define ROC_NIX_LF_RX_CFG_LEN_OL4     BIT_ULL(40)
#define ROC_NIX_LF_RX_CFG_LEN_OL3     BIT_ULL(41)

#define ROC_NIX_LF_RX_CFG_RX_ERROR_MASK 0xFFFFFFFFFFF80000
#define ROC_NIX_RE_PARTIAL		BIT_ULL(1)
#define ROC_NIX_RE_JABBER		BIT_ULL(2)
#define ROC_NIX_RE_CRC8_PCH		BIT_ULL(5)
#define ROC_NIX_RE_CNC_INV		BIT_ULL(6)
#define ROC_NIX_RE_FCS			BIT_ULL(7)
#define ROC_NIX_RE_FCS_RCV		BIT_ULL(8)
#define ROC_NIX_RE_TERMINATE		BIT_ULL(9)
#define ROC_NIX_RE_MACSEC		BIT_ULL(10)
#define ROC_NIX_RE_RX_CTL		BIT_ULL(11)
#define ROC_NIX_RE_SKIP			BIT_ULL(12)
#define ROC_NIX_RE_DMAPKT		BIT_ULL(15)
#define ROC_NIX_RE_UNDERSIZE		BIT_ULL(16)
#define ROC_NIX_RE_OVERSIZE		BIT_ULL(17)
#define ROC_NIX_RE_OL2_LENMISM		BIT_ULL(18)

/* Group 0 will be used for RSS, 1 -7 will be used for npc_flow RSS action*/
#define ROC_NIX_RSS_GROUP_DEFAULT    0
#define ROC_NIX_RSS_GRPS	     8
#define ROC_NIX_RSS_RETA_MAX	     ROC_NIX_RSS_RETA_SZ_256
#define ROC_NIX_RSS_KEY_LEN	     48 /* 352 Bits */
#define ROC_NIX_RSS_MCAM_IDX_DEFAULT (-1)

#define ROC_NIX_DEFAULT_HW_FRS 1514

#define ROC_NIX_VWQE_MAX_SIZE_LOG2 11
#define ROC_NIX_VWQE_MIN_SIZE_LOG2 2

struct roc_nix_stats {
	/* Rx */
	uint64_t rx_octs;
	uint64_t rx_ucast;
	uint64_t rx_bcast;
	uint64_t rx_mcast;
	uint64_t rx_drop;
	uint64_t rx_drop_octs;
	uint64_t rx_fcs;
	uint64_t rx_err;
	uint64_t rx_drop_bcast;
	uint64_t rx_drop_mcast;
	uint64_t rx_drop_l3_bcast;
	uint64_t rx_drop_l3_mcast;
	/* Tx */
	uint64_t tx_ucast;
	uint64_t tx_bcast;
	uint64_t tx_mcast;
	uint64_t tx_drop;
	uint64_t tx_octs;
};

struct roc_nix_stats_queue {
	union {
		struct {
			/* Rx */
			uint64_t rx_pkts;
			uint64_t rx_octs;
			uint64_t rx_drop_pkts;
			uint64_t rx_drop_octs;
			uint64_t rx_error_pkts;
		};
		struct {
			/* Tx */
			uint64_t tx_pkts;
			uint64_t tx_octs;
			uint64_t tx_drop_pkts;
			uint64_t tx_drop_octs;
			uint64_t tx_age_drop_pkts;
			uint64_t tx_age_drop_octs;
		};
	};
};

struct roc_nix_rq {
	/* Input parameters */
	uint16_t qid;
	uint16_t cqid; /* Not valid when SSO is enabled */
	uint16_t bpf_id;
	uint64_t aura_handle;
	bool ipsech_ena;
	uint16_t first_skip;
	uint16_t later_skip;
	uint16_t wqe_skip;
	uint16_t lpb_size;
	uint32_t tag_mask;
	uint32_t flow_tag_width;
	uint8_t tt;	/* Valid when SSO is enabled */
	uint16_t hwgrp; /* Valid when SSO is enabled */
	bool sso_ena;
	bool vwqe_ena;
	uint64_t spb_aura_handle; /* Valid when SPB is enabled */
	uint16_t spb_size;	  /* Valid when SPB is enabled */
	bool spb_ena;
	uint8_t vwqe_first_skip;
	uint32_t vwqe_max_sz_exp;
	uint64_t vwqe_wait_tmo;
	uint64_t vwqe_aura_handle;
	/* Average LPB aura level drop threshold for RED */
	uint8_t red_drop;
	/* Average LPB aura level pass threshold for RED */
	uint8_t red_pass;
	/* Average SPB aura level drop threshold for RED */
	uint8_t spb_red_drop;
	/* Average SPB aura level pass threshold for RED */
	uint8_t xqe_red_pass;
	/* Average xqe level drop threshold for RED */
	uint8_t xqe_red_drop;
	/* Average xqe level pass threshold for RED */
	uint8_t spb_red_pass;
	/* LPB aura drop enable */
	bool lpb_drop_ena;
	/* SPB aura drop enable */
	bool spb_drop_ena;
	/* End of Input parameters */
	struct roc_nix *roc_nix;
	uint64_t meta_aura_handle;
	uint16_t inl_dev_refs;
	uint8_t tc;
};

struct roc_nix_cq {
	/* Input parameters */
	uint16_t qid;
	uint32_t nb_desc;
	/* End of Input parameters */
	uint16_t drop_thresh;
	struct roc_nix *roc_nix;
	uintptr_t door;
	int64_t *status;
	uint64_t wdata;
	void *desc_base;
	uint32_t qmask;
	uint32_t head;
};

struct roc_nix_sq {
	/* Input parameters */
	enum roc_nix_sq_max_sqe_sz max_sqe_sz;
	uint32_t nb_desc;
	uint16_t qid;
	uint16_t cqid;
	uint16_t cq_drop_thresh;
	bool sso_ena;
	bool cq_ena;
	uint8_t fc_hyst_bits;
	/* End of Input parameters */
	uint16_t sqes_per_sqb_log2;
	struct roc_nix *roc_nix;
	uint64_t aura_handle;
	int16_t nb_sqb_bufs_adj;
	uint16_t nb_sqb_bufs;
	uint16_t aura_sqb_bufs;
	plt_iova_t io_addr;
	void *lmt_addr;
	void *sqe_mem;
	void *fc;
	uint8_t tc;
};

struct roc_nix_link_info {
	uint64_t status : 1;
	uint64_t full_duplex : 1;
	uint64_t lmac_type_id : 4;
	uint64_t speed : 20;
	uint64_t autoneg : 1;
	uint64_t fec : 2;
	uint64_t port : 8;
};

/** Maximum name length for extended statistics counters */
#define ROC_NIX_XSTATS_NAME_SIZE 64

struct roc_nix_xstat {
	uint64_t id;	/**< The index in xstats name array. */
	uint64_t value; /**< The statistic counter value. */
};

struct roc_nix_xstat_name {
	char name[ROC_NIX_XSTATS_NAME_SIZE];
};

struct roc_nix_ipsec_cfg {
	uint32_t sa_size;
	uint32_t tag_const;
	plt_iova_t iova;
	uint16_t max_sa;
	uint8_t tt;
};

/* Link status update callback */
typedef void (*link_status_t)(struct roc_nix *roc_nix,
			      struct roc_nix_link_info *link);

/* PTP info update callback */
typedef int (*ptp_info_update_t)(struct roc_nix *roc_nix, bool enable);

/* Queue Error get callback */
typedef void (*q_err_get_t)(struct roc_nix *roc_nix, void *data);

/* Link status get callback */
typedef void (*link_info_get_t)(struct roc_nix *roc_nix,
				struct roc_nix_link_info *link);

TAILQ_HEAD(roc_nix_list, roc_nix);

struct roc_nix {
	/* Input parameters */
	struct plt_pci_device *pci_dev;
	uint16_t port_id;
	bool rss_tag_as_xor;
	uint16_t max_sqb_count;
	enum roc_nix_rss_reta_sz reta_sz;
	bool enable_loop;
	bool tx_compl_ena;
	bool hw_vlan_ins;
	uint8_t lock_rx_ctx;
	uint16_t sqb_slack;
	uint16_t outb_nb_crypto_qs;
	uint32_t outb_nb_desc;
	uint32_t ipsec_in_min_spi;
	uint32_t ipsec_in_max_spi;
	uint32_t ipsec_out_max_sa;
	uint32_t dwrr_mtu;
	bool ipsec_out_sso_pffunc;
	bool custom_sa_action;
	bool local_meta_aura_ena;
	uint32_t meta_buf_sz;
	bool force_rx_aura_bp;
	bool custom_meta_aura_ena;
	/* End of input parameters */
	/* LMT line base for "Per Core Tx LMT line" mode*/
	uintptr_t lmt_base;
	bool io_enabled;
	bool rx_ptp_ena;
	uint16_t cints;
	uint32_t buf_sz;
	uint64_t meta_aura_handle;
	uintptr_t meta_mempool;
	TAILQ_ENTRY(roc_nix) next;

#define ROC_NIX_MEM_SZ (6 * 1070)
	uint8_t reserved[ROC_NIX_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

enum roc_nix_lso_tun_type {
	ROC_NIX_LSO_TUN_V4V4,
	ROC_NIX_LSO_TUN_V4V6,
	ROC_NIX_LSO_TUN_V6V4,
	ROC_NIX_LSO_TUN_V6V6,
	ROC_NIX_LSO_TUN_MAX,
};

/* Restrict CN9K sched weight to have a minimum quantum */
#define ROC_NIX_CN9K_TM_RR_WEIGHT_MAX 255u

/* NIX TM Inlines */
static inline uint64_t
roc_nix_tm_max_sched_wt_get(void)
{
	if (roc_model_is_cn9k())
		return ROC_NIX_CN9K_TM_RR_WEIGHT_MAX;
	else
		return NIX_TM_RR_WEIGHT_MAX;
}

static inline uint64_t
roc_nix_tm_max_shaper_burst_get(void)
{
	if (roc_model_is_cn9k())
		return NIX_CN9K_TM_MAX_SHAPER_BURST;
	else
		return NIX_TM_MAX_SHAPER_BURST;
}

/* Dev */
int __roc_api roc_nix_dev_init(struct roc_nix *roc_nix);
int __roc_api roc_nix_dev_fini(struct roc_nix *roc_nix);

/* Type */
bool __roc_api roc_nix_is_lbk(struct roc_nix *roc_nix);
bool __roc_api roc_nix_is_sdp(struct roc_nix *roc_nix);
bool __roc_api roc_nix_is_pf(struct roc_nix *roc_nix);
bool __roc_api roc_nix_is_vf_or_sdp(struct roc_nix *roc_nix);
int __roc_api roc_nix_get_base_chan(struct roc_nix *roc_nix);
int __roc_api roc_nix_get_pf(struct roc_nix *roc_nix);
int __roc_api roc_nix_get_vf(struct roc_nix *roc_nix);
uint16_t __roc_api roc_nix_get_pf_func(struct roc_nix *roc_nix);
uint16_t __roc_api roc_nix_get_vwqe_interval(struct roc_nix *roc_nix);
int __roc_api roc_nix_max_pkt_len(struct roc_nix *roc_nix);

/* LF ops */
int __roc_api roc_nix_lf_alloc(struct roc_nix *roc_nix, uint32_t nb_rxq,
			       uint32_t nb_txq, uint64_t rx_cfg);
int __roc_api roc_nix_lf_free(struct roc_nix *roc_nix);
int __roc_api roc_nix_lf_inl_ipsec_cfg(struct roc_nix *roc_nix,
				       struct roc_nix_ipsec_cfg *cfg, bool enb);
int __roc_api roc_nix_cpt_ctx_cache_sync(struct roc_nix *roc_nix);
int __roc_api roc_nix_rx_drop_re_set(struct roc_nix *roc_nix, bool ena);

/* Debug */
int __roc_api roc_nix_lf_get_reg_count(struct roc_nix *roc_nix);
int __roc_api roc_nix_lf_reg_dump(struct roc_nix *roc_nix, uint64_t *data);
int __roc_api roc_nix_queues_ctx_dump(struct roc_nix *roc_nix, FILE *file);
void __roc_api roc_nix_cqe_dump(const struct nix_cqe_hdr_s *cq);
void __roc_api roc_nix_rq_dump(struct roc_nix_rq *rq, FILE *file);
void __roc_api roc_nix_cq_dump(struct roc_nix_cq *cq, FILE *file);
void __roc_api roc_nix_sq_dump(struct roc_nix_sq *sq, FILE *file);
void __roc_api roc_nix_tm_dump(struct roc_nix *roc_nix, FILE *file);
void __roc_api roc_nix_dump(struct roc_nix *roc_nix, FILE *file);

/* IRQ */
void __roc_api roc_nix_rx_queue_intr_enable(struct roc_nix *roc_nix,
					    uint16_t rxq_id);
void __roc_api roc_nix_rx_queue_intr_disable(struct roc_nix *roc_nix,
					     uint16_t rxq_id);
void __roc_api roc_nix_err_intr_ena_dis(struct roc_nix *roc_nix, bool enb);
void __roc_api roc_nix_ras_intr_ena_dis(struct roc_nix *roc_nix, bool enb);
int __roc_api roc_nix_register_queue_irqs(struct roc_nix *roc_nix);
void __roc_api roc_nix_unregister_queue_irqs(struct roc_nix *roc_nix);
int __roc_api roc_nix_register_cq_irqs(struct roc_nix *roc_nix);
void __roc_api roc_nix_unregister_cq_irqs(struct roc_nix *roc_nix);

/* Traffic Management */
#define ROC_NIX_TM_SHAPER_PROFILE_NONE UINT32_MAX
#define ROC_NIX_TM_NODE_ID_INVALID     UINT32_MAX

enum roc_nix_tm_tree {
	ROC_NIX_TM_DEFAULT = 0,
	ROC_NIX_TM_RLIMIT,
	ROC_NIX_TM_PFC,
	ROC_NIX_TM_USER,
	ROC_NIX_TM_TREE_MAX,
};

enum roc_tm_node_level {
	ROC_TM_LVL_ROOT = 0,
	ROC_TM_LVL_SCH1,
	ROC_TM_LVL_SCH2,
	ROC_TM_LVL_SCH3,
	ROC_TM_LVL_SCH4,
	ROC_TM_LVL_QUEUE,
	ROC_TM_LVL_MAX,
};

/*
 * TM runtime hierarchy init API.
 */
int __roc_api roc_nix_tm_init(struct roc_nix *roc_nix);
void __roc_api roc_nix_tm_fini(struct roc_nix *roc_nix);
int __roc_api roc_nix_tm_sq_aura_fc(struct roc_nix_sq *sq, bool enable);
int __roc_api roc_nix_tm_sq_flush_spin(struct roc_nix_sq *sq);

/*
 * TM User hierarchy API.
 */

struct roc_nix_tm_node {
#define ROC_NIX_TM_NODE_SZ (128)
	uint8_t reserved[ROC_NIX_TM_NODE_SZ];

	uint32_t id;
	uint32_t parent_id;
	uint32_t priority;
	uint32_t weight;
	uint32_t shaper_profile_id;
	uint16_t lvl;
	bool pkt_mode;
	bool pkt_mode_set;
	/* Function to free this memory */
	void (*free_fn)(void *node);
};

struct roc_nix_tm_shaper_profile {
#define ROC_NIX_TM_SHAPER_PROFILE_SZ (128)
	uint8_t reserved[ROC_NIX_TM_SHAPER_PROFILE_SZ];

	uint32_t id;
	uint64_t commit_rate;
	uint64_t commit_sz;
	uint64_t peak_rate;
	uint64_t peak_sz;
	int32_t pkt_len_adj;
	bool pkt_mode;
	int8_t accuracy;
	uint8_t red_algo;
	/* Function to free this memory */
	void (*free_fn)(void *profile);
};

enum roc_nix_tm_node_stats_type {
	ROC_NIX_TM_NODE_PKTS_DROPPED,
	ROC_NIX_TM_NODE_BYTES_DROPPED,
	ROC_NIX_TM_NODE_GREEN_PKTS,
	ROC_NIX_TM_NODE_GREEN_BYTES,
	ROC_NIX_TM_NODE_YELLOW_PKTS,
	ROC_NIX_TM_NODE_YELLOW_BYTES,
	ROC_NIX_TM_NODE_RED_PKTS,
	ROC_NIX_TM_NODE_RED_BYTES,
	ROC_NIX_TM_NODE_STATS_MAX,
};

struct roc_nix_tm_node_stats {
	uint64_t stats[ROC_NIX_TM_NODE_STATS_MAX];
};

enum roc_nix_tm_mark {
	ROC_NIX_TM_MARK_VLAN_DEI,
	ROC_NIX_TM_MARK_IPV4_DSCP,
	ROC_NIX_TM_MARK_IPV4_ECN,
	ROC_NIX_TM_MARK_IPV6_DSCP,
	ROC_NIX_TM_MARK_IPV6_ECN,
	ROC_NIX_TM_MARK_MAX
};

enum roc_nix_tm_mark_color {
	ROC_NIX_TM_MARK_COLOR_Y,
	ROC_NIX_TM_MARK_COLOR_R,
	ROC_NIX_TM_MARK_COLOR_Y_R,
	ROC_NIX_TM_MARK_COLOR_MAX
};

int __roc_api roc_nix_tm_node_add(struct roc_nix *roc_nix,
				  struct roc_nix_tm_node *roc_node);
int __roc_api roc_nix_tm_node_delete(struct roc_nix *roc_nix, uint32_t node_id,
				     bool free);
int __roc_api roc_nix_tm_free_resources(struct roc_nix *roc_nix, bool hw_only);
int __roc_api roc_nix_tm_node_suspend_resume(struct roc_nix *roc_nix,
					     uint32_t node_id, bool suspend);
int __roc_api roc_nix_tm_node_parent_update(struct roc_nix *roc_nix,
					    uint32_t node_id,
					    uint32_t new_parent_id,
					    uint32_t priority, uint32_t weight);
int __roc_api roc_nix_tm_node_shaper_update(struct roc_nix *roc_nix,
					    uint32_t node_id,
					    uint32_t profile_id,
					    bool force_update);
int __roc_api roc_nix_tm_node_pkt_mode_update(struct roc_nix *roc_nix,
					      uint32_t node_id, bool pkt_mode);
int __roc_api roc_nix_tm_shaper_profile_add(
	struct roc_nix *roc_nix, struct roc_nix_tm_shaper_profile *profile);
int __roc_api roc_nix_tm_shaper_profile_update(
	struct roc_nix *roc_nix, struct roc_nix_tm_shaper_profile *profile);
int __roc_api roc_nix_tm_shaper_profile_delete(struct roc_nix *roc_nix,
					       uint32_t id);

int __roc_api roc_nix_tm_prealloc_res(struct roc_nix *roc_nix, uint8_t lvl,
				      uint16_t discontig, uint16_t contig);
uint16_t __roc_api roc_nix_tm_leaf_cnt(struct roc_nix *roc_nix);

struct roc_nix_tm_node *__roc_api roc_nix_tm_node_get(struct roc_nix *roc_nix,
						      uint32_t node_id);
struct roc_nix_tm_node *__roc_api
roc_nix_tm_node_next(struct roc_nix *roc_nix, struct roc_nix_tm_node *__prev);
struct roc_nix_tm_shaper_profile *__roc_api
roc_nix_tm_shaper_profile_get(struct roc_nix *roc_nix, uint32_t profile_id);
struct roc_nix_tm_shaper_profile *__roc_api roc_nix_tm_shaper_profile_next(
	struct roc_nix *roc_nix, struct roc_nix_tm_shaper_profile *__prev);

int __roc_api roc_nix_tm_node_stats_get(struct roc_nix *roc_nix,
					uint32_t node_id, bool clear,
					struct roc_nix_tm_node_stats *stats);
/*
 * TM ratelimit tree API.
 */
int __roc_api roc_nix_tm_rlimit_sq(struct roc_nix *roc_nix, uint16_t qid,
				   uint64_t rate);
/*
 * TM hierarchy enable/disable API.
 */
int __roc_api roc_nix_tm_hierarchy_disable(struct roc_nix *roc_nix);
int __roc_api roc_nix_tm_hierarchy_enable(struct roc_nix *roc_nix,
					  enum roc_nix_tm_tree tree,
					  bool xmit_enable);
int __roc_api roc_nix_tm_hierarchy_xmit_enable(struct roc_nix *roc_nix, enum roc_nix_tm_tree tree);


/*
 * TM utilities API.
 */
int __roc_api roc_nix_tm_node_lvl(struct roc_nix *roc_nix, uint32_t node_id);
bool __roc_api roc_nix_tm_root_has_sp(struct roc_nix *roc_nix);
void __roc_api roc_nix_tm_rsrc_max(bool pf, uint16_t schq[ROC_TM_LVL_MAX]);
int __roc_api roc_nix_tm_rsrc_count(struct roc_nix *roc_nix,
				    uint16_t schq[ROC_TM_LVL_MAX]);
int __roc_api roc_nix_tm_node_name_get(struct roc_nix *roc_nix,
				       uint32_t node_id, char *buf,
				       size_t buflen);
int __roc_api roc_nix_smq_flush(struct roc_nix *roc_nix);
int __roc_api roc_nix_tm_max_prio(struct roc_nix *roc_nix, int lvl);
int __roc_api roc_nix_tm_lvl_is_leaf(struct roc_nix *roc_nix, int lvl);
void __roc_api
roc_nix_tm_shaper_default_red_algo(struct roc_nix_tm_node *node,
				   struct roc_nix_tm_shaper_profile *profile);
int __roc_api roc_nix_tm_lvl_cnt_get(struct roc_nix *roc_nix);
int __roc_api roc_nix_tm_lvl_have_link_access(struct roc_nix *roc_nix, int lvl);
int __roc_api roc_nix_tm_prepare_rate_limited_tree(struct roc_nix *roc_nix);
int __roc_api roc_nix_tm_pfc_prepare_tree(struct roc_nix *roc_nix);
bool __roc_api roc_nix_tm_is_user_hierarchy_enabled(struct roc_nix *nix);
int __roc_api roc_nix_tm_tree_type_get(struct roc_nix *nix);
int __roc_api roc_nix_tm_mark_config(struct roc_nix *roc_nix,
				     enum roc_nix_tm_mark type, int mark_yellow,
				     int mark_red);
uint64_t __roc_api roc_nix_tm_mark_format_get(struct roc_nix *roc_nix,
					      uint64_t *flags);

/* Ingress Policer API */
int __roc_api roc_nix_bpf_timeunit_get(struct roc_nix *roc_nix,
				       uint32_t *time_unit);

int __roc_api
roc_nix_bpf_count_get(struct roc_nix *roc_nix, uint8_t lvl_mask,
		      uint16_t count[ROC_NIX_BPF_LEVEL_MAX] /* Out */);

int __roc_api roc_nix_bpf_alloc(struct roc_nix *roc_nix, uint8_t lvl_mask,
				uint16_t per_lvl_cnt[ROC_NIX_BPF_LEVEL_MAX],
				struct roc_nix_bpf_objs *profs /* Out */);

int __roc_api roc_nix_bpf_free(struct roc_nix *roc_nix,
			       struct roc_nix_bpf_objs *profs,
			       uint8_t num_prof);

int __roc_api roc_nix_bpf_free_all(struct roc_nix *roc_nix);

int __roc_api roc_nix_bpf_config(struct roc_nix *roc_nix, uint16_t id,
				 enum roc_nix_bpf_level_flag lvl_flag,
				 struct roc_nix_bpf_cfg *cfg);

int __roc_api roc_nix_bpf_ena_dis(struct roc_nix *roc_nix, uint16_t id,
				  struct roc_nix_rq *rq, bool enable);

int __roc_api roc_nix_bpf_dump(struct roc_nix *roc_nix, uint16_t id,
			       enum roc_nix_bpf_level_flag lvl_flag);

int __roc_api roc_nix_bpf_pre_color_tbl_setup(
	struct roc_nix *roc_nix, uint16_t id,
	enum roc_nix_bpf_level_flag lvl_flag, struct roc_nix_bpf_precolor *tbl);

/* Use ROC_NIX_BPF_ID_INVALID as dst_id to disconnect */
int __roc_api roc_nix_bpf_connect(struct roc_nix *roc_nix,
				  enum roc_nix_bpf_level_flag lvl_flag,
				  uint16_t src_id, uint16_t dst_id);

int __roc_api
roc_nix_bpf_stats_read(struct roc_nix *roc_nix, uint16_t id, uint64_t mask,
		       enum roc_nix_bpf_level_flag lvl_flag,
		       uint64_t stats[ROC_NIX_BPF_STATS_MAX] /* Out */);

int __roc_api roc_nix_bpf_stats_reset(struct roc_nix *roc_nix, uint16_t id,
				      uint64_t mask,
				      enum roc_nix_bpf_level_flag lvl_flag);

int __roc_api
roc_nix_bpf_lf_stats_read(struct roc_nix *roc_nix, uint64_t mask,
			  uint64_t stats[ROC_NIX_BPF_STATS_MAX] /* Out */);

int __roc_api roc_nix_bpf_lf_stats_reset(struct roc_nix *roc_nix,
					 uint64_t mask);

uint8_t __roc_api
roc_nix_bpf_level_to_idx(enum roc_nix_bpf_level_flag lvl_flag);

uint8_t __roc_api roc_nix_bpf_stats_to_idx(enum roc_nix_bpf_stats lvl_flag);

/* MAC */
int __roc_api roc_nix_mac_rxtx_start_stop(struct roc_nix *roc_nix, bool start);
int __roc_api roc_nix_mac_link_event_start_stop(struct roc_nix *roc_nix,
						bool start);
int __roc_api roc_nix_mac_loopback_enable(struct roc_nix *roc_nix, bool enable);
int __roc_api roc_nix_mac_addr_set(struct roc_nix *roc_nix,
				   const uint8_t addr[]);
int __roc_api roc_nix_mac_max_entries_get(struct roc_nix *roc_nix);
int __roc_api roc_nix_mac_addr_add(struct roc_nix *roc_nix, uint8_t addr[]);
int __roc_api roc_nix_mac_addr_del(struct roc_nix *roc_nix, uint32_t index);
int __roc_api roc_nix_mac_promisc_mode_enable(struct roc_nix *roc_nix,
					      int enable);
int __roc_api roc_nix_mac_link_state_set(struct roc_nix *roc_nix, uint8_t up);
int __roc_api roc_nix_mac_link_info_set(struct roc_nix *roc_nix,
					struct roc_nix_link_info *link_info);
int __roc_api roc_nix_mac_link_info_get(struct roc_nix *roc_nix,
					struct roc_nix_link_info *link_info);
int __roc_api roc_nix_mac_mtu_set(struct roc_nix *roc_nix, uint16_t mtu);
int __roc_api roc_nix_mac_max_rx_len_set(struct roc_nix *roc_nix,
					 uint16_t maxlen);
int __roc_api roc_nix_mac_link_cb_register(struct roc_nix *roc_nix,
					   link_status_t link_update);
void __roc_api roc_nix_mac_link_cb_unregister(struct roc_nix *roc_nix);
int __roc_api roc_nix_mac_link_info_get_cb_register(
	struct roc_nix *roc_nix, link_info_get_t link_info_get);
void __roc_api roc_nix_mac_link_info_get_cb_unregister(struct roc_nix *roc_nix);
int __roc_api roc_nix_q_err_cb_register(struct roc_nix *roc_nix, q_err_get_t sq_err_handle);
void __roc_api roc_nix_q_err_cb_unregister(struct roc_nix *roc_nix);

/* Ops */
int __roc_api roc_nix_switch_hdr_set(struct roc_nix *roc_nix,
				     uint64_t switch_header_type,
				     uint8_t pre_l2_size_offset,
				     uint8_t pre_l2_size_offset_mask,
				     uint8_t pre_l2_size_shift_dir);
int __roc_api roc_nix_lso_fmt_setup(struct roc_nix *roc_nix);
int __roc_api roc_nix_lso_fmt_get(struct roc_nix *roc_nix,
				  uint8_t udp_tun[ROC_NIX_LSO_TUN_MAX],
				  uint8_t tun[ROC_NIX_LSO_TUN_MAX]);
int __roc_api roc_nix_lso_custom_fmt_setup(struct roc_nix *roc_nix,
					   struct nix_lso_format *fields,
					   uint16_t nb_fields);

int __roc_api roc_nix_eeprom_info_get(struct roc_nix *roc_nix,
				      struct roc_nix_eeprom_info *info);

/* Flow control */
int __roc_api roc_nix_fc_config_set(struct roc_nix *roc_nix,
				    struct roc_nix_fc_cfg *fc_cfg);

int __roc_api roc_nix_fc_config_get(struct roc_nix *roc_nix,
				    struct roc_nix_fc_cfg *fc_cfg);

int __roc_api roc_nix_fc_mode_set(struct roc_nix *roc_nix,
				  enum roc_nix_fc_mode mode);

int __roc_api roc_nix_pfc_mode_set(struct roc_nix *roc_nix,
				   struct roc_nix_pfc_cfg *pfc_cfg);

int __roc_api roc_nix_pfc_mode_get(struct roc_nix *roc_nix,
				   struct roc_nix_pfc_cfg *pfc_cfg);

uint16_t __roc_api roc_nix_chan_count_get(struct roc_nix *roc_nix);

enum roc_nix_fc_mode __roc_api roc_nix_fc_mode_get(struct roc_nix *roc_nix);

void __roc_api roc_nix_fc_npa_bp_cfg(struct roc_nix *roc_nix, uint64_t pool_id, uint8_t ena,
				     uint8_t force, uint8_t tc, uint64_t drop_percent);
int __roc_api roc_nix_bpids_alloc(struct roc_nix *roc_nix, uint8_t type,
				  uint8_t bp_cnt, uint16_t *bpids);
int __roc_api roc_nix_bpids_free(struct roc_nix *roc_nix, uint8_t bp_cnt,
				 uint16_t *bpids);
int __roc_api roc_nix_rx_chan_cfg_get(struct roc_nix *roc_nix, uint16_t chan,
				      bool is_cpt, uint64_t *cfg);
int __roc_api roc_nix_rx_chan_cfg_set(struct roc_nix *roc_nix, uint16_t chan,
				      bool is_cpt, uint64_t val);
int __roc_api roc_nix_chan_bpid_set(struct roc_nix *roc_nix, uint16_t chan,
				    uint64_t bpid, int ena, bool cpt_chan);

/* NPC */
int __roc_api roc_nix_npc_promisc_ena_dis(struct roc_nix *roc_nix, int enable);

int __roc_api roc_nix_npc_mac_addr_set(struct roc_nix *roc_nix, uint8_t addr[]);

int __roc_api roc_nix_npc_mac_addr_get(struct roc_nix *roc_nix, uint8_t *addr);

int __roc_api roc_nix_npc_rx_ena_dis(struct roc_nix *roc_nix, bool enable);

int __roc_api roc_nix_npc_mcast_config(struct roc_nix *roc_nix,
				       bool mcast_enable, bool prom_enable);

/* RSS */
void __roc_api roc_nix_rss_key_default_fill(struct roc_nix *roc_nix,
					    uint8_t key[ROC_NIX_RSS_KEY_LEN]);
void __roc_api roc_nix_rss_key_set(struct roc_nix *roc_nix,
				   const uint8_t key[ROC_NIX_RSS_KEY_LEN]);
void __roc_api roc_nix_rss_key_get(struct roc_nix *roc_nix,
				   uint8_t key[ROC_NIX_RSS_KEY_LEN]);
int __roc_api roc_nix_rss_reta_set(struct roc_nix *roc_nix, uint8_t group,
				   uint16_t reta[ROC_NIX_RSS_RETA_MAX]);
int __roc_api roc_nix_rss_reta_get(struct roc_nix *roc_nix, uint8_t group,
				   uint16_t reta[ROC_NIX_RSS_RETA_MAX]);
int __roc_api roc_nix_rss_flowkey_set(struct roc_nix *roc_nix, uint8_t *alg_idx,
				      uint32_t flowkey, uint8_t group,
				      int mcam_index);
int __roc_api roc_nix_rss_default_setup(struct roc_nix *roc_nix,
					uint32_t flowkey);

/* Stats */
int __roc_api roc_nix_stats_get(struct roc_nix *roc_nix,
				struct roc_nix_stats *stats);
int __roc_api roc_nix_stats_reset(struct roc_nix *roc_nix);
int __roc_api roc_nix_stats_queue_get(struct roc_nix *roc_nix, uint16_t qid,
				      bool is_rx,
				      struct roc_nix_stats_queue *qstats);
int __roc_api roc_nix_stats_queue_reset(struct roc_nix *roc_nix, uint16_t qid,
					bool is_rx);
int __roc_api roc_nix_num_xstats_get(struct roc_nix *roc_nix);
int __roc_api roc_nix_xstats_get(struct roc_nix *roc_nix,
				 struct roc_nix_xstat *xstats, unsigned int n);
int __roc_api roc_nix_xstats_names_get(struct roc_nix *roc_nix,
				       struct roc_nix_xstat_name *xstats_names,
				       unsigned int limit);

/* Queue */
int __roc_api roc_nix_rq_init(struct roc_nix *roc_nix, struct roc_nix_rq *rq,
			      bool ena);
int __roc_api roc_nix_rq_modify(struct roc_nix *roc_nix, struct roc_nix_rq *rq,
				bool ena);
int __roc_api roc_nix_rq_cman_config(struct roc_nix *roc_nix, struct roc_nix_rq *rq);
int __roc_api roc_nix_rq_ena_dis(struct roc_nix_rq *rq, bool enable);
int __roc_api roc_nix_rq_is_sso_enable(struct roc_nix *roc_nix, uint32_t qid);
int __roc_api roc_nix_rq_fini(struct roc_nix_rq *rq);
int __roc_api roc_nix_cq_init(struct roc_nix *roc_nix, struct roc_nix_cq *cq);
int __roc_api roc_nix_cq_fini(struct roc_nix_cq *cq);
void __roc_api roc_nix_cq_head_tail_get(struct roc_nix *roc_nix, uint16_t qid,
					uint32_t *head, uint32_t *tail);
int __roc_api roc_nix_sq_init(struct roc_nix *roc_nix, struct roc_nix_sq *sq);
int __roc_api roc_nix_sq_fini(struct roc_nix_sq *sq);
void __roc_api roc_nix_sq_head_tail_get(struct roc_nix *roc_nix, uint16_t qid,
					uint32_t *head, uint32_t *tail);

/* PTP */
int __roc_api roc_nix_ptp_rx_ena_dis(struct roc_nix *roc_nix, int enable);
int __roc_api roc_nix_ptp_tx_ena_dis(struct roc_nix *roc_nix, int enable);
int __roc_api roc_nix_ptp_clock_read(struct roc_nix *roc_nix, uint64_t *clock,
				     uint64_t *tsc, uint8_t is_pmu);
int __roc_api roc_nix_ptp_sync_time_adjust(struct roc_nix *roc_nix,
					   int64_t delta);
int __roc_api roc_nix_ptp_info_cb_register(struct roc_nix *roc_nix,
					   ptp_info_update_t ptp_update);
void __roc_api roc_nix_ptp_info_cb_unregister(struct roc_nix *roc_nix);
bool __roc_api roc_nix_ptp_is_enable(struct roc_nix *roc_nix);

/* VLAN */
int __roc_api
roc_nix_vlan_mcam_entry_read(struct roc_nix *roc_nix, uint32_t index,
			     struct npc_mcam_read_entry_rsp **rsp);
int __roc_api roc_nix_vlan_mcam_entry_write(struct roc_nix *roc_nix,
					    uint32_t index,
					    struct mcam_entry *entry,
					    uint8_t intf, uint8_t enable);
int __roc_api roc_nix_vlan_mcam_entry_alloc_and_write(struct roc_nix *roc_nix,
						      struct mcam_entry *entry,
						      uint8_t intf,
						      uint8_t priority,
						      uint8_t ref_entry);
int __roc_api roc_nix_vlan_mcam_entry_free(struct roc_nix *roc_nix,
					   uint32_t index);
int __roc_api roc_nix_vlan_mcam_entry_ena_dis(struct roc_nix *roc_nix,
					      uint32_t index, const int enable);
int __roc_api roc_nix_vlan_strip_vtag_ena_dis(struct roc_nix *roc_nix,
					      bool enable);
int __roc_api roc_nix_vlan_insert_ena_dis(struct roc_nix *roc_nix,
					  struct roc_nix_vlan_config *vlan_cfg,
					  uint64_t *mcam_index, bool enable);
int __roc_api roc_nix_vlan_tpid_set(struct roc_nix *roc_nix, uint32_t type,
				    uint16_t tpid);

/* MCAST*/
int __roc_api roc_nix_mcast_mcam_entry_alloc(struct roc_nix *roc_nix,
					     uint16_t nb_entries,
					     uint8_t priority,
					     uint16_t index[]);
int __roc_api roc_nix_mcast_mcam_entry_free(struct roc_nix *roc_nix,
					    uint32_t index);
int __roc_api roc_nix_mcast_mcam_entry_write(struct roc_nix *roc_nix,
					     struct mcam_entry *entry,
					     uint32_t index, uint8_t intf,
					     uint64_t action);
int __roc_api roc_nix_mcast_mcam_entry_ena_dis(struct roc_nix *roc_nix,
					       uint32_t index, bool enable);
#endif /* _ROC_NIX_H_ */
