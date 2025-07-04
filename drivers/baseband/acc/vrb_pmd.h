/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _VRB_PMD_H_
#define _VRB_PMD_H_

#include "acc_common.h"
#include "vrb1_pf_enum.h"
#include "vrb1_vf_enum.h"
#include "vrb2_pf_enum.h"
#include "vrb2_vf_enum.h"
#include "vrb_cfg.h"

/* Helper macro for logging */
#define rte_bbdev_log(level, ...) \
	RTE_LOG_LINE(level, VRB, __VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(...) \
	rte_bbdev_log(DEBUG, __VA_ARGS__)
#else
#define rte_bbdev_log_debug(...)
#endif

/* VRB1 PF and VF driver names */
#define VRB_PF_DRIVER_NAME           intel_vran_boost_pf
#define VRB_VF_DRIVER_NAME           intel_vran_boost_vf

/* VRB1 PCI vendor & device IDs */
#define RTE_VRB1_VENDOR_ID           (0x8086)
#define RTE_VRB1_PF_DEVICE_ID        (0x57C0)
#define RTE_VRB1_VF_DEVICE_ID        (0x57C1)
#define RTE_VRB2_VENDOR_ID           (0x8086)
#define RTE_VRB2_PF_DEVICE_ID        (0x57C2)
#define RTE_VRB2_VF_DEVICE_ID        (0x57C3)

#define VRB_NUM_ACCS                 6
#define VRB_MAX_QGRPS                32
#define VRB_MAX_AQS                  64

#define ACC_STATUS_WAIT      10
#define ACC_STATUS_TO        100

/* VRB1 specific flags */

#define VRB1_NUM_VFS                  16
#define VRB1_NUM_QGRPS                16
#define VRB1_NUM_AQS                  16
#define VRB1_WORDS_IN_ARAM_SIZE (256 * 1024 / 4)

/* VRB1 Mapping of signals for the available engines */
#define VRB1_SIG_UL_5G       0
#define VRB1_SIG_UL_5G_LAST  4
#define VRB1_SIG_DL_5G      10
#define VRB1_SIG_DL_5G_LAST 11
#define VRB1_SIG_UL_4G      12
#define VRB1_SIG_UL_4G_LAST 16
#define VRB1_SIG_DL_4G      21
#define VRB1_SIG_DL_4G_LAST 23
#define VRB1_SIG_FFT        24
#define VRB1_SIG_FFT_LAST   24
#define VRB1_NUM_ACCS       5

/* VRB1 Configuration */
#define VRB1_FABRIC_MODE      0x8000103
#define VRB1_CFG_DMA_ERROR    0x3DF
#define VRB1_CFG_AXI_CACHE    0x11
#define VRB1_CFG_QMGR_HI_P    0x0F0F
#define VRB1_RESET_HARD       0x1FF
#define VRB1_ENGINES_MAX      9
#define VRB1_GPEX_AXIMAP_NUM  17
#define VRB1_CLOCK_GATING_EN  0x30000
#define VRB1_FFT_CFG_0        0x2001
#define VRB1_FFT_RAM_EN       0x80008000
#define VRB1_FFT_RAM_DIS      0x0
#define VRB1_FFT_RAM_SIZE     512
#define VRB1_CLK_EN           0x00010A01
#define VRB1_CLK_DIS          0x01F10A01
#define VRB1_PG_MASK_0        0x1F
#define VRB1_PG_MASK_1        0xF
#define VRB1_PG_MASK_2        0x1
#define VRB1_PG_MASK_3        0x0
#define VRB1_PG_MASK_FFT      1
#define VRB1_PG_MASK_4GUL     4
#define VRB1_PG_MASK_5GUL     8
#define VRB1_REG_IRQ_EN_ALL          0x1FF83FF  /* Enable all interrupts */
#define VRB1_MAX_PF_MSIX            (256+32)
#define VRB1_MAX_VF_MSIX            (256+7)

/* VRB2 specific flags */

#define VRB2_NUM_VFS        64
#define VRB2_NUM_QGRPS      32
#define VRB2_NUM_AQS        64
#define VRB2_WORDS_IN_ARAM_SIZE (512 * 1024 / 4)
#define VRB2_NUM_ACCS        6
#define VRB2_AQ_REG_NUM      4

/* VRB2 Mapping of signals for the available engines */
#define VRB2_SIG_UL_5G       0
#define VRB2_SIG_UL_5G_LAST  5
#define VRB2_SIG_DL_5G       9
#define VRB2_SIG_DL_5G_LAST 11
#define VRB2_SIG_UL_4G      12
#define VRB2_SIG_UL_4G_LAST 16
#define VRB2_SIG_DL_4G      21
#define VRB2_SIG_DL_4G_LAST 23
#define VRB2_SIG_FFT        24
#define VRB2_SIG_FFT_LAST   26
#define VRB2_SIG_MLD        30
#define VRB2_SIG_MLD_LAST   31
#define VRB2_FFT_NUM        3

#define VRB2_FCW_MLDTS_BLEN 32
#define VRB2_MLD_MIN_LAYER   2
#define VRB2_MLD_MAX_LAYER   4
#define VRB2_MLD_MAX_RREP    5
#define VRB2_MLD_LAY_SIZE    3
#define VRB2_MLD_RREP_SIZE   6
#define VRB2_MLD_M2DLEN      3

#define VRB2_MAX_PF_MSIX      (256+32)
#define VRB2_MAX_VF_MSIX      (64+7)
#define VRB2_REG_IRQ_EN_ALL   0xFFFFFFFF  /* Enable all interrupts */
#define VRB2_FABRIC_MODE      0x8000103
#define VRB2_CFG_DMA_ERROR    0x7DF
#define VRB2_CFG_AXI_CACHE    0x11
#define VRB2_CFG_QMGR_HI_P    0x0F0F
#define VRB2_RESET_HARD       0x1FF
#define VRB2_ENGINES_MAX      9
#define VRB2_GPEX_AXIMAP_NUM  17
#define VRB2_CLOCK_GATING_EN  0x30000
#define VRB2_FFT_CFG_0        0x2001
#define VRB2_FFT_ECC          0x60
#define VRB2_FFT_RAM_EN       0x80008000
#define VRB2_FFT_RAM_DIS      0x0
#define VRB2_FFT_RAM_SIZE     512
#define VRB2_CLK_EN           0x00010A01
#define VRB2_CLK_DIS          0x01F10A01
#define VRB2_PG_MASK_0        0x1F
#define VRB2_PG_MASK_1        0xF
#define VRB2_PG_MASK_2        0x1
#define VRB2_PG_MASK_3        0x0
#define VRB2_PG_MASK_FFT      1
#define VRB2_PG_MASK_4GUL     4
#define VRB2_PG_MASK_5GUL     8
#define VRB2_PF_PM_REG_OFFSET 0x10000
#define VRB2_VF_PM_REG_OFFSET 0x40
#define VRB2_PM_START         0x2

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
	unsigned int dma_ring_mld_hi;
	unsigned int dma_ring_mld_lo;
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
	unsigned int tail_ptrs_mld_hi;
	unsigned int tail_ptrs_mld_lo;
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
static const struct acc_registry_addr vrb1_pf_reg_addr = {
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
	.dma_ring_mld_hi = 0,
	.dma_ring_mld_lo = 0,
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
	.tail_ptrs_mld_hi = 0,
	.tail_ptrs_mld_lo = 0,
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
static const struct acc_registry_addr vrb1_vf_reg_addr = {
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
	.dma_ring_mld_hi = 0,
	.dma_ring_mld_lo = 0,
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
	.tail_ptrs_mld_hi = 0,
	.tail_ptrs_mld_lo = 0,
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


/* Structure holding registry addresses for PF */
static const struct acc_registry_addr vrb2_pf_reg_addr = {
	.dma_ring_dl5g_hi =  VRB2_PfDmaFec5GdlDescBaseHiRegVf,
	.dma_ring_dl5g_lo =  VRB2_PfDmaFec5GdlDescBaseLoRegVf,
	.dma_ring_ul5g_hi =  VRB2_PfDmaFec5GulDescBaseHiRegVf,
	.dma_ring_ul5g_lo =  VRB2_PfDmaFec5GulDescBaseLoRegVf,
	.dma_ring_dl4g_hi =  VRB2_PfDmaFec4GdlDescBaseHiRegVf,
	.dma_ring_dl4g_lo =  VRB2_PfDmaFec4GdlDescBaseLoRegVf,
	.dma_ring_ul4g_hi =  VRB2_PfDmaFec4GulDescBaseHiRegVf,
	.dma_ring_ul4g_lo =  VRB2_PfDmaFec4GulDescBaseLoRegVf,
	.dma_ring_fft_hi =   VRB2_PfDmaFftDescBaseHiRegVf,
	.dma_ring_fft_lo =   VRB2_PfDmaFftDescBaseLoRegVf,
	.dma_ring_mld_hi =   VRB2_PfDmaMldDescBaseHiRegVf,
	.dma_ring_mld_lo =   VRB2_PfDmaMldDescBaseLoRegVf,
	.ring_size =         VRB2_PfQmgrRingSizeVf,
	.info_ring_hi =      VRB2_PfHiInfoRingBaseHiRegPf,
	.info_ring_lo =      VRB2_PfHiInfoRingBaseLoRegPf,
	.info_ring_en =      VRB2_PfHiInfoRingIntWrEnRegPf,
	.info_ring_ptr =     VRB2_PfHiInfoRingPointerRegPf,
	.tail_ptrs_dl5g_hi = VRB2_PfDmaFec5GdlRespPtrHiRegVf,
	.tail_ptrs_dl5g_lo = VRB2_PfDmaFec5GdlRespPtrLoRegVf,
	.tail_ptrs_ul5g_hi = VRB2_PfDmaFec5GulRespPtrHiRegVf,
	.tail_ptrs_ul5g_lo = VRB2_PfDmaFec5GulRespPtrLoRegVf,
	.tail_ptrs_dl4g_hi = VRB2_PfDmaFec4GdlRespPtrHiRegVf,
	.tail_ptrs_dl4g_lo = VRB2_PfDmaFec4GdlRespPtrLoRegVf,
	.tail_ptrs_ul4g_hi = VRB2_PfDmaFec4GulRespPtrHiRegVf,
	.tail_ptrs_ul4g_lo = VRB2_PfDmaFec4GulRespPtrLoRegVf,
	.tail_ptrs_fft_hi =  VRB2_PfDmaFftRespPtrHiRegVf,
	.tail_ptrs_fft_lo =  VRB2_PfDmaFftRespPtrLoRegVf,
	.tail_ptrs_mld_hi =  VRB2_PfDmaFftRespPtrHiRegVf,
	.tail_ptrs_mld_lo =  VRB2_PfDmaFftRespPtrLoRegVf,
	.depth_log0_offset = VRB2_PfQmgrGrpDepthLog20Vf,
	.depth_log1_offset = VRB2_PfQmgrGrpDepthLog21Vf,
	.qman_group_func =   VRB2_PfQmgrGrpFunction0,
	.hi_mode =           VRB2_PfHiMsixVectorMapperPf,
	.pf_mode =           VRB2_PfHiPfMode,
	.pmon_ctrl_a =       VRB2_PfPermonACntrlRegVf,
	.pmon_ctrl_b =       VRB2_PfPermonBCntrlRegVf,
	.pmon_ctrl_c =       VRB2_PfPermonCCntrlRegVf,
	.vf2pf_doorbell =    0,
	.pf2vf_doorbell =    0,
};

/* Structure holding registry addresses for VF */
static const struct acc_registry_addr vrb2_vf_reg_addr = {
	.dma_ring_dl5g_hi =  VRB2_VfDmaFec5GdlDescBaseHiRegVf,
	.dma_ring_dl5g_lo =  VRB2_VfDmaFec5GdlDescBaseLoRegVf,
	.dma_ring_ul5g_hi =  VRB2_VfDmaFec5GulDescBaseHiRegVf,
	.dma_ring_ul5g_lo =  VRB2_VfDmaFec5GulDescBaseLoRegVf,
	.dma_ring_dl4g_hi =  VRB2_VfDmaFec4GdlDescBaseHiRegVf,
	.dma_ring_dl4g_lo =  VRB2_VfDmaFec4GdlDescBaseLoRegVf,
	.dma_ring_ul4g_hi =  VRB2_VfDmaFec4GulDescBaseHiRegVf,
	.dma_ring_ul4g_lo =  VRB2_VfDmaFec4GulDescBaseLoRegVf,
	.dma_ring_fft_hi =   VRB2_VfDmaFftDescBaseHiRegVf,
	.dma_ring_fft_lo =   VRB2_VfDmaFftDescBaseLoRegVf,
	.dma_ring_mld_hi =   VRB2_VfDmaMldDescBaseHiRegVf,
	.dma_ring_mld_lo =   VRB2_VfDmaMldDescBaseLoRegVf,
	.ring_size =         VRB2_VfQmgrRingSizeVf,
	.info_ring_hi =      VRB2_VfHiInfoRingBaseHiVf,
	.info_ring_lo =      VRB2_VfHiInfoRingBaseLoVf,
	.info_ring_en =      VRB2_VfHiInfoRingIntWrEnVf,
	.info_ring_ptr =     VRB2_VfHiInfoRingPointerVf,
	.tail_ptrs_dl5g_hi = VRB2_VfDmaFec5GdlRespPtrHiRegVf,
	.tail_ptrs_dl5g_lo = VRB2_VfDmaFec5GdlRespPtrLoRegVf,
	.tail_ptrs_ul5g_hi = VRB2_VfDmaFec5GulRespPtrHiRegVf,
	.tail_ptrs_ul5g_lo = VRB2_VfDmaFec5GulRespPtrLoRegVf,
	.tail_ptrs_dl4g_hi = VRB2_VfDmaFec4GdlRespPtrHiRegVf,
	.tail_ptrs_dl4g_lo = VRB2_VfDmaFec4GdlRespPtrLoRegVf,
	.tail_ptrs_ul4g_hi = VRB2_VfDmaFec4GulRespPtrHiRegVf,
	.tail_ptrs_ul4g_lo = VRB2_VfDmaFec4GulRespPtrLoRegVf,
	.tail_ptrs_fft_hi =  VRB2_VfDmaFftRespPtrHiRegVf,
	.tail_ptrs_fft_lo =  VRB2_VfDmaFftRespPtrLoRegVf,
	.tail_ptrs_mld_hi =  VRB2_VfDmaMldRespPtrHiRegVf,
	.tail_ptrs_mld_lo =  VRB2_VfDmaMldRespPtrLoRegVf,
	.depth_log0_offset = VRB2_VfQmgrGrpDepthLog20Vf,
	.depth_log1_offset = VRB2_VfQmgrGrpDepthLog21Vf,
	.qman_group_func =   VRB2_VfQmgrGrpFunction0Vf,
	.hi_mode =           VRB2_VfHiMsixVectorMapperVf,
	.pf_mode =           0,
	.pmon_ctrl_a =       VRB2_VfPmACntrlRegVf,
	.pmon_ctrl_b =       VRB2_VfPmBCntrlRegVf,
	.pmon_ctrl_c =       VRB2_VfPmCCntrlRegVf,
	.vf2pf_doorbell =    VRB2_VfHiVfToPfDbellVf,
	.pf2vf_doorbell =    VRB2_VfHiPfToVfDbellVf,
};


#endif /* _VRB_PMD_H_ */
