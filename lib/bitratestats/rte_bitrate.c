/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_metrics.h>
#include <rte_bitrate.h>

/*
 * Persistent bit-rate data.
 * @internal
 */
struct rte_stats_bitrate {
	uint64_t last_ibytes;
	uint64_t last_obytes;
	uint64_t peak_ibits;
	uint64_t peak_obits;
	uint64_t mean_ibits;
	uint64_t mean_obits;
	uint64_t ewma_ibits;
	uint64_t ewma_obits;
};

struct rte_stats_bitrates {
	struct rte_stats_bitrate port_stats[RTE_MAX_ETHPORTS];
	uint16_t id_stats_set;
};

struct rte_stats_bitrates *
rte_stats_bitrate_create(void)
{
	return rte_zmalloc(NULL, sizeof(struct rte_stats_bitrates),
		RTE_CACHE_LINE_SIZE);
}

void
rte_stats_bitrate_free(struct rte_stats_bitrates *bitrate_data)
{
	rte_free(bitrate_data);
}

int
rte_stats_bitrate_reg(struct rte_stats_bitrates *bitrate_data)
{
	const char * const names[] = {
		"ewma_bits_in", "ewma_bits_out",
		"mean_bits_in", "mean_bits_out",
		"peak_bits_in", "peak_bits_out",
	};
	int return_value;

	if (bitrate_data == NULL)
		return -EINVAL;

	return_value = rte_metrics_reg_names(&names[0], RTE_DIM(names));
	if (return_value >= 0)
		bitrate_data->id_stats_set = return_value;
	return return_value;
}

int
rte_stats_bitrate_calc(struct rte_stats_bitrates *bitrate_data,
			uint16_t port_id)
{
	struct rte_stats_bitrate *port_data;
	struct rte_eth_stats eth_stats;
	int ret_code;
	uint64_t cnt_bits;
	int64_t delta;
	const int64_t alpha_percent = 20;
	uint64_t values[6];
	int ret;

	if (bitrate_data == NULL)
		return -EINVAL;

	ret_code = rte_eth_stats_get(port_id, &eth_stats);
	if (ret_code != 0)
		return ret_code;

	port_data = &bitrate_data->port_stats[port_id];

	/* Incoming bitrate. This is an iteratively calculated EWMA
	 * (Exponentially Weighted Moving Average) that uses a
	 * weighting factor of alpha_percent. An unsmoothed mean
	 * for just the current time delta is also calculated for the
	 * benefit of people who don't understand signal processing.
	 */
	cnt_bits = (eth_stats.ibytes - port_data->last_ibytes) << 3;
	port_data->last_ibytes = eth_stats.ibytes;
	if (cnt_bits > port_data->peak_ibits)
		port_data->peak_ibits = cnt_bits;
	delta = cnt_bits;
	delta -= port_data->ewma_ibits;
	/* The +-50 fixes integer rounding during division */
	if (delta > 0)
		delta = (delta * alpha_percent + 50) / 100;
	else
		delta = (delta * alpha_percent - 50) / 100;
	port_data->ewma_ibits += delta;
	/* Integer roundoff prevents EWMA between 0 and (100/alpha_percent)
	 * ever reaching zero in no-traffic conditions
	 */
	if (cnt_bits == 0 && delta == 0)
		port_data->ewma_ibits = 0;
	port_data->mean_ibits = cnt_bits;

	/* Outgoing bitrate (also EWMA) */
	cnt_bits = (eth_stats.obytes - port_data->last_obytes) << 3;
	port_data->last_obytes = eth_stats.obytes;
	if (cnt_bits > port_data->peak_obits)
		port_data->peak_obits = cnt_bits;
	delta = cnt_bits;
	delta -= port_data->ewma_obits;
	if (delta > 0)
		delta = (delta * alpha_percent + 50) / 100;
	else
		delta = (delta * alpha_percent - 50) / 100;
	port_data->ewma_obits += delta;
	if (cnt_bits == 0 && delta == 0)
		port_data->ewma_obits = 0;
	port_data->mean_obits = cnt_bits;

	values[0] = port_data->ewma_ibits;
	values[1] = port_data->ewma_obits;
	values[2] = port_data->mean_ibits;
	values[3] = port_data->mean_obits;
	values[4] = port_data->peak_ibits;
	values[5] = port_data->peak_obits;
	ret = rte_metrics_update_values(port_id, bitrate_data->id_stats_set,
		values, RTE_DIM(values));
	if (ret < 0)
		return ret;

	return 0;
}
