/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <stdlib.h>

#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_cpuflags.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_power_intrinsics.h>

#include "rte_power_pmd_mgmt.h"
#include "power_common.h"

unsigned int emptypoll_max;
unsigned int pause_duration;
unsigned int scale_freq_min[RTE_MAX_LCORE];
unsigned int scale_freq_max[RTE_MAX_LCORE];

/* store some internal state */
static struct pmd_conf_data {
	/** what do we support? */
	struct rte_cpu_intrinsics intrinsics_support;
	/** pre-calculated tsc diff for 1us */
	uint64_t tsc_per_us;
	/** how many rte_pause can we fit in a microsecond? */
	uint64_t pause_per_us;
} global_data;

/**
 * Possible power management states of an ethdev port.
 */
enum pmd_mgmt_state {
	/** Device power management is disabled. */
	PMD_MGMT_DISABLED = 0,
	/** Device power management is enabled. */
	PMD_MGMT_ENABLED
};

union queue {
	uint32_t val;
	struct {
		uint16_t portid;
		uint16_t qid;
	};
};

struct queue_list_entry {
	TAILQ_ENTRY(queue_list_entry) next;
	union queue queue;
	uint64_t n_empty_polls;
	uint64_t n_sleeps;
	const struct rte_eth_rxtx_callback *cb;
};

struct pmd_core_cfg {
	TAILQ_HEAD(queue_list_head, queue_list_entry) head;
	/**< List of queues associated with this lcore */
	size_t n_queues;
	/**< How many queues are in the list? */
	volatile enum pmd_mgmt_state pwr_mgmt_state;
	/**< State of power management for this queue */
	enum rte_power_pmd_mgmt_type cb_mode;
	/**< Callback mode for this queue */
	uint64_t n_queues_ready_to_sleep;
	/**< Number of queues ready to enter power optimized state */
	uint64_t sleep_target;
	/**< Prevent a queue from triggering sleep multiple times */
} __rte_cache_aligned;
static struct pmd_core_cfg lcore_cfgs[RTE_MAX_LCORE];

static inline bool
queue_equal(const union queue *l, const union queue *r)
{
	return l->val == r->val;
}

static inline void
queue_copy(union queue *dst, const union queue *src)
{
	dst->val = src->val;
}

static struct queue_list_entry *
queue_list_find(const struct pmd_core_cfg *cfg, const union queue *q)
{
	struct queue_list_entry *cur;

	TAILQ_FOREACH(cur, &cfg->head, next) {
		if (queue_equal(&cur->queue, q))
			return cur;
	}
	return NULL;
}

static int
queue_list_add(struct pmd_core_cfg *cfg, const union queue *q)
{
	struct queue_list_entry *qle;

	/* is it already in the list? */
	if (queue_list_find(cfg, q) != NULL)
		return -EEXIST;

	qle = malloc(sizeof(*qle));
	if (qle == NULL)
		return -ENOMEM;
	memset(qle, 0, sizeof(*qle));

	queue_copy(&qle->queue, q);
	TAILQ_INSERT_TAIL(&cfg->head, qle, next);
	cfg->n_queues++;

	return 0;
}

static struct queue_list_entry *
queue_list_take(struct pmd_core_cfg *cfg, const union queue *q)
{
	struct queue_list_entry *found;

	found = queue_list_find(cfg, q);
	if (found == NULL)
		return NULL;

	TAILQ_REMOVE(&cfg->head, found, next);
	cfg->n_queues--;

	/* freeing is responsibility of the caller */
	return found;
}

static inline int
get_monitor_addresses(struct pmd_core_cfg *cfg,
		struct rte_power_monitor_cond *pmc, size_t len)
{
	const struct queue_list_entry *qle;
	size_t i = 0;
	int ret;

	TAILQ_FOREACH(qle, &cfg->head, next) {
		const union queue *q = &qle->queue;
		struct rte_power_monitor_cond *cur;

		/* attempted out of bounds access */
		if (i >= len) {
			RTE_LOG(ERR, POWER, "Too many queues being monitored\n");
			return -1;
		}

		cur = &pmc[i++];
		ret = rte_eth_get_monitor_addr(q->portid, q->qid, cur);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static void
calc_tsc(void)
{
	const uint64_t hz = rte_get_timer_hz();
	const uint64_t tsc_per_us = hz / US_PER_S; /* 1us */

	global_data.tsc_per_us = tsc_per_us;

	/* only do this if we don't have tpause */
	if (!global_data.intrinsics_support.power_pause) {
		const uint64_t start = rte_rdtsc_precise();
		const uint32_t n_pauses = 10000;
		double us, us_per_pause;
		uint64_t end;
		unsigned int i;

		/* estimate number of rte_pause() calls per us*/
		for (i = 0; i < n_pauses; i++)
			rte_pause();

		end = rte_rdtsc_precise();
		us = (end - start) / (double)tsc_per_us;
		us_per_pause = us / n_pauses;

		global_data.pause_per_us = (uint64_t)(1.0 / us_per_pause);
	}
}

static inline void
queue_reset(struct pmd_core_cfg *cfg, struct queue_list_entry *qcfg)
{
	const bool is_ready_to_sleep = qcfg->n_sleeps == cfg->sleep_target;

	/* reset empty poll counter for this queue */
	qcfg->n_empty_polls = 0;
	/* reset the queue sleep counter as well */
	qcfg->n_sleeps = 0;
	/* remove the queue from list of queues ready to sleep */
	if (is_ready_to_sleep)
		cfg->n_queues_ready_to_sleep--;
	/*
	 * no need change the lcore sleep target counter because this lcore will
	 * reach the n_sleeps anyway, and the other cores are already counted so
	 * there's no need to do anything else.
	 */
}

static inline bool
queue_can_sleep(struct pmd_core_cfg *cfg, struct queue_list_entry *qcfg)
{
	/* this function is called - that means we have an empty poll */
	qcfg->n_empty_polls++;

	/* if we haven't reached threshold for empty polls, we can't sleep */
	if (qcfg->n_empty_polls <= emptypoll_max)
		return false;

	/*
	 * we've reached a point where we are able to sleep, but we still need
	 * to check if this queue has already been marked for sleeping.
	 */
	if (qcfg->n_sleeps == cfg->sleep_target)
		return true;

	/* mark this queue as ready for sleep */
	qcfg->n_sleeps = cfg->sleep_target;
	cfg->n_queues_ready_to_sleep++;

	return true;
}

static inline bool
lcore_can_sleep(struct pmd_core_cfg *cfg)
{
	/* are all queues ready to sleep? */
	if (cfg->n_queues_ready_to_sleep != cfg->n_queues)
		return false;

	/* we've reached an iteration where we can sleep, reset sleep counter */
	cfg->n_queues_ready_to_sleep = 0;
	cfg->sleep_target++;
	/*
	 * we do not reset any individual queue empty poll counters, because
	 * we want to keep sleeping on every poll until we actually get traffic.
	 */

	return true;
}

static uint16_t
clb_multiwait(uint16_t port_id __rte_unused, uint16_t qidx __rte_unused,
		struct rte_mbuf **pkts __rte_unused, uint16_t nb_rx,
		uint16_t max_pkts __rte_unused, void *arg)
{
	const unsigned int lcore = rte_lcore_id();
	struct queue_list_entry *queue_conf = arg;
	struct pmd_core_cfg *lcore_conf;
	const bool empty = nb_rx == 0;

	lcore_conf = &lcore_cfgs[lcore];

	/* early exit */
	if (likely(!empty))
		/* early exit */
		queue_reset(lcore_conf, queue_conf);
	else {
		struct rte_power_monitor_cond pmc[lcore_conf->n_queues];
		int ret;

		/* can this queue sleep? */
		if (!queue_can_sleep(lcore_conf, queue_conf))
			return nb_rx;

		/* can this lcore sleep? */
		if (!lcore_can_sleep(lcore_conf))
			return nb_rx;

		/* gather all monitoring conditions */
		ret = get_monitor_addresses(lcore_conf, pmc,
				lcore_conf->n_queues);
		if (ret < 0)
			return nb_rx;

		rte_power_monitor_multi(pmc, lcore_conf->n_queues, UINT64_MAX);
	}

	return nb_rx;
}

static uint16_t
clb_umwait(uint16_t port_id, uint16_t qidx, struct rte_mbuf **pkts __rte_unused,
		uint16_t nb_rx, uint16_t max_pkts __rte_unused, void *arg)
{
	struct queue_list_entry *queue_conf = arg;

	/* this callback can't do more than one queue, omit multiqueue logic */
	if (unlikely(nb_rx == 0)) {
		queue_conf->n_empty_polls++;
		if (unlikely(queue_conf->n_empty_polls > emptypoll_max)) {
			struct rte_power_monitor_cond pmc;
			int ret;

			/* use monitoring condition to sleep */
			ret = rte_eth_get_monitor_addr(port_id, qidx,
					&pmc);
			if (ret == 0)
				rte_power_monitor(&pmc, UINT64_MAX);
		}
	} else
		queue_conf->n_empty_polls = 0;

	return nb_rx;
}

static uint16_t
clb_pause(uint16_t port_id __rte_unused, uint16_t qidx __rte_unused,
		struct rte_mbuf **pkts __rte_unused, uint16_t nb_rx,
		uint16_t max_pkts __rte_unused, void *arg)
{
	const unsigned int lcore = rte_lcore_id();
	struct queue_list_entry *queue_conf = arg;
	struct pmd_core_cfg *lcore_conf;
	const bool empty = nb_rx == 0;
	uint32_t pause_duration = rte_power_pmd_mgmt_get_pause_duration();

	lcore_conf = &lcore_cfgs[lcore];

	if (likely(!empty))
		/* early exit */
		queue_reset(lcore_conf, queue_conf);
	else {
		/* can this queue sleep? */
		if (!queue_can_sleep(lcore_conf, queue_conf))
			return nb_rx;

		/* can this lcore sleep? */
		if (!lcore_can_sleep(lcore_conf))
			return nb_rx;

		/* sleep for 1 microsecond, use tpause if we have it */
		if (global_data.intrinsics_support.power_pause) {
			const uint64_t cur = rte_rdtsc();
			const uint64_t wait_tsc =
					cur + global_data.tsc_per_us * pause_duration;
			rte_power_pause(wait_tsc);
		} else {
			uint64_t i;
			for (i = 0; i < global_data.pause_per_us * pause_duration; i++)
				rte_pause();
		}
	}

	return nb_rx;
}

static uint16_t
clb_scale_freq(uint16_t port_id __rte_unused, uint16_t qidx __rte_unused,
		struct rte_mbuf **pkts __rte_unused, uint16_t nb_rx,
		uint16_t max_pkts __rte_unused, void *arg)
{
	const unsigned int lcore = rte_lcore_id();
	const bool empty = nb_rx == 0;
	struct pmd_core_cfg *lcore_conf = &lcore_cfgs[lcore];
	struct queue_list_entry *queue_conf = arg;

	if (likely(!empty)) {
		/* early exit */
		queue_reset(lcore_conf, queue_conf);

		/* scale up freq immediately */
		rte_power_freq_max(rte_lcore_id());
	} else {
		/* can this queue sleep? */
		if (!queue_can_sleep(lcore_conf, queue_conf))
			return nb_rx;

		/* can this lcore sleep? */
		if (!lcore_can_sleep(lcore_conf))
			return nb_rx;

		rte_power_freq_min(rte_lcore_id());
	}

	return nb_rx;
}

static int
queue_stopped(const uint16_t port_id, const uint16_t queue_id)
{
	struct rte_eth_rxq_info qinfo;

	int ret = rte_eth_rx_queue_info_get(port_id, queue_id, &qinfo);
	if (ret < 0) {
		if (ret == -ENOTSUP)
			return 1;
		else
			return -1;
	}

	return qinfo.queue_state == RTE_ETH_QUEUE_STATE_STOPPED;
}

static int
cfg_queues_stopped(struct pmd_core_cfg *queue_cfg)
{
	const struct queue_list_entry *entry;

	TAILQ_FOREACH(entry, &queue_cfg->head, next) {
		const union queue *q = &entry->queue;
		int ret = queue_stopped(q->portid, q->qid);
		if (ret != 1)
			return ret;
	}
	return 1;
}

static int
check_scale(unsigned int lcore)
{
	enum power_management_env env;

	/* only PSTATE and ACPI modes are supported */
	if (!rte_power_check_env_supported(PM_ENV_ACPI_CPUFREQ) &&
			!rte_power_check_env_supported(PM_ENV_PSTATE_CPUFREQ) &&
			!rte_power_check_env_supported(PM_ENV_AMD_PSTATE_CPUFREQ)) {
		RTE_LOG(DEBUG, POWER, "Neither ACPI nor PSTATE modes are supported\n");
		return -ENOTSUP;
	}
	/* ensure we could initialize the power library */
	if (rte_power_init(lcore))
		return -EINVAL;

	/* ensure we initialized the correct env */
	env = rte_power_get_env();
	if (env != PM_ENV_ACPI_CPUFREQ && env != PM_ENV_PSTATE_CPUFREQ &&
			env != PM_ENV_AMD_PSTATE_CPUFREQ) {
		RTE_LOG(DEBUG, POWER, "Neither ACPI nor PSTATE modes were initialized\n");
		return -ENOTSUP;
	}

	/* we're done */
	return 0;
}

static int
check_monitor(struct pmd_core_cfg *cfg, const union queue *qdata)
{
	struct rte_power_monitor_cond dummy;
	bool multimonitor_supported;

	/* check if rte_power_monitor is supported */
	if (!global_data.intrinsics_support.power_monitor) {
		RTE_LOG(DEBUG, POWER, "Monitoring intrinsics are not supported\n");
		return -ENOTSUP;
	}
	/* check if multi-monitor is supported */
	multimonitor_supported =
			global_data.intrinsics_support.power_monitor_multi;

	/* if we're adding a new queue, do we support multiple queues? */
	if (cfg->n_queues > 0 && !multimonitor_supported) {
		RTE_LOG(DEBUG, POWER, "Monitoring multiple queues is not supported\n");
		return -ENOTSUP;
	}

	/* check if the device supports the necessary PMD API */
	if (rte_eth_get_monitor_addr(qdata->portid, qdata->qid,
			&dummy) == -ENOTSUP) {
		RTE_LOG(DEBUG, POWER, "The device does not support rte_eth_get_monitor_addr\n");
		return -ENOTSUP;
	}

	/* we're done */
	return 0;
}

static inline rte_rx_callback_fn
get_monitor_callback(void)
{
	return global_data.intrinsics_support.power_monitor_multi ?
		clb_multiwait : clb_umwait;
}

int
rte_power_ethdev_pmgmt_queue_enable(unsigned int lcore_id, uint16_t port_id,
		uint16_t queue_id, enum rte_power_pmd_mgmt_type mode)
{
	const union queue qdata = {.portid = port_id, .qid = queue_id};
	struct pmd_core_cfg *lcore_cfg;
	struct queue_list_entry *queue_cfg;
	struct rte_eth_dev_info info;
	rte_rx_callback_fn clb;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	if (queue_id >= RTE_MAX_QUEUES_PER_PORT || lcore_id >= RTE_MAX_LCORE) {
		ret = -EINVAL;
		goto end;
	}

	if (rte_eth_dev_info_get(port_id, &info) < 0) {
		ret = -EINVAL;
		goto end;
	}

	/* check if queue id is valid */
	if (queue_id >= info.nb_rx_queues) {
		ret = -EINVAL;
		goto end;
	}

	/* check if the queue is stopped */
	ret = queue_stopped(port_id, queue_id);
	if (ret != 1) {
		/* error means invalid queue, 0 means queue wasn't stopped */
		ret = ret < 0 ? -EINVAL : -EBUSY;
		goto end;
	}

	lcore_cfg = &lcore_cfgs[lcore_id];

	/* check if other queues are stopped as well */
	ret = cfg_queues_stopped(lcore_cfg);
	if (ret != 1) {
		/* error means invalid queue, 0 means queue wasn't stopped */
		ret = ret < 0 ? -EINVAL : -EBUSY;
		goto end;
	}

	/* if callback was already enabled, check current callback type */
	if (lcore_cfg->pwr_mgmt_state != PMD_MGMT_DISABLED &&
			lcore_cfg->cb_mode != mode) {
		ret = -EINVAL;
		goto end;
	}

	/* we need this in various places */
	rte_cpu_get_intrinsics_support(&global_data.intrinsics_support);

	switch (mode) {
	case RTE_POWER_MGMT_TYPE_MONITOR:
		/* check if we can add a new queue */
		ret = check_monitor(lcore_cfg, &qdata);
		if (ret < 0)
			goto end;

		clb = get_monitor_callback();
		break;
	case RTE_POWER_MGMT_TYPE_SCALE:
		clb = clb_scale_freq;

		/* we only have to check this when enabling first queue */
		if (lcore_cfg->pwr_mgmt_state != PMD_MGMT_DISABLED)
			break;
		/* check if we can add a new queue */
		ret = check_scale(lcore_id);
		if (ret < 0)
			goto end;
		break;
	case RTE_POWER_MGMT_TYPE_PAUSE:
		/* figure out various time-to-tsc conversions */
		if (global_data.tsc_per_us == 0)
			calc_tsc();

		clb = clb_pause;
		break;
	default:
		RTE_LOG(DEBUG, POWER, "Invalid power management type\n");
		ret = -EINVAL;
		goto end;
	}
	/* add this queue to the list */
	ret = queue_list_add(lcore_cfg, &qdata);
	if (ret < 0) {
		RTE_LOG(DEBUG, POWER, "Failed to add queue to list: %s\n",
				strerror(-ret));
		goto end;
	}
	/* new queue is always added last */
	queue_cfg = TAILQ_LAST(&lcore_cfg->head, queue_list_head);

	/* when enabling first queue, ensure sleep target is not 0 */
	if (lcore_cfg->n_queues == 1 && lcore_cfg->sleep_target == 0)
		lcore_cfg->sleep_target = 1;

	/* initialize data before enabling the callback */
	if (lcore_cfg->n_queues == 1) {
		lcore_cfg->cb_mode = mode;
		lcore_cfg->pwr_mgmt_state = PMD_MGMT_ENABLED;
	}
	queue_cfg->cb = rte_eth_add_rx_callback(port_id, queue_id,
			clb, queue_cfg);

	ret = 0;
end:
	return ret;
}

int
rte_power_ethdev_pmgmt_queue_disable(unsigned int lcore_id,
		uint16_t port_id, uint16_t queue_id)
{
	const union queue qdata = {.portid = port_id, .qid = queue_id};
	struct pmd_core_cfg *lcore_cfg;
	struct queue_list_entry *queue_cfg;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	if (lcore_id >= RTE_MAX_LCORE || queue_id >= RTE_MAX_QUEUES_PER_PORT)
		return -EINVAL;

	/* check if the queue is stopped */
	ret = queue_stopped(port_id, queue_id);
	if (ret != 1) {
		/* error means invalid queue, 0 means queue wasn't stopped */
		return ret < 0 ? -EINVAL : -EBUSY;
	}

	/* no need to check queue id as wrong queue id would not be enabled */
	lcore_cfg = &lcore_cfgs[lcore_id];

	/* check if other queues are stopped as well */
	ret = cfg_queues_stopped(lcore_cfg);
	if (ret != 1) {
		/* error means invalid queue, 0 means queue wasn't stopped */
		return ret < 0 ? -EINVAL : -EBUSY;
	}

	if (lcore_cfg->pwr_mgmt_state != PMD_MGMT_ENABLED)
		return -EINVAL;

	/*
	 * There is no good/easy way to do this without race conditions, so we
	 * are just going to throw our hands in the air and hope that the user
	 * has read the documentation and has ensured that ports are stopped at
	 * the time we enter the API functions.
	 */
	queue_cfg = queue_list_take(lcore_cfg, &qdata);
	if (queue_cfg == NULL)
		return -ENOENT;

	/* if we've removed all queues from the lists, set state to disabled */
	if (lcore_cfg->n_queues == 0)
		lcore_cfg->pwr_mgmt_state = PMD_MGMT_DISABLED;

	switch (lcore_cfg->cb_mode) {
	case RTE_POWER_MGMT_TYPE_MONITOR: /* fall-through */
	case RTE_POWER_MGMT_TYPE_PAUSE:
		rte_eth_remove_rx_callback(port_id, queue_id, queue_cfg->cb);
		break;
	case RTE_POWER_MGMT_TYPE_SCALE:
		rte_eth_remove_rx_callback(port_id, queue_id, queue_cfg->cb);
		/* disable power library on this lcore if this was last queue */
		if (lcore_cfg->pwr_mgmt_state == PMD_MGMT_DISABLED) {
			rte_power_freq_max(lcore_id);
			rte_power_exit(lcore_id);
		}
		break;
	}
	/*
	 * the API doc mandates that the user stops all processing on affected
	 * ports before calling any of these API's, so we can assume that the
	 * callbacks can be freed. we're intentionally casting away const-ness.
	 */
	rte_free((void *)queue_cfg->cb);
	free(queue_cfg);

	return 0;
}

void
rte_power_pmd_mgmt_set_emptypoll_max(unsigned int max)
{
	emptypoll_max = max;
}

unsigned int
rte_power_pmd_mgmt_get_emptypoll_max(void)
{
	return emptypoll_max;
}

int
rte_power_pmd_mgmt_set_pause_duration(unsigned int duration)
{
	if (duration == 0) {
		RTE_LOG(ERR, POWER, "Pause duration must be greater than 0, value unchanged\n");
		return -EINVAL;
	}
	pause_duration = duration;

	return 0;
}

unsigned int
rte_power_pmd_mgmt_get_pause_duration(void)
{
	return pause_duration;
}

int
rte_power_pmd_mgmt_set_scaling_freq_min(unsigned int lcore, unsigned int min)
{
	if (lcore >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID: %u\n", lcore);
		return -EINVAL;
	}

	if (min > scale_freq_max[lcore]) {
		RTE_LOG(ERR, POWER, "Invalid min frequency: Cannot be greater than max frequency\n");
		return -EINVAL;
	}
	scale_freq_min[lcore] = min;

	return 0;
}

int
rte_power_pmd_mgmt_set_scaling_freq_max(unsigned int lcore, unsigned int max)
{
	if (lcore >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID: %u\n", lcore);
		return -EINVAL;
	}

	/* Zero means 'not set'. Use UINT32_MAX to enable RTE_MIN/MAX macro use when scaling. */
	if (max == 0)
		max = UINT32_MAX;
	if (max < scale_freq_min[lcore]) {
		RTE_LOG(ERR, POWER, "Invalid max frequency: Cannot be less than min frequency\n");
		return -EINVAL;
	}

	scale_freq_max[lcore] = max;

	return 0;
}

int
rte_power_pmd_mgmt_get_scaling_freq_min(unsigned int lcore)
{
	if (lcore >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID: %u\n", lcore);
		return -EINVAL;
	}

	if (scale_freq_max[lcore] == 0)
		RTE_LOG(DEBUG, POWER, "Scaling freq min config not set. Using sysfs min freq.\n");

	return scale_freq_min[lcore];
}

int
rte_power_pmd_mgmt_get_scaling_freq_max(unsigned int lcore)
{
	if (lcore >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID: %u\n", lcore);
		return -EINVAL;
	}

	if (scale_freq_max[lcore] == UINT32_MAX) {
		RTE_LOG(DEBUG, POWER, "Scaling freq max config not set. Using sysfs max freq.\n");
		return 0;
	}

	return scale_freq_max[lcore];
}

RTE_INIT(rte_power_ethdev_pmgmt_init) {
	size_t i;
	int j;

	/* initialize all tailqs */
	for (i = 0; i < RTE_DIM(lcore_cfgs); i++) {
		struct pmd_core_cfg *cfg = &lcore_cfgs[i];
		TAILQ_INIT(&cfg->head);
	}

	/* initialize config defaults */
	emptypoll_max = 512;
	pause_duration = 1;
	/* scaling defaults out of range to ensure not used unless set by user or app */
	for (j = 0; j < RTE_MAX_LCORE; j++) {
		scale_freq_min[j] = 0;
		scale_freq_max[j] = UINT32_MAX;
	}
}
