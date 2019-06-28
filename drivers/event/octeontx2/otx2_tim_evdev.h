/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TIM_EVDEV_H__
#define __OTX2_TIM_EVDEV_H__

#include <rte_event_timer_adapter.h>
#include <rte_event_timer_adapter_pmd.h>

#include "otx2_dev.h"

#define OTX2_TIM_EVDEV_NAME otx2_tim_eventdev

#define otx2_tim_func_trace otx2_tim_dbg

#define TIM_LF_RING_AURA		(0x0)
#define TIM_LF_RING_BASE		(0x130)

#define OTX2_MAX_TIM_RINGS		(256)
#define OTX2_TIM_MAX_BUCKETS		(0xFFFFF)
#define OTX2_TIM_RING_DEF_CHUNK_SZ	(4096)
#define OTX2_TIM_CHUNK_ALIGNMENT	(16)
#define OTX2_TIM_NB_CHUNK_SLOTS(sz)	(((sz) / OTX2_TIM_CHUNK_ALIGNMENT) - 1)
#define OTX2_TIM_MIN_CHUNK_SLOTS	(0x1)
#define OTX2_TIM_MAX_CHUNK_SLOTS	(0x1FFE)
#define OTX2_TIM_MIN_TMO_TKS		(256)

enum otx2_tim_clk_src {
	OTX2_TIM_CLK_SRC_10NS = RTE_EVENT_TIMER_ADAPTER_CPU_CLK,
	OTX2_TIM_CLK_SRC_GPIO = RTE_EVENT_TIMER_ADAPTER_EXT_CLK0,
	OTX2_TIM_CLK_SRC_GTI  = RTE_EVENT_TIMER_ADAPTER_EXT_CLK1,
	OTX2_TIM_CLK_SRC_PTP  = RTE_EVENT_TIMER_ADAPTER_EXT_CLK2,
};

struct otx2_tim_bkt {
	uint64_t first_chunk;
	union {
		uint64_t w1;
		struct {
			uint32_t nb_entry;
			uint8_t sbt:1;
			uint8_t hbt:1;
			uint8_t bsk:1;
			uint8_t rsvd:5;
			uint8_t lock;
			int16_t chunk_remainder;
		};
	};
	uint64_t current_chunk;
	uint64_t pad;
} __rte_packed __rte_aligned(32);

struct otx2_tim_evdev {
	struct rte_pci_device *pci_dev;
	struct rte_eventdev *event_dev;
	struct otx2_mbox *mbox;
	uint16_t nb_rings;
	uint32_t chunk_sz;
	uintptr_t bar2;
	/* Dev args */
	uint8_t disable_npa;
	uint16_t chunk_slots;
};

struct otx2_tim_ring {
	uintptr_t base;
	uint16_t nb_chunk_slots;
	uint32_t nb_bkts;
	struct otx2_tim_bkt *bkt;
	struct rte_mempool *chunk_pool;
	uint64_t tck_int;
	uint8_t prod_type_sp;
	uint8_t disable_npa;
	uint8_t optimized;
	uint8_t ena_dfb;
	uint16_t ring_id;
	uint32_t aura;
	uint64_t tck_nsec;
	uint64_t max_tout;
	uint64_t nb_chunks;
	uint64_t chunk_sz;
	uint64_t tenns_clk_freq;
	enum otx2_tim_clk_src clk_src;
} __rte_cache_aligned;

static inline struct otx2_tim_evdev *
tim_priv_get(void)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(RTE_STR(OTX2_TIM_EVDEV_NAME));
	if (mz == NULL)
		return NULL;

	return mz->addr;
}

int otx2_tim_caps_get(const struct rte_eventdev *dev, uint64_t flags,
		      uint32_t *caps,
		      const struct rte_event_timer_adapter_ops **ops);

void otx2_tim_init(struct rte_pci_device *pci_dev, struct otx2_dev *cmn_dev);
void otx2_tim_fini(void);

#endif /* __OTX2_TIM_EVDEV_H__ */
