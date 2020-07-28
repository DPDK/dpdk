/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef __TIMVF_EVDEV_H__
#define __TIMVF_EVDEV_H__

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_eventdev.h>
#include <rte_event_timer_adapter.h>
#include <rte_event_timer_adapter_pmd.h>
#include <rte_io.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_prefetch.h>
#include <rte_reciprocal.h>

#include <octeontx_mbox.h>
#include <octeontx_fpavf.h>

#define timvf_log(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, otx_logtype_timvf, \
			"[%s] %s() " fmt "\n", \
			RTE_STR(event_timer_octeontx), __func__, ## args)

#define timvf_log_info(fmt, ...) timvf_log(INFO, fmt, ##__VA_ARGS__)
#define timvf_log_dbg(fmt, ...) timvf_log(DEBUG, fmt, ##__VA_ARGS__)
#define timvf_log_err(fmt, ...) timvf_log(ERR, fmt, ##__VA_ARGS__)
#define timvf_func_trace timvf_log_dbg

#define TIM_COPROC				(8)
#define TIM_GET_DEV_INFO			(1)
#define TIM_GET_RING_INFO			(2)
#define TIM_SET_RING_INFO			(3)
#define TIM_RING_START_CYC_GET			(4)

#define TIM_MAX_RINGS				(64)
#define TIM_DEV_PER_NODE			(1)
#define TIM_VF_PER_DEV				(64)
#define TIM_RING_PER_DEV			(TIM_VF_PER_DEV)
#define TIM_RING_NODE_SHIFT			(6)
#define TIM_RING_MASK				((TIM_RING_PER_DEV) - 1)
#define TIM_RING_INVALID			(-1)

#define TIM_MIN_INTERVAL			(1E3)
#define TIM_MAX_INTERVAL			((1ull << 32) - 1)
#define TIM_MAX_BUCKETS				(1ull << 20)
#define TIM_CHUNK_SIZE				(4096)
#define TIM_MAX_CHUNKS_PER_BUCKET		(1ull << 32)

#define TIMVF_MAX_BURST				(8)

/* TIM VF Control/Status registers (CSRs): */
/* VF_BAR0: */
#define TIM_VF_NRSPERR_INT			(0x0)
#define TIM_VF_NRSPERR_INT_W1S			(0x8)
#define TIM_VF_NRSPERR_ENA_W1C			(0x10)
#define TIM_VF_NRSPERR_ENA_W1S			(0x18)
#define TIM_VRING_FR_RN_CYCLES			(0x20)
#define TIM_VRING_FR_RN_GPIOS			(0x28)
#define TIM_VRING_FR_RN_GTI			(0x30)
#define TIM_VRING_FR_RN_PTP			(0x38)
#define TIM_VRING_CTL0				(0x40)
#define TIM_VRING_CTL1				(0x50)
#define TIM_VRING_CTL2				(0x60)
#define TIM_VRING_BASE				(0x100)
#define TIM_VRING_AURA				(0x108)
#define TIM_VRING_REL				(0x110)

#define TIM_CTL1_W0_S_BUCKET			20
#define TIM_CTL1_W0_M_BUCKET			((1ull << (40 - 20)) - 1)

#define TIM_BUCKET_W1_S_NUM_ENTRIES		(0) /*Shift*/
#define TIM_BUCKET_W1_M_NUM_ENTRIES		((1ull << (32 - 0)) - 1)
#define TIM_BUCKET_W1_S_SBT			(32)
#define TIM_BUCKET_W1_M_SBT			((1ull << (33 - 32)) - 1)
#define TIM_BUCKET_W1_S_HBT			(33)
#define TIM_BUCKET_W1_M_HBT			((1ull << (34 - 33)) - 1)
#define TIM_BUCKET_W1_S_BSK			(34)
#define TIM_BUCKET_W1_M_BSK			((1ull << (35 - 34)) - 1)
#define TIM_BUCKET_W1_S_LOCK			(40)
#define TIM_BUCKET_W1_M_LOCK			((1ull << (48 - 40)) - 1)
#define TIM_BUCKET_W1_S_CHUNK_REMAINDER		(48)
#define TIM_BUCKET_W1_M_CHUNK_REMAINDER		((1ull << (64 - 48)) - 1)

#define TIM_BUCKET_SEMA	\
	(TIM_BUCKET_CHUNK_REMAIN)

#define TIM_BUCKET_CHUNK_REMAIN \
	(TIM_BUCKET_W1_M_CHUNK_REMAINDER << TIM_BUCKET_W1_S_CHUNK_REMAINDER)

#define TIM_BUCKET_LOCK \
	(TIM_BUCKET_W1_M_LOCK << TIM_BUCKET_W1_S_LOCK)

#define TIM_BUCKET_SEMA_WLOCK \
	(TIM_BUCKET_CHUNK_REMAIN | (1ull << TIM_BUCKET_W1_S_LOCK))

#define NSEC_PER_SEC 1E9
#define NSEC2CLK(__ns, __freq) (((__ns) * (__freq)) / NSEC_PER_SEC)
#define CLK2NSEC(__clk, __freq) (((__clk) * NSEC_PER_SEC) / (__freq))

#define timvf_read64 rte_read64_relaxed
#define timvf_write64 rte_write64_relaxed

#define TIMVF_ENABLE_STATS_ARG               ("timvf_stats")

extern int otx_logtype_timvf;
static const uint16_t nb_chunk_slots = (TIM_CHUNK_SIZE / 16) - 1;

enum timvf_clk_src {
	TIM_CLK_SRC_SCLK = RTE_EVENT_TIMER_ADAPTER_CPU_CLK,
	TIM_CLK_SRC_GPIO = RTE_EVENT_TIMER_ADAPTER_EXT_CLK0,
	TIM_CLK_SRC_GTI = RTE_EVENT_TIMER_ADAPTER_EXT_CLK1,
	TIM_CLK_SRC_PTP = RTE_EVENT_TIMER_ADAPTER_EXT_CLK2,
};

/* TIM_MEM_BUCKET */
struct tim_mem_bucket {
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
} __rte_packed __rte_aligned(8);

struct tim_mem_entry {
	uint64_t w0;
	uint64_t wqe;
} __rte_packed;

struct timvf_ctrl_reg {
	uint64_t rctrl0;
	uint64_t rctrl1;
	uint64_t rctrl2;
	uint8_t use_pmu;
} __rte_packed;

struct timvf_ring;

typedef uint32_t (*bkt_id)(const uint32_t bkt_tcks, const uint32_t nb_bkts);
typedef struct tim_mem_entry * (*refill_chunk)(
		struct tim_mem_bucket * const bkt,
		struct timvf_ring * const timr);

struct timvf_ring {
	bkt_id get_target_bkt;
	refill_chunk refill_chunk;
	struct rte_reciprocal_u64 fast_div;
	uint64_t ring_start_cyc;
	uint32_t nb_bkts;
	struct tim_mem_bucket *bkt;
	void *chunk_pool;
	uint64_t tck_int;
	volatile uint64_t tim_arm_cnt;
	uint64_t tck_nsec;
	void  *vbar0;
	void *bkt_pos;
	uint64_t max_tout;
	uint64_t nb_chunks;
	uint64_t nb_timers;
	enum timvf_clk_src clk_src;
	uint16_t tim_ring_id;
} __rte_cache_aligned;

static __rte_always_inline uint32_t
bkt_mod(const uint32_t rel_bkt, const uint32_t nb_bkts)
{
	return rel_bkt % nb_bkts;
}

static __rte_always_inline uint32_t
bkt_and(uint32_t rel_bkt, uint32_t nb_bkts)
{
	return rel_bkt & (nb_bkts - 1);
}

uint8_t timvf_get_ring(void);
void timvf_release_ring(uint8_t vfid);
void *timvf_bar(uint8_t id, uint8_t bar);
int timvf_timer_adapter_caps_get(const struct rte_eventdev *dev, uint64_t flags,
		uint32_t *caps, const struct rte_event_timer_adapter_ops **ops,
		uint8_t enable_stats);
uint16_t timvf_timer_cancel_burst(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers);
uint16_t timvf_timer_arm_burst_sp(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers);
uint16_t timvf_timer_arm_burst_sp_stats(
		const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers);
uint16_t timvf_timer_arm_burst_mp(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers);
uint16_t timvf_timer_arm_burst_mp_stats(
		const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers);
uint16_t timvf_timer_arm_tmo_brst(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint64_t timeout_tick,
		const uint16_t nb_timers);
uint16_t timvf_timer_arm_tmo_brst_stats(
		const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint64_t timeout_tick,
		const uint16_t nb_timers);
void timvf_set_chunk_refill(struct timvf_ring * const timr, uint8_t use_fpa);
void timvf_set_eventdevice(struct rte_eventdev *dev);

#endif /* __TIMVF_EVDEV_H__ */
