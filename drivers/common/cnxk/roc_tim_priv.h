/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_TIM_PRIV_H_
#define _ROC_TIM_PRIV_H_

struct tim {
	uint16_t tim_msix_offsets[MAX_RVU_BLKLF_CNT];
};

enum tim_err_status {
	TIM_ERR_PARAM = -5120,
};

static inline struct tim *
roc_tim_to_tim_priv(struct roc_tim *roc_tim)
{
	return (struct tim *)&roc_tim->reserved[0];
}

/* TIM IRQ*/
int tim_register_irq_priv(struct roc_tim *roc_tim,
			  struct plt_intr_handle *handle, uint8_t ring_id,
			  uint16_t msix_offset);
void tim_unregister_irq_priv(struct roc_tim *roc_tim,
			     struct plt_intr_handle *handle, uint8_t ring_id,
			     uint16_t msix_offset);

#endif /* _ROC_TIM_PRIV_H_ */
