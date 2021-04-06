/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_NIX_H_
#define _ROC_NIX_H_

/* Constants */
enum roc_nix_rss_reta_sz {
	ROC_NIX_RSS_RETA_SZ_64 = 64,
	ROC_NIX_RSS_RETA_SZ_128 = 128,
	ROC_NIX_RSS_RETA_SZ_256 = 256,
};

enum roc_nix_sq_max_sqe_sz {
	roc_nix_maxsqesz_w16 = NIX_MAXSQESZ_W16,
	roc_nix_maxsqesz_w8 = NIX_MAXSQESZ_W8,
};

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

/* Group 0 will be used for RSS, 1 -7 will be used for npc_flow RSS action*/
#define ROC_NIX_RSS_GROUP_DEFAULT 0
#define ROC_NIX_RSS_GRPS	  8
#define ROC_NIX_RSS_RETA_MAX	  ROC_NIX_RSS_RETA_SZ_256
#define ROC_NIX_RSS_KEY_LEN	  48 /* 352 Bits */

#define ROC_NIX_DEFAULT_HW_FRS 1514

#define ROC_NIX_VWQE_MAX_SIZE_LOG2 11
#define ROC_NIX_VWQE_MIN_SIZE_LOG2 2

struct roc_nix_rq {
	/* Input parameters */
	uint16_t qid;
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
	/* End of Input parameters */
	struct roc_nix *roc_nix;
};

struct roc_nix_cq {
	/* Input parameters */
	uint16_t qid;
	uint16_t nb_desc;
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
	/* End of Input parameters */
	uint16_t sqes_per_sqb_log2;
	struct roc_nix *roc_nix;
	uint64_t aura_handle;
	int16_t nb_sqb_bufs_adj;
	uint16_t nb_sqb_bufs;
	plt_iova_t io_addr;
	void *lmt_addr;
	void *sqe_mem;
	void *fc;
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

struct roc_nix {
	/* Input parameters */
	struct plt_pci_device *pci_dev;
	uint16_t port_id;
	bool rss_tag_as_xor;
	uint16_t max_sqb_count;
	enum roc_nix_rss_reta_sz reta_sz;
	bool enable_loop;
	/* End of input parameters */
	/* LMT line base for "Per Core Tx LMT line" mode*/
	uintptr_t lmt_base;
	bool io_enabled;
	bool rx_ptp_ena;
	uint16_t cints;

#define ROC_NIX_MEM_SZ (6 * 1024)
	uint8_t reserved[ROC_NIX_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

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

/* Queue */
int __roc_api roc_nix_rq_init(struct roc_nix *roc_nix, struct roc_nix_rq *rq,
			      bool ena);
int __roc_api roc_nix_rq_modify(struct roc_nix *roc_nix, struct roc_nix_rq *rq,
				bool ena);
int __roc_api roc_nix_rq_ena_dis(struct roc_nix_rq *rq, bool enable);
int __roc_api roc_nix_rq_fini(struct roc_nix_rq *rq);
int __roc_api roc_nix_cq_init(struct roc_nix *roc_nix, struct roc_nix_cq *cq);
int __roc_api roc_nix_cq_fini(struct roc_nix_cq *cq);
int __roc_api roc_nix_sq_init(struct roc_nix *roc_nix, struct roc_nix_sq *sq);
int __roc_api roc_nix_sq_fini(struct roc_nix_sq *sq);

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
