/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"
#include "nt_util.h"

#include "hw_mod_backend.h"
#include "flm_age_queue.h"
#include "flm_evt_queue.h"
#include "flm_lrn_queue.h"
#include "flow_api.h"
#include "flow_api_engine.h"
#include "flow_api_hw_db_inline.h"
#include "flow_api_profile_inline_config.h"
#include "flow_id_table.h"
#include "rte_flow.h"
#include "stream_binary_flow_api.h"

#include "flow_api_profile_inline.h"
#include "ntnic_mod_reg.h"
#include "flow_hsh_cfg.h"
#include <rte_spinlock.h>
#include <rte_common.h>

#define FLM_MTR_PROFILE_SIZE 0x100000
#define FLM_MTR_STAT_SIZE 0x1000000
#define UINT64_MSB ((uint64_t)1 << 63)

#define DMA_BLOCK_SIZE 256
#define DMA_OVERHEAD 20
#define WORDS_PER_STA_DATA (sizeof(struct flm_v25_sta_data_s) / sizeof(uint32_t))
#define MAX_STA_DATA_RECORDS_PER_READ ((DMA_BLOCK_SIZE - DMA_OVERHEAD) / WORDS_PER_STA_DATA)
#define WORDS_PER_INF_DATA (sizeof(struct flm_v25_inf_data_s) / sizeof(uint32_t))
#define MAX_INF_DATA_RECORDS_PER_READ ((DMA_BLOCK_SIZE - DMA_OVERHEAD) / WORDS_PER_INF_DATA)

#define NT_FLM_MISS_FLOW_TYPE 0
#define NT_FLM_UNHANDLED_FLOW_TYPE 1
#define NT_FLM_OP_UNLEARN 0
#define NT_FLM_OP_LEARN 1
#define NT_FLM_OP_RELEARN 2

#define NT_FLM_VIOLATING_MBR_FLOW_TYPE 15
#define NT_VIOLATING_MBR_CFN 0
#define NT_VIOLATING_MBR_QSL 1

#define NT_FLM_OP_UNLEARN 0
#define NT_FLM_OP_LEARN 1

#define NT_FLM_MISS_FLOW_TYPE 0
#define NT_FLM_UNHANDLED_FLOW_TYPE 1
#define NT_FLM_VIOLATING_MBR_FLOW_TYPE 15

#define NT_VIOLATING_MBR_CFN 0
#define NT_VIOLATING_MBR_QSL 1

#define POLICING_PARAMETER_OFFSET 4096
#define SIZE_CONVERTER 1099.511627776

#define CELL_STATUS_UNINITIALIZED 0
#define CELL_STATUS_INITIALIZING 1
#define CELL_STATUS_INITIALIZED_TYPE_FLOW 2
#define CELL_STATUS_INITIALIZED_TYPE_FLM 3

struct flm_mtr_stat_s {
	struct dual_buckets_s *buckets;
	atomic_uint_fast64_t n_pkt;
	atomic_uint_fast64_t n_bytes;
	uint64_t n_pkt_base;
	uint64_t n_bytes_base;
	atomic_uint_fast64_t stats_mask;
	uint32_t flm_id;
};

struct flm_mtr_shared_stats_s {
	struct flm_mtr_stat_s *stats;
	uint32_t size;
	int shared;
};

struct flm_flow_mtr_handle_s {
	struct dual_buckets_s {
		uint16_t rate_a;
		uint16_t rate_b;
		uint16_t size_a;
		uint16_t size_b;
	} dual_buckets[FLM_MTR_PROFILE_SIZE];

	struct flm_mtr_shared_stats_s *port_stats[UINT8_MAX];
};

static void *flm_lrn_queue_arr;

static int flow_mtr_supported(struct flow_eth_dev *dev)
{
	return nthw_mod_flm_present(&dev->ndev->be) && dev->ndev->be.flm.nb_variant == 2;
}

static uint64_t flow_mtr_meter_policy_n_max(void)
{
	return FLM_MTR_PROFILE_SIZE;
}

static inline uint64_t convert_policing_parameter(uint64_t value)
{
	uint64_t limit = POLICING_PARAMETER_OFFSET;
	uint64_t shift = 0;
	uint64_t res = value;

	while (shift < 15 && value >= limit) {
		limit <<= 1;
		++shift;
	}

	if (shift != 0) {
		uint64_t tmp = POLICING_PARAMETER_OFFSET * (1ULL << (shift - 1ULL));

		if (tmp > value) {
			res = 0;

		} else {
			tmp = value - tmp;
			res = tmp >> (shift - 1);
		}

		if (res >= POLICING_PARAMETER_OFFSET)
			res = POLICING_PARAMETER_OFFSET - 1;

		res = res | (shift << 12);
	}

	return res;
}

static int flow_mtr_set_profile(struct flow_eth_dev *dev, uint32_t profile_id,
	uint64_t bucket_rate_a, uint64_t bucket_size_a, uint64_t bucket_rate_b,
	uint64_t bucket_size_b)
{
	struct flow_nic_dev *ndev = dev->ndev;
	struct flm_flow_mtr_handle_s *handle =
		(struct flm_flow_mtr_handle_s *)ndev->flm_mtr_handle;
	struct dual_buckets_s *buckets = &handle->dual_buckets[profile_id];

	/* Round rates up to nearest 128 bytes/sec and shift to 128 bytes/sec units */
	bucket_rate_a = (bucket_rate_a + 127) >> 7;
	bucket_rate_b = (bucket_rate_b + 127) >> 7;

	buckets->rate_a = convert_policing_parameter(bucket_rate_a);
	buckets->rate_b = convert_policing_parameter(bucket_rate_b);

	/* Round size down to 38-bit int */
	if (bucket_size_a > 0x3fffffffff)
		bucket_size_a = 0x3fffffffff;

	if (bucket_size_b > 0x3fffffffff)
		bucket_size_b = 0x3fffffffff;

	/* Convert size to units of 2^40 / 10^9. Output is a 28-bit int. */
	bucket_size_a = bucket_size_a / SIZE_CONVERTER;
	bucket_size_b = bucket_size_b / SIZE_CONVERTER;

	buckets->size_a = convert_policing_parameter(bucket_size_a);
	buckets->size_b = convert_policing_parameter(bucket_size_b);

	return 0;
}

static int flow_mtr_set_policy(struct flow_eth_dev *dev, uint32_t policy_id, int drop)
{
	(void)dev;
	(void)policy_id;
	(void)drop;
	return 0;
}

static uint32_t flow_mtr_meters_supported(struct flow_eth_dev *dev, uint8_t caller_id)
{
	struct flm_flow_mtr_handle_s *handle = dev->ndev->flm_mtr_handle;
	return handle->port_stats[caller_id]->size;
}

static int flow_mtr_create_meter(struct flow_eth_dev *dev,
	uint8_t caller_id,
	uint32_t mtr_id,
	uint32_t profile_id,
	uint32_t policy_id,
	uint64_t stats_mask)
{
	(void)policy_id;
	struct flm_v25_lrn_data_s *learn_record = NULL;

	rte_spinlock_lock(&dev->ndev->mtx);

	learn_record =
		(struct flm_v25_lrn_data_s *)
			nthw_flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);

	while (learn_record == NULL) {
		nthw_os_wait_usec(1);
		learn_record =
			(struct flm_v25_lrn_data_s *)
				nthw_flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);
	}

	struct flm_flow_mtr_handle_s *handle = dev->ndev->flm_mtr_handle;

	struct dual_buckets_s *buckets = &handle->dual_buckets[profile_id];

	memset(learn_record, 0x0, sizeof(struct flm_v25_lrn_data_s));

	union flm_handles flm_h;
	flm_h.idx = mtr_id;
	uint32_t flm_id = ntnic_id_table_get_id(dev->ndev->id_table_handle, flm_h, caller_id, 2);

	learn_record->sw9 = flm_id;
	learn_record->kid = 1;

	learn_record->rate = buckets->rate_a;
	learn_record->size = buckets->size_a;
	learn_record->fill = buckets->size_a;

	learn_record->ft_mbr =
		NT_FLM_VIOLATING_MBR_FLOW_TYPE;	/* FT to assign if MBR has been exceeded */

	learn_record->ent = 1;
	learn_record->op = 1;
	learn_record->eor = 1;

	learn_record->id = flm_id;

	if (stats_mask)
		learn_record->vol_idx = 1;

	nthw_flm_lrn_queue_release_write_buffer(flm_lrn_queue_arr);

	struct flm_mtr_stat_s *mtr_stat = handle->port_stats[caller_id]->stats;
	mtr_stat[mtr_id].buckets = buckets;
	mtr_stat[mtr_id].flm_id = flm_id;
	atomic_store(&mtr_stat[mtr_id].stats_mask, stats_mask);

	rte_spinlock_unlock(&dev->ndev->mtx);

	return 0;
}

static int flow_mtr_probe_meter(struct flow_eth_dev *dev, uint8_t caller_id, uint32_t mtr_id)
{
	struct flm_v25_lrn_data_s *learn_record = NULL;

	rte_spinlock_lock(&dev->ndev->mtx);

	learn_record =
		(struct flm_v25_lrn_data_s *)
			nthw_flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);

	while (learn_record == NULL) {
		nthw_os_wait_usec(1);
		learn_record =
			(struct flm_v25_lrn_data_s *)
				nthw_flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);
	}

	struct flm_flow_mtr_handle_s *handle = dev->ndev->flm_mtr_handle;

	struct flm_mtr_stat_s *mtr_stat = handle->port_stats[caller_id]->stats;
	uint32_t flm_id = mtr_stat[mtr_id].flm_id;

	memset(learn_record, 0x0, sizeof(struct flm_v25_lrn_data_s));

	learn_record->sw9 = flm_id;
	learn_record->kid = 1;

	learn_record->ent = 1;
	learn_record->op = 3;
	learn_record->eor = 1;

	learn_record->id = flm_id;

	nthw_flm_lrn_queue_release_write_buffer(flm_lrn_queue_arr);

	rte_spinlock_unlock(&dev->ndev->mtx);

	return 0;
}

static int flow_mtr_destroy_meter(struct flow_eth_dev *dev, uint8_t caller_id, uint32_t mtr_id)
{
	struct flm_v25_lrn_data_s *learn_record = NULL;

	rte_spinlock_lock(&dev->ndev->mtx);

	learn_record =
		(struct flm_v25_lrn_data_s *)
			nthw_flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);

	while (learn_record == NULL) {
		nthw_os_wait_usec(1);
		learn_record =
			(struct flm_v25_lrn_data_s *)
				nthw_flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);
	}

	struct flm_flow_mtr_handle_s *handle = dev->ndev->flm_mtr_handle;

	struct flm_mtr_stat_s *mtr_stat = handle->port_stats[caller_id]->stats;
	uint32_t flm_id = mtr_stat[mtr_id].flm_id;

	memset(learn_record, 0x0, sizeof(struct flm_v25_lrn_data_s));

	learn_record->sw9 = flm_id;
	learn_record->kid = 1;

	learn_record->ent = 1;
	learn_record->op = 0;
	/* Suppress generation of statistics INF_DATA */
	learn_record->nofi = 1;
	learn_record->eor = 1;

	learn_record->id = flm_id;

	/* Clear statistics so stats_mask prevents updates of counters on deleted meters */
	atomic_store(&mtr_stat[mtr_id].stats_mask, 0);
	atomic_store(&mtr_stat[mtr_id].n_bytes, 0);
	atomic_store(&mtr_stat[mtr_id].n_pkt, 0);
	mtr_stat[mtr_id].n_bytes_base = 0;
	mtr_stat[mtr_id].n_pkt_base = 0;
	mtr_stat[mtr_id].buckets = NULL;

	ntnic_id_table_free_id(dev->ndev->id_table_handle, flm_id);

	nthw_flm_lrn_queue_release_write_buffer(flm_lrn_queue_arr);

	rte_spinlock_unlock(&dev->ndev->mtx);

	return 0;
}

static int flm_mtr_adjust_stats(struct flow_eth_dev *dev, uint8_t caller_id, uint32_t mtr_id,
	uint32_t adjust_value)
{
	struct flm_v25_lrn_data_s *learn_record = NULL;

	rte_spinlock_lock(&dev->ndev->mtx);

	learn_record =
		(struct flm_v25_lrn_data_s *)
			nthw_flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);

	while (learn_record == NULL) {
		nthw_os_wait_usec(1);
		learn_record =
			(struct flm_v25_lrn_data_s *)
				nthw_flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);
	}

	struct flm_flow_mtr_handle_s *handle = dev->ndev->flm_mtr_handle;

	struct flm_mtr_stat_s *mtr_stat = &handle->port_stats[caller_id]->stats[mtr_id];

	memset(learn_record, 0x0, sizeof(struct flm_v25_lrn_data_s));

	learn_record->sw9 = mtr_stat->flm_id;
	learn_record->kid = 1;

	learn_record->rate = mtr_stat->buckets->rate_a;
	learn_record->size = mtr_stat->buckets->size_a;
	learn_record->adj = adjust_value;

	learn_record->ft_mbr = NT_FLM_VIOLATING_MBR_FLOW_TYPE;

	learn_record->ent = 1;
	learn_record->op = 2;
	learn_record->eor = 1;

	if (atomic_load(&mtr_stat->stats_mask))
		learn_record->vol_idx = 1;

	nthw_flm_lrn_queue_release_write_buffer(flm_lrn_queue_arr);

	rte_spinlock_unlock(&dev->ndev->mtx);

	return 0;
}

static void flm_setup_queues(void)
{
	flm_lrn_queue_arr = nthw_flm_lrn_queue_create();
	RTE_ASSERT(flm_lrn_queue_arr != NULL);
}

static void flm_free_queues(void)
{
	nthw_flm_lrn_queue_free(flm_lrn_queue_arr);
}

static uint32_t flm_lrn_update(struct flow_eth_dev *dev, uint32_t *inf_word_cnt,
	uint32_t *sta_word_cnt)
{
	read_record r = nthw_flm_lrn_queue_get_read_buffer(flm_lrn_queue_arr);
	uint32_t handled_records = 0;

	if (r.num) {
		if (nthw_mod_flm_lrn_data_set_flush(&dev->ndev->be, HW_FLM_FLOW_LRN_DATA, r.p,
			r.num, &handled_records, inf_word_cnt, sta_word_cnt))
			NT_LOG(ERR, FILTER, "Flow programming failed");
	}
	nthw_flm_lrn_queue_release_read_buffer(flm_lrn_queue_arr, handled_records);

	return r.num;
}

static inline bool is_remote_caller(uint8_t caller_id, uint8_t *port)
{
	if (caller_id < MAX_VDPA_PORTS + 1) {
		*port = caller_id;
		return true;
	}

	*port = caller_id - MAX_VDPA_PORTS - 1;
	return false;
}

static void flm_mtr_read_inf_records(struct flow_eth_dev *dev, uint32_t *data, uint32_t records)
{
	struct flm_flow_mtr_handle_s *handle = dev->ndev->flm_mtr_handle;

	for (uint32_t i = 0; i < records; ++i) {
		struct flm_v25_inf_data_s *inf_data =
			(struct flm_v25_inf_data_s *)&data[i * WORDS_PER_INF_DATA];
		uint8_t caller_id;
		uint8_t type;
		union flm_handles flm_h;
		ntnic_id_table_find(dev->ndev->id_table_handle, inf_data->id, &flm_h, &caller_id,
			&type);

		/* Check that received record hold valid meter statistics */
		if (type == 2) {
			uint64_t mtr_id = flm_h.idx;

			if (mtr_id < handle->port_stats[caller_id]->size) {
				struct flm_mtr_stat_s *mtr_stat =
						handle->port_stats[caller_id]->stats;

				/* Don't update a deleted meter */
				uint64_t stats_mask = atomic_load(&mtr_stat[mtr_id].stats_mask);

				if (stats_mask) {
					atomic_store(&mtr_stat[mtr_id].n_pkt,
						inf_data->packets | UINT64_MSB);
					atomic_store(&mtr_stat[mtr_id].n_bytes, inf_data->bytes);
					atomic_store(&mtr_stat[mtr_id].n_pkt, inf_data->packets);
					struct flm_info_event_s stat_data;
					bool remote_caller;
					uint8_t port;

					remote_caller = is_remote_caller(caller_id, &port);

					/* Save stat data to flm stat queue */
					stat_data.bytes = inf_data->bytes;
					stat_data.packets = inf_data->packets;
					stat_data.id = mtr_id;
					stat_data.timestamp = inf_data->ts;
					stat_data.cause = inf_data->cause;
					nthw_flm_inf_queue_put(port, remote_caller, &stat_data);
				}
			}

			/* Check that received record hold valid flow data */

		} else if (type == 1) {
			switch (inf_data->cause) {
			case INF_DATA_CAUSE_TIMEOUT_FLOW_DELETED:
			case INF_DATA_CAUSE_TIMEOUT_FLOW_KEPT: {
				struct flow_handle *fh = (struct flow_handle *)flm_h.p;
				struct flm_age_event_s age_event;
				uint8_t port;
				bool is_remote;

				age_event.context = fh->context;

				is_remote = is_remote_caller(caller_id, &port);

				nthw_flm_age_queue_put(caller_id, &age_event);
				/* age events are supported only for physical ports */
				if (!is_remote)
					nthw_flm_age_event_set(port);
			}
			break;

			case INF_DATA_CAUSE_SW_UNLEARN:
			case INF_DATA_CAUSE_NA:
			case INF_DATA_CAUSE_PERIODIC_FLOW_INFO:
			case INF_DATA_CAUSE_SW_PROBE:
			default:
				break;
			}
		}
	}
}

static void flm_mtr_read_sta_records(struct flow_eth_dev *dev, uint32_t *data, uint32_t records)
{
	for (uint32_t i = 0; i < records; ++i) {
		struct flm_v25_sta_data_s *sta_data =
			(struct flm_v25_sta_data_s *)&data[i * WORDS_PER_STA_DATA];
		uint8_t caller_id;
		uint8_t type;
		union flm_handles flm_h;
		ntnic_id_table_find(dev->ndev->id_table_handle, sta_data->id, &flm_h, &caller_id,
			&type);

		if (type == 1) {
			uint8_t port;
			bool remote_caller = is_remote_caller(caller_id, &port);

			rte_atomic_store_explicit(&((struct flow_handle *)flm_h.p)->learn_ignored,
				1, rte_memory_order_seq_cst);
			struct flm_status_event_s data = {
				.flow = flm_h.p,
				.learn_ignore = sta_data->lis,
				.learn_failed = sta_data->lfs,
			};

			nthw_flm_sta_queue_put(port, remote_caller, &data);
		}
	}
}

static uint32_t flm_update(struct flow_eth_dev *dev)
{
	static uint32_t inf_word_cnt;
	static uint32_t sta_word_cnt;

	uint32_t inf_data[DMA_BLOCK_SIZE];
	uint32_t sta_data[DMA_BLOCK_SIZE];

	if (inf_word_cnt >= WORDS_PER_INF_DATA || sta_word_cnt >= WORDS_PER_STA_DATA) {
		uint32_t inf_records = inf_word_cnt / WORDS_PER_INF_DATA;

		if (inf_records > MAX_INF_DATA_RECORDS_PER_READ)
			inf_records = MAX_INF_DATA_RECORDS_PER_READ;

		uint32_t sta_records = sta_word_cnt / WORDS_PER_STA_DATA;

		if (sta_records > MAX_STA_DATA_RECORDS_PER_READ)
			sta_records = MAX_STA_DATA_RECORDS_PER_READ;

		nthw_mod_flm_inf_sta_data_update_get(&dev->ndev->be, HW_FLM_FLOW_INF_STA_DATA,
			inf_data, inf_records * WORDS_PER_INF_DATA,
			&inf_word_cnt, sta_data,
			sta_records * WORDS_PER_STA_DATA,
			&sta_word_cnt);

		if (inf_records > 0)
			flm_mtr_read_inf_records(dev, inf_data, inf_records);

		if (sta_records > 0)
			flm_mtr_read_sta_records(dev, sta_data, sta_records);

		return 1;
	}

	if (flm_lrn_update(dev, &inf_word_cnt, &sta_word_cnt) != 0)
		return 1;

	nthw_mod_flm_buf_ctrl_update(&dev->ndev->be);
	nthw_mod_flm_buf_ctrl_get(&dev->ndev->be, HW_FLM_BUF_CTRL_INF_AVAIL, &inf_word_cnt);
	nthw_mod_flm_buf_ctrl_get(&dev->ndev->be, HW_FLM_BUF_CTRL_STA_AVAIL, &sta_word_cnt);

	return inf_word_cnt + sta_word_cnt;
}

static void flm_mtr_read_stats(struct flow_eth_dev *dev,
	uint8_t caller_id,
	uint32_t id,
	uint64_t *stats_mask,
	uint64_t *green_pkt,
	uint64_t *green_bytes,
	int clear)
{
	struct flm_flow_mtr_handle_s *handle = dev->ndev->flm_mtr_handle;
	struct flm_mtr_stat_s *mtr_stat = handle->port_stats[caller_id]->stats;
	*stats_mask = atomic_load(&mtr_stat[id].stats_mask);

	if (*stats_mask) {
		uint64_t pkt_1;
		uint64_t pkt_2;
		uint64_t nb;

		do {
			do {
				pkt_1 = atomic_load(&mtr_stat[id].n_pkt);
			} while (pkt_1 & UINT64_MSB);

			nb = atomic_load(&mtr_stat[id].n_bytes);
			pkt_2 = atomic_load(&mtr_stat[id].n_pkt);
		} while (pkt_1 != pkt_2);

		*green_pkt = pkt_1 - mtr_stat[id].n_pkt_base;
		*green_bytes = nb - mtr_stat[id].n_bytes_base;

		if (clear) {
			mtr_stat[id].n_pkt_base = pkt_1;
			mtr_stat[id].n_bytes_base = nb;
		}
	}
}

static int rx_queue_idx_to_hw_id(const struct flow_eth_dev *dev, int id)
{
	for (int i = 0; i < dev->num_queues; ++i)
		if (dev->rx_queue[i].id == id)
			return dev->rx_queue[i].hw_id;

	return -1;
}

/*
 * Flow Matcher functionality
 */

static int flm_sdram_calibrate(struct flow_nic_dev *ndev)
{
	int success = 0;
	uint32_t fail_value = 0;
	uint32_t value = 0;

	nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_PRESET_ALL, 0x0);
	nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_SPLIT_SDRAM_USAGE, 0x10);
	nthw_mod_flm_control_flush(&ndev->be);

	/* Wait for ddr4 calibration/init done */
	for (uint32_t i = 0; i < 1000000; ++i) {
		nthw_mod_flm_status_update(&ndev->be);
		nthw_mod_flm_status_get(&ndev->be, HW_FLM_STATUS_CALIB_SUCCESS, &value);
		nthw_mod_flm_status_get(&ndev->be, HW_FLM_STATUS_CALIB_FAIL, &fail_value);

		if (value & 0x80000000) {
			success = 1;
			break;
		}

		if (fail_value != 0)
			break;

		nthw_os_wait_usec(1);
	}

	if (!success) {
		NT_LOG(ERR, FILTER, "FLM initialization failed - SDRAM calibration failed");
		NT_LOG(ERR, FILTER,
			"Calibration status: success 0x%08" PRIx32 " - fail 0x%08" PRIx32,
			value, fail_value);
		return -1;
	}

	return 0;
}

static int flm_sdram_reset(struct flow_nic_dev *ndev, int enable)
{
	int success = 0;

	/*
	 * Make sure no lookup is performed during init, i.e.
	 * disable every category and disable FLM
	 */
	nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_ENABLE, 0x0);
	nthw_mod_flm_control_flush(&ndev->be);

	for (uint32_t i = 1; i < ndev->be.flm.nb_categories; ++i)
		nthw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_PRESET_ALL, i, 0x0);

	nthw_mod_flm_rcp_flush(&ndev->be, 1, ndev->be.flm.nb_categories - 1);

	/* Wait for FLM to enter Idle state */
	for (uint32_t i = 0; i < 1000000; ++i) {
		uint32_t value = 0;
		nthw_mod_flm_status_update(&ndev->be);
		nthw_mod_flm_status_get(&ndev->be, HW_FLM_STATUS_IDLE, &value);

		if (value) {
			success = 1;
			break;
		}

		nthw_os_wait_usec(1);
	}

	if (!success) {
		NT_LOG(ERR, FILTER, "FLM initialization failed - Never idle");
		return -1;
	}

	success = 0;

	/* Start SDRAM initialization */
	nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_INIT, 0x1);
	nthw_mod_flm_control_flush(&ndev->be);

	for (uint32_t i = 0; i < 1000000; ++i) {
		uint32_t value = 0;
		nthw_mod_flm_status_update(&ndev->be);
		nthw_mod_flm_status_get(&ndev->be, HW_FLM_STATUS_INITDONE, &value);

		if (value) {
			success = 1;
			break;
		}

		nthw_os_wait_usec(1);
	}

	if (!success) {
		NT_LOG(ERR, FILTER,
			"FLM initialization failed - SDRAM initialization incomplete");
		return -1;
	}

	/* Set the INIT value back to zero to clear the bit in the SW register cache */
	nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_INIT, 0x0);
	nthw_mod_flm_control_flush(&ndev->be);

	/* Enable FLM */
	nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_ENABLE, enable);
	nthw_mod_flm_control_flush(&ndev->be);

	int nb_rpp_per_ps = ndev->be.flm.nb_rpp_clock_in_ps;
	int nb_load_aps_max = ndev->be.flm.nb_load_aps_max;
	uint32_t scan_i_value = 0;

	if (NTNIC_SCANNER_LOAD > 0) {
		scan_i_value = (1 / (nb_rpp_per_ps * 0.000000000001)) /
			(nb_load_aps_max * NTNIC_SCANNER_LOAD);
	}

	nthw_mod_flm_scan_set(&ndev->be, HW_FLM_SCAN_I, scan_i_value);
	nthw_mod_flm_scan_flush(&ndev->be);

	return 0;
}



struct flm_flow_key_def_s {
	union {
		struct {
			uint64_t qw0_dyn : 7;
			uint64_t qw0_ofs : 8;
			uint64_t qw4_dyn : 7;
			uint64_t qw4_ofs : 8;
			uint64_t sw8_dyn : 7;
			uint64_t sw8_ofs : 8;
			uint64_t sw9_dyn : 7;
			uint64_t sw9_ofs : 8;
			uint64_t outer_proto : 1;
			uint64_t inner_proto : 1;
			uint64_t pad : 2;
		};
		uint64_t data;
	};
	uint32_t mask[10];
};

/*
 * Flow Matcher functionality
 */
static inline void set_key_def_qw(struct flm_flow_key_def_s *key_def, unsigned int qw,
	unsigned int dyn, unsigned int ofs)
{
	RTE_ASSERT(qw < 2);

	if (qw == 0) {
		key_def->qw0_dyn = dyn & 0x7f;
		key_def->qw0_ofs = ofs & 0xff;

	} else {
		key_def->qw4_dyn = dyn & 0x7f;
		key_def->qw4_ofs = ofs & 0xff;
	}
}

static inline void set_key_def_sw(struct flm_flow_key_def_s *key_def, unsigned int sw,
	unsigned int dyn, unsigned int ofs)
{
	RTE_ASSERT(sw < 2);

	if (sw == 0) {
		key_def->sw8_dyn = dyn & 0x7f;
		key_def->sw8_ofs = ofs & 0xff;

	} else {
		key_def->sw9_dyn = dyn & 0x7f;
		key_def->sw9_ofs = ofs & 0xff;
	}
}

static inline uint8_t convert_port_to_ifr_mtu_recipe(uint32_t port)
{
	return port + 1;
}

static uint8_t get_port_from_port_id(const struct flow_nic_dev *ndev, uint32_t port_id)
{
	struct flow_eth_dev *dev = ndev->eth_base;

	while (dev) {
		if (dev->port_id == port_id)
			return dev->port;

		dev = dev->next;
	}

	return UINT8_MAX;
}

static void nic_insert_flow(struct flow_nic_dev *ndev, struct flow_handle *fh)
{
	rte_spinlock_lock(&ndev->flow_mtx);

	if (ndev->flow_base)
		ndev->flow_base->prev = fh;

	fh->next = ndev->flow_base;
	fh->prev = NULL;
	ndev->flow_base = fh;

	rte_spinlock_unlock(&ndev->flow_mtx);
}

static void nic_remove_flow(struct flow_nic_dev *ndev, struct flow_handle *fh)
{
	struct flow_handle *next = fh->next;
	struct flow_handle *prev = fh->prev;

	rte_spinlock_lock(&ndev->flow_mtx);

	if (next && prev) {
		prev->next = next;
		next->prev = prev;

	} else if (next) {
		ndev->flow_base = next;
		next->prev = NULL;

	} else if (prev) {
		prev->next = NULL;

	} else if (ndev->flow_base == fh) {
		ndev->flow_base = NULL;
	}

	rte_spinlock_unlock(&ndev->flow_mtx);
}

static void nic_insert_flow_flm(struct flow_nic_dev *ndev, struct flow_handle *fh)
{
	rte_spinlock_lock(&ndev->flow_mtx);

	if (ndev->flow_base_flm)
		ndev->flow_base_flm->prev = fh;

	fh->next = ndev->flow_base_flm;
	fh->prev = NULL;
	ndev->flow_base_flm = fh;

	rte_spinlock_unlock(&ndev->flow_mtx);
}

static void nic_remove_flow_flm(struct flow_nic_dev *ndev, struct flow_handle *fh_flm)
{
	struct flow_handle *next = fh_flm->next;
	struct flow_handle *prev = fh_flm->prev;

	rte_spinlock_lock(&ndev->flow_mtx);

	if (next && prev) {
		prev->next = next;
		next->prev = prev;

	} else if (next) {
		ndev->flow_base_flm = next;
		next->prev = NULL;

	} else if (prev) {
		prev->next = NULL;

	} else if (ndev->flow_base_flm == fh_flm) {
		ndev->flow_base_flm = NULL;
	}

	rte_spinlock_unlock(&ndev->flow_mtx);
}

static inline struct nic_flow_def *prepare_nic_flow_def(struct nic_flow_def *fd)
{
	if (fd) {
		fd->full_offload = -1;
		fd->in_port_override = -1;
		fd->mark = UINT32_MAX;
		fd->jump_to_group = UINT32_MAX;

		memset(fd->mtr_ids, 0xff, sizeof(uint32_t) * MAX_FLM_MTRS_SUPPORTED);

		fd->l2_prot = -1;
		fd->l3_prot = -1;
		fd->l4_prot = -1;
		fd->vlans = 0;
		fd->tunnel_prot = -1;
		fd->tunnel_l3_prot = -1;
		fd->tunnel_l4_prot = -1;
		fd->fragmentation = -1;
		fd->ip_prot = -1;
		fd->tunnel_ip_prot = -1;

		fd->non_empty = -1;
	}

	return fd;
}

static inline struct nic_flow_def *allocate_nic_flow_def(void)
{
	return prepare_nic_flow_def(calloc(1, sizeof(struct nic_flow_def)));
}

static bool fd_has_empty_pattern(const struct nic_flow_def *fd)
{
	return fd && fd->vlans == 0 && fd->l2_prot < 0 && fd->l3_prot < 0 && fd->l4_prot < 0 &&
		fd->tunnel_prot < 0 && fd->tunnel_l3_prot < 0 && fd->tunnel_l4_prot < 0 &&
		fd->ip_prot < 0 && fd->tunnel_ip_prot < 0 && fd->non_empty < 0;
}

static inline const void *memcpy_mask_if(void *dest, const void *src, const void *mask,
	size_t count)
{
	if (mask == NULL)
		return src;

	unsigned char *dest_ptr = (unsigned char *)dest;
	const unsigned char *src_ptr = (const unsigned char *)src;
	const unsigned char *mask_ptr = (const unsigned char *)mask;

	for (size_t i = 0; i < count; ++i)
		dest_ptr[i] = src_ptr[i] & mask_ptr[i];

	return dest;
}

static int flm_flow_programming(struct flow_handle *fh, uint32_t flm_op)
{
	struct flm_v25_lrn_data_s *learn_record = NULL;

	if (fh->type != FLOW_HANDLE_TYPE_FLM)
		return -1;

	if (flm_op == NT_FLM_OP_LEARN) {
		union flm_handles flm_h;
		flm_h.p = fh;
		fh->flm_id = ntnic_id_table_get_id(fh->dev->ndev->id_table_handle, flm_h,
			fh->caller_id, 1);
	}

	uint32_t flm_id = fh->flm_id;

	if (flm_op == NT_FLM_OP_UNLEARN) {
		ntnic_id_table_free_id(fh->dev->ndev->id_table_handle, flm_id);

		if (rte_atomic_load_explicit(&fh->learn_ignored, rte_memory_order_seq_cst) == 1)
			return 0;
	}

	learn_record =
		(struct flm_v25_lrn_data_s *)
			nthw_flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);

	while (learn_record == NULL) {
		nthw_os_wait_usec(1);
		learn_record =
			(struct flm_v25_lrn_data_s *)
			nthw_flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);
	}

	memset(learn_record, 0x0, sizeof(struct flm_v25_lrn_data_s));

	learn_record->id = flm_id;

	learn_record->qw0[0] = fh->flm_data[9];
	learn_record->qw0[1] = fh->flm_data[8];
	learn_record->qw0[2] = fh->flm_data[7];
	learn_record->qw0[3] = fh->flm_data[6];
	learn_record->qw4[0] = fh->flm_data[5];
	learn_record->qw4[1] = fh->flm_data[4];
	learn_record->qw4[2] = fh->flm_data[3];
	learn_record->qw4[3] = fh->flm_data[2];
	learn_record->sw8 = fh->flm_data[1];
	learn_record->sw9 = fh->flm_data[0];
	learn_record->prot = fh->flm_prot;

	learn_record->mbr_idx1 = fh->flm_mtr_ids[0];
	learn_record->mbr_idx2 = fh->flm_mtr_ids[1];
	learn_record->mbr_idx3 = fh->flm_mtr_ids[2];
	learn_record->mbr_idx4 = fh->flm_mtr_ids[3];

	/* Last non-zero mtr is used for statistics */
	uint8_t mbrs = 0;

	while (mbrs < MAX_FLM_MTRS_SUPPORTED && fh->flm_mtr_ids[mbrs] != 0)
		++mbrs;

	learn_record->vol_idx = mbrs;

	learn_record->nat_ip = fh->flm_nat_ipv4;
	learn_record->nat_port = fh->flm_nat_port;
	learn_record->nat_en = fh->flm_nat_ipv4 || fh->flm_nat_port ? 1 : 0;

	learn_record->dscp = fh->flm_dscp;
	learn_record->teid = fh->flm_teid;
	learn_record->qfi = fh->flm_qfi;
	learn_record->rqi = fh->flm_rqi;
	/* Lower 10 bits used for RPL EXT PTR */
	learn_record->color = fh->flm_rpl_ext_ptr & 0x3ff;
	/* Bit [13:10] used for MTU recipe */
	learn_record->color |= (fh->flm_mtu_fragmentation_recipe & 0xf) << 10;

	learn_record->ent = 0;
	learn_record->op = flm_op & 0xf;
	/* Suppress generation of statistics INF_DATA */
	learn_record->nofi = 1;
	learn_record->prio = fh->flm_prio & 0x3;
	learn_record->ft = fh->flm_ft;
	learn_record->kid = fh->flm_kid;
	learn_record->eor = 1;
	learn_record->scrub_prof = fh->flm_scrub_prof;

	nthw_flm_lrn_queue_release_write_buffer(flm_lrn_queue_arr);
	return 0;
}

static inline const void *memcpy_or(void *dest, const void *src, size_t count)
{
	unsigned char *dest_ptr = (unsigned char *)dest;
	const unsigned char *src_ptr = (const unsigned char *)src;

	for (size_t i = 0; i < count; ++i)
		dest_ptr[i] |= src_ptr[i];

	return dest;
}

/*
 * This function must be callable without locking any mutexes
 */
static int interpret_flow_actions(const struct flow_eth_dev *dev,
	const struct rte_flow_action action[],
	const struct rte_flow_action *action_mask,
	struct nic_flow_def *fd,
	struct rte_flow_error *error,
	uint32_t *num_dest_port,
	uint32_t *num_queues)
{
	int mtr_count = 0;

	unsigned int encap_decap_order = 0;

	uint64_t modify_field_use_flags = 0x0;

	*num_dest_port = 0;
	*num_queues = 0;

	if (action == NULL) {
		nthw_flow_nic_set_error(ERR_FAILED, error);
		NT_LOG(ERR, FILTER, "Flow actions missing");
		return -1;
	}

	/*
	 * Gather flow match + actions and convert into internal flow definition structure (struct
	 * nic_flow_def_s) This is the 1st step in the flow creation - validate, convert and
	 * prepare
	 */
	for (int aidx = 0; action[aidx].type != RTE_FLOW_ACTION_TYPE_END; ++aidx) {
		switch (action[aidx].type) {
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_PORT_ID", dev);

			if (action[aidx].conf) {
				struct rte_flow_action_port_id port_id_tmp;
				const struct rte_flow_action_port_id *port_id =
					memcpy_mask_if(&port_id_tmp, action[aidx].conf,
					action_mask ? action_mask[aidx].conf : NULL,
					sizeof(struct rte_flow_action_port_id));

				if (*num_dest_port > 0) {
					NT_LOG(ERR, FILTER,
						"Multiple port_id actions for one flow is not supported");
					nthw_flow_nic_set_error(
						ERR_ACTION_MULTIPLE_PORT_ID_UNSUPPORTED,
						error);
					return -1;
				}

				uint8_t port = get_port_from_port_id(dev->ndev, port_id->id);

				if (fd->dst_num_avail == MAX_OUTPUT_DEST) {
					NT_LOG(ERR, FILTER, "Too many output destinations");
					nthw_flow_nic_set_error(ERR_OUTPUT_TOO_MANY, error);
					return -1;
				}

				if (port >= dev->ndev->be.num_phy_ports) {
					NT_LOG(ERR, FILTER, "Phy port out of range");
					nthw_flow_nic_set_error(ERR_OUTPUT_INVALID, error);
					return -1;
				}

				/* New destination port to add */
				fd->dst_id[fd->dst_num_avail].owning_port_id = port_id->id;
				fd->dst_id[fd->dst_num_avail].type = PORT_PHY;
				fd->dst_id[fd->dst_num_avail].id = (int)port;
				fd->dst_id[fd->dst_num_avail].active = 1;
				fd->dst_num_avail++;

				fd->flm_mtu_fragmentation_recipe =
					convert_port_to_ifr_mtu_recipe(port);

				if (fd->full_offload < 0)
					fd->full_offload = 1;

				*num_dest_port += 1;

				NT_LOG(DBG, FILTER, "Phy port ID: %i", (int)port);
			}

			break;

		case RTE_FLOW_ACTION_TYPE_QUEUE:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_QUEUE", dev);

			if (action[aidx].conf) {
				struct rte_flow_action_queue queue_tmp;
				const struct rte_flow_action_queue *queue =
					memcpy_mask_if(&queue_tmp, action[aidx].conf,
					action_mask ? action_mask[aidx].conf : NULL,
					sizeof(struct rte_flow_action_queue));

				int hw_id = rx_queue_idx_to_hw_id(dev, queue->index);

				fd->dst_id[fd->dst_num_avail].owning_port_id = dev->port;
				fd->dst_id[fd->dst_num_avail].id = hw_id;
				fd->dst_id[fd->dst_num_avail].type = PORT_VIRT;
				fd->dst_id[fd->dst_num_avail].active = 1;
				fd->dst_num_avail++;

				NT_LOG(DBG, FILTER,
					"Dev:%p: RTE_FLOW_ACTION_TYPE_QUEUE port %u, queue index:"
					"%" PRIu16 ",hw id %i",
					dev, (unsigned int)dev->port, queue->index, hw_id);

				fd->full_offload = 0;
				*num_queues += 1;
			}

			break;

		case RTE_FLOW_ACTION_TYPE_RSS:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_RSS", dev);

			if (action[aidx].conf) {
				struct rte_flow_action_rss rss_tmp;
				const struct rte_flow_action_rss *rss =
					memcpy_mask_if(&rss_tmp, action[aidx].conf,
					action_mask ? action_mask[aidx].conf : NULL,
					sizeof(struct rte_flow_action_rss));

				if (rss->key_len > MAX_RSS_KEY_LEN) {
					NT_LOG(ERR, FILTER,
						"ERROR: RSS hash key length %u exceeds maximum value %u",
						rss->key_len, MAX_RSS_KEY_LEN);
					nthw_flow_nic_set_error(ERR_RSS_TOO_LONG_KEY, error);
					return -1;
				}

				for (uint32_t i = 0; i < rss->queue_num; ++i) {
					int hw_id = rx_queue_idx_to_hw_id(dev, rss->queue[i]);

					fd->dst_id[fd->dst_num_avail].owning_port_id = dev->port;
					fd->dst_id[fd->dst_num_avail].id = hw_id;
					fd->dst_id[fd->dst_num_avail].type = PORT_VIRT;
					fd->dst_id[fd->dst_num_avail].active = 1;
					fd->dst_num_avail++;
				}

				fd->hsh.func = rss->func;
				fd->hsh.types = rss->types;
				fd->hsh.key = rss->key;
				fd->hsh.key_len = rss->key_len;

				NT_LOG(DBG, FILTER,
					"Dev:%p: RSS func: %i, types: 0x%" PRIX64
					", key_len: %" PRIu32 "",
					dev, (int)rss->func, rss->types, rss->key_len);

				fd->full_offload = 0;
				*num_queues += rss->queue_num;
			}

			break;

		case RTE_FLOW_ACTION_TYPE_MARK:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_MARK", dev);

			if (action[aidx].conf) {
				struct rte_flow_action_mark mark_tmp;
				const struct rte_flow_action_mark *mark =
					memcpy_mask_if(&mark_tmp, action[aidx].conf,
					action_mask ? action_mask[aidx].conf : NULL,
					sizeof(struct rte_flow_action_mark));

				fd->mark = mark->id;
				NT_LOG(DBG, FILTER, "Mark: %u", mark->id);
			}

			break;

		case RTE_FLOW_ACTION_TYPE_JUMP:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_JUMP", dev);

			if (action[aidx].conf) {
				struct rte_flow_action_jump jump_tmp;
				const struct rte_flow_action_jump *jump =
					memcpy_mask_if(&jump_tmp, action[aidx].conf,
					action_mask ? action_mask[aidx].conf : NULL,
					sizeof(struct rte_flow_action_jump));

				fd->jump_to_group = jump->group;
				NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_JUMP: group %u",
					dev, jump->group);
			}

			break;

		case RTE_FLOW_ACTION_TYPE_DROP:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_DROP", dev);

			if (action[aidx].conf) {
				fd->dst_id[fd->dst_num_avail].owning_port_id = 0;
				fd->dst_id[fd->dst_num_avail].id = 0;
				fd->dst_id[fd->dst_num_avail].type = PORT_NONE;
				fd->dst_num_avail++;
			}

			break;

		case RTE_FLOW_ACTION_TYPE_METER:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_METER", dev);

			if (action[aidx].conf) {
				struct rte_flow_action_meter meter_tmp;
				const struct rte_flow_action_meter *meter =
					memcpy_mask_if(&meter_tmp, action[aidx].conf,
					action_mask ? action_mask[aidx].conf : NULL,
					sizeof(struct rte_flow_action_meter));

				if (mtr_count >= MAX_FLM_MTRS_SUPPORTED) {
					NT_LOG(ERR, FILTER,
						"ERROR: - Number of METER actions exceeds %d.",
						MAX_FLM_MTRS_SUPPORTED);
					nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED, error);
					return -1;
				}

				fd->mtr_ids[mtr_count++] = meter->mtr_id;
			}

			break;

		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_RAW_ENCAP", dev);

			if (action[aidx].conf) {
				const struct flow_action_raw_encap *encap =
					(const struct flow_action_raw_encap *)action[aidx].conf;
				const struct flow_action_raw_encap *encap_mask = action_mask
					? (const struct flow_action_raw_encap *)action_mask[aidx]
					.conf
					: NULL;
				const struct rte_flow_item *items = encap->items;

				if (encap_decap_order != 1) {
					NT_LOG(ERR, FILTER,
						"ERROR: - RAW_ENCAP must follow RAW_DECAP.");
					nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED, error);
					return -1;
				}

				if (encap->size == 0 || encap->size > 255 ||
					encap->item_count < 2) {
					NT_LOG(ERR, FILTER,
						"ERROR: - RAW_ENCAP data/size invalid.");
					nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED, error);
					return -1;
				}

				encap_decap_order = 2;

				fd->tun_hdr.len = (uint8_t)encap->size;

				if (encap_mask) {
					memcpy_mask_if(fd->tun_hdr.d.hdr8, encap->data,
						encap_mask->data, fd->tun_hdr.len);

				} else {
					memcpy(fd->tun_hdr.d.hdr8, encap->data, fd->tun_hdr.len);
				}

				while (items->type != RTE_FLOW_ITEM_TYPE_END) {
					switch (items->type) {
					case RTE_FLOW_ITEM_TYPE_ETH:
						fd->tun_hdr.l2_len = 14;
						break;

					case RTE_FLOW_ITEM_TYPE_VLAN:
						fd->tun_hdr.nb_vlans += 1;
						fd->tun_hdr.l2_len += 4;
						break;

					case RTE_FLOW_ITEM_TYPE_IPV4:
						fd->tun_hdr.ip_version = 4;
						fd->tun_hdr.l3_len = sizeof(struct rte_ipv4_hdr);
						fd->tun_hdr.new_outer = 1;

						/* Patch length */
						fd->tun_hdr.d.hdr8[fd->tun_hdr.l2_len + 2] = 0x07;
						fd->tun_hdr.d.hdr8[fd->tun_hdr.l2_len + 3] = 0xfd;
						break;

					case RTE_FLOW_ITEM_TYPE_IPV6:
						fd->tun_hdr.ip_version = 6;
						fd->tun_hdr.l3_len = sizeof(struct rte_ipv6_hdr);
						fd->tun_hdr.new_outer = 1;

						/* Patch length */
						fd->tun_hdr.d.hdr8[fd->tun_hdr.l2_len + 4] = 0x07;
						fd->tun_hdr.d.hdr8[fd->tun_hdr.l2_len + 5] = 0xfd;
						break;

					case RTE_FLOW_ITEM_TYPE_SCTP:
						fd->tun_hdr.l4_len = sizeof(struct rte_sctp_hdr);
						break;

					case RTE_FLOW_ITEM_TYPE_TCP:
						fd->tun_hdr.l4_len = sizeof(struct rte_tcp_hdr);
						break;

					case RTE_FLOW_ITEM_TYPE_UDP:
						fd->tun_hdr.l4_len = sizeof(struct rte_udp_hdr);

						/* Patch length */
						fd->tun_hdr.d.hdr8[fd->tun_hdr.l2_len +
							fd->tun_hdr.l3_len + 4] = 0x07;
						fd->tun_hdr.d.hdr8[fd->tun_hdr.l2_len +
							fd->tun_hdr.l3_len + 5] = 0xfd;
						break;

					case RTE_FLOW_ITEM_TYPE_ICMP:
						fd->tun_hdr.l4_len = sizeof(struct rte_icmp_hdr);
						break;

					case RTE_FLOW_ITEM_TYPE_ICMP6:
						fd->tun_hdr.l4_len =
							sizeof(struct rte_flow_item_icmp6);
						break;

					case RTE_FLOW_ITEM_TYPE_GTP:
						/* Patch length */
						fd->tun_hdr.d.hdr8[fd->tun_hdr.l2_len +
							fd->tun_hdr.l3_len +
							fd->tun_hdr.l4_len + 2] = 0x07;
						fd->tun_hdr.d.hdr8[fd->tun_hdr.l2_len +
							fd->tun_hdr.l3_len +
							fd->tun_hdr.l4_len + 3] = 0xfd;
						break;

					default:
						break;
					}

					items++;
				}

				if (fd->tun_hdr.nb_vlans > 3) {
					NT_LOG(ERR, FILTER,
						"ERROR: - Encapsulation with %d vlans not supported.",
						(int)fd->tun_hdr.nb_vlans);
					nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED, error);
					return -1;
				}

				/* Convert encap data to 128-bit little endian */
				for (size_t i = 0; i < (encap->size + 15) / 16; ++i) {
					uint8_t *data = fd->tun_hdr.d.hdr8 + i * 16;

					for (unsigned int j = 0; j < 8; ++j) {
						uint8_t t = data[j];
						data[j] = data[15 - j];
						data[15 - j] = t;
					}
				}
			}

			break;

		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_RAW_DECAP", dev);

			if (action[aidx].conf) {
				/* Mask is N/A for RAW_DECAP */
				const struct flow_action_raw_decap *decap =
					(const struct flow_action_raw_decap *)action[aidx].conf;

				if (encap_decap_order != 0) {
					NT_LOG(ERR, FILTER,
						"ERROR: - RAW_ENCAP must follow RAW_DECAP.");
					nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED, error);
					return -1;
				}

				if (decap->item_count < 2) {
					NT_LOG(ERR, FILTER,
						"ERROR: - RAW_DECAP must decap something.");
					nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED, error);
					return -1;
				}

				encap_decap_order = 1;

				switch (decap->items[decap->item_count - 2].type) {
				case RTE_FLOW_ITEM_TYPE_ETH:
				case RTE_FLOW_ITEM_TYPE_VLAN:
					fd->header_strip_end_dyn = DYN_L3;
					fd->header_strip_end_ofs = 0;
					break;

				case RTE_FLOW_ITEM_TYPE_IPV4:
				case RTE_FLOW_ITEM_TYPE_IPV6:
					fd->header_strip_end_dyn = DYN_L4;
					fd->header_strip_end_ofs = 0;
					break;

				case RTE_FLOW_ITEM_TYPE_SCTP:
				case RTE_FLOW_ITEM_TYPE_TCP:
				case RTE_FLOW_ITEM_TYPE_UDP:
				case RTE_FLOW_ITEM_TYPE_ICMP:
				case RTE_FLOW_ITEM_TYPE_ICMP6:
					fd->header_strip_end_dyn = DYN_L4_PAYLOAD;
					fd->header_strip_end_ofs = 0;
					break;

				case RTE_FLOW_ITEM_TYPE_GTP:
					fd->header_strip_end_dyn = DYN_TUN_L3;
					fd->header_strip_end_ofs = 0;
					break;

				default:
					fd->header_strip_end_dyn = DYN_L2;
					fd->header_strip_end_ofs = 0;
					break;
				}
			}

			break;

		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_MODIFY_FIELD", dev);
			{
				/* Note: This copy method will not work for FLOW_FIELD_POINTER */
				struct rte_flow_action_modify_field modify_field_tmp;
				const struct rte_flow_action_modify_field *modify_field =
					memcpy_mask_if(&modify_field_tmp, action[aidx].conf,
					action_mask ? action_mask[aidx].conf : NULL,
					sizeof(struct rte_flow_action_modify_field));

				uint64_t modify_field_use_flag = 0;

				if (modify_field->src.field != RTE_FLOW_FIELD_VALUE) {
					NT_LOG(ERR, FILTER,
						"MODIFY_FIELD only src type VALUE is supported.");
					nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED, error);
					return -1;
				}

				if (modify_field->dst.level > 2) {
					NT_LOG(ERR, FILTER,
						"MODIFY_FIELD only dst level 0, 1, and 2 is supported.");
					nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED, error);
					return -1;
				}

				if (modify_field->dst.field == RTE_FLOW_FIELD_IPV4_TTL ||
					modify_field->dst.field == RTE_FLOW_FIELD_IPV6_HOPLIMIT) {
					if (modify_field->operation != RTE_FLOW_MODIFY_SUB) {
						NT_LOG(ERR, FILTER,
							"MODIFY_FIELD only operation SUB is supported for TTL/HOPLIMIT.");
						nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED,
							error);
						return -1;
					}

					if (fd->ttl_sub_enable) {
						NT_LOG(ERR, FILTER,
							"MODIFY_FIELD TTL/HOPLIMIT resource already in use.");
						nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED,
							error);
						return -1;
					}

					fd->ttl_sub_enable = 1;
					fd->ttl_sub_ipv4 =
						(modify_field->dst.field == RTE_FLOW_FIELD_IPV4_TTL)
						? 1
						: 0;
					fd->ttl_sub_outer = (modify_field->dst.level <= 1) ? 1 : 0;

				} else {
					if (modify_field->operation != RTE_FLOW_MODIFY_SET) {
						NT_LOG(ERR, FILTER,
							"MODIFY_FIELD only operation SET is supported in general.");
						nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED,
							error);
						return -1;
					}

					if (fd->modify_field_count >=
						dev->ndev->be.tpe.nb_cpy_writers) {
						NT_LOG(ERR, FILTER,
							"MODIFY_FIELD exceeded maximum of %u MODIFY_FIELD actions.",
							dev->ndev->be.tpe.nb_cpy_writers);
						nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED,
							error);
						return -1;
					}

					int mod_outer = modify_field->dst.level <= 1;

					switch (modify_field->dst.field) {
					case RTE_FLOW_FIELD_IPV4_DSCP:
						fd->modify_field[fd->modify_field_count].select =
							CPY_SELECT_DSCP_IPV4;
						fd->modify_field[fd->modify_field_count].dyn =
							mod_outer ? DYN_L3 : DYN_TUN_L3;
						fd->modify_field[fd->modify_field_count].ofs = 1;
						fd->modify_field[fd->modify_field_count].len = 1;
						break;

					case RTE_FLOW_FIELD_IPV6_DSCP:
						fd->modify_field[fd->modify_field_count].select =
							CPY_SELECT_DSCP_IPV6;
						fd->modify_field[fd->modify_field_count].dyn =
							mod_outer ? DYN_L3 : DYN_TUN_L3;
						fd->modify_field[fd->modify_field_count].ofs = 0;
						/*
						 * len=2 is needed because
						 * IPv6 DSCP overlaps 2 bytes.
						 */
						fd->modify_field[fd->modify_field_count].len = 2;
						break;

					case RTE_FLOW_FIELD_GTP_PSC_QFI:
						fd->modify_field[fd->modify_field_count].select =
							CPY_SELECT_RQI_QFI;
						fd->modify_field[fd->modify_field_count].dyn =
							mod_outer ? DYN_L4_PAYLOAD
							: DYN_TUN_L4_PAYLOAD;
						fd->modify_field[fd->modify_field_count].ofs = 14;
						fd->modify_field[fd->modify_field_count].len = 1;
						break;

					case RTE_FLOW_FIELD_IPV4_SRC:
						fd->modify_field[fd->modify_field_count].select =
							CPY_SELECT_IPV4;
						fd->modify_field[fd->modify_field_count].dyn =
							mod_outer ? DYN_L3 : DYN_TUN_L3;
						fd->modify_field[fd->modify_field_count].ofs = 12;
						fd->modify_field[fd->modify_field_count].len = 4;
						break;

					case RTE_FLOW_FIELD_IPV4_DST:
						fd->modify_field[fd->modify_field_count].select =
							CPY_SELECT_IPV4;
						fd->modify_field[fd->modify_field_count].dyn =
							mod_outer ? DYN_L3 : DYN_TUN_L3;
						fd->modify_field[fd->modify_field_count].ofs = 16;
						fd->modify_field[fd->modify_field_count].len = 4;
						break;

					case RTE_FLOW_FIELD_TCP_PORT_SRC:
					case RTE_FLOW_FIELD_UDP_PORT_SRC:
						fd->modify_field[fd->modify_field_count].select =
							CPY_SELECT_PORT;
						fd->modify_field[fd->modify_field_count].dyn =
							mod_outer ? DYN_L4 : DYN_TUN_L4;
						fd->modify_field[fd->modify_field_count].ofs = 0;
						fd->modify_field[fd->modify_field_count].len = 2;
						break;

					case RTE_FLOW_FIELD_TCP_PORT_DST:
					case RTE_FLOW_FIELD_UDP_PORT_DST:
						fd->modify_field[fd->modify_field_count].select =
							CPY_SELECT_PORT;
						fd->modify_field[fd->modify_field_count].dyn =
							mod_outer ? DYN_L4 : DYN_TUN_L4;
						fd->modify_field[fd->modify_field_count].ofs = 2;
						fd->modify_field[fd->modify_field_count].len = 2;
						break;

					case RTE_FLOW_FIELD_GTP_TEID:
						fd->modify_field[fd->modify_field_count].select =
							CPY_SELECT_TEID;
						fd->modify_field[fd->modify_field_count].dyn =
							mod_outer ? DYN_L4_PAYLOAD
							: DYN_TUN_L4_PAYLOAD;
						fd->modify_field[fd->modify_field_count].ofs = 4;
						fd->modify_field[fd->modify_field_count].len = 4;
						break;

					default:
						NT_LOG(ERR, FILTER,
							"MODIFY_FIELD dst type is not supported.");
						nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED,
							error);
						return -1;
					}

					modify_field_use_flag = (uint64_t)1
						<< fd->modify_field[fd->modify_field_count].select;

					if (modify_field_use_flag & modify_field_use_flags) {
						NT_LOG(ERR, FILTER,
							"MODIFY_FIELD dst type hardware resource already used.");
						nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED,
							error);
						return -1;
					}

					memcpy(fd->modify_field[fd->modify_field_count].value8,
						modify_field->src.value, 16);

					fd->modify_field[fd->modify_field_count].level =
						modify_field->dst.level;

					modify_field_use_flags |= modify_field_use_flag;
					fd->modify_field_count += 1;
				}
			}

			break;

		case RTE_FLOW_ACTION_TYPE_AGE:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_AGE", dev);

			if (action[aidx].conf) {
				struct rte_flow_action_age age_tmp;
				const struct rte_flow_action_age *age =
					memcpy_mask_if(&age_tmp, action[aidx].conf,
					action_mask ? action_mask[aidx].conf : NULL,
					sizeof(struct rte_flow_action_age));
				fd->age.timeout = nthw_mod_flm_scrub_timeout_encode(age->timeout);
				fd->age.context = age->context;
				NT_LOG(DBG, FILTER,
					"normalized timeout: %u, original timeout: %u, context: %p",
					nthw_mod_flm_scrub_timeout_decode(fd->age.timeout),
					age->timeout, fd->age.context);
			}

			break;

		default:
			NT_LOG(ERR, FILTER, "Invalid or unsupported flow action received - %i",
				action[aidx].type);
			nthw_flow_nic_set_error(ERR_ACTION_UNSUPPORTED, error);
			return -1;
		}
	}

	if (!(encap_decap_order == 0 || encap_decap_order == 2)) {
		NT_LOG(ERR, FILTER, "Invalid encap/decap actions");
		return -1;
	}

	return 0;
}

static int interpret_flow_elements(const struct flow_eth_dev *dev,
	const struct rte_flow_item elem[],
	struct nic_flow_def *fd __rte_unused,
	struct rte_flow_error *error,
	uint16_t implicit_vlan_vid __rte_unused,
	uint32_t *in_port_id,
	uint32_t *packet_data,
	uint32_t *packet_mask,
	struct flm_flow_key_def_s *key_def)
{
	uint32_t any_count = 0;

	unsigned int qw_counter = 0;
	unsigned int sw_counter = 0;

	*in_port_id = UINT32_MAX;

	if (packet_data == NULL || packet_mask == NULL || key_def == NULL || elem == NULL) {
		nthw_flow_nic_set_error(ERR_FAILED, error);
		NT_LOG(ERR, FILTER, "Flow items missing");
		return -1;
	}
	memset(packet_data, 0x0, sizeof(uint32_t) * 10);
	memset(packet_mask, 0x0, sizeof(uint32_t) * 10);
	memset(key_def, 0x0, sizeof(struct flm_flow_key_def_s));

	if (implicit_vlan_vid > 0) {
		uint32_t *sw_data = &packet_data[1 - sw_counter];
		uint32_t *sw_mask = &packet_mask[1 - sw_counter];

		sw_mask[0] = 0x0fff;
		sw_data[0] = implicit_vlan_vid & sw_mask[0];

		nthw_km_add_match_elem(&fd->km, &sw_data[0], &sw_mask[0], 1, DYN_FIRST_VLAN, 0);
		set_key_def_sw(key_def, sw_counter, DYN_FIRST_VLAN, 0);
		sw_counter += 1;

		fd->vlans += 1;
	}

	int qw_reserved_mac = 0;
	int qw_reserved_ipv6 = 0;

	for (int eidx = 0; elem[eidx].type != RTE_FLOW_ITEM_TYPE_END; ++eidx) {
		switch (elem[eidx].type) {
		case RTE_FLOW_ITEM_TYPE_ETH: {
			const struct rte_ether_hdr *eth_spec =
				(const struct rte_ether_hdr *)elem[eidx].spec;
			const struct rte_ether_hdr *eth_mask =
				(const struct rte_ether_hdr *)elem[eidx].mask;

			if (eth_spec != NULL && eth_mask != NULL) {
				if (is_non_zero(eth_mask->dst_addr.addr_bytes, 6) ||
					is_non_zero(eth_mask->src_addr.addr_bytes, 6)) {
					qw_reserved_mac += 1;
				}
			}
		}
		break;

		case RTE_FLOW_ITEM_TYPE_IPV6: {
			const struct rte_flow_item_ipv6 *ipv6_spec =
				(const struct rte_flow_item_ipv6 *)elem[eidx].spec;
			const struct rte_flow_item_ipv6 *ipv6_mask =
				(const struct rte_flow_item_ipv6 *)elem[eidx].mask;

			if (ipv6_spec != NULL && ipv6_mask != NULL) {
				if (is_non_zero(&ipv6_spec->hdr.src_addr, 16))
					qw_reserved_ipv6 += 1;

				if (is_non_zero(&ipv6_spec->hdr.dst_addr, 16))
					qw_reserved_ipv6 += 1;
			}
		}
		break;

		default:
			break;
		}
	}

	int qw_free = 2 - qw_reserved_mac - qw_reserved_ipv6;

	if (qw_free < 0) {
		NT_LOG(ERR, FILTER, "Key size too big. Out of QW resources.");
		nthw_flow_nic_set_error(ERR_FAILED, error);
		return -1;
	}

	for (int eidx = 0; elem[eidx].type != RTE_FLOW_ITEM_TYPE_END; ++eidx) {
		switch (elem[eidx].type) {
		case RTE_FLOW_ITEM_TYPE_ANY:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_ANY",
				dev->ndev->adapter_no, dev->port);
			any_count += 1;
			break;

		case RTE_FLOW_ITEM_TYPE_ETH:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_ETH",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_ether_hdr *eth_spec =
					(const struct rte_ether_hdr *)elem[eidx].spec;
				const struct rte_ether_hdr *eth_mask =
					(const struct rte_ether_hdr *)elem[eidx].mask;

				if (any_count > 0) {
					NT_LOG(ERR, FILTER,
						"Tunneled L2 ethernet not supported");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				if (eth_spec == NULL || eth_mask == NULL) {
					fd->l2_prot = PROT_L2_ETH2;
					break;
				}

				int non_zero = is_non_zero(eth_mask->dst_addr.addr_bytes, 6) ||
					is_non_zero(eth_mask->src_addr.addr_bytes, 6);

				if (non_zero ||
					(eth_mask->ether_type != 0 && sw_counter >= 2)) {
					if (qw_counter >= 2) {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}

					uint32_t *qw_data =
						&packet_data[2 + 4 - qw_counter * 4];
					uint32_t *qw_mask =
						&packet_mask[2 + 4 - qw_counter * 4];

					qw_data[0] = ((eth_spec->dst_addr.addr_bytes[0] &
						eth_mask->dst_addr.addr_bytes[0]) << 24) +
						((eth_spec->dst_addr.addr_bytes[1] &
						eth_mask->dst_addr.addr_bytes[1]) << 16) +
						((eth_spec->dst_addr.addr_bytes[2] &
						eth_mask->dst_addr.addr_bytes[2]) << 8) +
						(eth_spec->dst_addr.addr_bytes[3] &
						eth_mask->dst_addr.addr_bytes[3]);

					qw_data[1] = ((eth_spec->dst_addr.addr_bytes[4] &
						eth_mask->dst_addr.addr_bytes[4]) << 24) +
						((eth_spec->dst_addr.addr_bytes[5] &
						eth_mask->dst_addr.addr_bytes[5]) << 16) +
						((eth_spec->src_addr.addr_bytes[0] &
						eth_mask->src_addr.addr_bytes[0]) << 8) +
						(eth_spec->src_addr.addr_bytes[1] &
						eth_mask->src_addr.addr_bytes[1]);

					qw_data[2] = ((eth_spec->src_addr.addr_bytes[2] &
						eth_mask->src_addr.addr_bytes[2]) << 24) +
						((eth_spec->src_addr.addr_bytes[3] &
						eth_mask->src_addr.addr_bytes[3]) << 16) +
						((eth_spec->src_addr.addr_bytes[4] &
						eth_mask->src_addr.addr_bytes[4]) << 8) +
						(eth_spec->src_addr.addr_bytes[5] &
						eth_mask->src_addr.addr_bytes[5]);

					qw_data[3] = ntohs(eth_spec->ether_type &
						eth_mask->ether_type) << 16;

					qw_mask[0] = (eth_mask->dst_addr.addr_bytes[0] << 24) +
						(eth_mask->dst_addr.addr_bytes[1] << 16) +
						(eth_mask->dst_addr.addr_bytes[2] << 8) +
						eth_mask->dst_addr.addr_bytes[3];

					qw_mask[1] = (eth_mask->dst_addr.addr_bytes[4] << 24) +
						(eth_mask->dst_addr.addr_bytes[5] << 16) +
						(eth_mask->src_addr.addr_bytes[0] << 8) +
						eth_mask->src_addr.addr_bytes[1];

					qw_mask[2] = (eth_mask->src_addr.addr_bytes[2] << 24) +
						(eth_mask->src_addr.addr_bytes[3] << 16) +
						(eth_mask->src_addr.addr_bytes[4] << 8) +
						eth_mask->src_addr.addr_bytes[5];

					qw_mask[3] = ntohs(eth_mask->ether_type) << 16;

					nthw_km_add_match_elem(&fd->km,
						&qw_data[(size_t)(qw_counter * 4)],
						&qw_mask[(size_t)(qw_counter * 4)], 4, DYN_L2, 0);
					set_key_def_qw(key_def, qw_counter, DYN_L2, 0);
					qw_counter += 1;

					if (!non_zero)
						qw_free -= 1;

				} else if (eth_mask->ether_type != 0) {
					if (sw_counter >= 2) {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW-QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}

					uint32_t *sw_data = &packet_data[1 - sw_counter];
					uint32_t *sw_mask = &packet_mask[1 - sw_counter];

					sw_mask[0] = ntohs(eth_mask->ether_type) << 16;
					sw_data[0] = ntohs(eth_spec->ether_type) << 16 & sw_mask[0];

					nthw_km_add_match_elem(&fd->km, &sw_data[0],
						&sw_mask[0], 1, DYN_L2, 12);
					set_key_def_sw(key_def, sw_counter, DYN_L2, 12);
					sw_counter += 1;
				}

				fd->l2_prot = PROT_L2_ETH2;
			}

			break;

		case RTE_FLOW_ITEM_TYPE_VLAN:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_VLAN",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_vlan_hdr *vlan_spec =
					(const struct rte_vlan_hdr *)elem[eidx].spec;
				const struct rte_vlan_hdr *vlan_mask =
					(const struct rte_vlan_hdr *)elem[eidx].mask;

				if (vlan_spec == NULL || vlan_mask == NULL) {
					fd->vlans += 1;
					break;
				}

				if (!vlan_mask->vlan_tci && !vlan_mask->eth_proto)
					break;

				if (implicit_vlan_vid > 0) {
					NT_LOG(ERR, FILTER,
						"Multiple VLANs not supported for implicit VLAN patterns.");
					nthw_flow_nic_set_error(
						ERR_MATCH_INVALID_OR_UNSUPPORTED_ELEM,
						error);
					return -1;
				}

				if (sw_counter < 2) {
					uint32_t *sw_data = &packet_data[1 - sw_counter];
					uint32_t *sw_mask = &packet_mask[1 - sw_counter];

					sw_mask[0] = ntohs(vlan_mask->vlan_tci) << 16 |
						ntohs(vlan_mask->eth_proto);
					sw_data[0] = ntohs(vlan_spec->vlan_tci) << 16 |
						ntohs(vlan_spec->eth_proto);
					sw_data[0] &= sw_mask[0];

					nthw_km_add_match_elem(&fd->km, &sw_data[0], &sw_mask[0], 1,
						DYN_FIRST_VLAN, 2 + 4 * fd->vlans);
					set_key_def_sw(key_def, sw_counter, DYN_FIRST_VLAN,
						2 + 4 * fd->vlans);
					sw_counter += 1;

				} else if (qw_counter < 2 && qw_free > 0) {
					uint32_t *qw_data = &packet_data[2 + 4 - qw_counter * 4];
					uint32_t *qw_mask = &packet_mask[2 + 4 - qw_counter * 4];

					qw_data[0] = ntohs(vlan_spec->vlan_tci) << 16 |
						ntohs(vlan_spec->eth_proto);
					qw_data[1] = 0;
					qw_data[2] = 0;
					qw_data[3] = 0;

					qw_mask[0] = ntohs(vlan_mask->vlan_tci) << 16 |
						ntohs(vlan_mask->eth_proto);
					qw_mask[1] = 0;
					qw_mask[2] = 0;
					qw_mask[3] = 0;

					qw_data[0] &= qw_mask[0];
					qw_data[1] &= qw_mask[1];
					qw_data[2] &= qw_mask[2];
					qw_data[3] &= qw_mask[3];

					nthw_km_add_match_elem(&fd->km, &qw_data[0], &qw_mask[0], 4,
						DYN_FIRST_VLAN, 2 + 4 * fd->vlans);
					set_key_def_qw(key_def, qw_counter, DYN_FIRST_VLAN,
						2 + 4 * fd->vlans);
					qw_counter += 1;
					qw_free -= 1;

				} else {
					NT_LOG(ERR, FILTER,
						"Key size too big. Out of SW-QW resources.");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				fd->vlans += 1;
			}

			break;

		case RTE_FLOW_ITEM_TYPE_IPV4:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_IPV4",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_flow_item_ipv4 *ipv4_spec =
					(const struct rte_flow_item_ipv4 *)elem[eidx].spec;
				const struct rte_flow_item_ipv4 *ipv4_mask =
					(const struct rte_flow_item_ipv4 *)elem[eidx].mask;

				if (ipv4_spec == NULL || ipv4_mask == NULL) {
					if (any_count > 0 || fd->l3_prot != -1)
						fd->tunnel_l3_prot = PROT_TUN_L3_IPV4;
					else
						fd->l3_prot = PROT_L3_IPV4;
					break;
				}

				if (ipv4_mask->hdr.version_ihl != 0 ||
					ipv4_mask->hdr.type_of_service != 0 ||
					ipv4_mask->hdr.total_length != 0 ||
					ipv4_mask->hdr.packet_id != 0 ||
					(ipv4_mask->hdr.fragment_offset != 0 &&
					(ipv4_spec->hdr.fragment_offset != 0xffff ||
					ipv4_mask->hdr.fragment_offset != 0xffff)) ||
					ipv4_mask->hdr.time_to_live != 0 ||
					ipv4_mask->hdr.hdr_checksum != 0) {
					NT_LOG(ERR, FILTER,
						"Requested IPv4 field not support by running SW version.");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				if (ipv4_spec->hdr.fragment_offset == 0xffff &&
					ipv4_mask->hdr.fragment_offset == 0xffff) {
					fd->fragmentation = 0xfe;
				}

				int match_cnt = (ipv4_mask->hdr.src_addr != 0) +
					(ipv4_mask->hdr.dst_addr != 0) +
					(ipv4_mask->hdr.next_proto_id != 0);

				if (match_cnt <= 0) {
					if (any_count > 0 || fd->l3_prot != -1)
						fd->tunnel_l3_prot = PROT_TUN_L3_IPV4;
					else
						fd->l3_prot = PROT_L3_IPV4;
					break;
				}

				if (qw_free > 0 &&
					(match_cnt >= 2 ||
					(match_cnt == 1 && sw_counter >= 2))) {
					if (qw_counter >= 2) {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED,
							error);
						return -1;
					}

					uint32_t *qw_data = &packet_data[2 + 4 - qw_counter * 4];
					uint32_t *qw_mask = &packet_mask[2 + 4 - qw_counter * 4];

					qw_mask[0] = 0;
					qw_data[0] = 0;

					qw_mask[1] = ipv4_mask->hdr.next_proto_id << 16;
					qw_data[1] = ipv4_spec->hdr.next_proto_id
						<< 16 & qw_mask[1];

					qw_mask[2] = ntohl(ipv4_mask->hdr.src_addr);
					qw_mask[3] = ntohl(ipv4_mask->hdr.dst_addr);

					qw_data[2] = ntohl(ipv4_spec->hdr.src_addr) & qw_mask[2];
					qw_data[3] = ntohl(ipv4_spec->hdr.dst_addr) & qw_mask[3];

					nthw_km_add_match_elem(&fd->km, &qw_data[0], &qw_mask[0], 4,
						any_count > 0 ? DYN_TUN_L3 : DYN_L3, 4);
					set_key_def_qw(key_def, qw_counter, any_count > 0
						? DYN_TUN_L3 : DYN_L3, 4);
					qw_counter += 1;
					qw_free -= 1;

					if (any_count > 0 || fd->l3_prot != -1)
						fd->tunnel_l3_prot = PROT_TUN_L3_IPV4;
					else
						fd->l3_prot = PROT_L3_IPV4;
					break;
				}

				if (ipv4_mask->hdr.src_addr) {
					if (sw_counter >= 2) {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}

					uint32_t *sw_data = &packet_data[1 - sw_counter];
					uint32_t *sw_mask = &packet_mask[1 - sw_counter];

					sw_mask[0] = ntohl(ipv4_mask->hdr.src_addr);
					sw_data[0] = ntohl(ipv4_spec->hdr.src_addr) & sw_mask[0];

					nthw_km_add_match_elem(&fd->km, &sw_data[0], &sw_mask[0], 1,
						any_count > 0 ? DYN_TUN_L3 : DYN_L3, 12);
					set_key_def_sw(key_def, sw_counter, any_count > 0
						? DYN_TUN_L3 : DYN_L3, 12);
					sw_counter += 1;
				}

				if (ipv4_mask->hdr.dst_addr) {
					if (sw_counter >= 2) {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}

					uint32_t *sw_data = &packet_data[1 - sw_counter];
					uint32_t *sw_mask = &packet_mask[1 - sw_counter];

					sw_mask[0] = ntohl(ipv4_mask->hdr.dst_addr);
					sw_data[0] = ntohl(ipv4_spec->hdr.dst_addr) & sw_mask[0];

					nthw_km_add_match_elem(&fd->km, &sw_data[0], &sw_mask[0], 1,
						any_count > 0 ? DYN_TUN_L3 : DYN_L3, 16);
					set_key_def_sw(key_def, sw_counter, any_count > 0
						? DYN_TUN_L3 : DYN_L3, 16);
					sw_counter += 1;
				}

				if (ipv4_mask->hdr.next_proto_id) {
					if (sw_counter >= 2) {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}

					uint32_t *sw_data = &packet_data[1 - sw_counter];
					uint32_t *sw_mask = &packet_mask[1 - sw_counter];

					sw_mask[0] = ipv4_mask->hdr.next_proto_id << 16;
					sw_data[0] = ipv4_spec->hdr.next_proto_id
						<< 16 & sw_mask[0];

					nthw_km_add_match_elem(&fd->km, &sw_data[0], &sw_mask[0], 1,
						any_count > 0 ? DYN_TUN_L3 : DYN_L3, 8);
					set_key_def_sw(key_def, sw_counter, any_count > 0
						? DYN_TUN_L3 : DYN_L3, 8);
					sw_counter += 1;
				}

				if (any_count > 0 || fd->l3_prot != -1)
					fd->tunnel_l3_prot = PROT_TUN_L3_IPV4;

				else
					fd->l3_prot = PROT_L3_IPV4;
			}

			break;

		case RTE_FLOW_ITEM_TYPE_IPV6:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_IPV6",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_flow_item_ipv6 *ipv6_spec =
					(const struct rte_flow_item_ipv6 *)elem[eidx].spec;
				const struct rte_flow_item_ipv6 *ipv6_mask =
					(const struct rte_flow_item_ipv6 *)elem[eidx].mask;

				if (ipv6_spec == NULL || ipv6_mask == NULL) {
					if (any_count > 0 || fd->l3_prot != -1)
						fd->tunnel_l3_prot = PROT_TUN_L3_IPV6;
					else
						fd->l3_prot = PROT_L3_IPV6;
					break;
				}

				if (ipv6_mask->hdr.vtc_flow != 0 ||
					ipv6_mask->hdr.payload_len != 0 ||
					ipv6_mask->hdr.hop_limits != 0) {
					NT_LOG(ERR, FILTER,
						"Requested IPv6 field not support by running SW version");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				if (is_non_zero(&ipv6_spec->hdr.src_addr, 16)) {
					if (qw_counter >= 2) {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}

					uint32_t *qw_data = &packet_data[2 + 4 - qw_counter * 4];
					uint32_t *qw_mask = &packet_mask[2 + 4 - qw_counter * 4];

					memcpy(&qw_data[0], &ipv6_spec->hdr.src_addr, 16);
					memcpy(&qw_mask[0], &ipv6_mask->hdr.src_addr, 16);

					qw_data[0] = ntohl(qw_data[0]);
					qw_data[1] = ntohl(qw_data[1]);
					qw_data[2] = ntohl(qw_data[2]);
					qw_data[3] = ntohl(qw_data[3]);

					qw_mask[0] = ntohl(qw_mask[0]);
					qw_mask[1] = ntohl(qw_mask[1]);
					qw_mask[2] = ntohl(qw_mask[2]);
					qw_mask[3] = ntohl(qw_mask[3]);

					qw_data[0] &= qw_mask[0];
					qw_data[1] &= qw_mask[1];
					qw_data[2] &= qw_mask[2];
					qw_data[3] &= qw_mask[3];

					nthw_km_add_match_elem(&fd->km, &qw_data[0], &qw_mask[0], 4,
						any_count > 0 ? DYN_TUN_L3 : DYN_L3, 8);
					set_key_def_qw(key_def, qw_counter, any_count > 0
						? DYN_TUN_L3 : DYN_L3, 8);
					qw_counter += 1;
				}

				if (is_non_zero(&ipv6_spec->hdr.dst_addr, 16)) {
					if (qw_counter >= 2) {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}

					uint32_t *qw_data = &packet_data[2 + 4 - qw_counter * 4];
					uint32_t *qw_mask = &packet_mask[2 + 4 - qw_counter * 4];

					memcpy(&qw_data[0], &ipv6_spec->hdr.dst_addr, 16);
					memcpy(&qw_mask[0], &ipv6_mask->hdr.dst_addr, 16);

					qw_data[0] = ntohl(qw_data[0]);
					qw_data[1] = ntohl(qw_data[1]);
					qw_data[2] = ntohl(qw_data[2]);
					qw_data[3] = ntohl(qw_data[3]);

					qw_mask[0] = ntohl(qw_mask[0]);
					qw_mask[1] = ntohl(qw_mask[1]);
					qw_mask[2] = ntohl(qw_mask[2]);
					qw_mask[3] = ntohl(qw_mask[3]);

					qw_data[0] &= qw_mask[0];
					qw_data[1] &= qw_mask[1];
					qw_data[2] &= qw_mask[2];
					qw_data[3] &= qw_mask[3];

					nthw_km_add_match_elem(&fd->km, &qw_data[0], &qw_mask[0], 4,
						any_count > 0 ? DYN_TUN_L3 : DYN_L3, 24);
					set_key_def_qw(key_def, qw_counter, any_count > 0
						? DYN_TUN_L3 : DYN_L3, 24);
					qw_counter += 1;
				}

				if (ipv6_mask->hdr.proto != 0) {
					if (sw_counter < 2) {
						uint32_t *sw_data = &packet_data[1 - sw_counter];
						uint32_t *sw_mask = &packet_mask[1 - sw_counter];

						sw_mask[0] = ipv6_mask->hdr.proto << 8;
						sw_data[0] = ipv6_spec->hdr.proto << 8 & sw_mask[0];

						nthw_km_add_match_elem(&fd->km,
							&sw_data[0], &sw_mask[0], 1,
							any_count > 0 ? DYN_TUN_L3 : DYN_L3, 4);
						set_key_def_sw(key_def, sw_counter, any_count > 0
							? DYN_TUN_L3 : DYN_L3, 4);
						sw_counter += 1;

					} else if (qw_counter < 2 && qw_free > 0) {
						uint32_t *qw_data =
							&packet_data[2 + 4 - qw_counter * 4];
						uint32_t *qw_mask =
							&packet_mask[2 + 4 - qw_counter * 4];

						qw_data[0] = 0;
						qw_data[1] = ipv6_mask->hdr.proto << 8;
						qw_data[2] = 0;
						qw_data[3] = 0;

						qw_mask[0] = 0;
						qw_mask[1] = ipv6_spec->hdr.proto << 8;
						qw_mask[2] = 0;
						qw_mask[3] = 0;

						qw_data[0] &= qw_mask[0];
						qw_data[1] &= qw_mask[1];
						qw_data[2] &= qw_mask[2];
						qw_data[3] &= qw_mask[3];

						nthw_km_add_match_elem(&fd->km,
							&qw_data[0], &qw_mask[0], 4,
							any_count > 0 ? DYN_TUN_L3 : DYN_L3, 0);
						set_key_def_qw(key_def, qw_counter, any_count > 0
							? DYN_TUN_L3 : DYN_L3, 0);
						qw_counter += 1;
						qw_free -= 1;

					} else {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW-QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}
				}

				if (any_count > 0 || fd->l3_prot != -1)
					fd->tunnel_l3_prot = PROT_TUN_L3_IPV6;

				else
					fd->l3_prot = PROT_L3_IPV6;
			}

			break;

		case RTE_FLOW_ITEM_TYPE_UDP:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_UDP",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_flow_item_udp *udp_spec =
					(const struct rte_flow_item_udp *)elem[eidx].spec;
				const struct rte_flow_item_udp *udp_mask =
					(const struct rte_flow_item_udp *)elem[eidx].mask;

				if (udp_spec == NULL || udp_mask == NULL) {
					if (any_count > 0 || fd->l4_prot != -1) {
						fd->tunnel_l4_prot = PROT_TUN_L4_UDP;
						key_def->inner_proto = 1;
					} else {
						fd->l4_prot = PROT_L4_UDP;
						key_def->outer_proto = 1;
					}
					break;
				}

				if (udp_mask->hdr.dgram_len != 0 ||
					udp_mask->hdr.dgram_cksum != 0) {
					NT_LOG(ERR, FILTER,
						"Requested UDP field not support by running SW version");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				if (udp_mask->hdr.src_port || udp_mask->hdr.dst_port) {
					if (sw_counter < 2) {
						uint32_t *sw_data = &packet_data[1 - sw_counter];
						uint32_t *sw_mask = &packet_mask[1 - sw_counter];

						sw_mask[0] = (ntohs(udp_mask->hdr.src_port) << 16) |
							ntohs(udp_mask->hdr.dst_port);
						sw_data[0] = ((ntohs(udp_spec->hdr.src_port)
							<< 16) | ntohs(udp_spec->hdr.dst_port)) &
							sw_mask[0];

						nthw_km_add_match_elem(&fd->km,
							&sw_data[0], &sw_mask[0], 1,
							any_count > 0 ? DYN_TUN_L4 : DYN_L4, 0);
						set_key_def_sw(key_def, sw_counter, any_count > 0
							? DYN_TUN_L4 : DYN_L4, 0);
						sw_counter += 1;

					} else if (qw_counter < 2 && qw_free > 0) {
						uint32_t *qw_data =
							&packet_data[2 + 4 - qw_counter * 4];
						uint32_t *qw_mask =
							&packet_mask[2 + 4 - qw_counter * 4];

						qw_data[0] = (ntohs(udp_spec->hdr.src_port)
							<< 16) | ntohs(udp_spec->hdr.dst_port);
						qw_data[1] = 0;
						qw_data[2] = 0;
						qw_data[3] = 0;

						qw_mask[0] = (ntohs(udp_mask->hdr.src_port)
							<< 16) | ntohs(udp_mask->hdr.dst_port);
						qw_mask[1] = 0;
						qw_mask[2] = 0;
						qw_mask[3] = 0;

						qw_data[0] &= qw_mask[0];
						qw_data[1] &= qw_mask[1];
						qw_data[2] &= qw_mask[2];
						qw_data[3] &= qw_mask[3];

						nthw_km_add_match_elem(&fd->km,
							&qw_data[0], &qw_mask[0], 4,
							any_count > 0 ? DYN_TUN_L4 : DYN_L4, 0);
						set_key_def_qw(key_def, qw_counter, any_count > 0
							? DYN_TUN_L4 : DYN_L4, 0);
						qw_counter += 1;
						qw_free -= 1;

					} else {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW-QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}
				}

				if (any_count > 0 || fd->l4_prot != -1) {
					fd->tunnel_l4_prot = PROT_TUN_L4_UDP;
					key_def->inner_proto = 1;

				} else {
					fd->l4_prot = PROT_L4_UDP;
					key_def->outer_proto = 1;
				}
			}

			break;

		case RTE_FLOW_ITEM_TYPE_SCTP:
			NT_LOG(DBG, FILTER, "Adap %i,Port %i:RTE_FLOW_ITEM_TYPE_SCTP",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_flow_item_sctp *sctp_spec =
					(const struct rte_flow_item_sctp *)elem[eidx].spec;
				const struct rte_flow_item_sctp *sctp_mask =
					(const struct rte_flow_item_sctp *)elem[eidx].mask;

				if (sctp_spec == NULL || sctp_mask == NULL) {
					if (any_count > 0 || fd->l4_prot != -1) {
						fd->tunnel_l4_prot = PROT_TUN_L4_SCTP;
						key_def->inner_proto = 1;
					} else {
						fd->l4_prot = PROT_L4_SCTP;
						key_def->outer_proto = 1;
					}
					break;
				}

				if (sctp_mask->hdr.tag != 0 || sctp_mask->hdr.cksum != 0) {
					NT_LOG(ERR, FILTER,
						"Requested SCTP field not support by running SW version");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				if (sctp_mask->hdr.src_port || sctp_mask->hdr.dst_port) {
					if (sw_counter < 2) {
						uint32_t *sw_data = &packet_data[1 - sw_counter];
						uint32_t *sw_mask = &packet_mask[1 - sw_counter];

						sw_mask[0] = (ntohs(sctp_mask->hdr.src_port)
							<< 16) | ntohs(sctp_mask->hdr.dst_port);
						sw_data[0] = ((ntohs(sctp_spec->hdr.src_port)
							<< 16) | ntohs(sctp_spec->hdr.dst_port)) &
							sw_mask[0];

						nthw_km_add_match_elem(&fd->km,
							&sw_data[0], &sw_mask[0], 1,
							any_count > 0 ? DYN_TUN_L4 : DYN_L4, 0);
						set_key_def_sw(key_def, sw_counter, any_count > 0
							? DYN_TUN_L4 : DYN_L4, 0);
						sw_counter += 1;

					} else if (qw_counter < 2 && qw_free > 0) {
						uint32_t *qw_data =
							&packet_data[2 + 4 - qw_counter * 4];
						uint32_t *qw_mask =
							&packet_mask[2 + 4 - qw_counter * 4];

						qw_data[0] = (ntohs(sctp_spec->hdr.src_port)
							<< 16) | ntohs(sctp_spec->hdr.dst_port);
						qw_data[1] = 0;
						qw_data[2] = 0;
						qw_data[3] = 0;

						qw_mask[0] = (ntohs(sctp_mask->hdr.src_port)
							<< 16) | ntohs(sctp_mask->hdr.dst_port);
						qw_mask[1] = 0;
						qw_mask[2] = 0;
						qw_mask[3] = 0;

						qw_data[0] &= qw_mask[0];
						qw_data[1] &= qw_mask[1];
						qw_data[2] &= qw_mask[2];
						qw_data[3] &= qw_mask[3];

						nthw_km_add_match_elem(&fd->km,
							&qw_data[0], &qw_mask[0], 4,
							any_count > 0 ? DYN_TUN_L4 : DYN_L4, 0);
						set_key_def_qw(key_def, qw_counter, any_count > 0
							? DYN_TUN_L4 : DYN_L4, 0);
						qw_counter += 1;
						qw_free -= 1;

					} else {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW-QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}
				}

				if (any_count > 0 || fd->l4_prot != -1) {
					fd->tunnel_l4_prot = PROT_TUN_L4_SCTP;
					key_def->inner_proto = 1;

				} else {
					fd->l4_prot = PROT_L4_SCTP;
					key_def->outer_proto = 1;
				}
			}

			break;

		case RTE_FLOW_ITEM_TYPE_ICMP:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_ICMP",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_flow_item_icmp *icmp_spec =
					(const struct rte_flow_item_icmp *)elem[eidx].spec;
				const struct rte_flow_item_icmp *icmp_mask =
					(const struct rte_flow_item_icmp *)elem[eidx].mask;

				if (icmp_spec == NULL || icmp_mask == NULL) {
					if (any_count > 0 || fd->l4_prot != -1) {
						fd->tunnel_l4_prot = PROT_TUN_L4_ICMP;
						fd->tunnel_ip_prot = 1;
						key_def->inner_proto = 1;
					} else {
						fd->l4_prot = PROT_L4_ICMP;
						fd->ip_prot = 1;
						key_def->outer_proto = 1;
					}
					break;
				}

				if (icmp_mask->hdr.icmp_cksum != 0 ||
					icmp_mask->hdr.icmp_ident != 0 ||
					icmp_mask->hdr.icmp_seq_nb != 0) {
					NT_LOG(ERR, FILTER,
						"Requested ICMP field not supported by running SW version");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				if (icmp_mask->hdr.icmp_type || icmp_mask->hdr.icmp_code) {
					if (sw_counter < 2) {
						uint32_t *sw_data = &packet_data[1 - sw_counter];
						uint32_t *sw_mask = &packet_mask[1 - sw_counter];

						sw_mask[0] = icmp_mask->hdr.icmp_type << 24 |
							icmp_mask->hdr.icmp_code << 16;
						sw_data[0] = icmp_spec->hdr.icmp_type << 24 |
							icmp_spec->hdr.icmp_code << 16;
						sw_data[0] &= sw_mask[0];

						nthw_km_add_match_elem(&fd->km, &sw_data[0],
							&sw_mask[0], 1, any_count > 0
							? DYN_TUN_L4 : DYN_L4, 0);
						set_key_def_sw(key_def, sw_counter,
							any_count > 0 ? DYN_TUN_L4 : DYN_L4, 0);
						sw_counter += 1;

					} else if (qw_counter < 2 && qw_free > 0) {
						uint32_t *qw_data =
							&packet_data[2 + 4 - qw_counter * 4];
						uint32_t *qw_mask =
							&packet_mask[2 + 4 - qw_counter * 4];

						qw_data[0] = icmp_spec->hdr.icmp_type << 24 |
							icmp_spec->hdr.icmp_code << 16;
						qw_data[1] = 0;
						qw_data[2] = 0;
						qw_data[3] = 0;

						qw_mask[0] = icmp_mask->hdr.icmp_type << 24 |
							icmp_mask->hdr.icmp_code << 16;
						qw_mask[1] = 0;
						qw_mask[2] = 0;
						qw_mask[3] = 0;

						qw_data[0] &= qw_mask[0];
						qw_data[1] &= qw_mask[1];
						qw_data[2] &= qw_mask[2];
						qw_data[3] &= qw_mask[3];

						nthw_km_add_match_elem(&fd->km,
							&qw_data[0], &qw_mask[0], 4,
							any_count > 0 ? DYN_TUN_L4 : DYN_L4, 0);
						set_key_def_qw(key_def, qw_counter, any_count > 0
							? DYN_TUN_L4 : DYN_L4, 0);
						qw_counter += 1;
						qw_free -= 1;

					} else {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW-QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}
				}

				if (any_count > 0 || fd->l4_prot != -1) {
					fd->tunnel_l4_prot = PROT_TUN_L4_ICMP;
					fd->tunnel_ip_prot = 1;
					key_def->inner_proto = 1;

				} else {
					fd->l4_prot = PROT_L4_ICMP;
					fd->ip_prot = 1;
					key_def->outer_proto = 1;
				}
			}

			break;

		case RTE_FLOW_ITEM_TYPE_ICMP6:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_ICMP6",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_flow_item_icmp6 *icmp_spec =
					(const struct rte_flow_item_icmp6 *)elem[eidx].spec;
				const struct rte_flow_item_icmp6 *icmp_mask =
					(const struct rte_flow_item_icmp6 *)elem[eidx].mask;

				if (icmp_spec == NULL || icmp_mask == NULL) {
					if (any_count > 0 || fd->l4_prot != -1) {
						fd->tunnel_l4_prot = PROT_TUN_L4_ICMP;
						fd->tunnel_ip_prot = 58;
						key_def->inner_proto = 1;
					} else {
						fd->l4_prot = PROT_L4_ICMP;
						fd->ip_prot = 58;
						key_def->outer_proto = 1;
					}
					break;
				}

				if (icmp_mask->checksum != 0) {
					NT_LOG(ERR, FILTER,
						"Requested ICMP6 field not supported by running SW version");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				if (icmp_mask->type || icmp_mask->code) {
					if (sw_counter < 2) {
						uint32_t *sw_data = &packet_data[1 - sw_counter];
						uint32_t *sw_mask = &packet_mask[1 - sw_counter];

						sw_mask[0] = icmp_mask->type << 24 |
							icmp_mask->code << 16;
						sw_data[0] = icmp_spec->type << 24 |
							icmp_spec->code << 16;
						sw_data[0] &= sw_mask[0];

						nthw_km_add_match_elem(&fd->km,
							&sw_data[0], &sw_mask[0], 1,
							any_count > 0 ? DYN_TUN_L4 : DYN_L4, 0);

						set_key_def_sw(key_def, sw_counter, any_count > 0
							? DYN_TUN_L4 : DYN_L4, 0);
						sw_counter += 1;

					} else if (qw_counter < 2 && qw_free > 0) {
						uint32_t *qw_data =
							&packet_data[2 + 4 - qw_counter * 4];
						uint32_t *qw_mask =
							&packet_mask[2 + 4 - qw_counter * 4];

						qw_data[0] = icmp_spec->type << 24 |
							icmp_spec->code << 16;
						qw_data[1] = 0;
						qw_data[2] = 0;
						qw_data[3] = 0;

						qw_mask[0] = icmp_mask->type << 24 |
							icmp_mask->code << 16;
						qw_mask[1] = 0;
						qw_mask[2] = 0;
						qw_mask[3] = 0;

						qw_data[0] &= qw_mask[0];
						qw_data[1] &= qw_mask[1];
						qw_data[2] &= qw_mask[2];
						qw_data[3] &= qw_mask[3];

						nthw_km_add_match_elem(&fd->km,
							&qw_data[0], &qw_mask[0], 4,
							any_count > 0 ? DYN_TUN_L4 : DYN_L4, 0);
						set_key_def_qw(key_def, qw_counter, any_count > 0
							? DYN_TUN_L4 : DYN_L4, 0);
						qw_counter += 1;
						qw_free -= 1;

					} else {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW-QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}
				}

				if (any_count > 0 || fd->l4_prot != -1) {
					fd->tunnel_l4_prot = PROT_TUN_L4_ICMP;
					fd->tunnel_ip_prot = 58;
					key_def->inner_proto = 1;

				} else {
					fd->l4_prot = PROT_L4_ICMP;
					fd->ip_prot = 58;
					key_def->outer_proto = 1;
				}
			}

			break;

		case RTE_FLOW_ITEM_TYPE_TCP:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_TCP",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_flow_item_tcp *tcp_spec =
					(const struct rte_flow_item_tcp *)elem[eidx].spec;
				const struct rte_flow_item_tcp *tcp_mask =
					(const struct rte_flow_item_tcp *)elem[eidx].mask;

				if (tcp_spec == NULL || tcp_mask == NULL) {
					if (any_count > 0 || fd->l4_prot != -1) {
						fd->tunnel_l4_prot = PROT_TUN_L4_TCP;
						key_def->inner_proto = 1;
					} else {
						fd->l4_prot = PROT_L4_TCP;
						key_def->outer_proto = 1;
					}
					break;
				}

				if (tcp_mask->hdr.sent_seq != 0 ||
					tcp_mask->hdr.recv_ack != 0 ||
					tcp_mask->hdr.data_off != 0 ||
					tcp_mask->hdr.tcp_flags != 0 ||
					tcp_mask->hdr.rx_win != 0 ||
					tcp_mask->hdr.cksum != 0 ||
					tcp_mask->hdr.tcp_urp != 0) {
					NT_LOG(ERR, FILTER,
						"Requested TCP field not support by running SW version");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				if (tcp_mask->hdr.src_port || tcp_mask->hdr.dst_port) {
					if (sw_counter < 2) {
						uint32_t *sw_data = &packet_data[1 - sw_counter];
						uint32_t *sw_mask = &packet_mask[1 - sw_counter];

						sw_mask[0] = (ntohs(tcp_mask->hdr.src_port)
							<< 16) | ntohs(tcp_mask->hdr.dst_port);
						sw_data[0] =
							((ntohs(tcp_spec->hdr.src_port) << 16) |
							ntohs(tcp_spec->hdr.dst_port)) & sw_mask[0];

						nthw_km_add_match_elem(&fd->km,
							&sw_data[0], &sw_mask[0], 1,
							any_count > 0 ? DYN_TUN_L4 : DYN_L4, 0);
						set_key_def_sw(key_def, sw_counter, any_count > 0
							? DYN_TUN_L4 : DYN_L4, 0);
						sw_counter += 1;

					} else if (qw_counter < 2 && qw_free > 0) {
						uint32_t *qw_data =
							&packet_data[2 + 4 - qw_counter * 4];
						uint32_t *qw_mask =
							&packet_mask[2 + 4 - qw_counter * 4];

						qw_data[0] = (ntohs(tcp_spec->hdr.src_port)
							<< 16) | ntohs(tcp_spec->hdr.dst_port);
						qw_data[1] = 0;
						qw_data[2] = 0;
						qw_data[3] = 0;

						qw_mask[0] = (ntohs(tcp_mask->hdr.src_port)
							<< 16) | ntohs(tcp_mask->hdr.dst_port);
						qw_mask[1] = 0;
						qw_mask[2] = 0;
						qw_mask[3] = 0;

						qw_data[0] &= qw_mask[0];
						qw_data[1] &= qw_mask[1];
						qw_data[2] &= qw_mask[2];
						qw_data[3] &= qw_mask[3];

						nthw_km_add_match_elem(&fd->km,
							&qw_data[0], &qw_mask[0], 4,
							any_count > 0 ? DYN_TUN_L4 : DYN_L4, 0);
						set_key_def_qw(key_def, qw_counter, any_count > 0
							? DYN_TUN_L4 : DYN_L4, 0);
						qw_counter += 1;
						qw_free -= 1;

					} else {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW-QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}
				}

				if (any_count > 0 || fd->l4_prot != -1) {
					fd->tunnel_l4_prot = PROT_TUN_L4_TCP;
					key_def->inner_proto = 1;

				} else {
					fd->l4_prot = PROT_L4_TCP;
					key_def->outer_proto = 1;
				}
			}

			break;

		case RTE_FLOW_ITEM_TYPE_GTP:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_GTP",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_gtp_hdr *gtp_spec =
					(const struct rte_gtp_hdr *)elem[eidx].spec;
				const struct rte_gtp_hdr *gtp_mask =
					(const struct rte_gtp_hdr *)elem[eidx].mask;

				if (gtp_spec == NULL || gtp_mask == NULL) {
					fd->tunnel_prot = PROT_TUN_GTPV1U;
					break;
				}

				if (gtp_mask->gtp_hdr_info != 0 ||
					gtp_mask->msg_type != 0 || gtp_mask->plen != 0) {
					NT_LOG(ERR, FILTER,
						"Requested GTP field not support by running SW version");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				if (gtp_mask->teid) {
					if (sw_counter < 2) {
						uint32_t *sw_data =
							&packet_data[1 - sw_counter];
						uint32_t *sw_mask =
							&packet_mask[1 - sw_counter];

						sw_mask[0] = ntohl(gtp_mask->teid);
						sw_data[0] =
							ntohl(gtp_spec->teid) & sw_mask[0];

						nthw_km_add_match_elem(&fd->km, &sw_data[0],
							&sw_mask[0], 1,
							DYN_L4_PAYLOAD, 4);
						set_key_def_sw(key_def, sw_counter,
							DYN_L4_PAYLOAD, 4);
						sw_counter += 1;

					} else if (qw_counter < 2 && qw_free > 0) {
						uint32_t *qw_data =
							&packet_data[2 + 4 -
							qw_counter * 4];
						uint32_t *qw_mask =
							&packet_mask[2 + 4 -
							qw_counter * 4];

						qw_data[0] = ntohl(gtp_spec->teid);
						qw_data[1] = 0;
						qw_data[2] = 0;
						qw_data[3] = 0;

						qw_mask[0] = ntohl(gtp_mask->teid);
						qw_mask[1] = 0;
						qw_mask[2] = 0;
						qw_mask[3] = 0;

						qw_data[0] &= qw_mask[0];
						qw_data[1] &= qw_mask[1];
						qw_data[2] &= qw_mask[2];
						qw_data[3] &= qw_mask[3];

						nthw_km_add_match_elem(&fd->km, &qw_data[0],
							&qw_mask[0], 4,
							DYN_L4_PAYLOAD, 4);
						set_key_def_qw(key_def, qw_counter,
							DYN_L4_PAYLOAD, 4);
						qw_counter += 1;
						qw_free -= 1;

					} else {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW-QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}
				}

				fd->tunnel_prot = PROT_TUN_GTPV1U;
			}

			break;

		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_GTP_PSC",
				dev->ndev->adapter_no, dev->port);
			{
				const struct rte_gtp_psc_generic_hdr *gtp_psc_spec =
					(const struct rte_gtp_psc_generic_hdr *)elem[eidx].spec;
				const struct rte_gtp_psc_generic_hdr *gtp_psc_mask =
					(const struct rte_gtp_psc_generic_hdr *)elem[eidx].mask;

				if (gtp_psc_spec == NULL || gtp_psc_mask == NULL) {
					fd->tunnel_prot = PROT_TUN_GTPV1U;
					break;
				}

				if (gtp_psc_mask->type != 0 ||
					gtp_psc_mask->ext_hdr_len != 0) {
					NT_LOG(ERR, FILTER,
						"Requested GTP PSC field is not supported by running SW version");
					nthw_flow_nic_set_error(ERR_FAILED, error);
					return -1;
				}

				if (gtp_psc_mask->qfi) {
					if (sw_counter < 2) {
						uint32_t *sw_data =
							&packet_data[1 - sw_counter];
						uint32_t *sw_mask =
							&packet_mask[1 - sw_counter];

						sw_mask[0] = ntohl(gtp_psc_mask->qfi);
						sw_data[0] = ntohl(gtp_psc_spec->qfi) &
							sw_mask[0];

						nthw_km_add_match_elem(&fd->km, &sw_data[0],
							&sw_mask[0], 1,
							DYN_L4_PAYLOAD, 14);
						set_key_def_sw(key_def, sw_counter,
							DYN_L4_PAYLOAD, 14);
						sw_counter += 1;

					} else if (qw_counter < 2 && qw_free > 0) {
						uint32_t *qw_data =
							&packet_data[2 + 4 -
							qw_counter * 4];
						uint32_t *qw_mask =
							&packet_mask[2 + 4 -
							qw_counter * 4];

						qw_data[0] = ntohl(gtp_psc_spec->qfi);
						qw_data[1] = 0;
						qw_data[2] = 0;
						qw_data[3] = 0;

						qw_mask[0] = ntohl(gtp_psc_mask->qfi);
						qw_mask[1] = 0;
						qw_mask[2] = 0;
						qw_mask[3] = 0;

						qw_data[0] &= qw_mask[0];
						qw_data[1] &= qw_mask[1];
						qw_data[2] &= qw_mask[2];
						qw_data[3] &= qw_mask[3];

						nthw_km_add_match_elem(&fd->km, &qw_data[0],
							&qw_mask[0], 4,
							DYN_L4_PAYLOAD, 14);
						set_key_def_qw(key_def, qw_counter,
							DYN_L4_PAYLOAD, 14);
						qw_counter += 1;
						qw_free -= 1;

					} else {
						NT_LOG(ERR, FILTER,
							"Key size too big. Out of SW-QW resources.");
						nthw_flow_nic_set_error(ERR_FAILED, error);
						return -1;
					}
				}

				fd->tunnel_prot = PROT_TUN_GTPV1U;
			}

			break;

		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_PORT_ID",
				dev->ndev->adapter_no, dev->port);

			if (elem[eidx].spec) {
				*in_port_id =
					((const struct rte_flow_item_port_id *)elem[eidx].spec)->id;
			}

			break;

		case RTE_FLOW_ITEM_TYPE_VOID:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_VOID",
				dev->ndev->adapter_no, dev->port);
			break;

		default:
			NT_LOG(ERR, FILTER, "Invalid or unsupported flow request: %d",
				(int)elem[eidx].type);
			nthw_flow_nic_set_error(ERR_MATCH_INVALID_OR_UNSUPPORTED_ELEM, error);
			return -1;
		}
	}

	return 0;
}

static bool has_only_valid_bits_set(const uint8_t *byte_array, const uint16_t byte_array_len,
	uint16_t bit_len)
{
	if (byte_array_len * 8 < bit_len)
		bit_len = byte_array_len * 8;

	uint8_t mask;
	uint16_t byte;

	for (byte = 0; byte < byte_array_len; byte++) {
		if (bit_len >= 8) {
			bit_len -= 8;
			mask = 0x00;

		} else if (bit_len > 0) {
			mask = 0xff >> bit_len << bit_len;
			bit_len = 0;

		} else {
			mask = 0xFF;
		}

		if (byte_array[byte] & mask)
			return false;
	}

	return true;
}

static int copy_fd_to_fh_flm(struct flow_handle *fh, const struct nic_flow_def *fd,
	const uint32_t *packet_data, uint32_t flm_key_id, uint32_t flm_ft,
	uint16_t rpl_ext_ptr, uint32_t flm_scrub __rte_unused, uint32_t priority)
{
	for (int i = 0; i < MAX_FLM_MTRS_SUPPORTED; ++i) {
		struct flm_flow_mtr_handle_s *handle = fh->dev->ndev->flm_mtr_handle;
		struct flm_mtr_stat_s *mtr_stat = handle->port_stats[fh->caller_id]->stats;
		fh->flm_mtr_ids[i] =
			fd->mtr_ids[i] == UINT32_MAX ? 0 : mtr_stat[fd->mtr_ids[i]].flm_id;
	}

	switch (fd->l4_prot) {
	case PROT_L4_TCP:
		fh->flm_prot = 6;
		break;

	case PROT_L4_UDP:
		fh->flm_prot = 17;
		break;

	case PROT_L4_SCTP:
		fh->flm_prot = 132;
		break;

	case PROT_L4_ICMP:
		fh->flm_prot = fd->ip_prot;
		break;

	default:
		switch (fd->tunnel_l4_prot) {
		case PROT_TUN_L4_TCP:
			fh->flm_prot = 6;
			break;

		case PROT_TUN_L4_UDP:
			fh->flm_prot = 17;
			break;

		case PROT_TUN_L4_SCTP:
			fh->flm_prot = 132;
			break;

		case PROT_TUN_L4_ICMP:
			fh->flm_prot = fd->tunnel_ip_prot;
			break;

		default:
			fh->flm_prot = 0;
			break;
		}

		break;
	}

	memcpy(fh->flm_data, packet_data, sizeof(uint32_t) * 10);

	fh->flm_kid = flm_key_id;
	fh->flm_rpl_ext_ptr = rpl_ext_ptr;
	fh->flm_prio = (uint8_t)priority;
	fh->flm_ft = (uint8_t)flm_ft;
	fh->flm_scrub_prof = (uint8_t)flm_scrub;

	for (unsigned int i = 0; i < fd->modify_field_count; ++i) {
		switch (fd->modify_field[i].select) {
		case CPY_SELECT_DSCP_IPV4:
		case CPY_SELECT_DSCP_IPV6:
			if (!has_only_valid_bits_set(fd->modify_field[i].value8, 16, 8)) {
				NT_LOG(ERR, FILTER, "IP DSCP value is out of the range");
				return -1;
			}

			fh->flm_dscp = fd->modify_field[i].value8[0];
			break;

		case CPY_SELECT_RQI_QFI:
			if (!has_only_valid_bits_set(fd->modify_field[i].value8, 16, 6)) {
				NT_LOG(ERR, FILTER, "GTPU QFI value is out of the range");
				return -1;
			}

			fh->flm_rqi = (fd->modify_field[i].value8[0] >> 6) & 0x1;
			fh->flm_qfi = fd->modify_field[i].value8[0] & 0x3f;
			break;

		case CPY_SELECT_IPV4:
			if (!has_only_valid_bits_set(fd->modify_field[i].value8, 16, 32)) {
				NT_LOG(ERR, FILTER, "IPv4 address value is out of the range");
				return -1;
			}

			fh->flm_nat_ipv4 = ntohl(fd->modify_field[i].value32[0]);
			break;

		case CPY_SELECT_PORT:
			if (!has_only_valid_bits_set(fd->modify_field[i].value8, 16, 16)) {
				NT_LOG(ERR, FILTER, "NAT port value is out of the range");
				return -1;
			}

			fh->flm_nat_port = ntohs(fd->modify_field[i].value16[0]);
			break;

		case CPY_SELECT_TEID:
			if (!has_only_valid_bits_set(fd->modify_field[i].value8, 16, 32)) {
				NT_LOG(ERR, FILTER, "GTPU TEID value is out of the range");
				return -1;
			}
			fh->flm_teid = ntohl(fd->modify_field[i].value32[0]);
			break;

		default:
			NT_LOG(DBG, FILTER, "Unknown modify field: %" PRIu32,
				fd->modify_field[i].select);
			break;
		}
	}

	fh->flm_mtu_fragmentation_recipe = fd->flm_mtu_fragmentation_recipe;
	fh->context = fd->age.context;

	return 0;
}

static int convert_fh_to_fh_flm(struct flow_handle *fh, const uint32_t *packet_data,
	uint32_t flm_key_id, uint32_t flm_ft, uint16_t rpl_ext_ptr,
	uint32_t flm_scrub, uint32_t priority)
{
	struct nic_flow_def *fd;
	struct flow_handle fh_copy;

	if (fh->type != FLOW_HANDLE_TYPE_FLOW)
		return -1;

	fh_copy = *fh;
	memset(fh, 0x0, sizeof(struct flow_handle));
	fd = fh_copy.fd;

	fh->type = FLOW_HANDLE_TYPE_FLM;
	fh->caller_id = fh_copy.caller_id;
	fh->dev = fh_copy.dev;
	fh->next = fh_copy.next;
	fh->prev = fh_copy.prev;
	fh->user_data = fh_copy.user_data;

	fh->flm_db_idx_counter = fh_copy.db_idx_counter;

	for (int i = 0; i < RES_COUNT; ++i)
		fh->flm_db_idxs[i] = fh_copy.db_idxs[i];

	if (copy_fd_to_fh_flm(fh, fd, packet_data, flm_key_id, flm_ft, rpl_ext_ptr,
		flm_scrub, priority) < 0) {
		return -1;
	}

	free(fd);

	return 0;
}


static void setup_db_qsl_data(struct nic_flow_def *fd, struct hw_db_inline_qsl_data *qsl_data,
	uint32_t num_dest_port, uint32_t num_queues)
{
	if (qsl_data == NULL)
		return;

	memset(qsl_data, 0x0, sizeof(struct hw_db_inline_qsl_data));

	if (fd->dst_num_avail <= 0) {
		qsl_data->drop = 1;

	} else {
		RTE_ASSERT(fd->dst_num_avail < HW_DB_INLINE_MAX_QST_PER_QSL);

		uint32_t ports[HW_DB_INLINE_MAX_QST_PER_QSL];
		uint32_t queues[HW_DB_INLINE_MAX_QST_PER_QSL];

		uint32_t port_index = 0;
		uint32_t queue_index = 0;
		uint32_t max = num_dest_port > num_queues ? num_dest_port : num_queues;

		memset(ports, 0, sizeof(ports));
		memset(queues, 0, sizeof(queues));

		qsl_data->table_size = max;
		qsl_data->retransmit = num_dest_port > 0 ? 1 : 0;

		for (int i = 0; i < fd->dst_num_avail; ++i)
			if (fd->dst_id[i].type == PORT_PHY)
				ports[port_index++] = fd->dst_id[i].id;

			else if (fd->dst_id[i].type == PORT_VIRT)
				queues[queue_index++] = fd->dst_id[i].id;

		for (uint32_t i = 0; i < max; ++i) {
			if (num_dest_port > 0) {
				qsl_data->table[i].tx_port = ports[i % num_dest_port];
				qsl_data->table[i].tx_port_en = 1;
			}

			if (num_queues > 0) {
				qsl_data->table[i].queue = queues[i % num_queues];
				qsl_data->table[i].queue_en = 1;
			}
		}
	}
}

static void setup_db_hsh_data(struct nic_flow_def *fd, struct hw_db_inline_hsh_data *hsh_data)
{
	if (hsh_data == NULL)
		return;

	memset(hsh_data, 0x0, sizeof(struct hw_db_inline_hsh_data));

	hsh_data->func = fd->hsh.func;
	hsh_data->hash_mask = fd->hsh.types;

	if (fd->hsh.key != NULL) {
		/*
		 * Just a safeguard. Check and error handling of rss_key_len
		 * shall be done at api layers above.
		 */
		memcpy(&hsh_data->key, fd->hsh.key,
			fd->hsh.key_len < MAX_RSS_KEY_LEN ? fd->hsh.key_len : MAX_RSS_KEY_LEN);
	}
}

static int setup_flow_flm_actions(struct flow_eth_dev *dev,
	const struct nic_flow_def *fd,
	const struct hw_db_inline_qsl_data *qsl_data,
	const struct hw_db_inline_hsh_data *hsh_data,
	uint32_t group,
	uint32_t local_idxs[],
	uint32_t *local_idx_counter,
	uint16_t *flm_rpl_ext_ptr,
	uint32_t *flm_ft,
	uint32_t *flm_scrub __rte_unused,
	struct rte_flow_error *error)
{
	const bool empty_pattern = fd_has_empty_pattern(fd);

	/* Setup COT */
	struct hw_db_inline_cot_data cot_data = {
		.matcher_color_contrib = empty_pattern ? 0x0 : 0x4,	/* FT key C */
		.frag_rcp = empty_pattern ? fd->flm_mtu_fragmentation_recipe : 0,
	};
	struct hw_db_cot_idx cot_idx =
		nthw_db_inline_cot_add(dev->ndev, dev->ndev->hw_db_handle, &cot_data);
	local_idxs[(*local_idx_counter)++] = cot_idx.raw;

	if (cot_idx.error) {
		NT_LOG(ERR, FILTER, "Could not reference COT resource");
		nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		return -1;
	}

	/* Finalize QSL */
	struct hw_db_qsl_idx qsl_idx =
		nthw_db_inline_qsl_add(dev->ndev, dev->ndev->hw_db_handle, qsl_data);
	local_idxs[(*local_idx_counter)++] = qsl_idx.raw;

	if (qsl_idx.error) {
		NT_LOG(ERR, FILTER, "Could not reference QSL resource");
		nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		return -1;
	}

	/* Setup HSH */
	struct hw_db_hsh_idx hsh_idx =
		nthw_db_inline_hsh_add(dev->ndev, dev->ndev->hw_db_handle, hsh_data);
	local_idxs[(*local_idx_counter)++] = hsh_idx.raw;

	if (hsh_idx.error) {
		NT_LOG(ERR, FILTER, "Could not reference HSH resource");
		nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		return -1;
	}

	/* Setup SLC LR */
	struct hw_db_slc_lr_idx slc_lr_idx = { .raw = 0 };

	if (fd->header_strip_end_dyn != 0 || fd->header_strip_end_ofs != 0) {
		struct hw_db_inline_slc_lr_data slc_lr_data = {
			.head_slice_en = 1,
			.head_slice_dyn = fd->header_strip_end_dyn,
			.head_slice_ofs = fd->header_strip_end_ofs,
		};
		slc_lr_idx =
			nthw_db_inline_slc_lr_add(dev->ndev, dev->ndev->hw_db_handle, &slc_lr_data);
		local_idxs[(*local_idx_counter)++] = slc_lr_idx.raw;

		if (slc_lr_idx.error) {
			NT_LOG(ERR, FILTER, "Could not reference SLC LR resource");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			return -1;
		}
	}

	/* Setup TPE EXT */
	if (fd->tun_hdr.len > 0) {
		RTE_ASSERT(fd->tun_hdr.len <= HW_DB_INLINE_MAX_ENCAP_SIZE);

		struct hw_db_inline_tpe_ext_data tpe_ext_data = {
			.size = fd->tun_hdr.len,
		};

		memset(tpe_ext_data.hdr8, 0x0, HW_DB_INLINE_MAX_ENCAP_SIZE);
		memcpy(tpe_ext_data.hdr8, fd->tun_hdr.d.hdr8, (fd->tun_hdr.len + 15) & ~15);

		struct hw_db_tpe_ext_idx tpe_ext_idx =
			nthw_db_inline_tpe_ext_add(dev->ndev, dev->ndev->hw_db_handle,
			&tpe_ext_data);
		local_idxs[(*local_idx_counter)++] = tpe_ext_idx.raw;

		if (tpe_ext_idx.error) {
			NT_LOG(ERR, FILTER, "Could not reference TPE EXT resource");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			return -1;
		}

		if (flm_rpl_ext_ptr)
			*flm_rpl_ext_ptr = tpe_ext_idx.ids;
	}

	/* Setup TPE */
	RTE_ASSERT(fd->modify_field_count <= 6);

	struct hw_db_inline_tpe_data tpe_data = {
		.insert_len = fd->tun_hdr.len,
		.new_outer = fd->tun_hdr.new_outer,
		.calc_eth_type_from_inner_ip =
			!fd->tun_hdr.new_outer && fd->header_strip_end_dyn == DYN_TUN_L3,
		.ttl_en = fd->ttl_sub_enable,
		.ttl_dyn = fd->ttl_sub_outer ? DYN_L3 : DYN_TUN_L3,
		.ttl_ofs = fd->ttl_sub_ipv4 ? 8 : 7,
	};

	for (unsigned int i = 0; i < fd->modify_field_count; ++i) {
		tpe_data.writer[i].en = 1;
		tpe_data.writer[i].reader_select = fd->modify_field[i].select;
		tpe_data.writer[i].dyn = fd->modify_field[i].dyn;
		tpe_data.writer[i].ofs = fd->modify_field[i].ofs;
		tpe_data.writer[i].len = fd->modify_field[i].len;
	}

	if (fd->tun_hdr.new_outer) {
		const int fcs_length = 4;

		/* L4 length */
		tpe_data.len_a_en = 1;
		tpe_data.len_a_pos_dyn = DYN_L4;
		tpe_data.len_a_pos_ofs = 4;
		tpe_data.len_a_add_dyn = 18;
		tpe_data.len_a_add_ofs = (uint32_t)(-fcs_length) & 0xff;
		tpe_data.len_a_sub_dyn = DYN_L4;

		/* L3 length */
		tpe_data.len_b_en = 1;
		tpe_data.len_b_pos_dyn = DYN_L3;
		tpe_data.len_b_pos_ofs = fd->tun_hdr.ip_version == 4 ? 2 : 4;
		tpe_data.len_b_add_dyn = 18;
		tpe_data.len_b_add_ofs = (uint32_t)(-fcs_length) & 0xff;
		tpe_data.len_b_sub_dyn = DYN_L3;

		/* GTP length */
		tpe_data.len_c_en = 1;
		tpe_data.len_c_pos_dyn = DYN_L4_PAYLOAD;
		tpe_data.len_c_pos_ofs = 2;
		tpe_data.len_c_add_dyn = 18;
		tpe_data.len_c_add_ofs = (uint32_t)(-8 - fcs_length) & 0xff;
		tpe_data.len_c_sub_dyn = DYN_L4_PAYLOAD;
	}

	struct hw_db_tpe_idx tpe_idx =
		nthw_db_inline_tpe_add(dev->ndev, dev->ndev->hw_db_handle, &tpe_data);

	local_idxs[(*local_idx_counter)++] = tpe_idx.raw;

	if (tpe_idx.error) {
		NT_LOG(ERR, FILTER, "Could not reference TPE resource");
		nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		return -1;
	}

	/* Setup SCRUB profile */
	struct hw_db_inline_scrub_data scrub_data = { .timeout = fd->age.timeout };
	struct hw_db_flm_scrub_idx scrub_idx =
		nthw_db_inline_scrub_add(dev->ndev, dev->ndev->hw_db_handle, &scrub_data);
	local_idxs[(*local_idx_counter)++] = scrub_idx.raw;

	if (scrub_idx.error) {
		NT_LOG(ERR, FILTER, "Could not reference FLM SCRUB resource");
		nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		return -1;
	}

	if (flm_scrub)
		*flm_scrub = scrub_idx.ids;

	/* Setup Action Set */
	struct hw_db_inline_action_set_data action_set_data = {
		.contains_jump = 0,
		.cot = cot_idx,
		.qsl = qsl_idx,
		.slc_lr = slc_lr_idx,
		.tpe = tpe_idx,
		.hsh = hsh_idx,
		.scrub = scrub_idx,
	};
	struct hw_db_action_set_idx action_set_idx =
		nthw_db_inline_action_set_add(dev->ndev, dev->ndev->hw_db_handle, &action_set_data);
	local_idxs[(*local_idx_counter)++] = action_set_idx.raw;

	if (action_set_idx.error) {
		NT_LOG(ERR, FILTER, "Could not reference Action Set resource");
		nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		return -1;
	}

	/* Setup FLM FT */
	struct hw_db_inline_flm_ft_data flm_ft_data = {
		.is_group_zero = 0,
		.group = group,
		.action_set = action_set_idx,
	};
	struct hw_db_flm_ft flm_ft_idx = empty_pattern
		? nthw_db_inline_flm_ft_default(dev->ndev, dev->ndev->hw_db_handle, &flm_ft_data)
		: nthw_db_inline_flm_ft_add(dev->ndev, dev->ndev->hw_db_handle, &flm_ft_data);
	local_idxs[(*local_idx_counter)++] = flm_ft_idx.raw;

	if (flm_ft_idx.error) {
		NT_LOG(ERR, FILTER, "Could not reference FLM FT resource");
		nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		return -1;
	}

	if (flm_ft)
		*flm_ft = flm_ft_idx.id1;

	return 0;
}

static struct flow_handle *create_flow_filter(struct flow_eth_dev *dev, struct nic_flow_def *fd,
	const struct rte_flow_attr *attr,
	uint16_t forced_vlan_vid __rte_unused, uint16_t caller_id,
	struct rte_flow_error *error, uint32_t port_id,
	uint32_t num_dest_port, uint32_t num_queues,
	uint32_t *packet_data, uint32_t *packet_mask,
	struct flm_flow_key_def_s *key_def)
{
	struct flow_handle *fh = calloc(1, sizeof(struct flow_handle));

	if (fh == NULL) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Failed to allocate flow_handle";
		goto error_out;
	}

	fh->type = FLOW_HANDLE_TYPE_FLOW;
	fh->port_id = port_id;
	fh->dev = dev;
	fh->fd = fd;
	fh->caller_id = caller_id;

	struct hw_db_inline_qsl_data qsl_data;
	setup_db_qsl_data(fd, &qsl_data, num_dest_port, num_queues);

	struct hw_db_inline_hsh_data hsh_data;
	setup_db_hsh_data(fd, &hsh_data);

	if (attr->group > 0 && fd_has_empty_pattern(fd)) {
		/*
		 * Default flow for group 1..32
		 */

		if (setup_flow_flm_actions(dev, fd, &qsl_data, &hsh_data, attr->group, fh->db_idxs,
			&fh->db_idx_counter, NULL, NULL, NULL, error)) {
			goto error_out;
		}

		fh->context = fd->age.context;
		nic_insert_flow(dev->ndev, fh);

	} else if (attr->group > 0) {
		/*
		 * Flow for group 1..32
		 */

		/* Setup FLM RCP */
		struct hw_db_inline_flm_rcp_data flm_data = {
			.qw0_dyn = key_def->qw0_dyn,
			.qw0_ofs = key_def->qw0_ofs,
			.qw4_dyn = key_def->qw4_dyn,
			.qw4_ofs = key_def->qw4_ofs,
			.sw8_dyn = key_def->sw8_dyn,
			.sw8_ofs = key_def->sw8_ofs,
			.sw9_dyn = key_def->sw9_dyn,
			.sw9_ofs = key_def->sw9_ofs,
			.outer_prot = key_def->outer_proto,
			.inner_prot = key_def->inner_proto,
		};
		memcpy(flm_data.mask, packet_mask, sizeof(uint32_t) * 10);
		struct hw_db_flm_idx flm_idx =
			nthw_db_inline_flm_add(dev->ndev, dev->ndev->hw_db_handle, &flm_data,
			attr->group);
		fh->db_idxs[fh->db_idx_counter++] = flm_idx.raw;

		if (flm_idx.error) {
			NT_LOG(ERR, FILTER, "Could not reference FLM RPC resource");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			goto error_out;
		}

		/* Setup Actions */
		uint16_t flm_rpl_ext_ptr = 0;
		uint32_t flm_ft = 0;
		uint32_t flm_scrub = 0;

		if (setup_flow_flm_actions(dev, fd, &qsl_data, &hsh_data, attr->group, fh->db_idxs,
			&fh->db_idx_counter, &flm_rpl_ext_ptr, &flm_ft,
			&flm_scrub, error)) {
			goto error_out;
		}

		/* Program flow */
		if (convert_fh_to_fh_flm(fh, packet_data, flm_idx.id1 + 2, flm_ft, flm_rpl_ext_ptr,
				flm_scrub, attr->priority & 0x3) != 0) {
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			goto error_out;
		}
		flm_flow_programming(fh, NT_FLM_OP_LEARN);

		nic_insert_flow_flm(dev->ndev, fh);

	} else {
		/*
		 * Flow for group 0
		 */
		int identical_km_entry_ft = -1;

		/* Setup Action Set */

		/* SCRUB/AGE action is not supported for group 0 */
		if (fd->age.timeout != 0 || fd->age.context != NULL) {
			NT_LOG(ERR, FILTER, "Action AGE is not supported for flow in group 0");
			nthw_flow_nic_set_error(ERR_ACTION_AGE_UNSUPPORTED_GROUP_0, error);
			goto error_out;
		}

		/* NOTE: SCRUB record 0 is used by default with timeout 0, i.e. flow will never
		 * AGE-out
		 */
		struct hw_db_inline_action_set_data action_set_data = { 0 };
		(void)action_set_data;

		if (fd->jump_to_group != UINT32_MAX) {
			/* Action Set only contains jump */
			action_set_data.contains_jump = 1;
			action_set_data.jump = fd->jump_to_group;

		} else {
			/* Action Set doesn't contain jump */
			action_set_data.contains_jump = 0;

			/* Group 0 supports only modify action for TTL/Hop limit. */
			if (fd->modify_field_count > 0) {
				NT_LOG(ERR, FILTER, "Unsupported MODIFY ACTION for group 0");
				nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
				goto error_out;
			}

			/* Setup COT */
			struct hw_db_inline_cot_data cot_data = {
				.matcher_color_contrib = 0,
				.frag_rcp = fd->flm_mtu_fragmentation_recipe,
			};
			struct hw_db_cot_idx cot_idx =
				nthw_db_inline_cot_add(dev->ndev, dev->ndev->hw_db_handle,
				&cot_data);
			fh->db_idxs[fh->db_idx_counter++] = cot_idx.raw;
			action_set_data.cot = cot_idx;

			if (cot_idx.error) {
				NT_LOG(ERR, FILTER, "Could not reference COT resource");
				nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
				goto error_out;
			}

			/* Finalize QSL */
			struct hw_db_qsl_idx qsl_idx =
				nthw_db_inline_qsl_add(dev->ndev, dev->ndev->hw_db_handle,
				&qsl_data);
			fh->db_idxs[fh->db_idx_counter++] = qsl_idx.raw;
			action_set_data.qsl = qsl_idx;

			if (qsl_idx.error) {
				NT_LOG(ERR, FILTER, "Could not reference QSL resource");
				nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
				goto error_out;
			}

			/* Setup HSH */
			struct hw_db_hsh_idx hsh_idx =
				nthw_db_inline_hsh_add(dev->ndev, dev->ndev->hw_db_handle,
				&hsh_data);
			fh->db_idxs[fh->db_idx_counter++] = hsh_idx.raw;
			action_set_data.hsh = hsh_idx;

			if (hsh_idx.error) {
				NT_LOG(ERR, FILTER, "Could not reference HSH resource");
				nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
				goto error_out;
			}

			/* Setup TPE */
			if (fd->ttl_sub_enable) {
				struct hw_db_inline_tpe_data tpe_data = {
					.insert_len = fd->tun_hdr.len,
					.new_outer = fd->tun_hdr.new_outer,
					.calc_eth_type_from_inner_ip = !fd->tun_hdr.new_outer &&
						fd->header_strip_end_dyn == DYN_TUN_L3,
					.ttl_en = fd->ttl_sub_enable,
					.ttl_dyn = fd->ttl_sub_outer ? DYN_L3 : DYN_TUN_L3,
					.ttl_ofs = fd->ttl_sub_ipv4 ? 8 : 7,
				};
				struct hw_db_tpe_idx tpe_idx =
					nthw_db_inline_tpe_add(dev->ndev, dev->ndev->hw_db_handle,
					&tpe_data);
				fh->db_idxs[fh->db_idx_counter++] = tpe_idx.raw;
				action_set_data.tpe = tpe_idx;

				if (tpe_idx.error) {
					NT_LOG(ERR, FILTER, "Could not reference TPE resource");
					nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION,
						error);
					goto error_out;
				}
			}
		}

		struct hw_db_action_set_idx action_set_idx =
			nthw_db_inline_action_set_add(dev->ndev, dev->ndev->hw_db_handle,
			&action_set_data);

		fh->db_idxs[fh->db_idx_counter++] = action_set_idx.raw;

		if (action_set_idx.error) {
			NT_LOG(ERR, FILTER, "Could not reference Action Set resource");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			goto error_out;
		}

		/* Setup CAT */
		struct hw_db_inline_cat_data cat_data = {
			.vlan_mask = (0xf << fd->vlans) & 0xf,
			.mac_port_mask = 1 << fh->port_id,
			.ptc_mask_frag = fd->fragmentation,
			.ptc_mask_l2 = fd->l2_prot != -1 ? (1 << fd->l2_prot) : -1,
			.ptc_mask_l3 = fd->l3_prot != -1 ? (1 << fd->l3_prot) : -1,
			.ptc_mask_l4 = fd->l4_prot != -1 ? (1 << fd->l4_prot) : -1,
			.err_mask_ttl = (fd->ttl_sub_enable &&
				fd->ttl_sub_outer) ? -1 : 0x1,
			.ptc_mask_tunnel = fd->tunnel_prot !=
			-1 ? (1 << (fd->tunnel_prot > 10 ? 10 : fd->tunnel_prot)) : -1,
			.ptc_mask_l3_tunnel =
				fd->tunnel_l3_prot != -1 ? (1 << fd->tunnel_l3_prot) : -1,
			.ptc_mask_l4_tunnel =
				fd->tunnel_l4_prot != -1 ? (1 << fd->tunnel_l4_prot) : -1,
			.err_mask_ttl_tunnel =
				(fd->ttl_sub_enable && !fd->ttl_sub_outer) ? -1 : 0x1,
			.ip_prot = fd->ip_prot,
			.ip_prot_tunnel = fd->tunnel_ip_prot,
		};
		struct hw_db_cat_idx cat_idx =
			nthw_db_inline_cat_add(dev->ndev, dev->ndev->hw_db_handle, &cat_data);
		fh->db_idxs[fh->db_idx_counter++] = cat_idx.raw;

		if (cat_idx.error) {
			NT_LOG(ERR, FILTER, "Could not reference CAT resource");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			goto error_out;
		}

		/* Setup KM RCP */
		struct hw_db_inline_km_rcp_data km_rcp_data = { .rcp = 0 };

		if (fd->km.num_ftype_elem) {
			struct flow_handle *flow = dev->ndev->flow_base, *found_flow = NULL;

			if (nthw_km_key_create(&fd->km, fh->port_id)) {
				NT_LOG(ERR, FILTER, "KM creation failed");
				nthw_flow_nic_set_error(ERR_MATCH_FAILED_BY_HW_LIMITS, error);
				goto error_out;
			}

			fd->km.be = &dev->ndev->be;

			/* Look for existing KM RCPs */
			while (flow) {
				if (flow->type == FLOW_HANDLE_TYPE_FLOW &&
					flow->fd->km.flow_type) {
					int res = nthw_km_key_compare(&fd->km, &flow->fd->km);

					if (res < 0) {
						/* Flow rcp and match data is identical */
						identical_km_entry_ft = flow->fd->km.flow_type;
						found_flow = flow;
						break;
					}

					if (res > 0) {
						/* Flow rcp found and match data is different */
						found_flow = flow;
					}
				}

				flow = flow->next;
			}

			nthw_km_attach_ndev_resource_management(&fd->km, &dev->ndev->km_res_handle);

			if (found_flow != NULL) {
				/* Reuse existing KM RCP */
				const struct hw_db_inline_km_rcp_data *other_km_rcp_data =
					nthw_db_inline_find_data(dev->ndev, dev->ndev->hw_db_handle,
					HW_DB_IDX_TYPE_KM_RCP,
					(struct hw_db_idx *)
					found_flow->flm_db_idxs,
					found_flow->flm_db_idx_counter);

				if (other_km_rcp_data == NULL ||
					nthw_flow_nic_ref_resource(dev->ndev, RES_KM_CATEGORY,
					other_km_rcp_data->rcp)) {
					NT_LOG(ERR, FILTER,
						"Could not reference existing KM RCP resource");
					nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION,
						error);
					goto error_out;
				}

				km_rcp_data.rcp = other_km_rcp_data->rcp;
			} else {
				/* Alloc new KM RCP */
				int rcp = nthw_flow_nic_alloc_resource(dev->ndev,
					RES_KM_CATEGORY, 1);

				if (rcp < 0) {
					NT_LOG(ERR, FILTER,
						"Could not reference KM RCP resource (flow_nic_alloc)");
					nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION,
						error);
					goto error_out;
				}

				nthw_km_rcp_set(&fd->km, rcp);
				km_rcp_data.rcp = (uint32_t)rcp;
			}
		}

		struct hw_db_km_idx km_idx =
			nthw_db_inline_km_add(dev->ndev, dev->ndev->hw_db_handle, &km_rcp_data);

		fh->db_idxs[fh->db_idx_counter++] = km_idx.raw;

		if (km_idx.error) {
			NT_LOG(ERR, FILTER, "Could not reference KM RCP resource (db_inline)");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			goto error_out;
		}

		/* Setup KM FT */
		struct hw_db_inline_km_ft_data km_ft_data = {
			.cat = cat_idx,
			.km = km_idx,
			.action_set = action_set_idx,
		};
		struct hw_db_km_ft km_ft_idx =
			nthw_db_inline_km_ft_add(dev->ndev, dev->ndev->hw_db_handle, &km_ft_data);
		fh->db_idxs[fh->db_idx_counter++] = km_ft_idx.raw;

		if (km_ft_idx.error) {
			NT_LOG(ERR, FILTER, "Could not reference KM FT resource");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			goto error_out;
		}

		/* Finalize KM RCP */
		if (fd->km.num_ftype_elem) {
			if (identical_km_entry_ft >= 0 && identical_km_entry_ft != km_ft_idx.id1) {
				NT_LOG(ERR, FILTER,
					"Identical KM matches cannot have different KM FTs");
				nthw_flow_nic_set_error(ERR_MATCH_FAILED_BY_HW_LIMITS, error);
				goto error_out;
			}

			fd->km.flow_type = km_ft_idx.id1;

			if (fd->km.target == KM_CAM) {
				uint32_t ft_a_mask = 0;
				nthw_mod_km_rcp_get(&dev->ndev->be, HW_KM_RCP_FTM_A,
					(int)km_rcp_data.rcp, 0, &ft_a_mask);
				nthw_mod_km_rcp_set(&dev->ndev->be, HW_KM_RCP_FTM_A,
					(int)km_rcp_data.rcp, 0,
					ft_a_mask | (1 << fd->km.flow_type));
			}

			nthw_mod_km_rcp_flush(&dev->ndev->be, (int)km_rcp_data.rcp, 1);

			nthw_km_write_data_match_entry(&fd->km, 0);
		}

		/* Setup Match Set */
		struct hw_db_inline_match_set_data match_set_data = {
			.cat = cat_idx,
			.km = km_idx,
			.km_ft = km_ft_idx,
			.action_set = action_set_idx,
			.jump = fd->jump_to_group != UINT32_MAX ? fd->jump_to_group : 0,
			.priority = attr->priority & 0xff,
		};
		struct hw_db_match_set_idx match_set_idx =
			nthw_db_inline_match_set_add(dev->ndev, dev->ndev->hw_db_handle,
			&match_set_data);
		fh->db_idxs[fh->db_idx_counter++] = match_set_idx.raw;

		if (match_set_idx.error) {
			NT_LOG(ERR, FILTER, "Could not reference Match Set resource");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			goto error_out;
		}

		/* Setup FLM FT */
		struct hw_db_inline_flm_ft_data flm_ft_data = {
			.is_group_zero = 1,
			.jump = fd->jump_to_group != UINT32_MAX ? fd->jump_to_group : 0,
			.action_set = action_set_idx,

		};
		struct hw_db_flm_ft flm_ft_idx =
			nthw_db_inline_flm_ft_add(dev->ndev, dev->ndev->hw_db_handle, &flm_ft_data);
		fh->db_idxs[fh->db_idx_counter++] = flm_ft_idx.raw;

		if (flm_ft_idx.error) {
			NT_LOG(ERR, FILTER, "Could not reference FLM FT resource");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			goto error_out;
		}

		nic_insert_flow(dev->ndev, fh);
	}

	return fh;

error_out:

	if (fh->type == FLOW_HANDLE_TYPE_FLM) {
		nthw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
			(struct hw_db_idx *)fh->flm_db_idxs,
			fh->flm_db_idx_counter);

	} else {
		nthw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
			(struct hw_db_idx *)fh->db_idxs, fh->db_idx_counter);
	}

	free(fh);

	return NULL;
}

/*
 * Public functions
 */

int nthw_init_flow_mgmnt_of_ndev_profile_inline(struct flow_nic_dev *ndev)
{
	if (!ndev->flow_mgnt_prepared) {
		/* Check static arrays are big enough */
		RTE_ASSERT(ndev->be.tpe.nb_cpy_writers <= MAX_CPY_WRITERS_SUPPORTED);
		/* KM Flow Type 0 is reserved */
		flow_nic_mark_resource_used(ndev, RES_KM_FLOW_TYPE, 0);
		flow_nic_mark_resource_used(ndev, RES_KM_CATEGORY, 0);

		/* Reserved FLM Flow Types */
		flow_nic_mark_resource_used(ndev, RES_FLM_FLOW_TYPE, NT_FLM_MISS_FLOW_TYPE);
		flow_nic_mark_resource_used(ndev, RES_FLM_FLOW_TYPE, NT_FLM_UNHANDLED_FLOW_TYPE);
		flow_nic_mark_resource_used(ndev, RES_FLM_FLOW_TYPE,
			NT_FLM_VIOLATING_MBR_FLOW_TYPE);
		flow_nic_mark_resource_used(ndev, RES_FLM_RCP, 0);

		/* COT is locked to CFN. Don't set color for CFN 0 */
		nthw_mod_cat_cot_set(&ndev->be, HW_CAT_COT_PRESET_ALL, 0, 0);

		if (nthw_mod_cat_cot_flush(&ndev->be, 0, 1) < 0)
			goto err_exit0;

		/* Initialize QSL with unmatched recipe index 0 - discard */
		if (nthw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_DISCARD, 0, 0x1) < 0)
			goto err_exit0;

		if (nthw_mod_qsl_rcp_flush(&ndev->be, 0, 1) < 0)
			goto err_exit0;

		flow_nic_mark_resource_used(ndev, RES_QSL_RCP, 0);

		/* Initialize QST with default index 0 */
		if (nthw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_PRESET_ALL, 0, 0x0) < 0)
			goto err_exit0;

		if (nthw_mod_qsl_qst_flush(&ndev->be, 0, 1) < 0)
			goto err_exit0;

		flow_nic_mark_resource_used(ndev, RES_QSL_QST, 0);

		/* SLC LR & TPE index 0 were reserved */
		flow_nic_mark_resource_used(ndev, RES_SLC_LR_RCP, 0);
		flow_nic_mark_resource_used(ndev, RES_TPE_RCP, 0);
		flow_nic_mark_resource_used(ndev, RES_TPE_EXT, 0);
		flow_nic_mark_resource_used(ndev, RES_TPE_RPL, 0);

		/* PDB setup Direct Virtio Scatter-Gather descriptor of 12 bytes for its recipe 0
		 */
		if (nthw_mod_pdb_rcp_set(&ndev->be, HW_PDB_RCP_DESCRIPTOR, 0, 7) < 0)
			goto err_exit0;

		if (nthw_mod_pdb_rcp_set(&ndev->be, HW_PDB_RCP_DESC_LEN, 0, 6) < 0)
			goto err_exit0;

		if (nthw_mod_pdb_rcp_flush(&ndev->be, 0, 1) < 0)
			goto err_exit0;

		flow_nic_mark_resource_used(ndev, RES_PDB_RCP, 0);

		/* Set default hasher recipe to 5-tuple:
		 * RTE_ETH_RSS_IPV6 - enables hashing on both IPv4/IPv6 SA and DA
		 * RTE_ETH_RSS_PORT - enables hashing on both L4 SP and DP and L4 protocol type
		 */
		struct nt_eth_rss_conf hsh_5_tuple = {
			.rss_key = {},
			.rss_hf = RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_PORT,
			.algorithm = 0,
		};
		nthw_hsh_set(ndev, 0, hsh_5_tuple);
		nthw_mod_hsh_rcp_flush(&ndev->be, 0, 1);

		flow_nic_mark_resource_used(ndev, RES_HSH_RCP, 0);

		/* Initialize SCRUB with default index 0, i.e. flow will never AGE-out */
		if (nthw_mod_flm_scrub_set(&ndev->be, HW_FLM_SCRUB_PRESET_ALL, 0, 0) < 0)
			goto err_exit0;

		if (nthw_mod_flm_scrub_flush(&ndev->be, 0, 1) < 0)
			goto err_exit0;

		flow_nic_mark_resource_used(ndev, RES_SCRUB_RCP, 0);

		/* Setup filter using matching all packets violating traffic policing parameters */
		flow_nic_mark_resource_used(ndev, RES_CAT_CFN, NT_VIOLATING_MBR_CFN);
		flow_nic_mark_resource_used(ndev, RES_QSL_RCP, NT_VIOLATING_MBR_QSL);

		if (nthw_db_inline_setup_mbr_filter(ndev, NT_VIOLATING_MBR_CFN,
			NT_FLM_VIOLATING_MBR_FLOW_TYPE,
			NT_VIOLATING_MBR_QSL) < 0)
			goto err_exit0;

		/* FLM */
		if (flm_sdram_calibrate(ndev) < 0)
			goto err_exit0;

		if (flm_sdram_reset(ndev, 1) < 0)
			goto err_exit0;

		/* Learn done status */
		nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_LDS, 0);
		/* Learn fail status */
		nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_LFS, 1);
		/* Learn ignore status */
		nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_LIS, 1);
		/* Unlearn done status */
		nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_UDS, 0);
		/* Unlearn ignore status */
		nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_UIS, 0);
		/* Relearn done status */
		nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_RDS, 0);
		/* Relearn ignore status */
		nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_RIS, 0);
		nthw_mod_flm_control_set(&ndev->be, HW_FLM_CONTROL_RBL, 4);
		nthw_mod_flm_control_flush(&ndev->be);

		/* Set the sliding windows size for flm load */
		uint32_t bin = (uint32_t)(((FLM_LOAD_WINDOWS_SIZE * 1000000000000ULL) /
			(32ULL * ndev->be.flm.nb_rpp_clock_in_ps)) -
			1ULL);
		nthw_mod_flm_load_bin_set(&ndev->be, HW_FLM_LOAD_BIN, bin);
		nthw_mod_flm_load_bin_flush(&ndev->be);

		nthw_mod_flm_prio_set(&ndev->be, HW_FLM_PRIO_LIMIT0,
			0);	/* Drop at 100% FIFO fill level */
		nthw_mod_flm_prio_set(&ndev->be, HW_FLM_PRIO_FT0, 1);
		nthw_mod_flm_prio_set(&ndev->be, HW_FLM_PRIO_LIMIT1,
			14);	/* Drop at 87,5% FIFO fill level */
		nthw_mod_flm_prio_set(&ndev->be, HW_FLM_PRIO_FT1, 1);
		nthw_mod_flm_prio_set(&ndev->be, HW_FLM_PRIO_LIMIT2,
			10);	/* Drop at 62,5% FIFO fill level */
		nthw_mod_flm_prio_set(&ndev->be, HW_FLM_PRIO_FT2, 1);
		nthw_mod_flm_prio_set(&ndev->be, HW_FLM_PRIO_LIMIT3,
			6);	/* Drop at 37,5% FIFO fill level */
		nthw_mod_flm_prio_set(&ndev->be, HW_FLM_PRIO_FT3, 1);
		nthw_mod_flm_prio_flush(&ndev->be);

		/* TODO How to set and use these limits */
		for (uint32_t i = 0; i < ndev->be.flm.nb_pst_profiles; ++i) {
			nthw_mod_flm_pst_set(&ndev->be, HW_FLM_PST_BP, i,
				NTNIC_FLOW_PERIODIC_STATS_BYTE_LIMIT);
			nthw_mod_flm_pst_set(&ndev->be, HW_FLM_PST_PP, i,
				NTNIC_FLOW_PERIODIC_STATS_PKT_LIMIT);
			nthw_mod_flm_pst_set(&ndev->be, HW_FLM_PST_TP, i,
				NTNIC_FLOW_PERIODIC_STATS_BYTE_TIMEOUT);
		}

		nthw_mod_flm_pst_flush(&ndev->be, 0, ALL_ENTRIES);

		ndev->id_table_handle = ntnic_id_table_create();

		if (ndev->id_table_handle == NULL)
			goto err_exit0;

		ndev->flm_mtr_handle = calloc(1, sizeof(struct flm_flow_mtr_handle_s));
		struct flm_mtr_shared_stats_s *flm_shared_stats =
			calloc(1, sizeof(struct flm_mtr_shared_stats_s));
		struct flm_mtr_stat_s *flm_stats =
			calloc(FLM_MTR_STAT_SIZE, sizeof(struct flm_mtr_stat_s));

		if (ndev->flm_mtr_handle == NULL || flm_shared_stats == NULL ||
			flm_stats == NULL) {
			free(ndev->flm_mtr_handle);
			free(flm_shared_stats);
			free(flm_stats);
			goto err_exit0;
		}

		for (uint32_t i = 0; i < UINT8_MAX; ++i) {
			((struct flm_flow_mtr_handle_s *)ndev->flm_mtr_handle)->port_stats[i] =
				flm_shared_stats;
		}

		flm_shared_stats->stats = flm_stats;
		flm_shared_stats->size = FLM_MTR_STAT_SIZE;
		flm_shared_stats->shared = UINT8_MAX;

		if (nthw_flow_group_handle_create(&ndev->group_handle, ndev->be.flm.nb_categories))
			goto err_exit0;

		if (nthw_db_inline_create(ndev, &ndev->hw_db_handle))
			goto err_exit0;

		ndev->flow_mgnt_prepared = 1;
	}

	return 0;

err_exit0:
	nthw_done_flow_mgmnt_of_ndev_profile_inline(ndev);
	return -1;
}

int nthw_done_flow_mgmnt_of_ndev_profile_inline(struct flow_nic_dev *ndev)
{
#ifdef FLOW_DEBUG
	ndev->be.iface->set_debug_mode(ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_WRITE);
#endif

	if (ndev->flow_mgnt_prepared) {
		flm_sdram_reset(ndev, 0);

		nthw_flow_nic_free_resource(ndev, RES_KM_FLOW_TYPE, 0);
		nthw_flow_nic_free_resource(ndev, RES_KM_CATEGORY, 0);

		nthw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_PRESET_ALL, 0, 0);
		nthw_mod_flm_rcp_flush(&ndev->be, 0, 1);
		nthw_flow_nic_free_resource(ndev, RES_FLM_FLOW_TYPE, 0);
		nthw_flow_nic_free_resource(ndev, RES_FLM_FLOW_TYPE, 1);
		nthw_flow_nic_free_resource(ndev, RES_FLM_RCP, 0);

		for (uint32_t i = 0; i < UINT8_MAX; ++i) {
			struct flm_flow_mtr_handle_s *handle = ndev->flm_mtr_handle;
			handle->port_stats[i]->shared -= 1;

			if (handle->port_stats[i]->shared == 0) {
				free(handle->port_stats[i]->stats);
				free(handle->port_stats[i]);
			}
		}

		free(ndev->flm_mtr_handle);

		nthw_flow_group_handle_destroy(&ndev->group_handle);
		ntnic_id_table_destroy(ndev->id_table_handle);

		nthw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PRESET_ALL, 0, 0, 0);
		nthw_mod_cat_cfn_flush(&ndev->be, 0, 1);
		nthw_mod_cat_cot_set(&ndev->be, HW_CAT_COT_PRESET_ALL, 0, 0);
		nthw_mod_cat_cot_flush(&ndev->be, 0, 1);
		nthw_flow_nic_free_resource(ndev, RES_CAT_CFN, 0);

		nthw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_PRESET_ALL, 0, 0);
		nthw_mod_qsl_rcp_flush(&ndev->be, 0, 1);
		nthw_flow_nic_free_resource(ndev, RES_QSL_RCP, 0);

		nthw_mod_slc_lr_rcp_set(&ndev->be, HW_SLC_LR_RCP_PRESET_ALL, 0, 0);
		nthw_mod_slc_lr_rcp_flush(&ndev->be, 0, 1);
		nthw_flow_nic_free_resource(ndev, RES_SLC_LR_RCP, 0);

		nthw_mod_tpe_reset(&ndev->be);
		nthw_flow_nic_free_resource(ndev, RES_TPE_RCP, 0);
		nthw_flow_nic_free_resource(ndev, RES_TPE_EXT, 0);
		nthw_flow_nic_free_resource(ndev, RES_TPE_RPL, 0);

		nthw_mod_pdb_rcp_set(&ndev->be, HW_PDB_RCP_PRESET_ALL, 0, 0);
		nthw_mod_pdb_rcp_flush(&ndev->be, 0, 1);
		nthw_flow_nic_free_resource(ndev, RES_PDB_RCP, 0);

		nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_PRESET_ALL, 0, 0, 0);
		nthw_mod_hsh_rcp_flush(&ndev->be, 0, 1);
		nthw_flow_nic_free_resource(ndev, RES_HSH_RCP, 0);

		nthw_mod_flm_scrub_set(&ndev->be, HW_FLM_SCRUB_PRESET_ALL, 0, 0);
		nthw_mod_flm_scrub_flush(&ndev->be, 0, 1);
		nthw_flow_nic_free_resource(ndev, RES_SCRUB_RCP, 0);

		nthw_db_inline_destroy(ndev->hw_db_handle);

#ifdef FLOW_DEBUG
		ndev->be.iface->set_debug_mode(ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_NONE);
#endif

		ndev->flow_mgnt_prepared = 0;
	}

	return 0;
}

struct flow_handle *nthw_flow_create_profile_inline(struct flow_eth_dev *dev __rte_unused,
	const struct rte_flow_attr *attr __rte_unused,
	uint16_t forced_vlan_vid __rte_unused,
	uint16_t caller_id __rte_unused,
	const struct rte_flow_item elem[] __rte_unused,
	const struct rte_flow_action action[] __rte_unused,
	struct rte_flow_error *error __rte_unused)
{
	struct flow_handle *fh = NULL;
	int res;

	uint32_t port_id = UINT32_MAX;
	uint32_t num_dest_port;
	uint32_t num_queues;

	uint32_t packet_data[10];
	uint32_t packet_mask[10];
	struct flm_flow_key_def_s key_def;

	struct rte_flow_attr attr_local;
	memcpy(&attr_local, attr, sizeof(struct rte_flow_attr));
	uint16_t forced_vlan_vid_local = forced_vlan_vid;
	uint16_t caller_id_local = caller_id;

	if (attr_local.group > 0)
		forced_vlan_vid_local = 0;

	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	struct nic_flow_def *fd = allocate_nic_flow_def();

	if (fd == NULL)
		goto err_exit0;

	res = interpret_flow_actions(dev, action, NULL, fd, error, &num_dest_port, &num_queues);

	if (res)
		goto err_exit0;

	res = interpret_flow_elements(dev, elem, fd, error, forced_vlan_vid_local, &port_id,
		packet_data, packet_mask, &key_def);

	if (res)
		goto err_exit0;

	rte_spinlock_lock(&dev->ndev->mtx);

	/* Translate group IDs */
	if (fd->jump_to_group != UINT32_MAX &&
		nthw_flow_group_translate_get(dev->ndev->group_handle, caller_id_local, dev->port,
		fd->jump_to_group, &fd->jump_to_group)) {
		NT_LOG(ERR, FILTER, "ERROR: Could not get group resource");
		nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		goto err_exit;
	}

	if (attr_local.group > 0 &&
		nthw_flow_group_translate_get(dev->ndev->group_handle, caller_id_local, dev->port,
		attr_local.group, &attr_local.group)) {
		NT_LOG(ERR, FILTER, "ERROR: Could not get group resource");
		nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		goto err_exit;
	}

	if (port_id == UINT32_MAX)
		port_id = dev->port_id;

	/* Create and flush filter to NIC */
	fh = create_flow_filter(dev, fd, &attr_local, forced_vlan_vid_local,
		caller_id_local, error, port_id, num_dest_port, num_queues, packet_data,
		packet_mask, &key_def);

	if (!fh)
		goto err_exit;

	NT_LOG(DBG, FILTER, "New FlOW: fh (flow handle) %p, fd (flow definition) %p", fh, fd);
	NT_LOG(DBG, FILTER, ">>>>> [Dev %p] Nic %i, Port %i: fh %p fd %p - implementation <<<<<",
		dev, dev->ndev->adapter_no, dev->port, fh, fd);

	rte_spinlock_unlock(&dev->ndev->mtx);

	return fh;

err_exit:

	free(fd);
	fd = NULL;

	rte_spinlock_unlock(&dev->ndev->mtx);

err_exit0:
	if (fd) {
		free(fd);
		fd = NULL;
	}

	NT_LOG(ERR, FILTER, "ERR: %s", __func__);
	return NULL;
}

int nthw_flow_destroy_locked_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *fh,
	struct rte_flow_error *error)
{
	RTE_ASSERT(dev);
	RTE_ASSERT(fh);

	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	/* take flow out of ndev list - may not have been put there yet */
	if (fh->type == FLOW_HANDLE_TYPE_FLM)
		nic_remove_flow_flm(dev->ndev, fh);

	else
		nic_remove_flow(dev->ndev, fh);

#ifdef FLOW_DEBUG
	dev->ndev->be.iface->set_debug_mode(dev->ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_WRITE);
#endif

	NT_LOG(DBG, FILTER, "removing flow :%p", fh);
	if (fh->type == FLOW_HANDLE_TYPE_FLM) {
		nthw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
			(struct hw_db_idx *)fh->flm_db_idxs,
			fh->flm_db_idx_counter);

		flm_flow_programming(fh, NT_FLM_OP_UNLEARN);

	} else {
		NT_LOG(DBG, FILTER, "removing flow :%p", fh);

		if (fh->fd->km.num_ftype_elem) {
			nthw_km_clear_data_match_entry(&fh->fd->km);

			const struct hw_db_inline_km_rcp_data *other_km_rcp_data =
				nthw_db_inline_find_data(dev->ndev, dev->ndev->hw_db_handle,
				HW_DB_IDX_TYPE_KM_RCP,
				(struct hw_db_idx *)fh->flm_db_idxs,
				fh->flm_db_idx_counter);

			if (other_km_rcp_data != NULL &&
				nthw_flow_nic_deref_resource(dev->ndev, RES_KM_CATEGORY,
				(int)other_km_rcp_data->rcp) == 0) {
				nthw_mod_km_rcp_set(&dev->ndev->be, HW_KM_RCP_PRESET_ALL,
					(int)other_km_rcp_data->rcp, 0, 0);
				nthw_mod_km_rcp_flush(&dev->ndev->be, (int)other_km_rcp_data->rcp,
					1);
			}
		}

		nthw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
			(struct hw_db_idx *)fh->db_idxs, fh->db_idx_counter);
		free(fh->fd);
		fh->fd = NULL;
	}

	free(fh);
	fh = NULL;

#ifdef FLOW_DEBUG
	dev->ndev->be.iface->set_debug_mode(dev->ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_NONE);
#endif

	return 0;
}

int nthw_flow_destroy_profile_inline(struct flow_eth_dev *dev, struct flow_handle *flow,
	struct rte_flow_error *error)
{
	int err = 0;

	if (flow && flow->type == FLOW_HANDLE_TYPE_FLM && flow->flm_async)
		return nthw_flow_async_destroy_profile_inline(dev, 0, NULL, flow, NULL, error);

	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	if (flow) {
		/* Delete this flow */
		rte_spinlock_lock(&dev->ndev->mtx);
		err = nthw_flow_destroy_locked_profile_inline(dev, flow, error);
		rte_spinlock_unlock(&dev->ndev->mtx);
	}

	return err;
}

int nthw_flow_flush_profile_inline(struct flow_eth_dev *dev,
	uint16_t caller_id,
	struct rte_flow_error *error)
{
	int err = 0;

	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	/*
	 * Delete all created FLM flows from this eth device.
	 * FLM flows must be deleted first because normal flows are their parents.
	 */
	struct flow_handle *flow = dev->ndev->flow_base_flm;

	while (flow && !err) {
		if (flow->dev == dev && flow->caller_id == caller_id) {
			struct flow_handle *flow_next = flow->next;
			err = nthw_flow_destroy_profile_inline(dev, flow, error);
			flow = flow_next;

		} else {
			flow = flow->next;
		}
	}

	/* Delete all created flows from this eth device */
	flow = dev->ndev->flow_base;

	while (flow && !err) {
		if (flow->dev == dev && flow->caller_id == caller_id) {
			struct flow_handle *flow_next = flow->next;
			err = nthw_flow_destroy_profile_inline(dev, flow, error);
			flow = flow_next;

		} else {
			flow = flow->next;
		}
	}

	return err;
}

int nthw_flow_actions_update_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *flow,
	const struct rte_flow_action action[],
	struct rte_flow_error *error)
{
	RTE_ASSERT(dev);
	RTE_ASSERT(flow);

	uint32_t num_dest_port = 0;
	uint32_t num_queues = 0;

	int group = (int)flow->flm_kid - 2;

	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	if (flow->type != FLOW_HANDLE_TYPE_FLM) {
		NT_LOG(ERR, FILTER,
			"Flow actions update not supported for group 0 or default flows");
		nthw_flow_nic_set_error(ERR_MATCH_INVALID_OR_UNSUPPORTED_ELEM, error);
		return -1;
	}

	struct nic_flow_def *fd = allocate_nic_flow_def();

	if (fd == NULL) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Failed to allocate nic_flow_def";
		return -1;
	}

	fd->non_empty = 1;

	int res =
		interpret_flow_actions(dev, action, NULL, fd, error, &num_dest_port, &num_queues);

	if (res) {
		free(fd);
		return -1;
	}

	rte_spinlock_lock(&dev->ndev->mtx);

	/* Setup new actions */
	uint32_t local_idx_counter = 0;
	uint32_t local_idxs[RES_COUNT];
	memset(local_idxs, 0x0, sizeof(uint32_t) * RES_COUNT);

	struct hw_db_inline_qsl_data qsl_data;
	setup_db_qsl_data(fd, &qsl_data, num_dest_port, num_queues);

	struct hw_db_inline_hsh_data hsh_data;
	setup_db_hsh_data(fd, &hsh_data);

	{
		uint32_t flm_ft = 0;
		uint32_t flm_scrub = 0;

		/* Setup FLM RCP */
		const struct hw_db_inline_flm_rcp_data *flm_data =
			nthw_db_inline_find_data(dev->ndev, dev->ndev->hw_db_handle,
				HW_DB_IDX_TYPE_FLM_RCP,
				(struct hw_db_idx *)flow->flm_db_idxs,
				flow->flm_db_idx_counter);

		if (flm_data == NULL) {
			NT_LOG(ERR, FILTER, "Could not retrieve FLM RPC resource");
			nthw_flow_nic_set_error(ERR_MATCH_INVALID_OR_UNSUPPORTED_ELEM, error);
			goto error_out;
		}

		struct hw_db_flm_idx flm_idx =
			nthw_db_inline_flm_add(dev->ndev, dev->ndev->hw_db_handle, flm_data, group);

		local_idxs[local_idx_counter++] = flm_idx.raw;

		if (flm_idx.error) {
			NT_LOG(ERR, FILTER, "Could not reference FLM RPC resource");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			goto error_out;
		}

		if (setup_flow_flm_actions(dev, fd, &qsl_data, &hsh_data, group, local_idxs,
				&local_idx_counter, &flow->flm_rpl_ext_ptr, &flm_ft,
				&flm_scrub, error)) {
			goto error_out;
		}

		/* Update flow_handle */
		for (int i = 0; i < MAX_FLM_MTRS_SUPPORTED; ++i) {
			struct flm_flow_mtr_handle_s *handle = dev->ndev->flm_mtr_handle;
			struct flm_mtr_stat_s *mtr_stat =
					handle->port_stats[flow->caller_id]->stats;
			flow->flm_mtr_ids[i] =
				fd->mtr_ids[i] == UINT32_MAX ? 0 : mtr_stat[fd->mtr_ids[i]].flm_id;
		}

		for (unsigned int i = 0; i < fd->modify_field_count; ++i) {
			switch (fd->modify_field[i].select) {
			case CPY_SELECT_DSCP_IPV4:

			/* fallthrough */
			case CPY_SELECT_DSCP_IPV6:
				flow->flm_dscp = fd->modify_field[i].value8[0];
				break;

			case CPY_SELECT_RQI_QFI:
				flow->flm_rqi = (fd->modify_field[i].value8[0] >> 6) & 0x1;
				flow->flm_qfi = fd->modify_field[i].value8[0] & 0x3f;
				break;

			case CPY_SELECT_IPV4:
				flow->flm_nat_ipv4 = ntohl(fd->modify_field[i].value32[0]);
				break;

			case CPY_SELECT_PORT:
				flow->flm_nat_port = ntohs(fd->modify_field[i].value16[0]);
				break;

			case CPY_SELECT_TEID:
				flow->flm_teid = ntohl(fd->modify_field[i].value32[0]);
				break;

			default:
				NT_LOG(DBG, FILTER, "Unknown modify field: %" PRIu32,
					fd->modify_field[i].select);
				break;
			}
		}

		flow->flm_ft = (uint8_t)flm_ft;
		flow->flm_scrub_prof = (uint8_t)flm_scrub;
		flow->context = fd->age.context;

		/* Program flow */
		flm_flow_programming(flow, NT_FLM_OP_RELEARN);

		nthw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
			(struct hw_db_idx *)flow->flm_db_idxs,
			flow->flm_db_idx_counter);
		memset(flow->flm_db_idxs, 0x0, sizeof(struct hw_db_idx) * RES_COUNT);

		flow->flm_db_idx_counter = local_idx_counter;

		for (int i = 0; i < RES_COUNT; ++i)
			flow->flm_db_idxs[i] = local_idxs[i];
	}

	rte_spinlock_unlock(&dev->ndev->mtx);

	free(fd);
	return 0;

error_out:
	nthw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
		(struct hw_db_idx *)local_idxs,
		local_idx_counter);

	rte_spinlock_unlock(&dev->ndev->mtx);

	free(fd);
	return -1;
}

static void dump_flm_data(const uint32_t *data, FILE *file)
{
	for (unsigned int i = 0; i < 10; ++i) {
		fprintf(file, "%s%02X %02X %02X %02X%s", i % 2 ? "" : "    ",
			(data[i] >> 24) & 0xff, (data[i] >> 16) & 0xff, (data[i] >> 8) & 0xff,
			data[i] & 0xff, i % 2 ? "\n" : " ");
	}
}

int nthw_flow_get_aged_flows_profile_inline(struct flow_eth_dev *dev,
	uint16_t caller_id,
	void **context,
	uint32_t nb_contexts,
	struct rte_flow_error *error)
{
	(void)dev;
	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	unsigned int queue_size = nthw_flm_age_queue_get_size(caller_id);

	if (queue_size == 0) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Aged queue size is not configured";
		return -1;
	}

	unsigned int queue_count = nthw_flm_age_queue_count(caller_id);

	if (context == NULL)
		return queue_count;

	if (queue_count < nb_contexts) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Aged queue size contains fewer records than the expected output";
		return -1;
	}

	if (queue_size < nb_contexts) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Defined aged queue size is smaller than the expected output";
		return -1;
	}

	uint32_t idx;

	for (idx = 0; idx < nb_contexts; ++idx) {
		struct flm_age_event_s obj;
		int ret = nthw_flm_age_queue_get(caller_id, &obj);

		if (ret != 0)
			break;

		context[idx] = obj.context;
	}

	return idx;
}

int nthw_flow_dev_dump_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *flow,
	uint16_t caller_id,
	FILE *file,
	struct rte_flow_error *error)
{
	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	rte_spinlock_lock(&dev->ndev->mtx);

	if (flow != NULL) {
		if (flow->type == FLOW_HANDLE_TYPE_FLM) {
			fprintf(file, "Port %d, caller %d, flow type FLM\n", (int)dev->port_id,
				(int)flow->caller_id);
			fprintf(file, "  FLM_DATA:\n");
			dump_flm_data(flow->flm_data, file);
			nthw_db_inline_dump(dev->ndev, dev->ndev->hw_db_handle,
				(struct hw_db_idx *)flow->flm_db_idxs,
				flow->flm_db_idx_counter, file);
			fprintf(file, "  Context: %p\n", flow->context);

		} else {
			fprintf(file, "Port %d, caller %d, flow type FLOW\n", (int)dev->port_id,
				(int)flow->caller_id);
			nthw_db_inline_dump(dev->ndev, dev->ndev->hw_db_handle,
				(struct hw_db_idx *)flow->db_idxs, flow->db_idx_counter,
				file);
		}

	} else {
		int max_flm_count = 1000;

		nthw_db_inline_dump_cfn(dev->ndev, dev->ndev->hw_db_handle, file);

		flow = dev->ndev->flow_base;

		while (flow) {
			if (flow->caller_id == caller_id) {
				fprintf(file, "Port %d, caller %d, flow type FLOW\n",
					(int)dev->port_id, (int)flow->caller_id);
				nthw_db_inline_dump(dev->ndev, dev->ndev->hw_db_handle,
					(struct hw_db_idx *)flow->db_idxs,
					flow->db_idx_counter, file);
			}

			flow = flow->next;
		}

		flow = dev->ndev->flow_base_flm;

		while (flow && max_flm_count >= 0) {
			if (flow->caller_id == caller_id) {
				fprintf(file, "Port %d, caller %d, flow type FLM\n",
					(int)dev->port_id, (int)flow->caller_id);
				fprintf(file, "  FLM_DATA:\n");
				dump_flm_data(flow->flm_data, file);
				nthw_db_inline_dump(dev->ndev, dev->ndev->hw_db_handle,
					(struct hw_db_idx *)flow->flm_db_idxs,
					flow->flm_db_idx_counter, file);
				fprintf(file, "  Context: %p\n", flow->context);
				max_flm_count -= 1;
			}

			flow = flow->next;
		}
	}

	rte_spinlock_unlock(&dev->ndev->mtx);

	return 0;
}

int nthw_flow_get_flm_stats_profile_inline(struct flow_nic_dev *ndev, uint64_t *data, uint64_t size)
{
	const enum hw_flm_e fields[] = {
		HW_FLM_STAT_FLOWS, HW_FLM_STAT_LRN_DONE, HW_FLM_STAT_LRN_IGNORE,
		HW_FLM_STAT_LRN_FAIL, HW_FLM_STAT_UNL_DONE, HW_FLM_STAT_UNL_IGNORE,
		HW_FLM_STAT_AUL_DONE, HW_FLM_STAT_AUL_IGNORE, HW_FLM_STAT_AUL_FAIL,
		HW_FLM_STAT_TUL_DONE, HW_FLM_STAT_REL_DONE, HW_FLM_STAT_REL_IGNORE,
		HW_FLM_STAT_PRB_DONE, HW_FLM_STAT_PRB_IGNORE,

		HW_FLM_STAT_STA_DONE, HW_FLM_STAT_INF_DONE, HW_FLM_STAT_INF_SKIP,
		HW_FLM_STAT_PCK_HIT, HW_FLM_STAT_PCK_MISS, HW_FLM_STAT_PCK_UNH,
		HW_FLM_STAT_PCK_DIS, HW_FLM_STAT_CSH_HIT, HW_FLM_STAT_CSH_MISS,
		HW_FLM_STAT_CSH_UNH, HW_FLM_STAT_CUC_START, HW_FLM_STAT_CUC_MOVE,

		HW_FLM_LOAD_LPS, HW_FLM_LOAD_APS,
	};

	const uint64_t fields_cnt = sizeof(fields) / sizeof(enum hw_flm_e);

	if (!ndev->flow_mgnt_prepared)
		return 0;

	if (size < fields_cnt)
		return -1;

	nthw_mod_flm_stat_update(&ndev->be);

	for (uint64_t i = 0; i < fields_cnt; ++i) {
		uint32_t value = 0;
		nthw_mod_flm_stat_get(&ndev->be, fields[i], &value);
		data[i] = (fields[i] == HW_FLM_STAT_FLOWS || fields[i] == HW_FLM_LOAD_LPS ||
				fields[i] == HW_FLM_LOAD_APS)
			? value
			: data[i] + value;

		if (ndev->be.flm.ver < 18 && fields[i] == HW_FLM_STAT_PRB_IGNORE)
			break;
	}

	return 0;
}

int nthw_flow_get_ifr_stats_profile_inline(struct flow_nic_dev *ndev, uint64_t *data,
	uint8_t port_count)
{
	/* IFR RCP 0 is reserved, port counters start from record 1 */
	nthw_mod_tpe_ifr_counters_update(&ndev->be, 1, port_count);
	uint8_t i = 0;

	for (i = 0; i < port_count; ++i) {
		uint8_t ifr_mtu_recipe = convert_port_to_ifr_mtu_recipe(i);
		uint32_t drop_cnt = 0;
		nthw_mod_tpe_ifr_counters_get(&ndev->be, HW_TPE_IFR_COUNTERS_DROP, ifr_mtu_recipe,
			&drop_cnt);
		data[i] = data[i] + drop_cnt;
	}

	return 0;
}

int nthw_flow_set_mtu_inline(struct flow_eth_dev *dev, uint32_t port, uint16_t mtu)
{
	if (port >= 255)
		return -1;

	uint32_t ipv4_en_frag;
	uint32_t ipv4_action;
	uint32_t ipv6_en_frag;
	uint32_t ipv6_action;

	if (port == 0) {
		ipv4_en_frag = PORT_0_IPV4_FRAGMENTATION;
		ipv4_action = PORT_0_IPV4_DF_ACTION;
		ipv6_en_frag = PORT_0_IPV6_FRAGMENTATION;
		ipv6_action = PORT_0_IPV6_ACTION;

	} else if (port == 1) {
		ipv4_en_frag = PORT_1_IPV4_FRAGMENTATION;
		ipv4_action = PORT_1_IPV4_DF_ACTION;
		ipv6_en_frag = PORT_1_IPV6_FRAGMENTATION;
		ipv6_action = PORT_1_IPV6_ACTION;

	} else {
		ipv4_en_frag = DISABLE_FRAGMENTATION;
		ipv4_action = IPV4_DF_DROP;
		ipv6_en_frag = DISABLE_FRAGMENTATION;
		ipv6_action = IPV6_DROP;
	}

	int err = 0;
	uint8_t ifr_mtu_recipe = convert_port_to_ifr_mtu_recipe(port);
	struct flow_nic_dev *ndev = dev->ndev;

	err |= nthw_mod_tpe_rpp_ifr_rcp_set(&ndev->be, HW_TPE_IFR_RCP_IPV4_EN, ifr_mtu_recipe,
			ipv4_en_frag);
	err |= nthw_mod_tpe_rpp_ifr_rcp_set(&ndev->be, HW_TPE_IFR_RCP_IPV6_EN, ifr_mtu_recipe,
			ipv6_en_frag);
	err |= nthw_mod_tpe_rpp_ifr_rcp_set(&ndev->be, HW_TPE_IFR_RCP_MTU, ifr_mtu_recipe, mtu);
	err |= nthw_mod_tpe_rpp_ifr_rcp_set(&ndev->be, HW_TPE_IFR_RCP_IPV4_DF_DROP, ifr_mtu_recipe,
			ipv4_action);
	err |= nthw_mod_tpe_rpp_ifr_rcp_set(&ndev->be, HW_TPE_IFR_RCP_IPV6_DROP, ifr_mtu_recipe,
			ipv6_action);

	err |= nthw_mod_tpe_ifr_rcp_set(&ndev->be, HW_TPE_IFR_RCP_IPV4_EN, ifr_mtu_recipe,
			ipv4_en_frag);
	err |= nthw_mod_tpe_ifr_rcp_set(&ndev->be, HW_TPE_IFR_RCP_IPV6_EN, ifr_mtu_recipe,
			ipv6_en_frag);
	err |= nthw_mod_tpe_ifr_rcp_set(&ndev->be, HW_TPE_IFR_RCP_MTU, ifr_mtu_recipe, mtu);
	err |= nthw_mod_tpe_ifr_rcp_set(&ndev->be, HW_TPE_IFR_RCP_IPV4_DF_DROP, ifr_mtu_recipe,
			ipv4_action);
	err |= nthw_mod_tpe_ifr_rcp_set(&ndev->be, HW_TPE_IFR_RCP_IPV6_DROP, ifr_mtu_recipe,
			ipv6_action);

	if (err == 0) {
		err |= nthw_mod_tpe_rpp_ifr_rcp_flush(&ndev->be, ifr_mtu_recipe, 1);
		err |= nthw_mod_tpe_ifr_rcp_flush(&ndev->be, ifr_mtu_recipe, 1);
	}

	return err;
}

int nthw_flow_info_get_profile_inline(struct flow_eth_dev *dev, uint8_t caller_id,
	struct rte_flow_port_info *port_info,
	struct rte_flow_queue_info *queue_info, struct rte_flow_error *error)
{
	(void)queue_info;
	(void)caller_id;
	int res = 0;

	nthw_flow_nic_set_error(ERR_SUCCESS, error);
	if (port_info)
		memset(port_info, 0, sizeof(struct rte_flow_port_info));

	port_info->max_nb_aging_objects = dev->nb_aging_objects;

	struct flm_flow_mtr_handle_s *mtr_handle = dev->ndev->flm_mtr_handle;

	if (mtr_handle)
		port_info->max_nb_meters = mtr_handle->port_stats[caller_id]->size;

	return res;
}

int nthw_flow_configure_profile_inline(struct flow_eth_dev *dev, uint8_t caller_id,
	const struct rte_flow_port_attr *port_attr, uint16_t nb_queue,
	const struct rte_flow_queue_attr *queue_attr[],
	struct rte_flow_error *error)
{
	(void)nb_queue;
	(void)queue_attr;
	int res = 0;

	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	if (port_attr->nb_aging_objects > 0) {
		if (dev->nb_aging_objects > 0) {
			nthw_flm_age_queue_free(dev->port_id, caller_id);
			dev->nb_aging_objects = 0;
		}

		struct rte_ring *age_queue =
			nthw_flm_age_queue_create(dev->port_id, caller_id,
				port_attr->nb_aging_objects);

		if (age_queue == NULL) {
			error->message = "Failed to allocate aging objects";
			goto error_out;
		}

		dev->nb_aging_objects = port_attr->nb_aging_objects;
	}

	if (port_attr->nb_meters > 0) {
		struct flm_flow_mtr_handle_s *mtr_handle = dev->ndev->flm_mtr_handle;

		if (mtr_handle->port_stats[caller_id]->shared == 1) {
			struct flm_mtr_stat_s *temp_stats =
				realloc(mtr_handle->port_stats[caller_id]->stats,
					port_attr->nb_meters);
			if (temp_stats == NULL) {
				res = -1;
			} else {
				res = 0;
				mtr_handle->port_stats[caller_id]->stats = temp_stats;
				mtr_handle->port_stats[caller_id]->size = port_attr->nb_meters;
			}

		} else {
			mtr_handle->port_stats[caller_id] =
				calloc(1, sizeof(struct flm_mtr_shared_stats_s));
			struct flm_mtr_stat_s *stats =
				calloc(port_attr->nb_meters, sizeof(struct flm_mtr_stat_s));

			if (mtr_handle->port_stats[caller_id] == NULL || stats == NULL) {
				free(mtr_handle->port_stats[caller_id]);
				free(stats);
				error->message = "Failed to allocate meter actions";
				goto error_out;
			}

			mtr_handle->port_stats[caller_id]->stats = stats;
			mtr_handle->port_stats[caller_id]->size = port_attr->nb_meters;
			mtr_handle->port_stats[caller_id]->shared = 1;
		}
	}

	return res;

error_out:
	error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;

	if (port_attr->nb_aging_objects > 0) {
		nthw_flm_age_queue_free(dev->port_id, caller_id);
		dev->nb_aging_objects = 0;
	}

	return -1;
}

struct flow_pattern_template *
nthw_flow_pattern_template_create_profile_inline(struct flow_eth_dev *dev,
	const struct rte_flow_pattern_template_attr *template_attr, uint16_t caller_id,
	const struct rte_flow_item pattern[], struct rte_flow_error *error)
{
	(void)template_attr;
	(void)caller_id;
	uint32_t port_id = 0;
	uint32_t packet_data[10];
	uint32_t packet_mask[10];
	struct flm_flow_key_def_s key_def;

	struct nic_flow_def *fd = allocate_nic_flow_def();

	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	if (fd == NULL) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Failed to allocate flow_def";
		return NULL;
	}

	/* Note that forced_vlan_vid is unavailable at this point in time */
	int res = interpret_flow_elements(dev, pattern, fd, error, 0, &port_id, packet_data,
			packet_mask, &key_def);

	if (res) {
		free(fd);
		return NULL;
	}

	struct flow_pattern_template *template = calloc(1, sizeof(struct flow_pattern_template));

	if (template == NULL) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Failed to allocate actions_template";
		free(fd);
		return NULL;
	}

	template->fd = fd;

	return template;
}

int nthw_flow_pattern_template_destroy_profile_inline(struct flow_eth_dev *dev,
	struct flow_pattern_template *pattern_template,
	struct rte_flow_error *error)
{
	(void)dev;
	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	free(pattern_template->fd);
	free(pattern_template);

	return 0;
}

struct flow_actions_template *
nthw_flow_actions_template_create_profile_inline(struct flow_eth_dev *dev,
	const struct rte_flow_actions_template_attr *template_attr, uint16_t caller_id,
	const struct rte_flow_action actions[],
	const struct rte_flow_action masks[],
	struct rte_flow_error *error)
{
	(void)template_attr;
	int res;

	uint32_t num_dest_port = 0;
	uint32_t num_queues = 0;

	struct nic_flow_def *fd = allocate_nic_flow_def();

	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	if (fd == NULL) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Failed to allocate flow_def";
		return NULL;
	}

	res = interpret_flow_actions(dev, actions, masks, fd, error, &num_dest_port, &num_queues);

	if (res) {
		free(fd);
		return NULL;
	}

	/* Translate group IDs */
	if (fd->jump_to_group != UINT32_MAX) {
		rte_spinlock_lock(&dev->ndev->mtx);
		res = nthw_flow_group_translate_get(dev->ndev->group_handle, caller_id,
				dev->port, fd->jump_to_group, &fd->jump_to_group);
		rte_spinlock_unlock(&dev->ndev->mtx);

		if (res) {
			NT_LOG(ERR, FILTER, "ERROR: Could not get group resource");
			nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
			free(fd);
			return NULL;
		}
	}

	struct flow_actions_template *template = calloc(1, sizeof(struct flow_actions_template));

	if (template == NULL) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Failed to allocate actions_template";
		free(fd);
		return NULL;
	}

	template->fd = fd;
	template->num_dest_port = num_dest_port;
	template->num_queues = num_queues;

	return template;
}

int nthw_flow_actions_template_destroy_profile_inline(struct flow_eth_dev *dev,
	struct flow_actions_template *actions_template,
	struct rte_flow_error *error)
{
	(void)dev;
	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	free(actions_template->fd);
	free(actions_template);

	return 0;
}

struct flow_template_table *nthw_flow_template_table_create_profile_inline(struct flow_eth_dev *dev,
	const struct rte_flow_template_table_attr *table_attr, uint16_t forced_vlan_vid,
	uint16_t caller_id,
	struct flow_pattern_template *pattern_templates[], uint8_t nb_pattern_templates,
	struct flow_actions_template *actions_templates[], uint8_t nb_actions_templates,
	struct rte_flow_error *error)
{
	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	struct flow_template_table *template_table = calloc(1, sizeof(struct flow_template_table));

	if (template_table == NULL) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Failed to allocate template_table";
		goto error_out;
	}

	template_table->pattern_templates =
		malloc(sizeof(struct flow_pattern_template *) * nb_pattern_templates);
	template_table->actions_templates =
		malloc(sizeof(struct flow_actions_template *) * nb_actions_templates);
	template_table->pattern_action_pairs =
		calloc((uint32_t)nb_pattern_templates * nb_actions_templates,
			sizeof(struct flow_template_table_cell));

	if (template_table->pattern_templates == NULL ||
		template_table->actions_templates == NULL ||
		template_table->pattern_action_pairs == NULL) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Failed to allocate template_table variables";
		goto error_out;
	}

	template_table->attr.priority = table_attr->flow_attr.priority;
	template_table->attr.group = table_attr->flow_attr.group;
	template_table->forced_vlan_vid = forced_vlan_vid;
	template_table->caller_id = caller_id;

	template_table->nb_pattern_templates = nb_pattern_templates;
	template_table->nb_actions_templates = nb_actions_templates;

	memcpy(template_table->pattern_templates, pattern_templates,
		sizeof(struct flow_pattern_template *) * nb_pattern_templates);
	memcpy(template_table->actions_templates, actions_templates,
		sizeof(struct rte_flow_actions_template *) * nb_actions_templates);

	rte_spinlock_lock(&dev->ndev->mtx);
	int res =
		nthw_flow_group_translate_get(dev->ndev->group_handle, caller_id, dev->port,
			template_table->attr.group, &template_table->attr.group);
	rte_spinlock_unlock(&dev->ndev->mtx);

	/* Translate group IDs */
	if (res) {
		NT_LOG(ERR, FILTER, "ERROR: Could not get group resource");
		nthw_flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		goto error_out;
	}

	return template_table;

error_out:

	if (template_table) {
		free(template_table->pattern_templates);
		free(template_table->actions_templates);
		free(template_table->pattern_action_pairs);
		free(template_table);
	}

	return NULL;
}

int nthw_flow_template_table_destroy_profile_inline(struct flow_eth_dev *dev,
	struct flow_template_table *template_table,
	struct rte_flow_error *error)
{
	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	const uint32_t nb_cells =
		template_table->nb_pattern_templates * template_table->nb_actions_templates;

	for (uint32_t i = 0; i < nb_cells; ++i) {
		struct flow_template_table_cell *cell = &template_table->pattern_action_pairs[i];

		if (cell->flm_db_idx_counter > 0) {
			nthw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
				(struct hw_db_idx *)cell->flm_db_idxs,
				cell->flm_db_idx_counter);
		}
	}

	free(template_table->pattern_templates);
	free(template_table->actions_templates);
	free(template_table->pattern_action_pairs);
	free(template_table);

	return 0;
}

struct flow_handle *nthw_flow_async_create_profile_inline(struct flow_eth_dev *dev,
	uint32_t queue_id,
	const struct rte_flow_op_attr *op_attr,
	struct flow_template_table *template_table,
	const struct rte_flow_item pattern[],
	uint8_t pattern_template_index,
	const struct rte_flow_action actions[],
	uint8_t actions_template_index,
	void *user_data,
	struct rte_flow_error *error)
{
	(void)queue_id;
	(void)op_attr;
	struct flow_handle *fh = NULL;
	int res, status;

	const uint32_t pattern_action_index =
		(uint32_t)template_table->nb_actions_templates * pattern_template_index +
		actions_template_index;
	struct flow_template_table_cell *pattern_action_pair =
			&template_table->pattern_action_pairs[pattern_action_index];

	uint32_t num_dest_port =
		template_table->actions_templates[actions_template_index]->num_dest_port;
	uint32_t num_queues =
		template_table->actions_templates[actions_template_index]->num_queues;

	uint32_t port_id = UINT32_MAX;
	uint32_t packet_data[10];
	uint32_t packet_mask[10];
	struct flm_flow_key_def_s key_def;

	nthw_flow_nic_set_error(ERR_SUCCESS, error);

	struct nic_flow_def *fd = malloc(sizeof(struct nic_flow_def));

	if (fd == NULL) {
		error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
		error->message = "Failed to allocate flow_def";
		goto err_exit;
	}

	memcpy(fd, template_table->actions_templates[actions_template_index]->fd,
		sizeof(struct nic_flow_def));

	res = interpret_flow_elements(dev, pattern, fd, error,
			template_table->forced_vlan_vid, &port_id, packet_data,
			packet_mask, &key_def);

	if (res)
		goto err_exit;

	if (port_id == UINT32_MAX)
		port_id = dev->port_id;

	{
		uint32_t num_dest_port_tmp = 0;
		uint32_t num_queues_tmp = 0;

		struct nic_flow_def action_fd = { 0 };
		prepare_nic_flow_def(&action_fd);

		res = interpret_flow_actions(dev, actions, NULL, &action_fd, error,
				&num_dest_port_tmp, &num_queues_tmp);

		if (res)
			goto err_exit;

		/* Copy FLM unique actions: modify_field, meter, encap/decap and age */
		memcpy_or(fd->mtr_ids, action_fd.mtr_ids, sizeof(action_fd.mtr_ids));
		memcpy_or(&fd->tun_hdr, &action_fd.tun_hdr, sizeof(struct tunnel_header_s));
		memcpy_or(fd->modify_field, action_fd.modify_field,
			sizeof(action_fd.modify_field));
		fd->modify_field_count = action_fd.modify_field_count;
		memcpy_or(&fd->age, &action_fd.age, sizeof(struct rte_flow_action_age));
	}

	status = atomic_load(&pattern_action_pair->status);

	/* Initializing template entry */
	if (status < CELL_STATUS_INITIALIZED_TYPE_FLOW) {
		if (status == CELL_STATUS_UNINITIALIZED &&
			atomic_compare_exchange_strong(&pattern_action_pair->status, &status,
				CELL_STATUS_INITIALIZING)) {
			rte_spinlock_lock(&dev->ndev->mtx);

			fh = create_flow_filter(dev, fd, &template_table->attr,
				template_table->forced_vlan_vid, template_table->caller_id,
					error, port_id, num_dest_port, num_queues, packet_data,
					packet_mask, &key_def);

			rte_spinlock_unlock(&dev->ndev->mtx);

			if (fh == NULL) {
				/* reset status to CELL_STATUS_UNINITIALIZED to avoid a deadlock */
				atomic_store(&pattern_action_pair->status,
					CELL_STATUS_UNINITIALIZED);
				goto err_exit;
			}

			if (fh->type == FLOW_HANDLE_TYPE_FLM) {
				rte_spinlock_lock(&dev->ndev->mtx);

				struct hw_db_idx *flm_ft_idx =
					nthw_db_inline_find_idx(dev->ndev, dev->ndev->hw_db_handle,
						HW_DB_IDX_TYPE_FLM_FT,
						(struct hw_db_idx *)fh->flm_db_idxs,
						fh->flm_db_idx_counter);

				rte_spinlock_unlock(&dev->ndev->mtx);

				pattern_action_pair->flm_db_idx_counter = fh->flm_db_idx_counter;
				memcpy(pattern_action_pair->flm_db_idxs, fh->flm_db_idxs,
					sizeof(struct hw_db_idx) * fh->flm_db_idx_counter);

				pattern_action_pair->flm_key_id = fh->flm_kid;
				pattern_action_pair->flm_ft = flm_ft_idx->id1;

				pattern_action_pair->flm_rpl_ext_ptr = fh->flm_rpl_ext_ptr;
				pattern_action_pair->flm_scrub_prof = fh->flm_scrub_prof;

				atomic_store(&pattern_action_pair->status,
					CELL_STATUS_INITIALIZED_TYPE_FLM);

				/* increment template table cell reference */
				atomic_fetch_add(&pattern_action_pair->counter, 1);
				fh->template_table_cell = pattern_action_pair;
				fh->flm_async = true;

			} else {
				atomic_store(&pattern_action_pair->status,
					CELL_STATUS_INITIALIZED_TYPE_FLOW);
			}

		} else {
			do {
				nthw_os_wait_usec(1);
				status = atomic_load(&pattern_action_pair->status);
			} while (status == CELL_STATUS_INITIALIZING);

			/* error handling in case that create_flow_filter() will fail in the other
			 * service
			 */
			if (status == CELL_STATUS_UNINITIALIZED)
				goto err_exit;
		}
	}

	/* FLM learn */
	if (fh == NULL && status == CELL_STATUS_INITIALIZED_TYPE_FLM) {
		fh = calloc(1, sizeof(struct flow_handle));

		if (fh == NULL) {
			error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
			error->message = "Failed to allocate flow_handle";
			goto err_exit;
		}

		fh->type = FLOW_HANDLE_TYPE_FLM;
		fh->dev = dev;
		fh->caller_id = template_table->caller_id;
		fh->user_data = user_data;

		if (copy_fd_to_fh_flm(fh, fd, packet_data, pattern_action_pair->flm_key_id,
				pattern_action_pair->flm_ft,
				pattern_action_pair->flm_rpl_ext_ptr,
				pattern_action_pair->flm_scrub_prof,
				template_table->attr.priority & 0x3) != 0) {
			goto err_exit;
		}

		free(fd);

		flm_flow_programming(fh, NT_FLM_OP_LEARN);

		nic_insert_flow_flm(dev->ndev, fh);

		/* increment template table cell reference */
		atomic_fetch_add(&pattern_action_pair->counter, 1);
		fh->template_table_cell = pattern_action_pair;
		fh->flm_async = true;

	} else if (fh == NULL) {
		rte_spinlock_lock(&dev->ndev->mtx);

		fh = create_flow_filter(dev, fd, &template_table->attr,
			template_table->forced_vlan_vid, template_table->caller_id,
				error, port_id, num_dest_port, num_queues, packet_data,
				packet_mask, &key_def);

		rte_spinlock_unlock(&dev->ndev->mtx);

		if (fh == NULL)
			goto err_exit;
	}

	if (fh) {
		fh->caller_id = template_table->caller_id;
		fh->user_data = user_data;
	}

	return fh;

err_exit:
	free(fd);
	free(fh);

	return NULL;
}

int nthw_flow_async_destroy_profile_inline(struct flow_eth_dev *dev, uint32_t queue_id,
	const struct rte_flow_op_attr *op_attr, struct flow_handle *flow,
	void *user_data, struct rte_flow_error *error)
{
	(void)queue_id;
	(void)op_attr;
	(void)user_data;

	if (flow->type == FLOW_HANDLE_TYPE_FLOW)
		return nthw_flow_destroy_profile_inline(dev, flow, error);

	if (flm_flow_programming(flow, NT_FLM_OP_UNLEARN)) {
		NT_LOG(ERR, FILTER, "FAILED to destroy flow: %p", flow);
		nthw_flow_nic_set_error(ERR_REMOVE_FLOW_FAILED, error);
		return -1;
	}

	nic_remove_flow_flm(dev->ndev, flow);

	free(flow);

	return 0;
}

static const struct profile_inline_ops ops = {
	/*
	 * Management
	 */
	.nthw_done_flow_mgmnt_of_ndev_profile_inline = nthw_done_flow_mgmnt_of_ndev_profile_inline,
	.nthw_init_flow_mgmnt_of_ndev_profile_inline =
		nthw_init_flow_mgmnt_of_ndev_profile_inline,
	.nthw_flow_dev_dump_profile_inline = nthw_flow_dev_dump_profile_inline,
	/*
	 * Flow functionality
	 */
	.nthw_flow_destroy_locked_profile_inline = nthw_flow_destroy_locked_profile_inline,
	.nthw_flow_create_profile_inline = nthw_flow_create_profile_inline,
	.nthw_flow_destroy_profile_inline = nthw_flow_destroy_profile_inline,
	.nthw_flow_flush_profile_inline = nthw_flow_flush_profile_inline,
	.nthw_flow_actions_update_profile_inline = nthw_flow_actions_update_profile_inline,
	.nthw_flow_get_aged_flows_profile_inline = nthw_flow_get_aged_flows_profile_inline,
	/*
	 * Stats
	 */
	.nthw_flow_get_flm_stats_profile_inline = nthw_flow_get_flm_stats_profile_inline,
	.nthw_flow_get_ifr_stats_profile_inline = nthw_flow_get_ifr_stats_profile_inline,
	.nthw_flow_info_get_profile_inline = nthw_flow_info_get_profile_inline,
	.nthw_flow_configure_profile_inline = nthw_flow_configure_profile_inline,
	.nthw_flow_pattern_template_create_profile_inline =
		nthw_flow_pattern_template_create_profile_inline,
	.nthw_flow_pattern_template_destroy_profile_inline =
		nthw_flow_pattern_template_destroy_profile_inline,
	.nthw_flow_actions_template_create_profile_inline =
		nthw_flow_actions_template_create_profile_inline,
	.nthw_flow_actions_template_destroy_profile_inline =
		nthw_flow_actions_template_destroy_profile_inline,
	.nthw_flow_template_table_create_profile_inline =
		nthw_flow_template_table_create_profile_inline,
	.nthw_flow_template_table_destroy_profile_inline =
		nthw_flow_template_table_destroy_profile_inline,
	.nthw_flow_async_create_profile_inline = nthw_flow_async_create_profile_inline,
	.nthw_flow_async_destroy_profile_inline = nthw_flow_async_destroy_profile_inline,
	/*
	 * NT Flow FLM Meter API
	 */
	.flow_mtr_supported = flow_mtr_supported,
	.flow_mtr_meter_policy_n_max = flow_mtr_meter_policy_n_max,
	.flow_mtr_set_profile = flow_mtr_set_profile,
	.flow_mtr_set_policy = flow_mtr_set_policy,
	.flow_mtr_create_meter = flow_mtr_create_meter,
	.flow_mtr_probe_meter = flow_mtr_probe_meter,
	.flow_mtr_destroy_meter = flow_mtr_destroy_meter,
	.flm_mtr_adjust_stats = flm_mtr_adjust_stats,
	.flow_mtr_meters_supported = flow_mtr_meters_supported,
	.flm_setup_queues = flm_setup_queues,
	.flm_free_queues = flm_free_queues,
	.flm_mtr_read_stats = flm_mtr_read_stats,
	.flm_update = flm_update,

	/*
	 * Config API
	 */
	.nthw_flow_set_mtu_inline = nthw_flow_set_mtu_inline,
};

void nthw_profile_inline_init(void)
{
	register_profile_inline_ops(&ops);
}
