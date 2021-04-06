/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_NIX_PRIV_H_
#define _ROC_NIX_PRIV_H_

/* Constants */
#define NIX_CQ_ENTRY_SZ	     128
#define NIX_CQ_ENTRY64_SZ    512
#define NIX_CQ_ALIGN	     ((uint16_t)512)
#define NIX_MAX_SQB	     ((uint16_t)512)
#define NIX_DEF_SQB	     ((uint16_t)16)
#define NIX_MIN_SQB	     ((uint16_t)8)
#define NIX_SQB_LIST_SPACE   ((uint16_t)2)
#define NIX_SQB_LOWER_THRESH ((uint16_t)70)

/* Apply BP/DROP when CQ is 95% full */
#define NIX_CQ_THRESH_LEVEL (5 * 256 / 100)

struct nix {
	uint16_t reta[ROC_NIX_RSS_GRPS][ROC_NIX_RSS_RETA_MAX];
	enum roc_nix_rss_reta_sz reta_sz;
	struct plt_pci_device *pci_dev;
	uint16_t bpid[NIX_MAX_CHAN];
	struct roc_nix_sq **sqs;
	uint16_t vwqe_interval;
	uint16_t tx_chan_base;
	uint16_t rx_chan_base;
	uint16_t nb_rx_queues;
	uint16_t nb_tx_queues;
	uint8_t lso_tsov6_idx;
	uint8_t lso_tsov4_idx;
	uint8_t lf_rx_stats;
	uint8_t lf_tx_stats;
	uint8_t rx_chan_cnt;
	uint8_t rss_alg_idx;
	uint8_t tx_chan_cnt;
	uintptr_t lmt_base;
	uint8_t cgx_links;
	uint8_t lbk_links;
	uint8_t sdp_links;
	uint8_t tx_link;
	uint16_t sqb_size;
	/* Without FCS, with L2 overhead */
	uint16_t mtu;
	uint16_t chan_cnt;
	uint16_t msixoff;
	uint8_t rx_pause;
	uint8_t tx_pause;
	struct dev dev;
	uint16_t cints;
	uint16_t qints;
	uintptr_t base;
	bool sdp_link;
	bool lbk_link;
	bool ptp_en;
	bool is_nix1;

} __plt_cache_aligned;

enum nix_err_status {
	NIX_ERR_PARAM = -2048,
	NIX_ERR_NO_MEM,
	NIX_ERR_INVALID_RANGE,
	NIX_ERR_INTERNAL,
	NIX_ERR_OP_NOTSUP,
	NIX_ERR_QUEUE_INVALID_RANGE,
	NIX_ERR_AQ_READ_FAILED,
	NIX_ERR_AQ_WRITE_FAILED,
	NIX_ERR_NDC_SYNC,
};

enum nix_q_size {
	nix_q_size_16, /* 16 entries */
	nix_q_size_64, /* 64 entries */
	nix_q_size_256,
	nix_q_size_1K,
	nix_q_size_4K,
	nix_q_size_16K,
	nix_q_size_64K,
	nix_q_size_256K,
	nix_q_size_1M, /* Million entries */
	nix_q_size_max
};

static inline struct nix *
roc_nix_to_nix_priv(struct roc_nix *roc_nix)
{
	return (struct nix *)&roc_nix->reserved[0];
}

static inline struct roc_nix *
nix_priv_to_roc_nix(struct nix *nix)
{
	return (struct roc_nix *)((char *)nix -
				  offsetof(struct roc_nix, reserved));
}

#endif /* _ROC_NIX_PRIV_H_ */
