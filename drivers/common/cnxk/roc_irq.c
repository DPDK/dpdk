/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#if defined(__linux__)

int
dev_irqs_disable(struct plt_intr_handle *intr_handle)
{
	return plt_irq_disable(intr_handle);
}

int
dev_irq_reconfigure(struct plt_intr_handle *intr_handle, uint16_t max_intr)
{
	return plt_irq_reconfigure(intr_handle, max_intr);
}

int
dev_irq_register(struct plt_intr_handle *intr_handle, plt_intr_callback_fn cb, void *data,
		 unsigned int vec)
{
	return plt_irq_register(intr_handle, cb, data, vec);
}

void
dev_irq_unregister(struct plt_intr_handle *intr_handle, plt_intr_callback_fn cb, void *data,
		   unsigned int vec)
{
	plt_irq_unregister(intr_handle, cb, data, vec);
}

#else

int
dev_irq_register(struct plt_intr_handle *intr_handle, plt_intr_callback_fn cb, void *data,
		 unsigned int vec)
{
	PLT_SET_USED(intr_handle);
	PLT_SET_USED(cb);
	PLT_SET_USED(data);
	PLT_SET_USED(vec);

	return -ENOTSUP;
}

void
dev_irq_unregister(struct plt_intr_handle *intr_handle, plt_intr_callback_fn cb, void *data,
		   unsigned int vec)
{
	PLT_SET_USED(intr_handle);
	PLT_SET_USED(cb);
	PLT_SET_USED(data);
	PLT_SET_USED(vec);
}

int
dev_irqs_disable(struct plt_intr_handle *intr_handle)
{
	PLT_SET_USED(intr_handle);

	return -ENOTSUP;
}

int
dev_irq_reconfigure(struct plt_intr_handle *intr_handle, uint16_t max_intr)
{
	PLT_SET_USED(intr_handle);
	PLT_SET_USED(max_intr);

	return -ENOTSUP;
}

#endif /* __linux__ */
