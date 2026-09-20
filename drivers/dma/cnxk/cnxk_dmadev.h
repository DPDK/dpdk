/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell International Ltd.
 */
#ifndef CNXK_DMADEV_H
#define CNXK_DMADEV_H

#include <string.h>
#include <unistd.h>

#include <bus_pci_driver.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_dmadev.h>
#include <rte_dmadev_pmd.h>
#include <rte_eal.h>
#include <rte_kvargs.h>
#include <rte_lcore.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_mcslock.h>
#include <rte_mempool.h>
#include <rte_pci.h>

#include <roc_api.h>

#include "cnxk_dma_event_dp.h"

#define CNXK_DPI_MAX_POINTER		    15
#define CNXK_DPI_STRM_INC(s, var)	    ((s).var = ((s).var + 1) & (s).max_cnt)
#define CNXK_DPI_STRM_DEC(s, var)	    ((s).var = ((s).var - 1) == -1 ? (s).max_cnt :	\
						((s).var - 1))
#define CNXK_DPI_MAX_DESC		    32768
#define CNXK_DPI_MIN_DESC		    2
#define CN10K_DPI_MAX_PRI		    2
#define CNXK_DPI_MAX_VCHANS_PER_QUEUE	    128
#define CNXK_DPI_QUEUE_BUF_SIZE		    16256
/* Maximum pool size supported by device is 128 * 1024. When RTE_LIBRTE_MEMPOOL_DEBUG is enabled
 * mempool->trailer size will be increased by 8B. Additionally if the pool is not created with
 * RTE_MEMPOOL_F_NO_CACHE_ALIGN, trailer will be expanded to cache line size.
 * To allow future needs, limit the max size to 127KB
 */
#define CNXK_DPI_QUEUE_BUF_SIZE_V2	    130048
#define CNXK_DPI_POOL_MAX_CACHE_SZ	    (16)
#define CNXK_DPI_DW_PER_SINGLE_CMD	    8
#define CNXK_DPI_HDR_LEN		    4
#define CNXK_DPI_CMD_LEN(src, dst)	    (CNXK_DPI_HDR_LEN + ((src) << 1) + ((dst) << 1))
#define CNXK_DPI_MAX_CMD_SZ		    CNXK_DPI_CMD_LEN(CNXK_DPI_MAX_POINTER,              \
							     CNXK_DPI_MAX_POINTER)
#define CNXK_DPI_CHUNKS_FROM_DESC(cz, desc) (((desc) / (((cz) / 8) / CNXK_DPI_MAX_CMD_SZ)) + 1)
#define CNXK_DPI_COMPL_OFFSET		    ROC_CACHE_LINE_SZ
#define CNXK_DPI_SRC_DST_KEY(src, dst)	    ((0x2040) | ((src & 0x3F) << 7) | (dst & 0x3F))

#define CN20K_DPI_MAX_POINTER		    4
#define CN20K_DPI_MAX_DESC		    2048
#define CN20K_DPI_MIN_DESC		    128
#define CN20K_DPI_MAX_VCHANS		    512
#define CN20K_DPI_DEF_VCHANS		    8
#define CN20K_DPI_MAX_LFS		    256

#define CN20K_DPI_NUM_VCHANS	"num_vchans"
#define CN20K_DPI_NUM_LFS	"num_lfs"

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

	struct cn20k_dpi_instr_cmd {
		uint64_t nfst : 3;
		uint64_t reserved_3 : 1;
		uint64_t nlst : 3;
		uint64_t reserved_7 : 1;
		uint64_t msix_int : 1;
		uint64_t ct : 3;
		uint64_t chan : 14;
		uint64_t reserved_26_29 : 4;
		uint64_t aura : 20;
		uint64_t xt : 2;
		uint64_t ivec : 9;
		uint64_t fe : 1;
		uint64_t reserved_62 : 1;
		uint64_t vld : 1;
		/* Word 0 - End */
	} cn20k;
};

struct cn20k_ring_conf {
	enum rte_dma_direction direction;
	uint16_t pending;
	uint16_t num_desc;
	uint8_t num_vchans;
	bool used;
};

struct cnxk_dpi_cdesc_data_s {
	uint16_t max_cnt;
	uint16_t head;
	uint16_t tail;
	uint8_t *compl_ptr;
	struct rte_dma_op **ops;
};

struct cnxk_dpi_conf {
	union cnxk_dpi_instr_cmd cmd;
	struct cnxk_dpi_cdesc_data_s c_desc;
	uint16_t desc_idx;
	uintptr_t dbell;
	struct rte_dma_stats stats;
	uint64_t completed_offset;
	struct roc_dpi_lf_que *que;
	union roc_dpi_lf_ccfg chan_cfg;
	struct roc_dpi_lf_ring_cfg cfg;
	uint16_t ridx;
	bool adapter_enabled;
	bool cfg_done;
};

struct cnxk_dpi_vf_s {
	/* Fast path */
	uint64_t *chunk_base;
	uint16_t chunk_head;
	uint16_t chunk_size_m1;
	uint16_t total_pnum_words;
	uint16_t vchans_per_ring;
	struct rte_mempool *chunk_pool;
	struct cnxk_dpi_conf *conf;
	struct cn20k_ring_conf *ring_conf;
	RTE_ATOMIC(rte_mcslock_t *) mcs_lock;
	/* Slow path */
	struct roc_dpi rdpi;
	uint32_t aura;
	uint16_t max_lfs;
	uint16_t max_vchans;
	uint16_t num_vchans;
	uint16_t chan_tbl;
	uint16_t flag;
	uint8_t is_cn10k;
	uint8_t is_ring_conf_done;
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
uint16_t cnxk_dma_ops_enqueue(void *dev_private, uint16_t vchan, struct rte_dma_op **ops,
			      uint16_t nb_ops);
uint16_t cn10k_dma_ops_enqueue(void *dev_private, uint16_t vchan, struct rte_dma_op **ops,
			       uint16_t nb_ops);
uint16_t cnxk_dma_ops_dequeue(void *dev_private, uint16_t vchan, struct rte_dma_op **ops,
			      uint16_t nb_ops);
int cn20k_dmadev_copy(void *dev_private, uint16_t vchan, rte_iova_t src, rte_iova_t dst,
		      uint32_t length, uint64_t flags);
int cn20k_dmadev_copy_sg(void *dev_private, uint16_t vchan, const struct rte_dma_sge *src,
			 const struct rte_dma_sge *dst, uint16_t nb_src, uint16_t nb_dst,
			 uint64_t flags);
int cn20k_dmadev_fill(void *dev_private, uint16_t vchan, uint64_t pattern, rte_iova_t dst,
		      uint32_t length, uint64_t flags);
uint16_t cn20k_dma_ops_enqueue(void *dev_private, uint16_t vchan, struct rte_dma_op **ops,
			       uint16_t nb_ops);
#endif
