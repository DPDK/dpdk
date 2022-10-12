/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _RTE_ACC200_PMD_H_
#define _RTE_ACC200_PMD_H_

#include "acc_common.h"
#include "acc200_pf_enum.h"
#include "acc200_vf_enum.h"
#include "acc200_cfg.h"

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, acc200_logtype, fmt "\n", \
		##__VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(fmt, ...) \
		rte_bbdev_log(DEBUG, "acc200_pmd: " fmt, \
		##__VA_ARGS__)
#else
#define rte_bbdev_log_debug(fmt, ...)
#endif

/* ACC200 PF and VF driver names */
#define ACC200PF_DRIVER_NAME           intel_acc200_pf
#define ACC200VF_DRIVER_NAME           intel_acc200_vf

/* ACC200 PCI vendor & device IDs */
#define RTE_ACC200_VENDOR_ID           (0x8086)
#define RTE_ACC200_PF_DEVICE_ID        (0x57C0)
#define RTE_ACC200_VF_DEVICE_ID        (0x57C1)

#define ACC200_MAX_PF_MSIX            (256+32)
#define ACC200_MAX_VF_MSIX            (256+7)

/* Values used in writing to the registers */
#define ACC200_REG_IRQ_EN_ALL          0x1FF83FF  /* Enable all interrupts */

/* Number of Virtual Functions ACC200 supports */
#define ACC200_NUM_VFS                  16
#define ACC200_NUM_QGRPS                16
#define ACC200_NUM_AQS                  16

#define ACC200_GRP_ID_SHIFT    10 /* Queue Index Hierarchy */
#define ACC200_VF_ID_SHIFT     4  /* Queue Index Hierarchy */
#define ACC200_WORDS_IN_ARAM_SIZE (256 * 1024 / 4)

/* Mapping of signals for the available engines */
#define ACC200_SIG_UL_5G       0
#define ACC200_SIG_UL_5G_LAST  4
#define ACC200_SIG_DL_5G      10
#define ACC200_SIG_DL_5G_LAST 11
#define ACC200_SIG_UL_4G      12
#define ACC200_SIG_UL_4G_LAST 16
#define ACC200_SIG_DL_4G      21
#define ACC200_SIG_DL_4G_LAST 23
#define ACC200_SIG_FFT        24
#define ACC200_SIG_FFT_LAST   24

#define ACC200_NUM_ACCS       5

/* ACC200 Configuration */
#define ACC200_FABRIC_MODE      0x8000103
#define ACC200_CFG_DMA_ERROR    0x3DF
#define ACC200_CFG_AXI_CACHE    0x11
#define ACC200_CFG_QMGR_HI_P    0x0F0F
#define ACC200_RESET_HARD       0x1FF
#define ACC200_ENGINES_MAX      9
#define ACC200_GPEX_AXIMAP_NUM  17
#define ACC200_CLOCK_GATING_EN  0x30000
#define ACC200_FFT_CFG_0        0x2001
#define ACC200_FFT_RAM_EN       0x80008000
#define ACC200_FFT_RAM_DIS      0x0
#define ACC200_FFT_RAM_SIZE     512
#define ACC200_CLK_EN           0x00010A01
#define ACC200_CLK_DIS          0x01F10A01
#define ACC200_PG_MASK_0        0x1F
#define ACC200_PG_MASK_1        0xF
#define ACC200_PG_MASK_2        0x1
#define ACC200_PG_MASK_3        0x0
#define ACC200_PG_MASK_FFT      1
#define ACC200_PG_MASK_4GUL     4
#define ACC200_PG_MASK_5GUL     8
#define ACC200_STATUS_WAIT      10
#define ACC200_STATUS_TO        100

struct acc200_registry_addr {
	unsigned int dma_ring_dl5g_hi;
	unsigned int dma_ring_dl5g_lo;
	unsigned int dma_ring_ul5g_hi;
	unsigned int dma_ring_ul5g_lo;
	unsigned int dma_ring_dl4g_hi;
	unsigned int dma_ring_dl4g_lo;
	unsigned int dma_ring_ul4g_hi;
	unsigned int dma_ring_ul4g_lo;
	unsigned int dma_ring_fft_hi;
	unsigned int dma_ring_fft_lo;
	unsigned int ring_size;
	unsigned int info_ring_hi;
	unsigned int info_ring_lo;
	unsigned int info_ring_en;
	unsigned int info_ring_ptr;
	unsigned int tail_ptrs_dl5g_hi;
	unsigned int tail_ptrs_dl5g_lo;
	unsigned int tail_ptrs_ul5g_hi;
	unsigned int tail_ptrs_ul5g_lo;
	unsigned int tail_ptrs_dl4g_hi;
	unsigned int tail_ptrs_dl4g_lo;
	unsigned int tail_ptrs_ul4g_hi;
	unsigned int tail_ptrs_ul4g_lo;
	unsigned int tail_ptrs_fft_hi;
	unsigned int tail_ptrs_fft_lo;
	unsigned int depth_log0_offset;
	unsigned int depth_log1_offset;
	unsigned int qman_group_func;
	unsigned int hi_mode;
	unsigned int pmon_ctrl_a;
	unsigned int pmon_ctrl_b;
	unsigned int pmon_ctrl_c;
};

/* Structure holding registry addresses for PF */
static const struct acc200_registry_addr pf_reg_addr = {
	.dma_ring_dl5g_hi = HWPfDmaFec5GdlDescBaseHiRegVf,
	.dma_ring_dl5g_lo = HWPfDmaFec5GdlDescBaseLoRegVf,
	.dma_ring_ul5g_hi = HWPfDmaFec5GulDescBaseHiRegVf,
	.dma_ring_ul5g_lo = HWPfDmaFec5GulDescBaseLoRegVf,
	.dma_ring_dl4g_hi = HWPfDmaFec4GdlDescBaseHiRegVf,
	.dma_ring_dl4g_lo = HWPfDmaFec4GdlDescBaseLoRegVf,
	.dma_ring_ul4g_hi = HWPfDmaFec4GulDescBaseHiRegVf,
	.dma_ring_ul4g_lo = HWPfDmaFec4GulDescBaseLoRegVf,
	.dma_ring_fft_hi = HWPDmaFftDescBaseHiRegVf,
	.dma_ring_fft_lo = HWPDmaFftDescBaseLoRegVf,
	.ring_size = HWPfQmgrRingSizeVf,
	.info_ring_hi = HWPfHiInfoRingBaseHiRegPf,
	.info_ring_lo = HWPfHiInfoRingBaseLoRegPf,
	.info_ring_en = HWPfHiInfoRingIntWrEnRegPf,
	.info_ring_ptr = HWPfHiInfoRingPointerRegPf,
	.tail_ptrs_dl5g_hi = HWPfDmaFec5GdlRespPtrHiRegVf,
	.tail_ptrs_dl5g_lo = HWPfDmaFec5GdlRespPtrLoRegVf,
	.tail_ptrs_ul5g_hi = HWPfDmaFec5GulRespPtrHiRegVf,
	.tail_ptrs_ul5g_lo = HWPfDmaFec5GulRespPtrLoRegVf,
	.tail_ptrs_dl4g_hi = HWPfDmaFec4GdlRespPtrHiRegVf,
	.tail_ptrs_dl4g_lo = HWPfDmaFec4GdlRespPtrLoRegVf,
	.tail_ptrs_ul4g_hi = HWPfDmaFec4GulRespPtrHiRegVf,
	.tail_ptrs_ul4g_lo = HWPfDmaFec4GulRespPtrLoRegVf,
	.tail_ptrs_fft_hi = HWPDmaFftRespPtrHiRegVf,
	.tail_ptrs_fft_lo = HWPDmaFftRespPtrLoRegVf,
	.depth_log0_offset = HWPfQmgrGrpDepthLog20Vf,
	.depth_log1_offset = HWPfQmgrGrpDepthLog21Vf,
	.qman_group_func = HWPfQmgrGrpFunction0,
	.hi_mode = HWPfHiMsixVectorMapperPf,
	.pmon_ctrl_a = HWPfPermonACntrlRegVf,
	.pmon_ctrl_b = HWPfPermonBCntrlRegVf,
	.pmon_ctrl_c = HWPfPermonCCntrlRegVf,
};

/* Structure holding registry addresses for VF */
static const struct acc200_registry_addr vf_reg_addr = {
	.dma_ring_dl5g_hi = HWVfDmaFec5GdlDescBaseHiRegVf,
	.dma_ring_dl5g_lo = HWVfDmaFec5GdlDescBaseLoRegVf,
	.dma_ring_ul5g_hi = HWVfDmaFec5GulDescBaseHiRegVf,
	.dma_ring_ul5g_lo = HWVfDmaFec5GulDescBaseLoRegVf,
	.dma_ring_dl4g_hi = HWVfDmaFec4GdlDescBaseHiRegVf,
	.dma_ring_dl4g_lo = HWVfDmaFec4GdlDescBaseLoRegVf,
	.dma_ring_ul4g_hi = HWVfDmaFec4GulDescBaseHiRegVf,
	.dma_ring_ul4g_lo = HWVfDmaFec4GulDescBaseLoRegVf,
	.dma_ring_fft_hi = HWVfDmaFftDescBaseHiRegVf,
	.dma_ring_fft_lo = HWVfDmaFftDescBaseLoRegVf,
	.ring_size = HWVfQmgrRingSizeVf,
	.info_ring_hi = HWVfHiInfoRingBaseHiVf,
	.info_ring_lo = HWVfHiInfoRingBaseLoVf,
	.info_ring_en = HWVfHiInfoRingIntWrEnVf,
	.info_ring_ptr = HWVfHiInfoRingPointerVf,
	.tail_ptrs_dl5g_hi = HWVfDmaFec5GdlRespPtrHiRegVf,
	.tail_ptrs_dl5g_lo = HWVfDmaFec5GdlRespPtrLoRegVf,
	.tail_ptrs_ul5g_hi = HWVfDmaFec5GulRespPtrHiRegVf,
	.tail_ptrs_ul5g_lo = HWVfDmaFec5GulRespPtrLoRegVf,
	.tail_ptrs_dl4g_hi = HWVfDmaFec4GdlRespPtrHiRegVf,
	.tail_ptrs_dl4g_lo = HWVfDmaFec4GdlRespPtrLoRegVf,
	.tail_ptrs_ul4g_hi = HWVfDmaFec4GulRespPtrHiRegVf,
	.tail_ptrs_ul4g_lo = HWVfDmaFec4GulRespPtrLoRegVf,
	.tail_ptrs_fft_hi = HWVfDmaFftRespPtrHiRegVf,
	.tail_ptrs_fft_lo = HWVfDmaFftRespPtrLoRegVf,
	.depth_log0_offset = HWVfQmgrGrpDepthLog20Vf,
	.depth_log1_offset = HWVfQmgrGrpDepthLog21Vf,
	.qman_group_func = HWVfQmgrGrpFunction0Vf,
	.hi_mode = HWVfHiMsixVectorMapperVf,
	.pmon_ctrl_a = HWVfPmACntrlRegVf,
	.pmon_ctrl_b = HWVfPmBCntrlRegVf,
	.pmon_ctrl_c = HWVfPmCCntrlRegVf,
};

#endif /* _RTE_ACC200_PMD_H_ */
