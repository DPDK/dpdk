/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _DPI_RAWDEV_H_
#define _DPI_RAWDEV_H_

#include "otx2_common.h"
#include "otx2_mempool.h"

#define DPI_QUEUE_OPEN	0x1
#define DPI_QUEUE_CLOSE	0x2

/* DPI VF register offsets from VF_BAR0 */
#define DPI_VDMA_EN             (0x0)
#define DPI_VDMA_REQQ_CTL       (0x8)
#define DPI_VDMA_DBELL          (0x10)
#define DPI_VDMA_SADDR          (0x18)
#define DPI_VDMA_COUNTS         (0x20)
#define DPI_VDMA_NADDR          (0x28)
#define DPI_VDMA_IWBUSY         (0x30)
#define DPI_VDMA_CNT            (0x38)
#define DPI_VF_INT              (0x100)
#define DPI_VF_INT_W1S          (0x108)
#define DPI_VF_INT_ENA_W1C      (0x110)
#define DPI_VF_INT_ENA_W1S      (0x118)

#define DPI_MAX_VFS             8
#define DPI_DMA_CMD_SIZE        64
#define DPI_CHUNK_SIZE		1024
#define DPI_QUEUE_STOP		0x0
#define DPI_QUEUE_START		0x1

#define DPI_VDMA_SADDR_REQ_IDLE	63
#define DPI_MAX_POINTER		15
#define STRM_INC(s)	((s)->tail = ((s)->tail + 1) % (s)->max_cnt)
#define DPI_QFINISH_TIMEOUT	(10 * 1000)

/* DPI Transfer Type, pointer type in DPI_DMA_INSTR_HDR_S[XTYPE] */
#define DPI_XTYPE_OUTBOUND      (0)
#define DPI_XTYPE_INBOUND       (1)
#define DPI_XTYPE_INTERNAL_ONLY (2)
#define DPI_XTYPE_EXTERNAL_ONLY (3)
#define DPI_XTYPE_MASK		0x3
#define DPI_HDR_PT_ZBW_CA	0x0
#define DPI_HDR_PT_ZBW_NC	0x1
#define DPI_HDR_PT_WQP		0x2
#define DPI_HDR_PT_WQP_NOSTATUS	0x0
#define DPI_HDR_PT_WQP_STATUSCA	0x1
#define DPI_HDR_PT_WQP_STATUSNC	0x3
#define DPI_HDR_PT_CNT		0x3
#define DPI_HDR_PT_MASK		0x3
#define DPI_W0_TT_MASK		0x3
#define DPI_W0_GRP_MASK		0x3FF
/* Set Completion data to 0xFF when request submitted,
 * upon successful request completion engine reset to completion status
 */
#define DPI_REQ_CDATA		0xFF

struct dpi_vf_s {
	struct rte_pci_device *dev;
	uint8_t state;
	uint16_t vf_id;
	uint8_t domain;
	uintptr_t vf_bar0;
	uintptr_t vf_bar2;

	uint16_t pool_size_m1;
	uint16_t index;
	uint64_t *base_ptr;
	void *chunk_pool;
	struct otx2_mbox *mbox;
};

struct dpi_rawdev_conf_s {
	void *chunk_pool;
};

enum dpi_dma_queue_result_e {
	DPI_DMA_QUEUE_SUCCESS = 0,
	DPI_DMA_QUEUE_NO_MEMORY = -1,
	DPI_DMA_QUEUE_INVALID_PARAM = -2,
};

struct dpi_dma_req_compl_s {
	uint64_t cdata;
	void (*compl_cb)(void *dev, void *arg);
	void *cb_data;
};

union dpi_dma_ptr_u {
	uint64_t u[2];
	struct dpi_dma_s {
		uint64_t length:16;
		uint64_t reserved:44;
		uint64_t bed:1; /* Big-Endian */
		uint64_t alloc_l2:1;
		uint64_t full_write:1;
		uint64_t invert:1;
		uint64_t ptr;
	} s;
};

struct dpi_dma_buf_ptr_s {
	union dpi_dma_ptr_u *rptr[DPI_MAX_POINTER]; /* Read From pointer list */
	union dpi_dma_ptr_u *wptr[DPI_MAX_POINTER]; /* Write to pointer list */
	uint8_t rptr_cnt;
	uint8_t wptr_cnt;
	struct dpi_dma_req_compl_s *comp_ptr;
};

struct dpi_cring_data_s {
	struct dpi_dma_req_compl_s **compl_data;
	uint16_t max_cnt;
	uint16_t head;
	uint16_t tail;
};

struct dpi_dma_queue_ctx_s {
	uint16_t xtype:2;

	/* Completion pointer type */
	uint16_t pt:2;

	/* Completion updated using WQE */
	uint16_t tt:2;
	uint16_t grp:10;
	uint32_t tag;

	/* Valid only for Outbound only mode */
	uint16_t aura:12;
	uint16_t csel:1;
	uint16_t ca:1;
	uint16_t fi:1;
	uint16_t ii:1;
	uint16_t fl:1;

	uint16_t pvfe:1;
	uint16_t dealloce:1;
	uint16_t req_type:2;
	uint16_t use_lock:1;
	uint16_t deallocv;

	struct dpi_cring_data_s *c_ring;
};

/* DPI DMA Instruction Header Format */
union dpi_dma_instr_hdr_u {
	uint64_t u[4];

	struct dpi_dma_instr_hdr_s_s {
		uint64_t tag:32;
		uint64_t tt:2;
		uint64_t grp:10;
		uint64_t reserved_44_47:4;
		uint64_t nfst:4;
		uint64_t reserved_52_53:2;
		uint64_t nlst:4;
		uint64_t reserved_58_63:6;
		/* Word 0 - End */

		uint64_t aura:12;
		uint64_t reserved_76_79:4;
		uint64_t deallocv:16;
		uint64_t dealloce:1;
		uint64_t pvfe:1;
		uint64_t reserved_98_99:2;
		uint64_t pt:2;
		uint64_t reserved_102_103:2;
		uint64_t fl:1;
		uint64_t ii:1;
		uint64_t fi:1;
		uint64_t ca:1;
		uint64_t csel:1;
		uint64_t reserved_109_111:3;
		uint64_t xtype:2;
		uint64_t reserved_114_119:6;
		uint64_t fport:2;
		uint64_t reserved_122_123:2;
		uint64_t lport:2;
		uint64_t reserved_126_127:2;
		/* Word 1 - End */

		uint64_t ptr:64;
		/* Word 2 - End */

		uint64_t reserved_192_255:64;
		/* Word 3 - End */
	} s;
};

int otx2_dpi_queue_open(uint16_t vf_id, uint32_t size, uint32_t gaura);
int otx2_dpi_queue_close(uint16_t vf_id);
int test_otx2_dma_rawdev(uint16_t val);

#endif /* _DPI_RAWDEV_H_ */
