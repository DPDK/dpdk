/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation.
 */

#ifndef _RTE_PMD_NTB_H_
#define _RTE_PMD_NTB_H_

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
	/**< memzone needs to be mw size align or not. */
	uint8_t mw_size_align;
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

#endif /* _RTE_PMD_NTB_H_ */
