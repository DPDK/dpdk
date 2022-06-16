/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef AFU_PMD_N3000_H
#define AFU_PMD_N3000_H

#ifdef __cplusplus
extern "C" {
#endif

#include "afu_pmd_core.h"
#include "rte_pmd_afu.h"

#define N3000_AFU_UUID_L  0xc000c9660d824272
#define N3000_AFU_UUID_H  0x9aeffe5f84570612
#define N3000_NLB0_UUID_L 0xf89e433683f9040b
#define N3000_NLB0_UUID_H 0xd8424dc4a4a3c413
#define N3000_DMA_UUID_L  0xa9149a35bace01ea
#define N3000_DMA_UUID_H  0xef82def7f6ec40fc

#define NUM_N3000_DMA  4
#define MAX_MSIX_VEC   7

/* N3000 DFL definition */
#define DFH_UUID_L_OFFSET  8
#define DFH_UUID_H_OFFSET  16
#define DFH_TYPE(hdr)  (((hdr) >> 60) & 0xf)
#define DFH_TYPE_AFU  1
#define DFH_TYPE_BBB  2
#define DFH_TYPE_PRIVATE  3
#define DFH_EOL(hdr)  (((hdr) >> 40) & 0x1)
#define DFH_NEXT_OFFSET(hdr)  (((hdr) >> 16) & 0xffffff)
#define DFH_FEATURE_ID(hdr)  ((hdr) & 0xfff)
#define PORT_ATTR_REG(n)  (((n) << 3) + 0x38)
#define PORT_IMPLEMENTED(attr)  (((attr) >> 60) & 0x1)
#define PORT_BAR(attr)  (((attr) >> 32) & 0x7)
#define PORT_OFFSET(attr)  ((attr) & 0xffffff)
#define PORT_FEATURE_UINT_ID  0x12
#define PORT_UINT_CAP_REG  0x8
#define PORT_VEC_START(cap)  (((cap) >> 12) & 0xfff)
#define PORT_VEC_COUNT(cap)  ((cap) >> 12 & 0xfff)
#define PORT_CTRL_REG  0x38
#define PORT_SOFT_RESET  (0x1 << 0)

/* NLB registers definition */
#define CSR_SCRATCHPAD0    0x100
#define CSR_SCRATCHPAD1    0x108
#define CSR_AFU_DSM_BASEL  0x110
#define CSR_AFU_DSM_BASEH  0x114
#define CSR_SRC_ADDR       0x120
#define CSR_DST_ADDR       0x128
#define CSR_NUM_LINES      0x130
#define CSR_CTL            0x138
#define CSR_CFG            0x140
#define CSR_INACT_THRESH   0x148
#define CSR_INTERRUPT0     0x150
#define CSR_SWTEST_MSG     0x158
#define CSR_STATUS0        0x160
#define CSR_STATUS1        0x168
#define CSR_ERROR          0x170
#define CSR_STRIDE         0x178
#define CSR_HE_INFO0       0x180

#define DSM_SIZE           0x200000
#define DSM_STATUS         0x40
#define DSM_POLL_INTERVAL  5  /* ms */
#define DSM_TIMEOUT        1000  /* ms */

#define NLB_BUF_SIZE  0x400000
#define TEST_MEM_ALIGN  1024

struct nlb_csr_ctl {
	union {
		uint32_t csr;
		struct {
			uint32_t reset:1;
			uint32_t start:1;
			uint32_t force_completion:1;
			uint32_t reserved:29;
		};
	};
};

struct nlb_csr_cfg {
	union {
		uint32_t csr;
		struct {
			uint32_t wrthru_en:1;
			uint32_t cont:1;
			uint32_t mode:3;
			uint32_t multicl_len:2;
			uint32_t rsvd1:1;
			uint32_t delay_en:1;
			uint32_t rdsel:2;
			uint32_t rsvd2:1;
			uint32_t chsel:3;
			uint32_t rsvd3:1;
			uint32_t wrpush_i:1;
			uint32_t wr_chsel:3;
			uint32_t rsvd4:3;
			uint32_t test_cfg:5;
			uint32_t interrupt_on_error:1;
			uint32_t interrupt_testmode:1;
			uint32_t wrfence_chsel:2;
		};
	};
};

struct nlb_status0 {
	union {
		uint64_t csr;
		struct {
			uint32_t num_writes;
			uint32_t num_reads;
		};
	};
};

struct nlb_status1 {
	union {
		uint64_t csr;
		struct {
			uint32_t num_pend_writes;
			uint32_t num_pend_reads;
		};
	};
};

struct nlb_dsm_status {
	uint32_t test_complete;
	uint32_t test_error;
	uint64_t num_clocks;
	uint32_t num_reads;
	uint32_t num_writes;
	uint32_t start_overhead;
	uint32_t end_overhead;
};

/* DMA registers definition */
#define DMA_CSR       0x40
#define DMA_DESC      0x60
#define DMA_ASE_CTRL  0x200
#define DMA_ASE_DATA  0x1000

#define DMA_ASE_WINDOW       4096
#define DMA_ASE_WINDOW_MASK  ((uint64_t)(DMA_ASE_WINDOW - 1))
#define INVALID_ASE_PAGE     0xffffffffffffffffULL

#define DMA_WF_MAGIC             0x5772745F53796E63ULL
#define DMA_WF_MAGIC_ROM         0x1000000000000
#define DMA_HOST_ADDR(addr)      ((addr) | 0x2000000000000)
#define DMA_WF_HOST_ADDR(addr)   ((addr) | 0x3000000000000)

#define NUM_DMA_BUF   8
#define HALF_DMA_BUF  (NUM_DMA_BUF / 2)

#define DMA_MASK_32_BIT 0xFFFFFFFF

#define DMA_CSR_BUSY           0x1
#define DMA_DESC_BUFFER_EMPTY  0x2
#define DMA_DESC_BUFFER_FULL   0x4

#define DWORD_BYTES 4
#define IS_ALIGNED_DWORD(addr) (((addr) % DWORD_BYTES) == 0)

#define QWORD_BYTES 8
#define IS_ALIGNED_QWORD(addr) (((addr) % QWORD_BYTES) == 0)

#define DMA_ALIGN_BYTES 64
#define IS_DMA_ALIGNED(addr) (((addr) % DMA_ALIGN_BYTES) == 0)

#define CCIP_ALIGN_BYTES (DMA_ALIGN_BYTES << 2)

#define DMA_TIMEOUT_MSEC  5000

#define MAGIC_BUF_SIZE  64
#define ERR_CHECK_LIMIT  64

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

typedef enum {
	HOST_TO_FPGA = 0,
	FPGA_TO_HOST,
	FPGA_TO_FPGA,
	FPGA_MAX_TRANSFER_TYPE,
} fpga_dma_type;

typedef union {
	uint32_t csr;
	struct {
		uint32_t tx_channel:8;
		uint32_t generate_sop:1;
		uint32_t generate_eop:1;
		uint32_t park_reads:1;
		uint32_t park_writes:1;
		uint32_t end_on_eop:1;
		uint32_t reserved_1:1;
		uint32_t transfer_irq_en:1;
		uint32_t early_term_irq_en:1;
		uint32_t trans_error_irq_en:8;
		uint32_t early_done_en:1;
		uint32_t reserved_2:6;
		uint32_t go:1;
	};
} msgdma_desc_ctrl;

typedef struct __rte_packed {
	uint32_t rd_address;
	uint32_t wr_address;
	uint32_t len;
	uint16_t seq_num;
	uint8_t rd_burst_count;
	uint8_t wr_burst_count;
	uint16_t rd_stride;
	uint16_t wr_stride;
	uint32_t rd_address_ext;
	uint32_t wr_address_ext;
	msgdma_desc_ctrl control;
} msgdma_ext_desc;

typedef union {
	uint32_t csr;
	struct {
		uint32_t busy:1;
		uint32_t desc_buf_empty:1;
		uint32_t desc_buf_full:1;
		uint32_t rsp_buf_empty:1;
		uint32_t rsp_buf_full:1;
		uint32_t stopped:1;
		uint32_t resetting:1;
		uint32_t stopped_on_error:1;
		uint32_t stopped_on_early_term:1;
		uint32_t irq:1;
		uint32_t reserved:22;
	};
} msgdma_status;

typedef union {
	uint32_t csr;
	struct {
		uint32_t stop_dispatcher:1;
		uint32_t reset_dispatcher:1;
		uint32_t stop_on_error:1;
		uint32_t stopped_on_early_term:1;
		uint32_t global_intr_en_mask:1;
		uint32_t stop_descriptors:1;
		uint32_t reserved:22;
	};
} msgdma_ctrl;

typedef union {
	uint32_t csr;
	struct {
		uint32_t rd_fill_level:16;
		uint32_t wr_fill_level:16;
	};
} msgdma_fill_level;

typedef union {
	uint32_t csr;
	struct {
		uint32_t rsp_fill_level:16;
		uint32_t reserved:16;
	};
} msgdma_rsp_level;

typedef union {
	uint32_t csr;
	struct {
		uint32_t rd_seq_num:16;
		uint32_t wr_seq_num:16;
	};
} msgdma_seq_num;

typedef struct __rte_packed {
	msgdma_status status;
	msgdma_ctrl ctrl;
	msgdma_fill_level fill_level;
	msgdma_rsp_level rsp;
	msgdma_seq_num seq_num;
} msgdma_csr;

#define CSR_STATUS(csr)   (&(((msgdma_csr *)(csr))->status))
#define CSR_CONTROL(csr)  (&(((msgdma_csr *)(csr))->ctrl))

struct nlb_afu_ctx {
	uint8_t *addr;
	uint8_t *dsm_ptr;
	uint64_t dsm_iova;
	uint8_t *src_ptr;
	uint64_t src_iova;
	uint8_t *dest_ptr;
	uint64_t dest_iova;
	struct nlb_dsm_status *status_ptr;
};

struct dma_afu_ctx {
	int index;
	uint8_t *addr;
	uint8_t *csr_addr;
	uint8_t *desc_addr;
	uint8_t *ase_ctrl_addr;
	uint8_t *ase_data_addr;
	uint64_t mem_size;
	uint64_t cur_ase_page;
	int event_fd;
	int verbose;
	int pattern;
	void *data_buf;
	void *ref_buf;
	msgdma_ext_desc *desc_buf;
	uint64_t *magic_buf;
	uint64_t magic_iova;
	uint32_t dma_buf_size;
	uint64_t *dma_buf[NUM_DMA_BUF];
	uint64_t dma_iova[NUM_DMA_BUF];
};

struct n3000_afu_priv {
	struct rte_pmd_afu_nlb_cfg nlb_cfg;
	struct rte_pmd_afu_dma_cfg dma_cfg;
	struct nlb_afu_ctx nlb_ctx;
	struct dma_afu_ctx dma_ctx[NUM_N3000_DMA];
	int num_dma;
	int cfg_type;
};

#ifdef __cplusplus
}
#endif

#endif /* AFU_PMD_N3000_H */
