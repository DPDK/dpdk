/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Xilinx, Inc.
 */
#include <dev_driver.h>
#include <rte_bitmap.h>

#include "sfc.h"
#include "sfc_rx.h"
#include "sfc_tx.h"
#include "sfc_sw_stats.h"

#define SFC_SW_STAT_INVALID		UINT64_MAX

#define SFC_SW_STATS_GROUP_SIZE_MAX	2U
#define SFC_SW_STAT_GOOD_PACKETS	"packets"
#define SFC_SW_STAT_GOOD_BYTES		"bytes"

enum sfc_sw_stats_type {
	SFC_SW_STATS_RX,
	SFC_SW_STATS_TX,
};

enum sfc_sw_stats_group_basic {
	SFC_SW_STATS_GROUP_BASIC_PKTS = 0,
	SFC_SW_STATS_GROUP_BASIC_BYTES,
	SFX_SW_STATS_GROUP_BASIC_MAX
};

typedef void sfc_get_sw_stat_val_t(struct sfc_adapter *sa, uint16_t qid,
				   uint64_t *values, unsigned int values_count);

struct sfc_sw_stat_descr {
	const char *name;
	enum sfc_sw_stats_type type;
	sfc_get_sw_stat_val_t *get_val;
	bool provide_total;
};

static sfc_get_sw_stat_val_t sfc_sw_stat_get_rx_good_pkts_bytes;
static void
sfc_sw_stat_get_rx_good_pkts_bytes(struct sfc_adapter *sa, uint16_t qid,
				   uint64_t *values,
				   unsigned int values_count)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	struct sfc_rxq_info *rxq_info;
	union sfc_pkts_bytes qstats;

	RTE_SET_USED(values_count);
	SFC_ASSERT(values_count == SFX_SW_STATS_GROUP_BASIC_MAX);
	rxq_info = sfc_rxq_info_by_ethdev_qid(sas, qid);
	if (rxq_info->state & SFC_RXQ_INITIALIZED) {
		sfc_pkts_bytes_get(&rxq_info->dp->dpq.stats, &qstats);
		values[SFC_SW_STATS_GROUP_BASIC_PKTS] = qstats.pkts;
		values[SFC_SW_STATS_GROUP_BASIC_BYTES] = qstats.bytes;
	} else {
		values[SFC_SW_STATS_GROUP_BASIC_PKTS] = 0;
		values[SFC_SW_STATS_GROUP_BASIC_BYTES] = 0;
	}
}

static sfc_get_sw_stat_val_t sfc_sw_stat_get_tx_good_pkts_bytes;
static void
sfc_sw_stat_get_tx_good_pkts_bytes(struct sfc_adapter *sa, uint16_t qid,
				   uint64_t *values,
				   unsigned int values_count)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	struct sfc_txq_info *txq_info;
	union sfc_pkts_bytes qstats;

	RTE_SET_USED(values_count);
	SFC_ASSERT(values_count == SFX_SW_STATS_GROUP_BASIC_MAX);
	txq_info = sfc_txq_info_by_ethdev_qid(sas, qid);
	if (txq_info->state & SFC_TXQ_INITIALIZED) {
		sfc_pkts_bytes_get(&txq_info->dp->dpq.stats, &qstats);
		values[SFC_SW_STATS_GROUP_BASIC_PKTS] = qstats.pkts;
		values[SFC_SW_STATS_GROUP_BASIC_BYTES] = qstats.bytes;
	} else {
		values[SFC_SW_STATS_GROUP_BASIC_PKTS] = 0;
		values[SFC_SW_STATS_GROUP_BASIC_BYTES] = 0;
	}
}

static sfc_get_sw_stat_val_t sfc_get_sw_stat_val_rx_dbells;
static void
sfc_get_sw_stat_val_rx_dbells(struct sfc_adapter *sa, uint16_t qid,
			       uint64_t *values, unsigned int values_count)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	struct sfc_rxq_info *rxq_info;

	RTE_SET_USED(values_count);
	SFC_ASSERT(values_count == 1);
	rxq_info = sfc_rxq_info_by_ethdev_qid(sas, qid);
	values[0] = rxq_info->state & SFC_RXQ_INITIALIZED ?
		    rxq_info->dp->dpq.dbells : 0;
}

static sfc_get_sw_stat_val_t sfc_get_sw_stat_val_tx_dbells;
static void
sfc_get_sw_stat_val_tx_dbells(struct sfc_adapter *sa, uint16_t qid,
			       uint64_t *values, unsigned int values_count)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	struct sfc_txq_info *txq_info;

	RTE_SET_USED(values_count);
	SFC_ASSERT(values_count == 1);
	txq_info = sfc_txq_info_by_ethdev_qid(sas, qid);
	values[0] = txq_info->state & SFC_TXQ_INITIALIZED ?
		    txq_info->dp->dpq.dbells : 0;
}

/*
 * SW stats can be grouped together. When stats are grouped the corresponding
 * stats values for each queue are obtained during calling one get value
 * callback. Stats of the same group are contiguous in the structure below.
 * The start of the group is denoted by stat implementing get value callback.
 */
const struct sfc_sw_stat_descr sfc_sw_stats_descr[] = {
	/* Group of Rx packets/bytes stats */
	{
		.name = SFC_SW_STAT_GOOD_PACKETS,
		.type = SFC_SW_STATS_RX,
		.get_val  = sfc_sw_stat_get_rx_good_pkts_bytes,
		.provide_total = false,
	},
	{
		.name = SFC_SW_STAT_GOOD_BYTES,
		.type = SFC_SW_STATS_RX,
		.get_val  = NULL,
		.provide_total = false,
	},
	/* Group of Tx packets/bytes stats */
	{
		.name = SFC_SW_STAT_GOOD_PACKETS,
		.type = SFC_SW_STATS_TX,
		.get_val  = sfc_sw_stat_get_tx_good_pkts_bytes,
		.provide_total = false,
	},
	{
		.name = SFC_SW_STAT_GOOD_BYTES,
		.type = SFC_SW_STATS_TX,
		.get_val  = NULL,
		.provide_total = false,
	},
	/* End of basic stats */
	{
		.name = "dbells",
		.type = SFC_SW_STATS_RX,
		.get_val  = sfc_get_sw_stat_val_rx_dbells,
		.provide_total = true,
	},
	{
		.name = "dbells",
		.type = SFC_SW_STATS_TX,
		.get_val  = sfc_get_sw_stat_val_tx_dbells,
		.provide_total = true,
	}
};

static int
sfc_sw_stat_get_name(struct sfc_adapter *sa,
		     const struct sfc_sw_stat_descr *sw_stat, char *name,
		     size_t name_size, unsigned int id_off)
{
	const char *prefix;
	int ret;

	switch (sw_stat->type) {
	case SFC_SW_STATS_RX:
		prefix = "rx";
		break;
	case SFC_SW_STATS_TX:
		prefix = "tx";
		break;
	default:
		sfc_err(sa, "%s: unknown software statistics type %d",
			__func__, sw_stat->type);
		return -EINVAL;
	}

	if (sw_stat->provide_total && id_off == 0) {
		ret = snprintf(name, name_size, "%s_%s", prefix,
							 sw_stat->name);
		if (ret < 0 || ret >= (int)name_size) {
			sfc_err(sa, "%s: failed to fill xstat name %s_%s, err %d",
				__func__, prefix, sw_stat->name, ret);
			return ret > 0 ? -EINVAL : ret;
		}
	} else {
		uint16_t qid = id_off - sw_stat->provide_total;
		ret = snprintf(name, name_size, "%s_q%u_%s", prefix, qid,
							sw_stat->name);
		if (ret < 0 || ret >= (int)name_size) {
			sfc_err(sa, "%s: failed to fill xstat name %s_q%u_%s, err %d",
				__func__, prefix, qid, sw_stat->name, ret);
			return ret > 0 ? -EINVAL : ret;
		}
	}

	return 0;
}

static unsigned int
sfc_sw_stat_get_queue_count(struct sfc_adapter *sa,
			    const struct sfc_sw_stat_descr *sw_stat)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);

	switch (sw_stat->type) {
	case SFC_SW_STATS_RX:
		return sas->ethdev_rxq_count;
	case SFC_SW_STATS_TX:
		return sas->ethdev_txq_count;
	default:
		sfc_err(sa, "%s: unknown software statistics type %d",
			__func__, sw_stat->type);
		return 0;
	}
}

static unsigned int
sfc_sw_xstat_per_queue_get_count(const struct sfc_sw_stat_descr *sw_stat,
				 unsigned int nb_queues)
{
	/* Take into account the total xstat of all queues */
	return nb_queues > 0 ? sw_stat->provide_total + nb_queues : 0;
}

static unsigned int
sfc_sw_xstat_get_nb_supported(struct sfc_adapter *sa,
			      const struct sfc_sw_stat_descr *sw_stat)
{
	unsigned int nb_queues;

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_stat);
	return sfc_sw_xstat_per_queue_get_count(sw_stat, nb_queues);
}

static int
sfc_sw_stat_get_names(struct sfc_adapter *sa,
		      const struct sfc_sw_stat_descr *sw_stat,
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

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_stat);
	if (nb_queues == 0)
		return 0;
	*nb_supported += sfc_sw_xstat_per_queue_get_count(sw_stat, nb_queues);

	/*
	 * The order of each software xstat type is the total xstat
	 * followed by per-queue xstats.
	 */
	if (*nb_written < xstats_names_sz && sw_stat->provide_total) {
		rc = sfc_sw_stat_get_name(sa, sw_stat,
					  xstats_names[*nb_written].name,
					  name_size, *nb_written - id_base);
		if (rc != 0)
			return rc;
		(*nb_written)++;
	}

	for (qid = 0; qid < nb_queues; ++qid) {
		if (*nb_written < xstats_names_sz) {
			rc = sfc_sw_stat_get_name(sa, sw_stat,
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
			     const struct sfc_sw_stat_descr *sw_stat,
			     const uint64_t *ids,
			     struct rte_eth_xstat_name *xstats_names,
			     unsigned int size,
			     unsigned int *nb_supported)
{
	const size_t name_size = sizeof(xstats_names[0].name);
	unsigned int id_base = *nb_supported;
	unsigned int id_end;
	unsigned int nb_queues;
	unsigned int i;
	int rc;

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_stat);
	if (nb_queues == 0)
		return 0;
	*nb_supported += sfc_sw_xstat_per_queue_get_count(sw_stat, nb_queues);

	/*
	 * The order of each software xstat type is the total xstat
	 * followed by per-queue xstats.
	 */
	id_end = id_base + sw_stat->provide_total + nb_queues;
	for (i = 0; i < size; i++) {
		if (id_base <= ids[i] && ids[i] < id_end) {
			rc = sfc_sw_stat_get_name(sa, sw_stat,
						  xstats_names[i].name,
						  name_size, ids[i] - id_base);
			if (rc != 0)
				return rc;
		}
	}

	return 0;
}

static uint64_t
sfc_sw_stat_get_val(struct sfc_adapter *sa,
		    unsigned int sw_stat_idx, uint16_t qid)
{
	struct sfc_sw_stats *sw_stats = &sa->sw_stats;
	uint64_t *res = &sw_stats->supp[sw_stat_idx].cache[qid];
	uint64_t values[SFC_SW_STATS_GROUP_SIZE_MAX];
	unsigned int group_start_idx;
	unsigned int group_size;
	unsigned int i;

	if (*res != SFC_SW_STAT_INVALID)
		return *res;

	/*
	 * Search for the group start, i.e. the stat that implements
	 * get value callback.
	 */
	group_start_idx = sw_stat_idx;
	while (sw_stats->supp[group_start_idx].descr->get_val == NULL)
		group_start_idx--;

	/*
	 * Calculate number of elements in the group with loop till the next
	 * group start or the list end.
	 */
	group_size = 1;
	for (i = sw_stat_idx + 1; i < sw_stats->supp_count; i++) {
		if (sw_stats->supp[i].descr->get_val != NULL)
			break;
		group_size++;
	}
	group_size += sw_stat_idx - group_start_idx;

	SFC_ASSERT(group_size <= SFC_SW_STATS_GROUP_SIZE_MAX);
	sw_stats->supp[group_start_idx].descr->get_val(sa, qid, values,
						       group_size);
	for (i = group_start_idx; i < (group_start_idx + group_size); i++)
		sw_stats->supp[i].cache[qid] = values[i - group_start_idx];

	return *res;
}

static void
sfc_sw_xstat_get_values(struct sfc_adapter *sa,
			const struct sfc_sw_stat_descr *sw_stat,
			unsigned int sw_stat_idx,
			struct rte_eth_xstat *xstats,
			unsigned int xstats_size,
			unsigned int *nb_written,
			unsigned int *nb_supported)
{
	unsigned int qid;
	uint64_t value;
	struct rte_eth_xstat *total_xstat;
	bool count_total_value = false;
	unsigned int nb_queues;

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_stat);
	if (nb_queues == 0)
		return;
	*nb_supported += sfc_sw_xstat_per_queue_get_count(sw_stat, nb_queues);

	/*
	 * The order of each software xstat type is the total xstat
	 * followed by per-queue xstats.
	 */
	if (*nb_written < xstats_size && sw_stat->provide_total) {
		count_total_value = true;
		total_xstat = &xstats[*nb_written];
		xstats[*nb_written].id = *nb_written;
		xstats[*nb_written].value = 0;
		(*nb_written)++;
	}

	for (qid = 0; qid < nb_queues; ++qid) {
		value = sfc_sw_stat_get_val(sa, sw_stat_idx, qid);

		if (*nb_written < xstats_size) {
			xstats[*nb_written].id = *nb_written;
			xstats[*nb_written].value = value;
			(*nb_written)++;
		}

		if (count_total_value)
			total_xstat->value += value;
	}
}

static void
sfc_sw_xstat_get_values_by_id(struct sfc_adapter *sa,
			      const struct sfc_sw_stat_descr *sw_stat,
			      unsigned int sw_stat_idx,
			      const uint64_t *ids,
			      uint64_t *values,
			      unsigned int ids_size,
			      unsigned int *nb_supported)
{
	rte_spinlock_t *bmp_lock = &sa->sw_stats.queues_bitmap_lock;
	struct rte_bitmap *bmp = sa->sw_stats.queues_bitmap;
	unsigned int id_base = *nb_supported;
	unsigned int id_base_q;
	unsigned int id_end;
	bool count_total_value = false;
	unsigned int total_value_idx;
	uint64_t total_value = 0;
	unsigned int i, qid;
	unsigned int nb_queues;


	rte_spinlock_lock(bmp_lock);
	rte_bitmap_reset(bmp);

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_stat);
	if (nb_queues == 0)
		goto unlock;
	*nb_supported += sfc_sw_xstat_per_queue_get_count(sw_stat, nb_queues);

	/*
	 * The order of each software xstat type is the total xstat
	 * followed by per-queue xstats.
	 */
	id_end = id_base + sw_stat->provide_total + nb_queues;
	for (i = 0; i < ids_size; i++) {
		if (id_base <= ids[i] && ids[i] < id_end) {
			if (sw_stat->provide_total && ids[i] == id_base) {
				/* Accumulative value */
				count_total_value = true;
				total_value_idx = i;
				continue;
			}
			id_base_q = id_base + sw_stat->provide_total;
			qid = ids[i] - id_base_q;
			values[i] = sfc_sw_stat_get_val(sa, sw_stat_idx, qid);
			total_value += values[i];

			rte_bitmap_set(bmp, qid);
		}
	}

	if (count_total_value) {
		values[total_value_idx] = 0;
		for (qid = 0; qid < nb_queues; ++qid) {
			if (rte_bitmap_get(bmp, qid) != 0)
				continue;
			values[total_value_idx] += sfc_sw_stat_get_val(sa,
								    sw_stat_idx,
								    qid);
		}
		values[total_value_idx] += total_value;
	}

unlock:
	rte_spinlock_unlock(bmp_lock);
}

unsigned int
sfc_sw_xstats_get_nb_supported(struct sfc_adapter *sa)
{
	SFC_ASSERT(sfc_adapter_is_locked(sa));
	return sa->sw_stats.xstats_count;
}

static void
sfc_sw_stats_clear_cache(struct sfc_adapter *sa)
{
	unsigned int cache_count = sa->sw_stats.cache_count;
	uint64_t *cache = sa->sw_stats.cache;

	RTE_BUILD_BUG_ON(UINT64_C(0xffffffffffffffff) != SFC_SW_STAT_INVALID);
	memset(cache, 0xff, cache_count * sizeof(*cache));
}

void
sfc_sw_xstats_get_vals(struct sfc_adapter *sa,
		       struct rte_eth_xstat *xstats,
		       unsigned int xstats_count,
		       unsigned int *nb_written,
		       unsigned int *nb_supported)
{
	uint64_t *reset_vals = sa->sw_stats.reset_vals;
	struct sfc_sw_stats *sw_stats = &sa->sw_stats;
	unsigned int sw_xstats_offset;
	unsigned int i;

	sfc_adapter_lock(sa);

	sfc_sw_stats_clear_cache(sa);

	sw_xstats_offset = *nb_supported;

	for (i = 0; i < sw_stats->supp_count; i++) {
		sfc_sw_xstat_get_values(sa, sw_stats->supp[i].descr, i,
				xstats, xstats_count, nb_written, nb_supported);
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
	struct sfc_sw_stats *sw_stats = &sa->sw_stats;
	unsigned int i;
	int ret;

	sfc_adapter_lock(sa);

	for (i = 0; i < sw_stats->supp_count; i++) {
		ret = sfc_sw_stat_get_names(sa, sw_stats->supp[i].descr,
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
	uint64_t *reset_vals = sa->sw_stats.reset_vals;
	struct sfc_sw_stats *sw_stats = &sa->sw_stats;
	unsigned int sw_xstats_offset;
	unsigned int i;

	sfc_adapter_lock(sa);

	sfc_sw_stats_clear_cache(sa);

	sw_xstats_offset = *nb_supported;

	for (i = 0; i < sw_stats->supp_count; i++) {
		sfc_sw_xstat_get_values_by_id(sa, sw_stats->supp[i].descr, i,
					      ids, values, n, nb_supported);
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
	struct sfc_sw_stats *sw_stats = &sa->sw_stats;
	unsigned int i;
	int ret;

	sfc_adapter_lock(sa);

	for (i = 0; i < sw_stats->supp_count; i++) {
		ret = sfc_sw_xstat_get_names_by_id(sa, sw_stats->supp[i].descr,
						   ids, xstats_names, size,
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
sfc_sw_xstat_reset(struct sfc_adapter *sa,
		   const struct sfc_sw_stat_descr *sw_stat,
		   unsigned int sw_stat_idx,
		   uint64_t *reset_vals)
{
	unsigned int nb_queues;
	unsigned int qid;
	uint64_t *total_xstat_reset = NULL;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	nb_queues = sfc_sw_stat_get_queue_count(sa, sw_stat);
	if (nb_queues == 0)
		return;

	/*
	 * The order of each software xstat type is the total xstat
	 * followed by per-queue xstats.
	 */
	if (sw_stat->provide_total) {
		total_xstat_reset = reset_vals;
		*total_xstat_reset = 0;
		reset_vals++;
	}

	for (qid = 0; qid < nb_queues; ++qid) {
		reset_vals[qid] = sfc_sw_stat_get_val(sa, sw_stat_idx, qid);
		if (sw_stat->provide_total)
			*total_xstat_reset += reset_vals[qid];
	}
}

void
sfc_sw_xstats_reset(struct sfc_adapter *sa)
{
	uint64_t *reset_vals = sa->sw_stats.reset_vals;
	struct sfc_sw_stats *sw_stats = &sa->sw_stats;
	unsigned int i;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	sfc_sw_stats_clear_cache(sa);

	for (i = 0; i < sw_stats->supp_count; i++) {
		sfc_sw_xstat_reset(sa, sw_stats->supp[i].descr, i, reset_vals);
		reset_vals += sfc_sw_xstat_get_nb_supported(sa,
						       sw_stats->supp[i].descr);
	}
}

static bool
sfc_sw_stats_is_packets_or_bytes(const char *xstat_name)
{
	return strcmp(xstat_name, SFC_SW_STAT_GOOD_PACKETS) == 0 ||
	       strcmp(xstat_name, SFC_SW_STAT_GOOD_BYTES) == 0;
}

static void
sfc_sw_stats_fill_available_descr(struct sfc_adapter *sa)
{
	const struct sfc_adapter_priv *sap = &sa->priv;
	bool have_dp_rx_stats = sap->dp_rx->features & SFC_DP_RX_FEAT_STATS;
	bool have_dp_tx_stats = sap->dp_tx->features & SFC_DP_TX_FEAT_STATS;
	struct sfc_sw_stats *sw_stats = &sa->sw_stats;
	const struct sfc_sw_stat_descr *sw_stat_descr;
	unsigned int i;

	sw_stats->supp_count = 0;
	for (i = 0; i < RTE_DIM(sfc_sw_stats_descr); i++) {
		sw_stat_descr = &sfc_sw_stats_descr[i];
		if (!have_dp_rx_stats &&
		    sw_stat_descr->type == SFC_SW_STATS_RX &&
		    sfc_sw_stats_is_packets_or_bytes(sw_stat_descr->name))
			continue;
		if (!have_dp_tx_stats &&
		    sw_stat_descr->type == SFC_SW_STATS_TX &&
		    sfc_sw_stats_is_packets_or_bytes(sw_stat_descr->name))
			continue;
		sw_stats->supp[sw_stats->supp_count].descr = sw_stat_descr;
		sw_stats->supp_count++;
	}
}

static int
sfc_sw_stats_set_reset_basic_stats(struct sfc_adapter *sa)
{
	uint64_t *reset_vals = sa->sw_stats.reset_vals;
	struct sfc_sw_stats *sw_stats = &sa->sw_stats;
	const struct sfc_sw_stat_descr *sw_stat;
	unsigned int i;

	for (i = 0; i < sw_stats->supp_count; i++) {
		sw_stat = sw_stats->supp[i].descr;

		switch (sw_stat->type) {
		case SFC_SW_STATS_RX:
			if (strcmp(sw_stat->name,
				   SFC_SW_STAT_GOOD_PACKETS) == 0)
				sa->sw_stats.reset_rx_pkts = reset_vals;
			else if (strcmp(sw_stat->name,
					SFC_SW_STAT_GOOD_BYTES) == 0)
				sa->sw_stats.reset_rx_bytes = reset_vals;
			break;
		case SFC_SW_STATS_TX:
			if (strcmp(sw_stat->name,
				   SFC_SW_STAT_GOOD_PACKETS) == 0)
				sa->sw_stats.reset_tx_pkts = reset_vals;
			else if (strcmp(sw_stat->name,
					SFC_SW_STAT_GOOD_BYTES) == 0)
				sa->sw_stats.reset_tx_bytes = reset_vals;
			break;
		default:
			SFC_GENERIC_LOG(ERR, "Unknown SW stat type");
			return -EINVAL;
		}

		reset_vals += sfc_sw_xstat_get_nb_supported(sa, sw_stat);
	}

	return 0;
}

int
sfc_sw_xstats_configure(struct sfc_adapter *sa)
{
	uint64_t **reset_vals = &sa->sw_stats.reset_vals;
	struct sfc_sw_stats *sw_stats = &sa->sw_stats;
	unsigned int cache_count = 0;
	uint64_t **cache =  &sa->sw_stats.cache;
	uint64_t *stat_cache;
	size_t nb_supported = 0;
	unsigned int i;
	int rc;

	sw_stats->supp_count = RTE_DIM(sfc_sw_stats_descr);
	if (sw_stats->supp == NULL) {
		sw_stats->supp = rte_malloc(NULL, sw_stats->supp_count *
					    sizeof(*sw_stats->supp), 0);
		if (sw_stats->supp == NULL)
			return -ENOMEM;
	}
	for (i = 0; i < sw_stats->supp_count; i++)
		sw_stats->supp[i].descr = &sfc_sw_stats_descr[i];
	sfc_sw_stats_fill_available_descr(sa);

	for (i = 0; i < sw_stats->supp_count; i++) {
		nb_supported += sfc_sw_xstat_get_nb_supported(sa,
						       sw_stats->supp[i].descr);
		cache_count += sfc_sw_stat_get_queue_count(sa,
						       sw_stats->supp[i].descr);
	}
	sa->sw_stats.xstats_count = nb_supported;

	*reset_vals = rte_realloc(*reset_vals,
				  nb_supported * sizeof(**reset_vals), 0);
	if (*reset_vals == NULL) {
		rc = -ENOMEM;
		goto fail_reset_vals;
	}

	memset(*reset_vals, 0, nb_supported * sizeof(**reset_vals));

	*cache = rte_realloc(*cache, cache_count * sizeof(**cache), 0);
	if (*cache == NULL) {
		rc = ENOMEM;
		goto fail_cache;
	}
	sa->sw_stats.cache_count = cache_count;
	stat_cache = *cache;
	rc = sfc_sw_stats_set_reset_basic_stats(sa);
	if (rc != 0)
		goto fail_reset_basic_stats;

	for (i = 0; i < sw_stats->supp_count; i++) {
		sw_stats->supp[i].cache = stat_cache;
		stat_cache += sfc_sw_stat_get_queue_count(sa,
						       sw_stats->supp[i].descr);
	}

	return 0;

fail_reset_basic_stats:
	rte_free(*cache);
	*cache = NULL;
	sa->sw_stats.cache_count = 0;
fail_cache:
	rte_free(*reset_vals);
	*reset_vals = NULL;
fail_reset_vals:
	sa->sw_stats.xstats_count = 0;
	rte_free(sw_stats->supp);
	sw_stats->supp = NULL;
	sw_stats->supp_count = 0;

	return rc;
}

static void
sfc_sw_xstats_free_queues_bitmap(struct sfc_adapter *sa)
{
	rte_bitmap_free(sa->sw_stats.queues_bitmap);
	rte_free(sa->sw_stats.queues_bitmap_mem);
}

static int
sfc_sw_xstats_alloc_queues_bitmap(struct sfc_adapter *sa)
{
	struct rte_bitmap **queues_bitmap = &sa->sw_stats.queues_bitmap;
	void **queues_bitmap_mem = &sa->sw_stats.queues_bitmap_mem;
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

	rte_spinlock_init(&sa->sw_stats.queues_bitmap_lock);
	return 0;

fail:
	sfc_sw_xstats_free_queues_bitmap(sa);
	return rc;
}

int
sfc_sw_xstats_init(struct sfc_adapter *sa)
{
	sa->sw_stats.xstats_count = 0;
	sa->sw_stats.supp = NULL;
	sa->sw_stats.supp_count = 0;
	sa->sw_stats.cache = NULL;
	sa->sw_stats.cache_count = 0;
	sa->sw_stats.reset_vals = NULL;

	return sfc_sw_xstats_alloc_queues_bitmap(sa);
}

void
sfc_sw_xstats_close(struct sfc_adapter *sa)
{
	sfc_sw_xstats_free_queues_bitmap(sa);
	sa->sw_stats.reset_vals = NULL;
	rte_free(sa->sw_stats.cache);
	sa->sw_stats.cache = NULL;
	sa->sw_stats.cache_count = 0;
	rte_free(sa->sw_stats.reset_vals);
	rte_free(sa->sw_stats.supp);
	sa->sw_stats.supp = NULL;
	sa->sw_stats.supp_count = 0;
	sa->sw_stats.xstats_count = 0;
}
