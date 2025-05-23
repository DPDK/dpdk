/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _ROC_NIX_INL_H_
#define _ROC_NIX_INL_H_

/* ON INB HW area */
#define ROC_NIX_INL_ON_IPSEC_INB_HW_SZ                                         \
	PLT_ALIGN(sizeof(struct roc_ie_on_inb_sa), ROC_ALIGN)
/* ON INB SW reserved area */
#define ROC_NIX_INL_ON_IPSEC_INB_SW_RSVD 640
#define ROC_NIX_INL_ON_IPSEC_INB_SA_SZ                                         \
	(ROC_NIX_INL_ON_IPSEC_INB_HW_SZ + ROC_NIX_INL_ON_IPSEC_INB_SW_RSVD)
#define ROC_NIX_INL_ON_IPSEC_INB_SA_SZ_LOG2 10

/* ON OUTB HW area */
#define ROC_NIX_INL_ON_IPSEC_OUTB_HW_SZ                                        \
	PLT_ALIGN(sizeof(struct roc_ie_on_outb_sa), ROC_ALIGN)
/* ON OUTB SW reserved area */
#define ROC_NIX_INL_ON_IPSEC_OUTB_SW_RSVD 256
#define ROC_NIX_INL_ON_IPSEC_OUTB_SA_SZ                                        \
	(ROC_NIX_INL_ON_IPSEC_OUTB_HW_SZ + ROC_NIX_INL_ON_IPSEC_OUTB_SW_RSVD)
#define ROC_NIX_INL_ON_IPSEC_OUTB_SA_SZ_LOG2 9

#define ROC_NIX_INL_SA_SOFT_EXP_ERR_MAX_POLL_COUNT 25

#define ROC_NIX_SOFT_EXP_ERR_RING_MAX_ENTRY_LOG2 16

#define ROC_NIX_SOFT_EXP_PER_PORT_MAX_RINGS 4

#define ROC_NIX_MAX_TOTAL_OUTB_IPSEC_SA                                        \
	(ROC_IPSEC_ERR_RING_MAX_ENTRY * ROC_NIX_SOFT_EXP_PER_PORT_MAX_RINGS)

#define ROC_NIX_INL_MAX_SOFT_EXP_RNGS                                          \
	(PLT_MAX_ETHPORTS * ROC_NIX_SOFT_EXP_PER_PORT_MAX_RINGS)
#define ROC_NIX_INL_INB_CUSTOM_SA_SZ 512

/* Reassembly configuration */
#define ROC_NIX_INL_REAS_ACTIVE_LIMIT	  0xFFF
#define ROC_NIX_INL_REAS_ACTIVE_THRESHOLD 10
#define ROC_NIX_INL_REAS_ZOMBIE_LIMIT	  0xFFF
#define ROC_NIX_INL_REAS_ZOMBIE_THRESHOLD 10

static inline struct roc_ie_on_inb_sa *
roc_nix_inl_on_ipsec_inb_sa(uintptr_t base, uint64_t idx)
{
	uint64_t off = idx << ROC_NIX_INL_ON_IPSEC_INB_SA_SZ_LOG2;

	return PLT_PTR_ADD(base, off);
}

static inline struct roc_ie_on_outb_sa *
roc_nix_inl_on_ipsec_outb_sa(uintptr_t base, uint64_t idx)
{
	uint64_t off = idx << ROC_NIX_INL_ON_IPSEC_OUTB_SA_SZ_LOG2;

	return PLT_PTR_ADD(base, off);
}

static inline void *
roc_nix_inl_on_ipsec_inb_sa_sw_rsvd(void *sa)
{
	return PLT_PTR_ADD(sa, ROC_NIX_INL_ON_IPSEC_INB_HW_SZ);
}

static inline void *
roc_nix_inl_on_ipsec_outb_sa_sw_rsvd(void *sa)
{
	return PLT_PTR_ADD(sa, ROC_NIX_INL_ON_IPSEC_OUTB_HW_SZ);
}

/* Inline device SSO Work callback */
typedef void (*roc_nix_inl_sso_work_cb_t)(uint64_t *gw, void *args,
					  uint32_t soft_exp_event);

typedef int (*roc_nix_inl_meta_pool_cb_t)(uint64_t *aura_handle,  uintptr_t *mpool,
					  uint32_t blk_sz, uint32_t nb_bufs, bool destroy,
					  const char *mempool_name);
typedef int (*roc_nix_inl_custom_meta_pool_cb_t)(uintptr_t pmpool, uintptr_t *mpool,
						 const char *mempool_name, uint64_t *aura_handle,
						 uint32_t blk_sz, uint32_t nb_bufs, bool destroy);

struct roc_nix_inl_dev {
	/* Input parameters */
	struct plt_pci_device *pci_dev;
	uint32_t ipsec_in_min_spi;
	uint32_t ipsec_in_max_spi;
	bool selftest;
	bool is_multi_channel;
	uint16_t channel;
	uint16_t chan_mask;
	uint16_t wqe_skip;
	uint8_t spb_drop_pc;
	uint8_t lpb_drop_pc;
	uint32_t soft_exp_poll_freq; /* Polling disabled if 0 */
	uint32_t nb_meta_bufs;
	uint32_t meta_buf_sz;
	uint32_t max_ipsec_rules;
	uint8_t rx_inj_ena; /* Rx Inject Enable */
	uint8_t custom_inb_sa;
	uint8_t nb_inb_cptlfs;
	int8_t res_addr_offset; /* CPT result address offset */

	/* End of input parameters */

#define ROC_NIX_INL_MEM_SZ (6144)
	uint8_t reserved[ROC_NIX_INL_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

struct roc_nix_inl_dev_q {
	uint32_t nb_desc;
	uintptr_t rbase;
	uintptr_t lmt_base;
	uint64_t __plt_atomic *fc_addr;
	uint64_t io_addr;
	int32_t fc_addr_sw;
} __plt_cache_aligned;

struct roc_nix_cpt_lf_stats {
	uint64_t enc_pkts;
	uint64_t enc_bytes;
	uint64_t dec_pkts;
	uint64_t dec_bytes;
};

/* NIX Inline Device API */
int __roc_api roc_nix_inl_dev_init(struct roc_nix_inl_dev *roc_inl_dev);
int __roc_api roc_nix_inl_dev_fini(struct roc_nix_inl_dev *roc_inl_dev);
void __roc_api roc_nix_inl_dev_dump(struct roc_nix_inl_dev *roc_inl_dev, FILE *file);
bool __roc_api roc_nix_inl_dev_is_probed(void);
void __roc_api roc_nix_inl_dev_lock(void);
void __roc_api roc_nix_inl_dev_unlock(void);
int __roc_api roc_nix_inl_dev_xaq_realloc(uint64_t aura_handle);
int __roc_api roc_nix_inl_dev_stats_get(struct roc_nix_stats *stats);
int __roc_api roc_nix_inl_dev_stats_reset(void);
int __roc_api roc_nix_inl_dev_cpt_setup(bool use_inl_dev_sso);
int __roc_api roc_nix_inl_dev_cpt_release(void);
bool __roc_api roc_nix_inl_dev_is_multi_channel(void);

/* NIX Inline Inbound API */
int __roc_api roc_nix_inl_inb_init(struct roc_nix *roc_nix);
int __roc_api roc_nix_inl_inb_fini(struct roc_nix *roc_nix);
bool __roc_api roc_nix_inl_inb_is_enabled(struct roc_nix *roc_nix);
uintptr_t __roc_api roc_nix_inl_inb_sa_base_get(struct roc_nix *roc_nix,
						bool inl_dev_sa);
uint16_t roc_nix_inl_inb_ipsec_profile_id_get(struct roc_nix *roc_nix, bool inb_inl_dev);
uint16_t roc_nix_inl_inb_reass_profile_id_get(struct roc_nix *roc_nix, bool inb_inl_dev);
bool __roc_api roc_nix_inl_inb_rx_inject_enable(struct roc_nix *roc_nix, bool inl_dev_sa);
uint32_t __roc_api roc_nix_inl_inb_spi_range(struct roc_nix *roc_nix,
					     bool inl_dev_sa, uint32_t *min,
					     uint32_t *max);
uint32_t __roc_api roc_nix_inl_inb_sa_sz(struct roc_nix *roc_nix,
					 bool inl_dev_sa);
uintptr_t __roc_api roc_nix_inl_inb_sa_get(struct roc_nix *roc_nix,
					   bool inl_dev_sa, uint32_t spi);
void __roc_api roc_nix_inb_mode_set(struct roc_nix *roc_nix, bool use_inl_dev);
void __roc_api roc_nix_inl_inb_set(struct roc_nix *roc_nix, bool ena);
int __roc_api roc_nix_inl_dev_rq_get(struct roc_nix_rq *rq, bool ena);
int __roc_api roc_nix_inl_dev_rq_put(struct roc_nix_rq *rq);
bool __roc_api roc_nix_inb_is_with_inl_dev(struct roc_nix *roc_nix);
struct roc_nix_rq *__roc_api roc_nix_inl_dev_rq(struct roc_nix *roc_nix);
int __roc_api roc_nix_inl_inb_tag_update(struct roc_nix *roc_nix, uint32_t tag_const, uint8_t tt);
int __roc_api roc_nix_reassembly_configure(struct roc_cpt_rxc_time_cfg *req_cfg,
					   uint32_t max_wait_time);
int __roc_api roc_nix_inl_ts_pkind_set(struct roc_nix *roc_nix, bool ts_ena, bool inb_inl_dev,
				       uint8_t profile_id);
int __roc_api roc_nix_inl_rq_ena_dis(struct roc_nix *roc_nix, bool ena);
int __roc_api roc_nix_inl_meta_aura_check(struct roc_nix *roc_nix, struct roc_nix_rq *rq);

/* NIX Inline Outbound API */
int __roc_api roc_nix_inl_outb_init(struct roc_nix *roc_nix);
int __roc_api roc_nix_inl_outb_fini(struct roc_nix *roc_nix);
bool __roc_api roc_nix_inl_outb_is_enabled(struct roc_nix *roc_nix);
uintptr_t __roc_api roc_nix_inl_outb_sa_base_get(struct roc_nix *roc_nix);
struct roc_cpt_lf *__roc_api
roc_nix_inl_outb_lf_base_get(struct roc_nix *roc_nix);
struct roc_cpt_lf *__roc_api roc_nix_inl_inb_inj_lf_get(struct roc_nix *roc_nix);
uint16_t __roc_api roc_nix_inl_outb_sso_pffunc_get(struct roc_nix *roc_nix);
int __roc_api roc_nix_inl_cb_register(roc_nix_inl_sso_work_cb_t cb, void *args);
int __roc_api roc_nix_inl_cb_unregister(roc_nix_inl_sso_work_cb_t cb,
					void *args);
int __roc_api roc_nix_inl_outb_soft_exp_poll_switch(struct roc_nix *roc_nix,
						    bool poll);
uint64_t *__roc_api roc_nix_inl_outb_ring_base_get(struct roc_nix *roc_nix);
void __roc_api roc_nix_inl_meta_pool_cb_register(roc_nix_inl_meta_pool_cb_t cb);
void __roc_api roc_nix_inl_custom_meta_pool_cb_register(roc_nix_inl_custom_meta_pool_cb_t cb);

/* NIX Inline/Outbound API */
enum roc_nix_inl_sa_sync_op {
	ROC_NIX_INL_SA_OP_FLUSH,
	ROC_NIX_INL_SA_OP_FLUSH_INVAL,
	ROC_NIX_INL_SA_OP_RELOAD,
};

int __roc_api roc_nix_inl_sa_sync(struct roc_nix *roc_nix, void *sa, bool inb,
				  enum roc_nix_inl_sa_sync_op op);
int __roc_api roc_nix_inl_ctx_write(struct roc_nix *roc_nix, void *sa_dptr,
				    void *sa_cptr, bool inb, uint16_t sa_len);
void __roc_api roc_nix_inl_outb_cpt_lfs_dump(struct roc_nix *roc_nix, FILE *file);
uint64_t __roc_api roc_nix_inl_eng_caps_get(struct roc_nix *roc_nix);
void *__roc_api roc_nix_inl_dev_qptr_get(uint8_t qid);

enum roc_nix_cpt_lf_stats_type {
	ROC_NIX_CPT_LF_STATS_INL_DEV,
	ROC_NIX_CPT_LF_STATS_KERNEL,
	ROC_NIX_CPT_LF_STATS_ETHDEV = 2,
};
int __roc_api roc_nix_inl_cpt_lf_stats_get(struct roc_nix *roc_nix,
					   enum roc_nix_cpt_lf_stats_type type,
					   struct roc_nix_cpt_lf_stats *stats, uint16_t idx);
#endif /* _ROC_NIX_INL_H_ */
