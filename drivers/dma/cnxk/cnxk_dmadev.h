/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell International Ltd.
 */
#ifndef CNXK_DMADEV_H
#define CNXK_DMADEV_H

#include <roc_api.h>

#define DPI_MAX_POINTER	     15
#define STRM_INC(s, var)     ((s).var = ((s).var + 1) & (s).max_cnt)
#define STRM_DEC(s, var)     ((s).var = ((s).var - 1) == -1 ? (s).max_cnt : ((s).var - 1))
#define DPI_MAX_DESC	     2048
#define DPI_MIN_DESC	     2
#define MAX_VCHANS_PER_QUEUE 4

/* Set Completion data to 0xFF when request submitted,
 * upon successful request completion engine reset to completion status
 */
#define DPI_REQ_CDATA 0xFF

#define CNXK_DPI_DEV_CONFIG (1ULL << 0)
#define CNXK_DPI_DEV_START  (1ULL << 1)

struct cnxk_dpi_compl_s {
	uint64_t cdata;
	void *cb_data;
};

struct cnxk_dpi_cdesc_data_s {
	struct cnxk_dpi_compl_s **compl_ptr;
	uint16_t max_cnt;
	uint16_t head;
	uint16_t tail;
};

struct cnxk_dpi_conf {
	union dpi_instr_hdr_s hdr;
	struct cnxk_dpi_cdesc_data_s c_desc;
	uint16_t pnum_words;
	uint16_t pending;
	uint16_t desc_idx;
	uint16_t pad0;
	struct rte_dma_stats stats;
};

struct cnxk_dpi_vf_s {
	struct roc_dpi rdpi;
	struct cnxk_dpi_conf conf[MAX_VCHANS_PER_QUEUE];
	uint16_t num_vchans;
	uint16_t flag;
} __plt_cache_aligned;

#endif
