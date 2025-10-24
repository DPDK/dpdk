/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell
 */

#ifndef RTE_PMU_H
#define RTE_PMU_H

/**
 * @file
 *
 * Performance Monitoring Unit (PMU) event tracing operations
 *
 * This file defines generic API and types necessary to setup PMU
 * and read selected counters in runtime.
 * Exported functions are generally not MT-safe.
 * One exception is rte_pmu_read() which can be called concurrently
 * once everything has been setup.
 *
 * In order to initialize the library,
 * following sequence of calls performed by the same EAL thread is required:
 *
 * rte_pmu_init()
 * rte_pmu_add_event()
 *
 * Afterwards all threads can read events by calling rte_pmu_read().
 */

#include <linux/perf_event.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_compat.h>
#include <rte_debug.h>
#include <rte_lcore.h>

#define RTE_PMU_SUPPORTED
#if defined(RTE_ARCH_ARM64)
#include "rte_pmu_pmc_arm64.h"
#elif defined(RTE_ARCH_X86_64)
#include "rte_pmu_pmc_x86_64.h"
#else
#undef RTE_PMU_SUPPORTED
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of events in a group. */
#define RTE_MAX_NUM_GROUP_EVENTS 8

/**
 * A structure describing a group of events.
 */
struct __rte_cache_aligned rte_pmu_event_group {
	/** array of user pages */
	struct perf_event_mmap_page *mmap_pages[RTE_MAX_NUM_GROUP_EVENTS];
	int fds[RTE_MAX_NUM_GROUP_EVENTS]; /**< array of event descriptors */
	TAILQ_ENTRY(rte_pmu_event_group) next; /**< list entry */
	bool enabled; /**< true if group was enabled on particular lcore */
};

/**
 * A PMU state container.
 */
struct rte_pmu {
	struct rte_pmu_event_group event_groups[RTE_MAX_LCORE]; /**< event groups */
	unsigned int num_group_events; /**< number of events in a group */
	unsigned int initialized; /**< initialization counter */
	char *name; /**< name of core PMU listed under /sys/bus/event_source/devices */
	TAILQ_HEAD(, rte_pmu_event) event_list; /**< list of matching events */
};

/** PMU state container. */
extern struct rte_pmu rte_pmu;

/* Each architecture supporting PMU needs to provide its own version. */
#ifndef rte_pmu_pmc_read
#define rte_pmu_pmc_read(index) ({ RTE_SET_USED(index); 0; })
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Read PMU counter.
 *
 * @warning This should not be called directly.
 *
 * @param pc
 *   Pointer to the mmapped user page.
 * @return
 *   Counter value read from hardware.
 */
__rte_experimental
static __rte_always_inline uint64_t
__rte_pmu_read_userpage(struct perf_event_mmap_page *pc)
{
#define __RTE_PMU_READ_ONCE(x) (*(const volatile typeof(x) *)&(x))
	uint64_t width, offset;
	uint32_t seq, index;
	int64_t pmc;

	for (;;) {
		seq = __RTE_PMU_READ_ONCE(pc->lock);
		rte_compiler_barrier();
		index = __RTE_PMU_READ_ONCE(pc->index);
		offset = __RTE_PMU_READ_ONCE(pc->offset);
		width = __RTE_PMU_READ_ONCE(pc->pmc_width);

		/* index set to 0 means that particular counter cannot be used */
		if (likely(pc->cap_user_rdpmc && index)) {
			pmc = rte_pmu_pmc_read(index - 1);
			pmc <<= 64 - width;
			pmc >>= 64 - width;
			offset += pmc;
		}

		rte_compiler_barrier();

		if (likely(__RTE_PMU_READ_ONCE(pc->lock) == seq))
			return offset;
	}

	return 0;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enable group of events on the calling lcore.
 *
 * @warning This should not be called directly.
 *
 * @param group
 *   Pointer to the group which will be enabled.
 * @return
 *   0 in case of success, negative value otherwise.
 */
__rte_experimental
int
__rte_pmu_enable_group(struct rte_pmu_event_group *group);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Initialize PMU library.
 *
 * @return
 *   0 in case of success, negative value otherwise.
 */
__rte_experimental
int
rte_pmu_init(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Finalize PMU library.
 */
__rte_experimental
void
rte_pmu_fini(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Add event to the group of enabled events.
 *
 * @param name
 *   Name of an event listed under /sys/bus/event_source/devices/pmu/events,
 *   where PMU is a placeholder for an event source.
 * @return
 *   Event index in case of success, negative value otherwise.
 */
__rte_experimental
int
rte_pmu_add_event(const char *name);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Read hardware counter configured to count occurrences of an event.
 *
 * This is called by an lcore (EAL thread) bound exclusively to particular CPU
 * and may not work as expected if gets migrated elsewhere.
 * Reason being event group is pinned
 * hence not supposed to be multiplexed with any other events.
 * This is the only API which can be called concurrently by different lcores.
 *
 * @param index
 *   Index of an event to be read.
 * @return
 *   Event value read from register.
 *   In case of errors or lack of support, 0 is returned.
 *   In other words, stream of zeros in a trace file
 *   indicates problem with reading particular PMU event register.
 */
__rte_experimental
static __rte_always_inline uint64_t
rte_pmu_read(unsigned int index)
{
#ifdef ALLOW_EXPERIMENTAL_API
	unsigned int lcore_id = rte_lcore_id();
	struct rte_pmu_event_group *group;

	if (unlikely(!rte_pmu.initialized))
		return 0;

	/* non-EAL threads are not supported */
	if (unlikely(lcore_id >= RTE_MAX_LCORE))
		return 0;

	if (unlikely(index >= rte_pmu.num_group_events))
		return 0;

	group = &rte_pmu.event_groups[lcore_id];
	if (unlikely(!group->enabled)) {
		if (__rte_pmu_enable_group(group))
			return 0;
	}

	return __rte_pmu_read_userpage(group->mmap_pages[index]);
#else
	RTE_SET_USED(index);
	RTE_VERIFY(false);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_PMU_H */
