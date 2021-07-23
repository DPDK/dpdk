/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Xilinx, Inc.
 */
#include <rte_dev.h>
#include <rte_bitmap.h>

#include "sfc.h"
#include "sfc_rx.h"
#include "sfc_tx.h"
#include "sfc_sw_stats.h"

enum sfc_sw_stats_type {
	SFC_SW_STATS_RX,
	SFC_SW_STATS_TX,
};

typedef uint64_t sfc_get_sw_xstat_val_t(struct sfc_adapter *sa, uint16_t qid);

struct sfc_sw_xstat_descr {
	const char *name;
	enum sfc_sw_stats_type type;
	sfc_get_sw_xstat_val_t *get_val;
};

static sfc_get_sw_xstat_val_t sfc_get_sw_xstat_val_rx_dbells;
static uint64_t
sfc_get_sw_xstat_val_rx_dbells(struct sfc_adapter *sa, uint16_t qid)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	struct sfc_rxq_info *rxq_info;

	rxq_info = sfc_rxq_info_by_ethdev_qid(sas, qid);
	if (rxq_info->state & SFC_RXQ_INITIALIZED)
		return rxq_info->dp->dpq.rx_dbells;
	return 0;
}

static sfc_get_sw_xstat_val_t sfc_get_sw_xstat_val_tx_dbells;
static uint64_t
sfc_get_sw_xstat_val_tx_dbells(struct sfc_adapter *sa, uint16_t qid)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	struct sfc_txq_info *txq_info;

	txq_info = sfc_txq_info_by_ethdev_qid(sas, qid);
	if (txq_info->state & SFC_TXQ_INITIALIZED)
		return txq_info->dp->dpq.tx_dbells;
	return 0;
}

struct sfc_sw_xstat_descr sfc_sw_xstats[] = {
	{
		.name = "dbells",
		.type = SFC_SW_STATS_RX,
		.get_val  = sfc_get_sw_xstat_val_rx_dbells,
	},
	{
		.name = "dbells",
		.type = SFC_SW_STATS_TX,
		.get_val  = sfc_get_sw_xstat_val_tx_dbells,
	}
};

static int
sfc_sw_stat_get_name(struct sfc_adapter *sa,
		     const struct sfc_sw_xstat_descr *sw_xstat, char *name,
		     size_t name_size, unsigned int id_off)
{
	const char *prefix;
	int ret;

	switch (sw_xstat->type) {
	case SFC_SW_STATS_RX:
		prefix = "rx";
		break;
	case SFC_SW_STATS_TX:
		prefix = "tx";
		break;
	default:
		sfc_err(sa, "%s: unknown software statistics type %d",
			__func__, sw_xstat->type);
		return -EINVAL;
	}

	if (id_off == 0) {
		ret = snprintf(name, name_size, "%s_%s", prefix,
							 sw_xstat->name);
		if (ret < 0 || ret >= (int)name_size) {
			sfc_err(sa, "%s: failed to fill xstat name %s_%s, err %d",
				__func__, prefix, sw_xstat->name, ret);
			return ret > 0 ? -EINVAL : ret;
		}
	} else {
		uint16_t qid = id_off - 1;
		ret = snprintf(name, name_size, "%s_q%u_%s", prefix, qid,
							sw_xstat->name);
		if (ret < 0 || ret >= (int)name_size) {
			sfc_err(sa, "%s: failed to fill xstat name %s_q%u_%s, err %d",
				__func__, prefix, qid, sw_xstat->name, ret);
			return ret > 0 ? -EINVAL : ret;
		}
	}

	return 0;
}

static unsigned int
sfc_sw_stat_get_queue_count(struct sfc_adapter *sa,
			    const struct sfc_sw_xstat_descr *sw_xstat)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);

	switch (sw_xstat->type) {
	case SFC_SW_STATS_RX:
		return sas->ethdev_rxq_count;
	case SFC_SW_STATS_TX:
		return sas->ethdev_txq_count;
	default:
		sfc_err(sa, "%s: unknown software statistics type %d",
			__func__, sw_xstat->type);
		return 0;
	}
}

static unsigned int
sfc_sw_xstat_per_queue_get_count(unsigned int nb_queues)
{
	/* Take into account the accumulative xstat of all queues */
	return nb_queues > 0 ? 1 + nb_queues : 0;
}

static unsigned int
sfc_sw_xstat_get_nb_supported(struct sfc_adapter *sa,
			      const struct sfc_sw_xstat_descr *sw_xstat)
{
	unsigned int nb_queues;

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_xstat);
	return sfc_sw_xstat_per_queue_get_count(nb_queues);
}

static int
sfc_sw_stat_get_names(struct sfc_adapter *sa,
		      const struct sfc_sw_xstat_descr *sw_xstat,
		      struct rte_eth_xstat_name *xstats_names,
		      unsigned int xstats_names_sz,
		      unsigned int *nb_written,
		      unsigned int *nb_supported)
{
	const size_t name_size = sizeof(xstats_names[0].name);
	unsigned int id_base = *nb_supported;
	unsigned int nb_queues;
	unsigned int qid;
	int rc;

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_xstat);
	if (nb_queues == 0)
		return 0;
	*nb_supported += sfc_sw_xstat_per_queue_get_count(nb_queues);

	/*
	 * The order of each software xstat type is the accumulative xstat
	 * followed by per-queue xstats.
	 */
	if (*nb_written < xstats_names_sz) {
		rc = sfc_sw_stat_get_name(sa, sw_xstat,
					  xstats_names[*nb_written].name,
					  name_size, *nb_written - id_base);
		if (rc != 0)
			return rc;
		(*nb_written)++;
	}

	for (qid = 0; qid < nb_queues; ++qid) {
		if (*nb_written < xstats_names_sz) {
			rc = sfc_sw_stat_get_name(sa, sw_xstat,
					      xstats_names[*nb_written].name,
					      name_size, *nb_written - id_base);
			if (rc != 0)
				return rc;
			(*nb_written)++;
		}
	}

	return 0;
}

static int
sfc_sw_xstat_get_names_by_id(struct sfc_adapter *sa,
			     const struct sfc_sw_xstat_descr *sw_xstat,
			     const uint64_t *ids,
			     struct rte_eth_xstat_name *xstats_names,
			     unsigned int size,
			     unsigned int *nb_supported)
{
	const size_t name_size = sizeof(xstats_names[0].name);
	unsigned int id_base = *nb_supported;
	unsigned int nb_queues;
	unsigned int i;
	int rc;

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_xstat);
	if (nb_queues == 0)
		return 0;
	*nb_supported += sfc_sw_xstat_per_queue_get_count(nb_queues);

	/*
	 * The order of each software xstat type is the accumulative xstat
	 * followed by per-queue xstats.
	 */
	for (i = 0; i < size; i++) {
		if (id_base <= ids[i] && ids[i] <= id_base + nb_queues) {
			rc = sfc_sw_stat_get_name(sa, sw_xstat,
						  xstats_names[i].name,
						  name_size, ids[i] - id_base);
			if (rc != 0)
				return rc;
		}
	}

	return 0;
}

static void
sfc_sw_xstat_get_values(struct sfc_adapter *sa,
			const struct sfc_sw_xstat_descr *sw_xstat,
			struct rte_eth_xstat *xstats,
			unsigned int xstats_size,
			unsigned int *nb_written,
			unsigned int *nb_supported)
{
	unsigned int qid;
	uint64_t value;
	struct rte_eth_xstat *accum_xstat;
	bool count_accum_value = false;
	unsigned int nb_queues;

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_xstat);
	if (nb_queues == 0)
		return;
	*nb_supported += sfc_sw_xstat_per_queue_get_count(nb_queues);

	/*
	 * The order of each software xstat type is the accumulative xstat
	 * followed by per-queue xstats.
	 */
	if (*nb_written < xstats_size) {
		count_accum_value = true;
		accum_xstat = &xstats[*nb_written];
		xstats[*nb_written].id = *nb_written;
		xstats[*nb_written].value = 0;
		(*nb_written)++;
	}

	for (qid = 0; qid < nb_queues; ++qid) {
		value = sw_xstat->get_val(sa, qid);

		if (*nb_written < xstats_size) {
			xstats[*nb_written].id = *nb_written;
			xstats[*nb_written].value = value;
			(*nb_written)++;
		}

		if (count_accum_value)
			accum_xstat->value += value;
	}
}

static void
sfc_sw_xstat_get_values_by_id(struct sfc_adapter *sa,
			      const struct sfc_sw_xstat_descr *sw_xstat,
			      const uint64_t *ids,
			      uint64_t *values,
			      unsigned int ids_size,
			      unsigned int *nb_supported)
{
	rte_spinlock_t *bmp_lock = &sa->sw_xstats.queues_bitmap_lock;
	struct rte_bitmap *bmp = sa->sw_xstats.queues_bitmap;
	unsigned int id_base = *nb_supported;
	bool count_accum_value = false;
	unsigned int accum_value_idx;
	uint64_t accum_value = 0;
	unsigned int i, qid;
	unsigned int nb_queues;


	rte_spinlock_lock(bmp_lock);
	rte_bitmap_reset(bmp);

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_xstat);
	if (nb_queues == 0)
		goto unlock;
	*nb_supported += sfc_sw_xstat_per_queue_get_count(nb_queues);

	/*
	 * The order of each software xstat type is the accumulative xstat
	 * followed by per-queue xstats.
	 */
	for (i = 0; i < ids_size; i++) {
		if (id_base <= ids[i] && ids[i] <= (id_base + nb_queues)) {
			if (ids[i] == id_base) { /* Accumulative value */
				count_accum_value = true;
				accum_value_idx = i;
				continue;
			}
			qid = ids[i] - id_base - 1;
			values[i] = sw_xstat->get_val(sa, qid);
			accum_value += values[i];

			rte_bitmap_set(bmp, qid);
		}
	}

	if (count_accum_value) {
		for (qid = 0; qid < nb_queues; ++qid) {
			if (rte_bitmap_get(bmp, qid) != 0)
				continue;
			values[accum_value_idx] += sw_xstat->get_val(sa, qid);
		}
		values[accum_value_idx] += accum_value;
	}

unlock:
	rte_spinlock_unlock(bmp_lock);
}

unsigned int
sfc_sw_xstats_get_nb_supported(struct sfc_adapter *sa)
{
	unsigned int nb_supported = 0;
	unsigned int i;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	for (i = 0; i < RTE_DIM(sfc_sw_xstats); i++) {
		nb_supported += sfc_sw_xstat_get_nb_supported(sa,
							     &sfc_sw_xstats[i]);
	}

	return nb_supported;
}

void
sfc_sw_xstats_get_vals(struct sfc_adapter *sa,
		       struct rte_eth_xstat *xstats,
		       unsigned int xstats_count,
		       unsigned int *nb_written,
		       unsigned int *nb_supported)
{
	uint64_t *reset_vals = sa->sw_xstats.reset_vals;
	unsigned int sw_xstats_offset;
	unsigned int i;

	sfc_adapter_lock(sa);

	sw_xstats_offset = *nb_supported;

	for (i = 0; i < RTE_DIM(sfc_sw_xstats); i++) {
		sfc_sw_xstat_get_values(sa, &sfc_sw_xstats[i], xstats,
					xstats_count, nb_written, nb_supported);
	}

	for (i = sw_xstats_offset; i < *nb_written; i++)
		xstats[i].value -= reset_vals[i - sw_xstats_offset];

	sfc_adapter_unlock(sa);
}

int
sfc_sw_xstats_get_names(struct sfc_adapter *sa,
			struct rte_eth_xstat_name *xstats_names,
			unsigned int xstats_count,
			unsigned int *nb_written,
			unsigned int *nb_supported)
{
	unsigned int i;
	int ret;

	sfc_adapter_lock(sa);

	for (i = 0; i < RTE_DIM(sfc_sw_xstats); i++) {
		ret = sfc_sw_stat_get_names(sa, &sfc_sw_xstats[i],
					    xstats_names, xstats_count,
					    nb_written, nb_supported);
		if (ret != 0) {
			sfc_adapter_unlock(sa);
			return ret;
		}
	}

	sfc_adapter_unlock(sa);

	return 0;
}

void
sfc_sw_xstats_get_vals_by_id(struct sfc_adapter *sa,
			     const uint64_t *ids,
			     uint64_t *values,
			     unsigned int n,
			     unsigned int *nb_supported)
{
	uint64_t *reset_vals = sa->sw_xstats.reset_vals;
	unsigned int sw_xstats_offset;
	unsigned int i;

	sfc_adapter_lock(sa);

	sw_xstats_offset = *nb_supported;

	for (i = 0; i < RTE_DIM(sfc_sw_xstats); i++) {
		sfc_sw_xstat_get_values_by_id(sa, &sfc_sw_xstats[i], ids,
					      values, n, nb_supported);
	}

	for (i = 0; i < n; i++) {
		if (sw_xstats_offset <= ids[i] && ids[i] < *nb_supported)
			values[i] -= reset_vals[ids[i] - sw_xstats_offset];
	}

	sfc_adapter_unlock(sa);
}

int
sfc_sw_xstats_get_names_by_id(struct sfc_adapter *sa,
			      const uint64_t *ids,
			      struct rte_eth_xstat_name *xstats_names,
			      unsigned int size,
			      unsigned int *nb_supported)
{
	unsigned int i;
	int ret;

	sfc_adapter_lock(sa);

	for (i = 0; i < RTE_DIM(sfc_sw_xstats); i++) {
		ret = sfc_sw_xstat_get_names_by_id(sa, &sfc_sw_xstats[i], ids,
						   xstats_names, size,
						   nb_supported);
		if (ret != 0) {
			sfc_adapter_unlock(sa);
			SFC_ASSERT(ret < 0);
			return ret;
		}
	}

	sfc_adapter_unlock(sa);

	return 0;
}

static void
sfc_sw_xstat_reset(struct sfc_adapter *sa, struct sfc_sw_xstat_descr *sw_xstat,
		   uint64_t *reset_vals)
{
	unsigned int nb_queues;
	unsigned int qid;
	uint64_t *accum_xstat_reset;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_xstat);
	if (nb_queues == 0)
		return;

	/*
	 * The order of each software xstat type is the accumulative xstat
	 * followed by per-queue xstats.
	 */
	accum_xstat_reset = reset_vals;
	*accum_xstat_reset = 0;
	reset_vals++;

	for (qid = 0; qid < nb_queues; ++qid) {
		reset_vals[qid] = sw_xstat->get_val(sa, qid);
		*accum_xstat_reset += reset_vals[qid];
	}
}

void
sfc_sw_xstats_reset(struct sfc_adapter *sa)
{
	uint64_t *reset_vals = sa->sw_xstats.reset_vals;
	struct sfc_sw_xstat_descr *sw_xstat;
	unsigned int i;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	for (i = 0; i < RTE_DIM(sfc_sw_xstats); i++) {
		sw_xstat = &sfc_sw_xstats[i];
		sfc_sw_xstat_reset(sa, sw_xstat, reset_vals);
		reset_vals += sfc_sw_xstat_get_nb_supported(sa, sw_xstat);
	}
}

int
sfc_sw_xstats_configure(struct sfc_adapter *sa)
{
	uint64_t **reset_vals = &sa->sw_xstats.reset_vals;
	size_t nb_supported = 0;
	unsigned int i;

	for (i = 0; i < RTE_DIM(sfc_sw_xstats); i++)
		nb_supported += sfc_sw_xstat_get_nb_supported(sa,
							&sfc_sw_xstats[i]);

	*reset_vals = rte_realloc(*reset_vals,
				  nb_supported * sizeof(**reset_vals), 0);
	if (*reset_vals == NULL)
		return ENOMEM;

	memset(*reset_vals, 0, nb_supported * sizeof(**reset_vals));

	return 0;
}

static void
sfc_sw_xstats_free_queues_bitmap(struct sfc_adapter *sa)
{
	rte_bitmap_free(sa->sw_xstats.queues_bitmap);
	rte_free(sa->sw_xstats.queues_bitmap_mem);
}

static int
sfc_sw_xstats_alloc_queues_bitmap(struct sfc_adapter *sa)
{
	struct rte_bitmap **queues_bitmap = &sa->sw_xstats.queues_bitmap;
	void **queues_bitmap_mem = &sa->sw_xstats.queues_bitmap_mem;
	uint32_t bmp_size;
	int rc;

	bmp_size = rte_bitmap_get_memory_footprint(RTE_MAX_QUEUES_PER_PORT);
	*queues_bitmap_mem = NULL;
	*queues_bitmap = NULL;

	*queues_bitmap_mem = rte_calloc_socket("bitmap_mem", bmp_size, 1, 0,
					       sa->socket_id);
	if (*queues_bitmap_mem == NULL)
		return ENOMEM;

	*queues_bitmap = rte_bitmap_init(RTE_MAX_QUEUES_PER_PORT,
					 *queues_bitmap_mem, bmp_size);
	if (*queues_bitmap == NULL) {
		rc = EINVAL;
		goto fail;
	}

	rte_spinlock_init(&sa->sw_xstats.queues_bitmap_lock);
	return 0;

fail:
	sfc_sw_xstats_free_queues_bitmap(sa);
	return rc;
}

int
sfc_sw_xstats_init(struct sfc_adapter *sa)
{
	sa->sw_xstats.reset_vals = NULL;

	return sfc_sw_xstats_alloc_queues_bitmap(sa);
}

void
sfc_sw_xstats_close(struct sfc_adapter *sa)
{
	rte_free(sa->sw_xstats.reset_vals);
	sa->sw_xstats.reset_vals = NULL;

	sfc_sw_xstats_free_queues_bitmap(sa);
}
