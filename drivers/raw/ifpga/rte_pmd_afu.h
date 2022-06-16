/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022 Intel Corporation
 */

#ifndef RTE_PMD_AFU_H
#define RTE_PMD_AFU_H

/**
 * @file rte_pmd_afu.h
 *
 * AFU PMD specific definitions.
 *
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define RTE_PMD_AFU_N3000_NLB   1
#define RTE_PMD_AFU_N3000_DMA   2

#define NLB_MODE_LPBK      0
#define NLB_MODE_READ      1
#define NLB_MODE_WRITE     2
#define NLB_MODE_TRPUT     3

#define NLB_VC_AUTO        0
#define NLB_VC_VL0         1
#define NLB_VC_VH0         2
#define NLB_VC_VH1         3
#define NLB_VC_RANDOM      4

#define NLB_WRLINE_M       0
#define NLB_WRLINE_I       1
#define NLB_WRPUSH_I       2

#define NLB_RDLINE_S       0
#define NLB_RDLINE_I       1
#define NLB_RDLINE_MIXED   2

#define MIN_CACHE_LINES   1
#define MAX_CACHE_LINES   1024

#define MIN_DMA_BUF_SIZE  64
#define MAX_DMA_BUF_SIZE  (1023 * 1024)

/**
 * NLB AFU configuration data structure.
 */
struct rte_pmd_afu_nlb_cfg {
	uint32_t mode;
	uint32_t begin;
	uint32_t end;
	uint32_t multi_cl;
	uint32_t cont;
	uint32_t timeout;
	uint32_t cache_policy;
	uint32_t cache_hint;
	uint32_t read_vc;
	uint32_t write_vc;
	uint32_t wrfence_vc;
	uint32_t freq_mhz;
};

/**
 * DMA AFU configuration data structure.
 */
struct rte_pmd_afu_dma_cfg {
	uint32_t index;     /* index of DMA controller */
	uint32_t length;    /* total length of data to DMA */
	uint32_t offset;    /* address offset of target memory */
	uint32_t size;      /* size of transfer buffer */
	uint32_t pattern;   /* data pattern to fill in test buffer */
	uint32_t unaligned; /* use unaligned address or length in sweep test */
	uint32_t verbose;   /* enable verbose error information in test */
};

/**
 * N3000 AFU configuration data structure.
 */
struct rte_pmd_afu_n3000_cfg {
	int type;   /* RTE_PMD_AFU_N3000_NLB or RTE_PMD_AFU_N3000_DMA */
	union {
		struct rte_pmd_afu_nlb_cfg nlb_cfg;
		struct rte_pmd_afu_dma_cfg dma_cfg;
	};
};

/**
 * HE-LPBK & HE-MEM-LPBK AFU configuration data structure.
 */
struct rte_pmd_afu_he_lpbk_cfg {
	uint32_t mode;
	uint32_t begin;
	uint32_t end;
	uint32_t multi_cl;
	uint32_t cont;
	uint32_t timeout;
	uint32_t trput_interleave;
	uint32_t freq_mhz;
};

/**
 * HE-MEM-TG AFU configuration data structure.
 */
struct rte_pmd_afu_he_mem_tg_cfg {
	uint32_t channel_mask;   /* mask of traffic generator channel */
};

#define NUM_RND_SEEDS  3

/**
 * HE-HSSI AFU configuration data structure.
 */
struct rte_pmd_afu_he_hssi_cfg {
	uint32_t port;
	uint32_t timeout;
	uint32_t num_packets;
	uint32_t random_length;
	uint32_t packet_length;
	uint32_t random_payload;
	uint32_t rnd_seed[NUM_RND_SEEDS];
	uint64_t src_addr;
	uint64_t dest_addr;
	int he_loopback;
};

#ifdef __cplusplus
}
#endif

#endif /* RTE_PMD_AFU_H */
