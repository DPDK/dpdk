/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TIM_EVDEV_H__
#define __OTX2_TIM_EVDEV_H__

#include <rte_event_timer_adapter.h>

#include "otx2_dev.h"

#define OTX2_TIM_EVDEV_NAME otx2_tim_eventdev

struct otx2_tim_evdev {
	struct rte_pci_device *pci_dev;
	struct otx2_mbox *mbox;
	uint16_t nb_rings;
	uintptr_t bar2;
};

static inline struct otx2_tim_evdev *
tim_priv_get(void)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(RTE_STR(OTX2_TIM_EVDEV_NAME));
	if (mz == NULL)
		return NULL;

	return mz->addr;
}

void otx2_tim_init(struct rte_pci_device *pci_dev, struct otx2_dev *cmn_dev);
void otx2_tim_fini(void);

#endif /* __OTX2_TIM_EVDEV_H__ */
