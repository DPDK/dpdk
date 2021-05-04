/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CNXK_TIM_EVDEV_H__
#define __CNXK_TIM_EVDEV_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <eventdev_pmd_pci.h>
#include <rte_event_timer_adapter.h>
#include <rte_memzone.h>

#include "roc_api.h"

#define CNXK_TIM_EVDEV_NAME	   cnxk_tim_eventdev
#define CNXK_TIM_RING_DEF_CHUNK_SZ (4096)

struct cnxk_tim_evdev {
	struct roc_tim tim;
	struct rte_eventdev *event_dev;
	uint16_t nb_rings;
	uint32_t chunk_sz;
};

static inline struct cnxk_tim_evdev *
tim_priv_get(void)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(RTE_STR(CNXK_TIM_EVDEV_NAME));
	if (mz == NULL)
		return NULL;

	return mz->addr;
}

void cnxk_tim_init(struct roc_sso *sso);
void cnxk_tim_fini(void);

#endif /* __CNXK_TIM_EVDEV_H__ */
