/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TIM_EVDEV_H__
#define __OTX2_TIM_EVDEV_H__

#include <rte_event_timer_adapter.h>
#include <rte_event_timer_adapter_pmd.h>
#include <rte_reciprocal.h>

#include "otx2_dev.h"

#define OTX2_TIM_EVDEV_NAME otx2_tim_eventdev

#define otx2_tim_func_trace otx2_tim_dbg

#define TIM_LF_RING_AURA		(0x0)
#define TIM_LF_RING_BASE		(0x130)
#define TIM_LF_NRSPERR_INT		(0x200)
#define TIM_LF_NRSPERR_INT_W1S		(0x208)
#define TIM_LF_NRSPERR_INT_ENA_W1S	(0x210)
#define TIM_LF_NRSPERR_INT_ENA_W1C	(0x218)
#define TIM_LF_RAS_INT			(0x300)
#define TIM_LF_RAS_INT_W1S		(0x308)
#define TIM_LF_RAS_INT_ENA_W1S		(0x310)
#define TIM_LF_RAS_INT_ENA_W1C		(0x318)
#define TIM_LF_RING_REL			(0x400)

#define TIM_BUCKET_W1_S_CHUNK_REMAINDER	(48)
#define TIM_BUCKET_W1_M_CHUNK_REMAINDER	((1ULL << (64 - \
					 TIM_BUCKET_W1_S_CHUNK_REMAINDER)) - 1)
#define TIM_BUCKET_W1_S_LOCK		(40)
#define TIM_BUCKET_W1_M_LOCK		((1ULL <<	\
					 (TIM_BUCKET_W1_S_CHUNK_REMAINDER - \
					  TIM_BUCKET_W1_S_LOCK)) - 1)
#define TIM_BUCKET_W1_S_RSVD		(35)
#define TIM_BUCKET_W1_S_BSK		(34)
#define TIM_BUCKET_W1_M_BSK		((1ULL <<	\
					 (TIM_BUCKET_W1_S_RSVD -	    \
					  TIM_BUCKET_W1_S_BSK)) - 1)
#define TIM_BUCKET_W1_S_HBT		(33)
#define TIM_BUCKET_W1_M_HBT		((1ULL <<	\
					 (TIM_BUCKET_W1_S_BSK -		    \
					  TIM_BUCKET_W1_S_HBT)) - 1)
#define TIM_BUCKET_W1_S_SBT		(32)
#define TIM_BUCKET_W1_M_SBT		((1ULL <<	\
					 (TIM_BUCKET_W1_S_HBT -		    \
					  TIM_BUCKET_W1_S_SBT)) - 1)
#define TIM_BUCKET_W1_S_NUM_ENTRIES	(0)
#define TIM_BUCKET_W1_M_NUM_ENTRIES	((1ULL <<	\
					 (TIM_BUCKET_W1_S_SBT -		    \
					  TIM_BUCKET_W1_S_NUM_ENTRIES)) - 1)

#define TIM_BUCKET_SEMA			(TIM_BUCKET_CHUNK_REMAIN)

#define TIM_BUCKET_CHUNK_REMAIN \
	(TIM_BUCKET_W1_M_CHUNK_REMAINDER << TIM_BUCKET_W1_S_CHUNK_REMAINDER)

#define TIM_BUCKET_LOCK \
	(TIM_BUCKET_W1_M_LOCK << TIM_BUCKET_W1_S_LOCK)

#define TIM_BUCKET_SEMA_WLOCK \
	(TIM_BUCKET_CHUNK_REMAIN | (1ull << TIM_BUCKET_W1_S_LOCK))

#define OTX2_MAX_TIM_RINGS		(256)
#define OTX2_TIM_MAX_BUCKETS		(0xFFFFF)
#define OTX2_TIM_RING_DEF_CHUNK_SZ	(4096)
#define OTX2_TIM_CHUNK_ALIGNMENT	(16)
#define OTX2_TIM_MAX_BURST		(RTE_CACHE_LINE_SIZE / \
						OTX2_TIM_CHUNK_ALIGNMENT)
#define OTX2_TIM_NB_CHUNK_SLOTS(sz)	(((sz) / OTX2_TIM_CHUNK_ALIGNMENT) - 1)
#define OTX2_TIM_MIN_CHUNK_SLOTS	(0x1)
#define OTX2_TIM_MAX_CHUNK_SLOTS	(0x1FFE)
#define OTX2_TIM_MIN_TMO_TKS		(256)

#define OTX2_TIM_SP             0x1
#define OTX2_TIM_MP             0x2
#define OTX2_TIM_BKT_AND        0x4
#define OTX2_TIM_BKT_MOD        0x8
#define OTX2_TIM_ENA_FB         0x10
#define OTX2_TIM_ENA_DFB        0x20
#define OTX2_TIM_ENA_STATS      0x40

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

struct otx2_tim_ent {
	uint64_t w0;
	uint64_t wqe;
} __rte_packed;

struct otx2_tim_ctl {
	uint16_t ring;
	uint16_t chunk_slots;
	uint16_t disable_npa;
	uint16_t enable_stats;
};

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
	uint16_t min_ring_cnt;
	uint8_t enable_stats;
	uint16_t ring_ctl_cnt;
	struct otx2_tim_ctl *ring_ctl_data;
	/* HW const */
	/* MSIX offsets */
	uint16_t tim_msixoff[OTX2_MAX_TIM_RINGS];
};

struct otx2_tim_ring {
	uintptr_t base;
	uint16_t nb_chunk_slots;
	uint32_t nb_bkts;
	uint64_t last_updt_cyc;
	uint64_t ring_start_cyc;
	uint64_t tck_int;
	uint64_t tot_int;
	struct otx2_tim_bkt *bkt;
	struct rte_mempool *chunk_pool;
	struct rte_reciprocal_u64 fast_div;
	uint64_t arm_cnt;
	uint8_t prod_type_sp;
	uint8_t enable_stats;
	uint8_t disable_npa;
	uint8_t optimized;
	uint8_t ena_dfb;
	uint16_t ring_id;
	uint32_t aura;
	uint64_t nb_timers;
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

#define TIM_ARM_FASTPATH_MODES						     \
FP(mod_sp,    0, 0, 0, 0, OTX2_TIM_BKT_MOD | OTX2_TIM_ENA_DFB | OTX2_TIM_SP) \
FP(mod_mp,    0, 0, 0, 1, OTX2_TIM_BKT_MOD | OTX2_TIM_ENA_DFB | OTX2_TIM_MP) \
FP(mod_fb_sp, 0, 0, 1, 0, OTX2_TIM_BKT_MOD | OTX2_TIM_ENA_FB  | OTX2_TIM_SP) \
FP(mod_fb_mp, 0, 0, 1, 1, OTX2_TIM_BKT_MOD | OTX2_TIM_ENA_FB  | OTX2_TIM_MP) \
FP(and_sp,    0, 1, 0, 0, OTX2_TIM_BKT_AND | OTX2_TIM_ENA_DFB | OTX2_TIM_SP) \
FP(and_mp,    0, 1, 0, 1, OTX2_TIM_BKT_AND | OTX2_TIM_ENA_DFB | OTX2_TIM_MP) \
FP(and_fb_sp, 0, 1, 1, 0, OTX2_TIM_BKT_AND | OTX2_TIM_ENA_FB  | OTX2_TIM_SP) \
FP(and_fb_mp, 0, 1, 1, 1, OTX2_TIM_BKT_AND | OTX2_TIM_ENA_FB  | OTX2_TIM_MP) \
FP(stats_mod_sp,    1, 0, 0, 0, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_MOD |	     \
	OTX2_TIM_ENA_DFB | OTX2_TIM_SP)					     \
FP(stats_mod_mp,    1, 0, 0, 1, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_MOD |	     \
	OTX2_TIM_ENA_DFB | OTX2_TIM_MP)					     \
FP(stats_mod_fb_sp, 1, 0, 1, 0, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_MOD |	     \
	OTX2_TIM_ENA_FB  | OTX2_TIM_SP)					     \
FP(stats_mod_fb_mp, 1, 0, 1, 1, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_MOD |	     \
	OTX2_TIM_ENA_FB  | OTX2_TIM_MP)					     \
FP(stats_and_sp,    1, 1, 0, 0, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_AND |	     \
	OTX2_TIM_ENA_DFB | OTX2_TIM_SP)					     \
FP(stats_and_mp,    1, 1, 0, 1, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_AND |	     \
	OTX2_TIM_ENA_DFB | OTX2_TIM_MP)					     \
FP(stats_and_fb_sp, 1, 1, 1, 0, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_AND |	     \
	OTX2_TIM_ENA_FB  | OTX2_TIM_SP)					     \
FP(stats_and_fb_mp, 1, 1, 1, 1, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_AND |	     \
	OTX2_TIM_ENA_FB  | OTX2_TIM_MP)

#define TIM_ARM_TMO_FASTPATH_MODES					\
FP(mod,		 0, 0, 0, OTX2_TIM_BKT_MOD | OTX2_TIM_ENA_DFB)		\
FP(mod_fb,	 0, 0, 1, OTX2_TIM_BKT_MOD | OTX2_TIM_ENA_FB)		\
FP(and,		 0, 1, 0, OTX2_TIM_BKT_AND | OTX2_TIM_ENA_DFB)		\
FP(and_fb,	 0, 1, 1, OTX2_TIM_BKT_AND | OTX2_TIM_ENA_FB)		\
FP(stats_mod,	 1, 0, 0, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_MOD |	\
	OTX2_TIM_ENA_DFB)						\
FP(stats_mod_fb, 1, 0, 1, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_MOD |	\
	OTX2_TIM_ENA_FB)						\
FP(stats_and,	 1, 1, 0, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_AND |	\
	OTX2_TIM_ENA_DFB)						\
FP(stats_and_fb, 1, 1, 1, OTX2_TIM_ENA_STATS | OTX2_TIM_BKT_AND |	\
	OTX2_TIM_ENA_FB)

#define FP(_name, _f4, _f3, _f2, _f1, flags)				   \
uint16_t								   \
otx2_tim_arm_burst_ ## _name(const struct rte_event_timer_adapter *adptr,  \
			     struct rte_event_timer **tim,		   \
			     const uint16_t nb_timers);
TIM_ARM_FASTPATH_MODES
#undef FP

#define FP(_name, _f3, _f2, _f1, flags)					\
uint16_t								\
otx2_tim_arm_tmo_tick_burst_ ## _name(					\
		const struct rte_event_timer_adapter *adptr,		\
		struct rte_event_timer **tim,				\
		const uint64_t timeout_tick, const uint16_t nb_timers);
TIM_ARM_TMO_FASTPATH_MODES
#undef FP

uint16_t otx2_tim_timer_cancel_burst(
		const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers);

int otx2_tim_caps_get(const struct rte_eventdev *dev, uint64_t flags,
		      uint32_t *caps,
		      const struct rte_event_timer_adapter_ops **ops);

void otx2_tim_init(struct rte_pci_device *pci_dev, struct otx2_dev *cmn_dev);
void otx2_tim_fini(void);

/* TIM IRQ */
int tim_register_irq(uint16_t ring_id);
void tim_unregister_irq(uint16_t ring_id);

#endif /* __OTX2_TIM_EVDEV_H__ */
