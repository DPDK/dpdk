/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell International Ltd.
 */
#ifndef CNXK_DMADEV_H
#define CNXK_DMADEV_H

#include <string.h>
#include <unistd.h>

#include <bus_pci_driver.h>
#include <rte_common.h>
#include <rte_dmadev.h>
#include <rte_dmadev_pmd.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_mempool.h>
#include <rte_pci.h>

#include <roc_api.h>

#define CNXK_DPI_MAX_POINTER		    15
#define CNXK_DPI_STRM_INC(s, var)	    ((s).var = ((s).var + 1) & (s).max_cnt)
#define CNXK_DPI_STRM_DEC(s, var)	    ((s).var = ((s).var - 1) == -1 ? (s).max_cnt :	\
						((s).var - 1))
#define CNXK_DPI_MAX_DESC		    32768
#define CNXK_DPI_MIN_DESC		    2
#define CNXK_DPI_MAX_VCHANS_PER_QUEUE	    4
#define CNXK_DPI_QUEUE_BUF_SIZE		    16256
#define CNXK_DPI_POOL_MAX_CACHE_SZ	    (16)
#define CNXK_DPI_DW_PER_SINGLE_CMD	    8
#define CNXK_DPI_HDR_LEN		    4
#define CNXK_DPI_CMD_LEN(src, dst)	    (CNXK_DPI_HDR_LEN + ((src) << 1) + ((dst) << 1))
#define CNXK_DPI_MAX_CMD_SZ		    CNXK_DPI_CMD_LEN(CNXK_DPI_MAX_POINTER,		\
							     CNXK_DPI_MAX_POINTER)
#define CNXK_DPI_CHUNKS_FROM_DESC(cz, desc) (((desc) / (((cz) / 8) / CNXK_DPI_MAX_CMD_SZ)) + 1)

/* Set Completion data to 0xFF when request submitted,
 * upon successful request completion engine reset to completion status
 */
#define CNXK_DPI_REQ_CDATA 0xFF

union cnxk_dpi_instr_cmd {
	uint64_t u;
	struct cn9k_dpi_instr_cmd {
		uint64_t aura : 20;
		uint64_t func : 16;
		uint64_t pt : 2;
		uint64_t reserved_102 : 1;
		uint64_t pvfe : 1;
		uint64_t fl : 1;
		uint64_t ii : 1;
		uint64_t fi : 1;
		uint64_t ca : 1;
		uint64_t csel : 1;
		uint64_t reserved_109_111 : 3;
		uint64_t xtype : 2;
		uint64_t reserved_114_119 : 6;
		uint64_t fport : 2;
		uint64_t reserved_122_123 : 2;
		uint64_t lport : 2;
		uint64_t reserved_126_127 : 2;
		/* Word 1 - End */
	} cn9k;

	struct cn10k_dpi_instr_cmd {
		uint64_t nfst : 4;
		uint64_t reserved_4_5 : 2;
		uint64_t nlst : 4;
		uint64_t reserved_10_11 : 2;
		uint64_t pvfe : 1;
		uint64_t reserved_13 : 1;
		uint64_t func : 16;
		uint64_t aura : 20;
		uint64_t xtype : 2;
		uint64_t reserved_52_53 : 2;
		uint64_t pt : 2;
		uint64_t fport : 2;
		uint64_t reserved_58_59 : 2;
		uint64_t lport : 2;
		uint64_t reserved_62_63 : 2;
		/* Word 0 - End */
	} cn10k;
};

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
	union cnxk_dpi_instr_cmd cmd;
	struct cnxk_dpi_cdesc_data_s c_desc;
	uint16_t pnum_words;
	uint16_t pending;
	uint16_t desc_idx;
	struct rte_dma_stats stats;
	uint64_t completed_offset;
};

struct cnxk_dpi_vf_s {
	/* Fast path */
	uint64_t *chunk_base;
	uint16_t chunk_head;
	uint16_t chunk_size_m1;
	struct rte_mempool *chunk_pool;
	struct cnxk_dpi_conf conf[CNXK_DPI_MAX_VCHANS_PER_QUEUE];
	/* Slow path */
	struct roc_dpi rdpi;
	uint32_t aura;
	uint16_t num_vchans;
	uint16_t flag;
	uint8_t is_cn10k;
} __plt_cache_aligned;

int cnxk_dmadev_copy(void *dev_private, uint16_t vchan, rte_iova_t src, rte_iova_t dst,
		     uint32_t length, uint64_t flags);
int cnxk_dmadev_copy_sg(void *dev_private, uint16_t vchan, const struct rte_dma_sge *src,
			const struct rte_dma_sge *dst, uint16_t nb_src, uint16_t nb_dst,
			uint64_t flags);
int cn10k_dmadev_copy(void *dev_private, uint16_t vchan, rte_iova_t src, rte_iova_t dst,
		      uint32_t length, uint64_t flags);
int cn10k_dmadev_copy_sg(void *dev_private, uint16_t vchan, const struct rte_dma_sge *src,
			 const struct rte_dma_sge *dst, uint16_t nb_src, uint16_t nb_dst,
			 uint64_t flags);

#endif
