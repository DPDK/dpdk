/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_eventdev.h"
#include "cnxk_tim_evdev.h"

void
cnxk_tim_init(struct roc_sso *sso)
{
	const struct rte_memzone *mz;
	struct cnxk_tim_evdev *dev;
	int rc;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	mz = rte_memzone_reserve(RTE_STR(CNXK_TIM_EVDEV_NAME),
				 sizeof(struct cnxk_tim_evdev), 0, 0);
	if (mz == NULL) {
		plt_tim_dbg("Unable to allocate memory for TIM Event device");
		return;
	}
	dev = mz->addr;

	dev->tim.roc_sso = sso;
	rc = roc_tim_init(&dev->tim);
	if (rc < 0) {
		plt_err("Failed to initialize roc tim resources");
		rte_memzone_free(mz);
		return;
	}
	dev->nb_rings = rc;
	dev->chunk_sz = CNXK_TIM_RING_DEF_CHUNK_SZ;
}

void
cnxk_tim_fini(void)
{
	struct cnxk_tim_evdev *dev = tim_priv_get();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	roc_tim_fini(&dev->tim);
	rte_memzone_free(rte_memzone_lookup(RTE_STR(CNXK_TIM_EVDEV_NAME)));
}
