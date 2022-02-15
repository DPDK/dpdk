/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <string.h>

#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_metrics.h>
#include <rte_memzone.h>
#include <rte_spinlock.h>

int metrics_initialized;

#define RTE_METRICS_MEMZONE_NAME "RTE_METRICS"

/**
 * Internal stats metadata and value entry.
 *
 * @internal
 */
struct rte_metrics_meta_s {
	/** Name of metric */
	char name[RTE_METRICS_MAX_NAME_LEN];
	/** Current value for metric */
	uint64_t value[RTE_MAX_ETHPORTS];
	/** Used for global metrics */
	uint64_t global_value;
	/** Index of next root element (zero for none) */
	uint16_t idx_next_set;
	/** Index of next metric in set (zero for none) */
	uint16_t idx_next_stat;
};

/**
 * Internal stats info structure.
 *
 * @internal
 * Offsets into metadata are used instead of pointers because ASLR
 * means that having the same physical addresses in different
 * processes is not guaranteed.
 */
struct rte_metrics_data_s {
	/**   Index of last metadata entry with valid data.
	 * This value is not valid if cnt_stats is zero.
	 */
	uint16_t idx_last_set;
	/**   Number of metrics. */
	uint16_t cnt_stats;
	/** Metric data memory block. */
	struct rte_metrics_meta_s metadata[RTE_METRICS_MAX_METRICS];
	/** Metric data access lock */
	rte_spinlock_t lock;
};

void
rte_metrics_init(int socket_id)
{
	struct rte_metrics_data_s *stats;
	const struct rte_memzone *memzone;

	if (metrics_initialized)
		return;
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	memzone = rte_memzone_lookup(RTE_METRICS_MEMZONE_NAME);
	if (memzone != NULL)
		return;
	memzone = rte_memzone_reserve(RTE_METRICS_MEMZONE_NAME,
		sizeof(struct rte_metrics_data_s), socket_id, 0);
	if (memzone == NULL)
		rte_exit(EXIT_FAILURE, "Unable to allocate stats memzone\n");
	stats = memzone->addr;
	memset(stats, 0, sizeof(struct rte_metrics_data_s));
	rte_spinlock_init(&stats->lock);
	metrics_initialized = 1;
}

int
rte_metrics_deinit(void)
{
	struct rte_metrics_data_s *stats;
	const struct rte_memzone *memzone;
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EINVAL;

	memzone = rte_memzone_lookup(RTE_METRICS_MEMZONE_NAME);
	if (memzone == NULL)
		return -EIO;

	stats = memzone->addr;
	memset(stats, 0, sizeof(struct rte_metrics_data_s));

	ret = rte_memzone_free(memzone);
	if (ret == 0)
		metrics_initialized = 0;
	return ret;
}

int
rte_metrics_reg_name(const char *name)
{
	const char * const list_names[] = {name};

	return rte_metrics_reg_names(list_names, 1);
}

int
rte_metrics_reg_names(const char * const *names, uint16_t cnt_names)
{
	struct rte_metrics_meta_s *entry = NULL;
	struct rte_metrics_data_s *stats;
	const struct rte_memzone *memzone;
	uint16_t idx_name;
	uint16_t idx_base;

	/* Some sanity checks */
	if (cnt_names < 1 || names == NULL)
		return -EINVAL;
	for (idx_name = 0; idx_name < cnt_names; idx_name++)
		if (names[idx_name] == NULL)
			return -EINVAL;

	memzone = rte_memzone_lookup(RTE_METRICS_MEMZONE_NAME);
	if (memzone == NULL)
		return -EIO;
	stats = memzone->addr;

	if (stats->cnt_stats + cnt_names >= RTE_METRICS_MAX_METRICS)
		return -ENOMEM;

	rte_spinlock_lock(&stats->lock);

	/* Overwritten later if this is actually first set.. */
	stats->metadata[stats->idx_last_set].idx_next_set = stats->cnt_stats;

	stats->idx_last_set = idx_base = stats->cnt_stats;

	for (idx_name = 0; idx_name < cnt_names; idx_name++) {
		entry = &stats->metadata[idx_name + stats->cnt_stats];
		strlcpy(entry->name, names[idx_name], RTE_METRICS_MAX_NAME_LEN);
		memset(entry->value, 0, sizeof(entry->value));
		entry->idx_next_stat = idx_name + stats->cnt_stats + 1;
	}
	entry->idx_next_stat = 0;
	entry->idx_next_set = 0;
	stats->cnt_stats += cnt_names;

	rte_spinlock_unlock(&stats->lock);

	return idx_base;
}

int
rte_metrics_update_value(int port_id, uint16_t key, const uint64_t value)
{
	return rte_metrics_update_values(port_id, key, &value, 1);
}

int
rte_metrics_update_values(int port_id,
	uint16_t key,
	const uint64_t *values,
	uint32_t count)
{
	struct rte_metrics_meta_s *entry;
	struct rte_metrics_data_s *stats;
	const struct rte_memzone *memzone;
	uint16_t idx_metric;
	uint16_t idx_value;
	uint16_t cnt_setsize;

	if (port_id != RTE_METRICS_GLOBAL &&
			(port_id < 0 || port_id >= RTE_MAX_ETHPORTS))
		return -EINVAL;

	if (values == NULL)
		return -EINVAL;

	memzone = rte_memzone_lookup(RTE_METRICS_MEMZONE_NAME);
	if (memzone == NULL)
		return -EIO;
	stats = memzone->addr;

	rte_spinlock_lock(&stats->lock);

	if (key >= stats->cnt_stats) {
		rte_spinlock_unlock(&stats->lock);
		return -EINVAL;
	}
	idx_metric = key;
	cnt_setsize = 1;
	while (idx_metric < stats->cnt_stats) {
		entry = &stats->metadata[idx_metric];
		if (entry->idx_next_stat == 0)
			break;
		cnt_setsize++;
		idx_metric++;
	}
	/* Check update does not cross set border */
	if (count > cnt_setsize) {
		rte_spinlock_unlock(&stats->lock);
		return -ERANGE;
	}

	if (port_id == RTE_METRICS_GLOBAL)
		for (idx_value = 0; idx_value < count; idx_value++) {
			idx_metric = key + idx_value;
			stats->metadata[idx_metric].global_value =
				values[idx_value];
		}
	else
		for (idx_value = 0; idx_value < count; idx_value++) {
			idx_metric = key + idx_value;
			stats->metadata[idx_metric].value[port_id] =
				values[idx_value];
		}
	rte_spinlock_unlock(&stats->lock);
	return 0;
}

int
rte_metrics_get_names(struct rte_metric_name *names,
	uint16_t capacity)
{
	struct rte_metrics_data_s *stats;
	const struct rte_memzone *memzone;
	uint16_t idx_name;
	int return_value;

	memzone = rte_memzone_lookup(RTE_METRICS_MEMZONE_NAME);
	if (memzone == NULL)
		return -EIO;

	stats = memzone->addr;
	rte_spinlock_lock(&stats->lock);
	if (names != NULL) {
		if (capacity < stats->cnt_stats) {
			return_value = stats->cnt_stats;
			rte_spinlock_unlock(&stats->lock);
			return return_value;
		}
		for (idx_name = 0; idx_name < stats->cnt_stats; idx_name++)
			strlcpy(names[idx_name].name,
				stats->metadata[idx_name].name,
				RTE_METRICS_MAX_NAME_LEN);
	}
	return_value = stats->cnt_stats;
	rte_spinlock_unlock(&stats->lock);
	return return_value;
}

int
rte_metrics_get_values(int port_id,
	struct rte_metric_value *values,
	uint16_t capacity)
{
	struct rte_metrics_meta_s *entry;
	struct rte_metrics_data_s *stats;
	const struct rte_memzone *memzone;
	uint16_t idx_name;
	int return_value;

	if (port_id != RTE_METRICS_GLOBAL &&
			(port_id < 0 || port_id >= RTE_MAX_ETHPORTS))
		return -EINVAL;

	memzone = rte_memzone_lookup(RTE_METRICS_MEMZONE_NAME);
	if (memzone == NULL)
		return -EIO;

	stats = memzone->addr;
	rte_spinlock_lock(&stats->lock);

	if (values != NULL) {
		if (capacity < stats->cnt_stats) {
			return_value = stats->cnt_stats;
			rte_spinlock_unlock(&stats->lock);
			return return_value;
		}
		if (port_id == RTE_METRICS_GLOBAL)
			for (idx_name = 0;
					idx_name < stats->cnt_stats;
					idx_name++) {
				entry = &stats->metadata[idx_name];
				values[idx_name].key = idx_name;
				values[idx_name].value = entry->global_value;
			}
		else
			for (idx_name = 0;
					idx_name < stats->cnt_stats;
					idx_name++) {
				entry = &stats->metadata[idx_name];
				values[idx_name].key = idx_name;
				values[idx_name].value = entry->value[port_id];
			}
	}
	return_value = stats->cnt_stats;
	rte_spinlock_unlock(&stats->lock);
	return return_value;
}
