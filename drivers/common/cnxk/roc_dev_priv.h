/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_DEV_PRIV_H
#define _ROC_DEV_PRIV_H

#define RVU_PFVF_PF_SHIFT   10
#define RVU_PFVF_PF_MASK    0x3F
#define RVU_PFVF_FUNC_SHIFT 0
#define RVU_PFVF_FUNC_MASK  0x3FF
#define RVU_MAX_INT_RETRY   3

static inline int
dev_get_vf(uint16_t pf_func)
{
	return (((pf_func >> RVU_PFVF_FUNC_SHIFT) & RVU_PFVF_FUNC_MASK) - 1);
}

static inline int
dev_get_pf(uint16_t pf_func)
{
	return (pf_func >> RVU_PFVF_PF_SHIFT) & RVU_PFVF_PF_MASK;
}

static inline int
dev_pf_func(int pf, int vf)
{
	return (pf << RVU_PFVF_PF_SHIFT) | ((vf << RVU_PFVF_FUNC_SHIFT) + 1);
}

struct dev {
	uint16_t pf;
	uint16_t pf_func;
	uint8_t mbox_active;
	bool drv_inited;
	uintptr_t bar2;
	uintptr_t bar4;
	uintptr_t lmt_base;
	struct mbox mbox_local;
	struct mbox mbox_up;
	uint64_t hwcap;
	struct mbox *mbox;
	bool disable_shared_lmt; /* false(default): shared lmt mode enabled */
} __plt_cache_aligned;

extern uint16_t dev_rclk_freq;
extern uint16_t dev_sclk_freq;

int dev_init(struct dev *dev, struct plt_pci_device *pci_dev);
int dev_fini(struct dev *dev, struct plt_pci_device *pci_dev);

int dev_irq_register(struct plt_intr_handle *intr_handle,
		     plt_intr_callback_fn cb, void *data, unsigned int vec);
void dev_irq_unregister(struct plt_intr_handle *intr_handle,
			plt_intr_callback_fn cb, void *data, unsigned int vec);
int dev_irqs_disable(struct plt_intr_handle *intr_handle);

#endif /* _ROC_DEV_PRIV_H */
