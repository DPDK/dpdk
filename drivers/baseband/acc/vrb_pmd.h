/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _VRB_PMD_H_
#define _VRB_PMD_H_

#include "acc_common.h"
#include "acc200_pf_enum.h"
#include "acc200_vf_enum.h"
#include "vrb_cfg.h"

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, vrb_logtype, fmt "\n", \
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

#define ACC200_VARIANT               2

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

struct acc_registry_addr {
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
	unsigned int pf_mode;
	unsigned int pmon_ctrl_a;
	unsigned int pmon_ctrl_b;
	unsigned int pmon_ctrl_c;
	unsigned int vf2pf_doorbell;
	unsigned int pf2vf_doorbell;
};

/* Structure holding registry addresses for PF */
static const struct acc_registry_addr acc200_pf_reg_addr = {
	.dma_ring_dl5g_hi = VRB1_PfDmaFec5GdlDescBaseHiRegVf,
	.dma_ring_dl5g_lo = VRB1_PfDmaFec5GdlDescBaseLoRegVf,
	.dma_ring_ul5g_hi = VRB1_PfDmaFec5GulDescBaseHiRegVf,
	.dma_ring_ul5g_lo = VRB1_PfDmaFec5GulDescBaseLoRegVf,
	.dma_ring_dl4g_hi = VRB1_PfDmaFec4GdlDescBaseHiRegVf,
	.dma_ring_dl4g_lo = VRB1_PfDmaFec4GdlDescBaseLoRegVf,
	.dma_ring_ul4g_hi = VRB1_PfDmaFec4GulDescBaseHiRegVf,
	.dma_ring_ul4g_lo = VRB1_PfDmaFec4GulDescBaseLoRegVf,
	.dma_ring_fft_hi = VRB1_PfDmaFftDescBaseHiRegVf,
	.dma_ring_fft_lo = VRB1_PfDmaFftDescBaseLoRegVf,
	.ring_size =      VRB1_PfQmgrRingSizeVf,
	.info_ring_hi = VRB1_PfHiInfoRingBaseHiRegPf,
	.info_ring_lo = VRB1_PfHiInfoRingBaseLoRegPf,
	.info_ring_en = VRB1_PfHiInfoRingIntWrEnRegPf,
	.info_ring_ptr = VRB1_PfHiInfoRingPointerRegPf,
	.tail_ptrs_dl5g_hi = VRB1_PfDmaFec5GdlRespPtrHiRegVf,
	.tail_ptrs_dl5g_lo = VRB1_PfDmaFec5GdlRespPtrLoRegVf,
	.tail_ptrs_ul5g_hi = VRB1_PfDmaFec5GulRespPtrHiRegVf,
	.tail_ptrs_ul5g_lo = VRB1_PfDmaFec5GulRespPtrLoRegVf,
	.tail_ptrs_dl4g_hi = VRB1_PfDmaFec4GdlRespPtrHiRegVf,
	.tail_ptrs_dl4g_lo = VRB1_PfDmaFec4GdlRespPtrLoRegVf,
	.tail_ptrs_ul4g_hi = VRB1_PfDmaFec4GulRespPtrHiRegVf,
	.tail_ptrs_ul4g_lo = VRB1_PfDmaFec4GulRespPtrLoRegVf,
	.tail_ptrs_fft_hi = VRB1_PfDmaFftRespPtrHiRegVf,
	.tail_ptrs_fft_lo = VRB1_PfDmaFftRespPtrLoRegVf,
	.depth_log0_offset = VRB1_PfQmgrGrpDepthLog20Vf,
	.depth_log1_offset = VRB1_PfQmgrGrpDepthLog21Vf,
	.qman_group_func = VRB1_PfQmgrGrpFunction0,
	.hi_mode = VRB1_PfHiMsixVectorMapperPf,
	.pf_mode = VRB1_PfHiPfMode,
	.pmon_ctrl_a = VRB1_PfPermonACntrlRegVf,
	.pmon_ctrl_b = VRB1_PfPermonBCntrlRegVf,
	.pmon_ctrl_c = VRB1_PfPermonCCntrlRegVf,
	.vf2pf_doorbell = 0,
	.pf2vf_doorbell = 0,
};

/* Structure holding registry addresses for VF */
static const struct acc_registry_addr acc200_vf_reg_addr = {
	.dma_ring_dl5g_hi = VRB1_VfDmaFec5GdlDescBaseHiRegVf,
	.dma_ring_dl5g_lo = VRB1_VfDmaFec5GdlDescBaseLoRegVf,
	.dma_ring_ul5g_hi = VRB1_VfDmaFec5GulDescBaseHiRegVf,
	.dma_ring_ul5g_lo = VRB1_VfDmaFec5GulDescBaseLoRegVf,
	.dma_ring_dl4g_hi = VRB1_VfDmaFec4GdlDescBaseHiRegVf,
	.dma_ring_dl4g_lo = VRB1_VfDmaFec4GdlDescBaseLoRegVf,
	.dma_ring_ul4g_hi = VRB1_VfDmaFec4GulDescBaseHiRegVf,
	.dma_ring_ul4g_lo = VRB1_VfDmaFec4GulDescBaseLoRegVf,
	.dma_ring_fft_hi = VRB1_VfDmaFftDescBaseHiRegVf,
	.dma_ring_fft_lo = VRB1_VfDmaFftDescBaseLoRegVf,
	.ring_size = VRB1_VfQmgrRingSizeVf,
	.info_ring_hi = VRB1_VfHiInfoRingBaseHiVf,
	.info_ring_lo = VRB1_VfHiInfoRingBaseLoVf,
	.info_ring_en = VRB1_VfHiInfoRingIntWrEnVf,
	.info_ring_ptr = VRB1_VfHiInfoRingPointerVf,
	.tail_ptrs_dl5g_hi = VRB1_VfDmaFec5GdlRespPtrHiRegVf,
	.tail_ptrs_dl5g_lo = VRB1_VfDmaFec5GdlRespPtrLoRegVf,
	.tail_ptrs_ul5g_hi = VRB1_VfDmaFec5GulRespPtrHiRegVf,
	.tail_ptrs_ul5g_lo = VRB1_VfDmaFec5GulRespPtrLoRegVf,
	.tail_ptrs_dl4g_hi = VRB1_VfDmaFec4GdlRespPtrHiRegVf,
	.tail_ptrs_dl4g_lo = VRB1_VfDmaFec4GdlRespPtrLoRegVf,
	.tail_ptrs_ul4g_hi = VRB1_VfDmaFec4GulRespPtrHiRegVf,
	.tail_ptrs_ul4g_lo = VRB1_VfDmaFec4GulRespPtrLoRegVf,
	.tail_ptrs_fft_hi = VRB1_VfDmaFftRespPtrHiRegVf,
	.tail_ptrs_fft_lo = VRB1_VfDmaFftRespPtrLoRegVf,
	.depth_log0_offset = VRB1_VfQmgrGrpDepthLog20Vf,
	.depth_log1_offset = VRB1_VfQmgrGrpDepthLog21Vf,
	.qman_group_func = VRB1_VfQmgrGrpFunction0Vf,
	.hi_mode = VRB1_VfHiMsixVectorMapperVf,
	.pf_mode = 0,
	.pmon_ctrl_a = VRB1_VfPmACntrlRegVf,
	.pmon_ctrl_b = VRB1_VfPmBCntrlRegVf,
	.pmon_ctrl_c = VRB1_VfPmCCntrlRegVf,
	.vf2pf_doorbell = VRB1_VfHiVfToPfDbellVf,
	.pf2vf_doorbell = VRB1_VfHiPfToVfDbellVf,
};

#endif /* _VRB_PMD_H_ */
