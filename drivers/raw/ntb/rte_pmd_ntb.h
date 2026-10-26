/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation.
 */

#ifndef _RTE_PMD_NTB_H_
#define _RTE_PMD_NTB_H_

#include <stdint.h>

#include <rte_compat.h>

/* App needs to set/get these attrs */
#define NTB_QUEUE_SZ_NAME           "queue_size"
#define NTB_QUEUE_NUM_NAME          "queue_num"
#define NTB_TOPO_NAME               "topo"
#define NTB_LINK_STATUS_NAME        "link_status"
#define NTB_SPEED_NAME              "speed"
#define NTB_WIDTH_NAME              "width"
#define NTB_MW_CNT_NAME             "mw_count"
#define NTB_DB_CNT_NAME             "db_count"
#define NTB_SPAD_CNT_NAME           "spad_count"

#define NTB_MAX_DESC_SIZE           1024
#define NTB_MIN_DESC_SIZE           64

struct ntb_dev_info {
	uint32_t ntb_hdr_size;
	uint8_t mw_cnt;
	uint64_t *mw_size;
};

struct ntb_dev_config {
	uint16_t num_queues;
	uint16_t queue_size;
	uint8_t mz_num;
	const struct rte_memzone **mz_list;
};

struct ntb_queue_conf {
	uint16_t nb_desc;
	uint16_t tx_free_thresh;
	struct rte_mempool *rx_mp;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the base-address alignment a memory window memzone must be reserved
 * with. Some NTB hardware constrains the address a memory window can be
 * translated to (for example, hardware that forms the peer target as
 * (base | offset) needs the base aligned to a power of two >= the window
 * length). Applications should reserve the memzone for memory window
 * @p mw_id, of length @p mw_len, with at least the returned alignment.
 *
 * @param dev_id
 *   The identifier of the raw device.
 * @param mw_id
 *   The memory window index.
 * @param mw_len
 *   The length, in bytes, of the memzone to be reserved.
 * @return
 *   The required base-address alignment in bytes, or 0 on error.
 */
__rte_experimental
uint64_t
rte_pmd_ntb_get_mem_align(uint16_t dev_id, uint32_t mw_id, uint64_t mw_len);

#endif /* _RTE_PMD_NTB_H_ */
