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
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_reciprocal.h>

#include "hw/tim.h"

#include "roc_model.h"
#include "roc_tim.h"

#define NSECPERSEC		 1E9
#define USECPERSEC		 1E6
#define TICK2NSEC(__tck, __freq) (((__tck)*NSECPERSEC) / (__freq))

#define CNXK_TIM_EVDEV_NAME	    cnxk_tim_eventdev
#define CNXK_TIM_MAX_BUCKETS	    (0xFFFFF)
#define CNXK_TIM_RING_DEF_CHUNK_SZ  (1024)
#define CNXK_TIM_CHUNK_ALIGNMENT    (16)
#define CNXK_TIM_MAX_BURST	    (16)
#define CNXK_TIM_NB_CHUNK_SLOTS(sz) (((sz) / CNXK_TIM_CHUNK_ALIGNMENT) - 1)
#define CNXK_TIM_MIN_CHUNK_SLOTS    (0x1)
#define CNXK_TIM_MAX_CHUNK_SLOTS    (0x1FFE)
#define CNXK_TIM_MAX_POOL_CACHE_SZ  (16)

#define CN9K_TIM_MIN_TMO_TKS (256)

#define CNXK_TIM_DISABLE_NPA "tim_disable_npa"
#define CNXK_TIM_CHNK_SLOTS  "tim_chnk_slots"
#define CNXK_TIM_STATS_ENA   "tim_stats_ena"
#define CNXK_TIM_RINGS_LMT   "tim_rings_lmt"
#define CNXK_TIM_RING_CTL    "tim_ring_ctl"
#define CNXK_TIM_EXT_CLK     "tim_eclk_freq"

#define CNXK_TIM_SP	   0x1
#define CNXK_TIM_MP	   0x2
#define CNXK_TIM_ENA_FB	   0x10
#define CNXK_TIM_ENA_DFB   0x20
#define CNXK_TIM_ENA_STATS 0x40

#define TIM_BUCKET_W1_S_CHUNK_REMAINDER (48)
#define TIM_BUCKET_W1_M_CHUNK_REMAINDER                                        \
	((1ULL << (64 - TIM_BUCKET_W1_S_CHUNK_REMAINDER)) - 1)
#define TIM_BUCKET_W1_S_LOCK (40)
#define TIM_BUCKET_W1_M_LOCK                                                   \
	((1ULL << (TIM_BUCKET_W1_S_CHUNK_REMAINDER - TIM_BUCKET_W1_S_LOCK)) - 1)
#define TIM_BUCKET_W1_S_RSVD (35)
#define TIM_BUCKET_W1_S_BSK  (34)
#define TIM_BUCKET_W1_M_BSK                                                    \
	((1ULL << (TIM_BUCKET_W1_S_RSVD - TIM_BUCKET_W1_S_BSK)) - 1)
#define TIM_BUCKET_W1_S_HBT (33)
#define TIM_BUCKET_W1_M_HBT                                                    \
	((1ULL << (TIM_BUCKET_W1_S_BSK - TIM_BUCKET_W1_S_HBT)) - 1)
#define TIM_BUCKET_W1_S_SBT (32)
#define TIM_BUCKET_W1_M_SBT                                                    \
	((1ULL << (TIM_BUCKET_W1_S_HBT - TIM_BUCKET_W1_S_SBT)) - 1)
#define TIM_BUCKET_W1_S_NUM_ENTRIES (0)
#define TIM_BUCKET_W1_M_NUM_ENTRIES                                            \
	((1ULL << (TIM_BUCKET_W1_S_SBT - TIM_BUCKET_W1_S_NUM_ENTRIES)) - 1)

#define TIM_BUCKET_SEMA (TIM_BUCKET_CHUNK_REMAIN)

#define TIM_BUCKET_CHUNK_REMAIN                                                \
	(TIM_BUCKET_W1_M_CHUNK_REMAINDER << TIM_BUCKET_W1_S_CHUNK_REMAINDER)

#define TIM_BUCKET_LOCK (TIM_BUCKET_W1_M_LOCK << TIM_BUCKET_W1_S_LOCK)

#define TIM_BUCKET_SEMA_WLOCK                                                  \
	(TIM_BUCKET_CHUNK_REMAIN | (1ull << TIM_BUCKET_W1_S_LOCK))

typedef void (*cnxk_sso_set_priv_mem_t)(const struct rte_eventdev *event_dev,
					void *lookup_mem);

struct cnxk_tim_ctl {
	uint16_t ring;
	uint16_t chunk_slots;
	uint16_t disable_npa;
	uint16_t enable_stats;
};

struct cnxk_tim_evdev {
	struct roc_tim tim;
	struct rte_eventdev *event_dev;
	uint16_t nb_rings;
	uint32_t chunk_sz;
	/* Dev args */
	uint8_t disable_npa;
	uint32_t chunk_slots;
	uint32_t min_ring_cnt;
	uint8_t enable_stats;
	uint16_t ring_ctl_cnt;
	uint64_t ext_clk_freq[ROC_TIM_CLK_SRC_INVALID];
	struct cnxk_tim_ctl *ring_ctl_data;
};

struct cnxk_tim_bkt {
	uint64_t first_chunk;
	union {
		uint64_t w1;
		struct {
			uint32_t nb_entry;
			uint8_t sbt : 1;
			uint8_t hbt : 1;
			uint8_t bsk : 1;
			uint8_t rsvd : 5;
			uint8_t lock;
			int16_t chunk_remainder;
		};
	};
	uint64_t current_chunk;
	uint64_t pad;
};

struct cnxk_tim_ring {
	uint16_t nb_chunk_slots;
	uint32_t nb_bkts;
	uintptr_t tbase;
	uint64_t (*tick_fn)(uint64_t tbase);
	uint64_t ring_start_cyc;
	struct cnxk_tim_bkt *bkt;
	struct rte_mempool *chunk_pool;
	struct rte_reciprocal_u64 fast_div;
	struct rte_reciprocal_u64 fast_bkt;
	uint64_t tck_int;
	uint64_t arm_cnt;
	uintptr_t base;
	uint8_t prod_type_sp;
	uint8_t enable_stats;
	uint8_t disable_npa;
	uint8_t ena_dfb;
	uint8_t ena_periodic;
	uint16_t ring_id;
	uint32_t aura;
	uint64_t nb_timers;
	uint64_t tck_nsec;
	uint64_t max_tout;
	uint64_t nb_chunks;
	uint64_t chunk_sz;
	enum roc_tim_clk_src clk_src;
} __rte_cache_aligned;

struct cnxk_tim_ent {
	uint64_t w0;
	uint64_t wqe;
};

static inline struct cnxk_tim_evdev *
cnxk_tim_priv_get(void)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(RTE_STR(CNXK_TIM_EVDEV_NAME));
	if (mz == NULL)
		return NULL;

	return mz->addr;
}

static inline double
cnxk_tim_ns_per_tck(uint64_t freq)
{
	return (double)NSECPERSEC / freq;
}

#ifdef RTE_ARCH_ARM64
static inline uint64_t
cnxk_tim_cntvct(uint64_t base __rte_unused)
{
	uint64_t tsc;

	asm volatile("mrs %0, CNTVCT_EL0" : "=r"(tsc)::"memory");
	return tsc;
}

static inline uint64_t
cnxk_tim_cntfrq(void)
{
	uint64_t freq;

	asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	return freq;
}
#else
static inline uint64_t
cnxk_tim_cntvct(uint64_t base __rte_unused)
{
	return 0;
}

static inline uint64_t
cnxk_tim_cntfrq(void)
{
	return 0;
}
#endif

static inline uint64_t
cnxk_tim_tick_read(uint64_t tick_base)
{
	return plt_read64(tick_base);
}

static inline enum roc_tim_clk_src
cnxk_tim_convert_clk_src(enum rte_event_timer_adapter_clk_src clk_src)
{
	switch (clk_src) {
	case RTE_EVENT_TIMER_ADAPTER_CPU_CLK:
		return ROC_TIM_CLK_SRC_GTI;
	case RTE_EVENT_TIMER_ADAPTER_EXT_CLK0:
		return ROC_TIM_CLK_SRC_10NS;
	case RTE_EVENT_TIMER_ADAPTER_EXT_CLK1:
		return ROC_TIM_CLK_SRC_GPIO;
	case RTE_EVENT_TIMER_ADAPTER_EXT_CLK2:
		return ROC_TIM_CLK_SRC_PTP;
	case RTE_EVENT_TIMER_ADAPTER_EXT_CLK3:
		return roc_model_constant_is_cn9k() ? ROC_TIM_CLK_SRC_INVALID :
						      ROC_TIM_CLK_SRC_SYNCE;
	default:
		return ROC_TIM_CLK_SRC_INVALID;
	}
}

static inline uintptr_t
cnxk_tim_get_tick_base(enum roc_tim_clk_src clk_src, uintptr_t base)
{
	switch (clk_src) {
	case ROC_TIM_CLK_SRC_GTI:
		return base + TIM_LF_FR_RN_GTI;
	case ROC_TIM_CLK_SRC_GPIO:
		return base + TIM_LF_FR_RN_GPIOS;
	case ROC_TIM_CLK_SRC_10NS:
		return base + TIM_LF_FR_RN_TENNS;
	case ROC_TIM_CLK_SRC_PTP:
		return base + TIM_LF_FR_RN_PTP;
	case ROC_TIM_CLK_SRC_SYNCE:
		return base + TIM_LF_FR_RN_SYNCE;
	case ROC_TIM_CLK_SRC_BTS:
		return base + TIM_LF_FR_RN_BTS;
	default:
		return ROC_TIM_CLK_SRC_INVALID;
	}
}

static inline int
cnxk_tim_get_clk_freq(struct cnxk_tim_evdev *dev, enum roc_tim_clk_src clk_src,
		      uint64_t *freq)
{
	if (freq == NULL)
		return -EINVAL;

	PLT_SET_USED(dev);
	switch (clk_src) {
	case ROC_TIM_CLK_SRC_GTI:
		*freq = cnxk_tim_cntfrq();
		break;
	case ROC_TIM_CLK_SRC_10NS:
		*freq = 1E8;
		break;
	case ROC_TIM_CLK_SRC_GPIO:
	case ROC_TIM_CLK_SRC_PTP:
	case ROC_TIM_CLK_SRC_SYNCE:
		*freq = dev->ext_clk_freq[clk_src];
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define TIM_ARM_FASTPATH_MODES                                                 \
	FP(sp, 0, 0, 0, CNXK_TIM_ENA_DFB | CNXK_TIM_SP)                        \
	FP(mp, 0, 0, 1, CNXK_TIM_ENA_DFB | CNXK_TIM_MP)                        \
	FP(fb_sp, 0, 1, 0, CNXK_TIM_ENA_FB | CNXK_TIM_SP)                      \
	FP(fb_mp, 0, 1, 1, CNXK_TIM_ENA_FB | CNXK_TIM_MP)                      \
	FP(stats_sp, 1, 0, 0,                                                  \
	   CNXK_TIM_ENA_STATS | CNXK_TIM_ENA_DFB | CNXK_TIM_SP)                \
	FP(stats_mp, 1, 0, 1,                                                  \
	   CNXK_TIM_ENA_STATS | CNXK_TIM_ENA_DFB | CNXK_TIM_MP)                \
	FP(stats_fb_sp, 1, 1, 0,                                               \
	   CNXK_TIM_ENA_STATS | CNXK_TIM_ENA_FB | CNXK_TIM_SP)                 \
	FP(stats_fb_mp, 1, 1, 1,                                               \
	   CNXK_TIM_ENA_STATS | CNXK_TIM_ENA_FB | CNXK_TIM_MP)

#define TIM_ARM_TMO_FASTPATH_MODES                                             \
	FP(dfb, 0, 0, CNXK_TIM_ENA_DFB)                                        \
	FP(fb, 0, 1, CNXK_TIM_ENA_FB)                                          \
	FP(stats_dfb, 1, 0, CNXK_TIM_ENA_STATS | CNXK_TIM_ENA_DFB)             \
	FP(stats_fb, 1, 1, CNXK_TIM_ENA_STATS | CNXK_TIM_ENA_FB)

#define FP(_name, _f3, _f2, _f1, flags)                                        \
	uint16_t cnxk_tim_arm_burst_##_name(                                   \
		const struct rte_event_timer_adapter *adptr,                   \
		struct rte_event_timer **tim, const uint16_t nb_timers);
TIM_ARM_FASTPATH_MODES
#undef FP

#define FP(_name, _f2, _f1, flags)                                             \
	uint16_t cnxk_tim_arm_tmo_tick_burst_##_name(                          \
		const struct rte_event_timer_adapter *adptr,                   \
		struct rte_event_timer **tim, const uint64_t timeout_tick,     \
		const uint16_t nb_timers);
TIM_ARM_TMO_FASTPATH_MODES
#undef FP

uint16_t
cnxk_tim_timer_cancel_burst(const struct rte_event_timer_adapter *adptr,
			    struct rte_event_timer **tim,
			    const uint16_t nb_timers);

int cnxk_tim_remaining_ticks_get(const struct rte_event_timer_adapter *adapter,
				 const struct rte_event_timer *evtim, uint64_t *ticks_remaining);

int cnxk_tim_caps_get(const struct rte_eventdev *dev, uint64_t flags,
		      uint32_t *caps,
		      const struct event_timer_adapter_ops **ops,
		      cnxk_sso_set_priv_mem_t priv_mem_fn);

void cnxk_tim_init(struct roc_sso *sso);
void cnxk_tim_fini(void);

#endif /* __CNXK_TIM_EVDEV_H__ */
