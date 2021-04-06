/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_DEV_PRIV_H
#define _ROC_DEV_PRIV_H

int dev_irq_register(struct plt_intr_handle *intr_handle,
		     plt_intr_callback_fn cb, void *data, unsigned int vec);
void dev_irq_unregister(struct plt_intr_handle *intr_handle,
			plt_intr_callback_fn cb, void *data, unsigned int vec);
int dev_irqs_disable(struct plt_intr_handle *intr_handle);

#endif /* _ROC_DEV_PRIV_H */
