/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _ROC_NIX_INL_PRIV_H_
#define _ROC_NIX_INL_PRIV_H_
#include <pthread.h>
#include <sys/types.h>

#define NIX_INL_META_SIZE      384u
#define MAX_NIX_INL_DEV_CPT_LF 18
#define NIX_INL_PROFILE_CNT    8

struct nix_inl_dev;
struct nix_inl_qint {
	struct nix_inl_dev *inl_dev;
	uint16_t qint;
};

struct nix_inl_dev {
	/* Base device object */
	struct dev dev;

	/* PCI device */
	struct plt_pci_device *pci_dev;

	/* LF specific BAR2 regions */
	uintptr_t nix_base;
	uintptr_t ssow_base;
	uintptr_t sso_base;
	uintptr_t cpt_base;

	/* MSIX vector offsets */
	uint16_t nix_msixoff;
	uint16_t ssow_msixoff;
	uint16_t sso_msixoff;
	uint16_t cpt_msixoff[MAX_NIX_INL_DEV_CPT_LF];

	/* SSO data */
	uint32_t xaq_buf_size;
	uint32_t xae_waes;
	uint32_t iue;
	uint32_t nb_xae;
	struct roc_sso_xaq_data xaq;
	roc_nix_inl_sso_work_cb_t work_cb;
	void *cb_args;
	uint64_t *pkt_pools;
	uint16_t pkt_pools_cnt;

	/* NIX data */
	uint8_t lf_tx_stats;
	uint8_t lf_rx_stats;
	uint16_t vwqe_interval;
	uint16_t cints;
	uint16_t qints;
	uint16_t configured_qints;
	struct roc_nix_rq *rqs;
	struct nix_inl_qint *qints_mem;
	uint16_t nb_rqs;
	bool is_nix1;
	uint8_t spb_drop_pc;
	uint8_t lpb_drop_pc;
	uint8_t reass_ena; /* Plain packet reassembly enable */
	uint64_t sso_work_cnt;

	/* NIX/CPT data */
	void *inb_sa_base[NIX_INL_PROFILE_CNT];
	uint16_t inb_sa_sz[NIX_INL_PROFILE_CNT];
	uint32_t inb_sa_max[NIX_INL_PROFILE_CNT];
	uint8_t nb_cptlf;
	int8_t res_addr_offset;

	/* CPT data */
	struct roc_cpt_lf cpt_lf[MAX_NIX_INL_DEV_CPT_LF];
	uint16_t eng_grpmask;

	/* OUTB soft expiry poll thread */
	plt_thread_t soft_exp_poll_thread;
	uint32_t soft_exp_poll_freq;
	uint64_t *sa_soft_exp_ring;
	bool set_soft_exp_poll;

	/* Soft expiry ring bitmap */
	struct plt_bitmap *soft_exp_ring_bmap;

	/* bitmap memory */
	void *soft_exp_ring_bmap_mem;

	/* Device arguments */
	uint8_t selftest;
	uint16_t channel;
	uint16_t chan_mask;
	bool is_multi_channel;
	uint32_t ipsec_in_min_spi;
	uint32_t ipsec_in_max_spi;
	uint32_t inb_spi_mask;
	bool attach_cptlf;
	uint16_t wqe_skip;
	bool ts_ena;
	uint32_t nb_meta_bufs;
	uint32_t meta_buf_sz;
	uint8_t rx_inj_ena; /* Rx Inject Enable */
	uint8_t custom_inb_sa;

	/* NPC */
	int *ipsec_index;
	uint32_t curr_ipsec_idx;
	uint32_t max_ipsec_rules;
	uint32_t alloc_ipsec_rules;

	struct roc_nix_inl_dev_q q_info[MAX_NIX_INL_DEV_CPT_LF];

	/* Inbound CPT LF info */
	uint16_t inb_cpt_lf_id;
	uint16_t nix_inb_qids[MAX_NIX_INL_DEV_CPT_LF];
	uint16_t nb_inb_cptlfs;
	int nix_inb_q_bpid;
	uint16_t ipsec_prof_id;
	uint8_t reass_prof_id;
};

#define NIX_INL_DFLT_IPSEC_DEF_CFG                                                                 \
	(BIT_ULL(30) | BIT_ULL(29) | BIT_ULL(28) | NPC_LID_LE << 8 | NPC_LT_LE_ESP << 4 | 0xFul)

#define NIX_INL_DFLT_IPSEC_EXTRACT_CFG (32UL << 8 | 32UL)

#define NIX_INL_DFLT_IPSEC_GEN_CFG                                                                 \
	(BIT_ULL(51) | ROC_CPT_DFLT_ENG_GRP_SE << 48 |                                             \
	 ROC_IE_OW_MAJOR_OP_PROCESS_INBOUND_IPSEC << 32 | ROC_IE_OW_INPLACE_BIT << 32 |            \
	 BIT_ULL(18))

#define NIX_INL_REASS_DEF_CFG                                                                      \
	(BIT_ULL(30) | BIT_ULL(29) | BIT_ULL(28) | NPC_LID_LC << 8 |                               \
	 (NPC_LT_LC_IP | NPC_LT_LC_IP6) << 4 | 0xFul)

#define NIX_INL_REASS_GEN_CFG                                                                      \
	(BIT_ULL(51) | (ROC_CPT_DFLT_ENG_GRP_SE << 48) |                                           \
	 (ROC_IE_OW_MAJOR_OP_PROCESS_INBOUND_REASS << 32 | ROC_IE_OW_INPLACE_BIT << 32))

int nix_inl_sso_register_irqs(struct nix_inl_dev *inl_dev);
void nix_inl_sso_unregister_irqs(struct nix_inl_dev *inl_dev);

int nix_inl_nix_register_irqs(struct nix_inl_dev *inl_dev);
void nix_inl_nix_unregister_irqs(struct nix_inl_dev *inl_dev);

uint16_t nix_inl_dev_pffunc_get(void);

int nix_inl_setup_dflt_ipsec_profile(struct dev *dev, uint16_t *prof_id);
int nix_inl_setup_reass_profile(struct dev *dev, uint8_t *prof_id);

#endif /* _ROC_NIX_INL_PRIV_H_ */
