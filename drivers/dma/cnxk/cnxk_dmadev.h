/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell International Ltd.
 */
#ifndef CNXK_DMADEV_H
#define CNXK_DMADEV_H

#define DPI_MAX_POINTER		15
#define DPI_QUEUE_STOP		0x0
#define DPI_QUEUE_START		0x1
#define STRM_INC(s)		((s).tail = ((s).tail + 1) % (s).max_cnt)
#define DPI_MAX_DESC		DPI_MAX_POINTER

/* Set Completion data to 0xFF when request submitted,
 * upon successful request completion engine reset to completion status
 */
#define DPI_REQ_CDATA		0xFF

struct cnxk_dpi_compl_s {
	uint64_t cdata;
	void *cb_data;
};

struct cnxk_dpi_cdesc_data_s {
	struct cnxk_dpi_compl_s *compl_ptr[DPI_MAX_DESC];
	uint16_t max_cnt;
	uint16_t head;
	uint16_t tail;
};

struct cnxk_dpi_conf {
	union dpi_instr_hdr_s hdr;
	struct cnxk_dpi_cdesc_data_s c_desc;
};

struct cnxk_dpi_vf_s {
	struct roc_dpi rdpi;
	struct cnxk_dpi_conf conf;
	struct rte_dma_stats stats;
	uint64_t cmd[DPI_MAX_CMD_SIZE];
	uint32_t num_words;
	uint16_t desc_idx;
};

#endif
