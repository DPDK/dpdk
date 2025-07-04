/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_DEV_PRIV_H
#define _ROC_DEV_PRIV_H

#define DEV_HWCAP_F_VF BIT_ULL(0) /* VF device */

/* PF and VF bit encoding parameters in pcifunc */
#define RVU_PFVF_PF_SHIFT_CN20K	  9
#define RVU_PFVF_PF_MASK_CN20K	  0x7F
#define RVU_PFVF_FUNC_SHIFT_CN20K 0
#define RVU_PFVF_FUNC_MASK_CN20K  0x1FF

#define RVU_PFVF_PF_SHIFT   10
#define RVU_PFVF_PF_MASK    0x3F
#define RVU_PFVF_FUNC_SHIFT 0
#define RVU_PFVF_FUNC_MASK  0x3FF

#define RVU_MAX_VF	  64 /* RVU_PF_VFPF_MBOX_INT(0..1) */
#define RVU_MAX_INT_RETRY 3

/* PF/VF message handling timer */
#define VF_PF_MBOX_TIMER_MS (20 * 1000)

typedef struct {
/* 128 devices translate to two 64 bits dwords */
#define MAX_VFPF_DWORD_BITS 2
	uint64_t bits[MAX_VFPF_DWORD_BITS];
} dev_intr_t;

/* Link status update callback */
typedef void (*link_info_t)(void *roc_nix,
			    struct cgx_link_user_info *link);

/* PTP info callback */
typedef int (*ptp_info_t)(void *roc_nix, bool enable);

/* Queue Error get callback */
typedef void (*q_err_cb_t)(void *roc_nix, void *data);

/* Link status get callback */
typedef void (*link_status_get_t)(void *roc_nix,
				  struct cgx_link_user_info *link);
/* Representee notification callback */
typedef int (*repte_notify_t)(void *roc_nix, void *notify_msg);

/* RVU Message process callback */
typedef int (*msg_process_cb_t)(uint16_t vf, uint16_t msg_id,
				void *req, uint16_t req_len,
				void **rsp, uint16_t *rsp_len);

struct dev_ops {
	link_info_t link_status_update;
	ptp_info_t ptp_info_update;
	link_status_get_t link_status_get;
	q_err_cb_t q_err_cb;
	msg_process_cb_t msg_process_cb;
	repte_notify_t repte_notify;
};

#define dev_is_vf(dev) ((dev)->hwcap & DEV_HWCAP_F_VF)

static inline int
dev_get_vf(uint16_t pf_func)
{
	if (roc_model_is_cn20k())
		return (((pf_func >> RVU_PFVF_FUNC_SHIFT_CN20K) & RVU_PFVF_FUNC_MASK_CN20K) - 1);
	else
		return (((pf_func >> RVU_PFVF_FUNC_SHIFT) & RVU_PFVF_FUNC_MASK) - 1);
}

static inline int
dev_get_pf(uint16_t pf_func)
{
	if (roc_model_is_cn20k())
		return (pf_func >> RVU_PFVF_PF_SHIFT_CN20K) & RVU_PFVF_PF_MASK_CN20K;
	else
		return (pf_func >> RVU_PFVF_PF_SHIFT) & RVU_PFVF_PF_MASK;
}

static inline int
dev_pf_func(int pf, int vf)
{
	if (roc_model_is_cn20k())
		return (pf << RVU_PFVF_PF_SHIFT_CN20K) | ((vf << RVU_PFVF_FUNC_SHIFT_CN20K) + 1);
	else
		return (pf << RVU_PFVF_PF_SHIFT) | ((vf << RVU_PFVF_FUNC_SHIFT) + 1);
}

static inline int
dev_is_afvf(uint16_t pf_func)
{
	if (roc_model_is_cn20k())
		return !(pf_func & ~RVU_PFVF_FUNC_MASK_CN20K);
	else
		return !(pf_func & ~RVU_PFVF_FUNC_MASK);
}

struct mbox_sync {
	bool start_thread;
	uint8_t msg_avail;
	plt_thread_t pfvf_msg_thread;
	pthread_cond_t pfvf_msg_cond;
	pthread_mutex_t mutex;
};

struct mbox_platform {
	uint8_t pfaf_vec;
	uint8_t pfvf_mbox0_vec;
	uint8_t pfvf_mbox1_vec;
	uint8_t pfvf1_mbox0_vec;
	uint8_t pfvf1_mbox1_vec;
	uint64_t pfvf_mbox_intx[MAX_VFPF_DWORD_BITS];
	uint64_t pfvf_mbox_int_ena_w1s[MAX_VFPF_DWORD_BITS];
	uint64_t pfvf_mbox_int_ena_w1c[MAX_VFPF_DWORD_BITS];
	uint64_t pfvf1_mbox_intx[MAX_VFPF_DWORD_BITS];
	uint64_t pfvf1_mbox_int_ena_w1s[MAX_VFPF_DWORD_BITS];
	uint64_t pfvf1_mbox_int_ena_w1c[MAX_VFPF_DWORD_BITS];
	uintptr_t mbox_reg_base;
	uintptr_t mbox_region_base;
};

struct dev {
	uint16_t pf;
	int16_t vf;
	uint16_t pf_func;
	uint8_t mbox_active;
	bool drv_inited;
	uint64_t active_vfs[MAX_VFPF_DWORD_BITS];
	uintptr_t bar2;
	uintptr_t bar4;
	uintptr_t lmt_base;
	struct mbox mbox_local;
	struct mbox mbox_up;
	struct mbox mbox_vfpf;
	struct mbox mbox_vfpf_up;
	dev_intr_t intr;
	dev_intr_t flr;
	uint64_t hwcap;
	struct npa_lf npa;
	struct mbox *mbox;
	uint16_t maxvf;
	struct dev_ops *ops;
	void *roc_nix;
	void *roc_cpt;
	void *roc_tim;
	void *roc_ml;
	void *roc_rvu_lf;
	bool disable_shared_lmt; /* false(default): shared lmt mode enabled */
	const struct plt_memzone *lmt_mz;
	struct mbox_sync sync;
	uintptr_t mbox_reg_base;
	uintptr_t vf_mbox_base;
	const struct plt_memzone *vf_mbox_mz;
	struct mbox_platform *mbox_plat;
} __plt_cache_aligned;

struct npa {
	struct plt_pci_device *pci_dev;
	struct dev dev;
} __plt_cache_aligned;

extern uint16_t dev_rclk_freq;
extern uint16_t dev_sclk_freq;

int dev_init(struct dev *dev, struct plt_pci_device *pci_dev);
int dev_fini(struct dev *dev, struct plt_pci_device *pci_dev);
int dev_active_vfs(struct dev *dev);

int dev_irq_register(struct plt_intr_handle *intr_handle,
		     plt_intr_callback_fn cb, void *data, unsigned int vec);
void dev_irq_unregister(struct plt_intr_handle *intr_handle,
			plt_intr_callback_fn cb, void *data, unsigned int vec);
int dev_irqs_disable(struct plt_intr_handle *intr_handle);
int dev_irq_reconfigure(struct plt_intr_handle *intr_handle, uint16_t max_intr);

int dev_mbox_register_irq(struct plt_pci_device *pci_dev, struct dev *dev);
void dev_mbox_unregister_irq(struct plt_pci_device *pci_dev, struct dev *dev);
int dev_vf_flr_register_irqs(struct plt_pci_device *pci_dev, struct dev *dev);
void dev_vf_flr_unregister_irqs(struct plt_pci_device *pci_dev, struct dev *dev);

#endif /* _ROC_DEV_PRIV_H */
