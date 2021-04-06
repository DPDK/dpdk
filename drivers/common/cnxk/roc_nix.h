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

#endif /* _ROC_NIX_H_ */
