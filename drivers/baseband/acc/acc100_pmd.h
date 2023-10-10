/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_ACC100_PMD_H_
#define _RTE_ACC100_PMD_H_

#include "acc100_pf_enum.h"
#include "acc100_vf_enum.h"
#include "rte_acc_cfg.h"
#include "acc_common.h"

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, acc100_logtype, fmt "\n", \
		##__VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(fmt, ...) \
		rte_bbdev_log(DEBUG, "acc100_pmd: " fmt, \
		##__VA_ARGS__)
#else
#define rte_bbdev_log_debug(fmt, ...)
#endif

#define ACC100_VARIANT 0
#define ACC101_VARIANT 1

/* ACC100 PF and VF driver names */
#define ACC100PF_DRIVER_NAME           intel_acc100_pf
#define ACC100VF_DRIVER_NAME           intel_acc100_vf

/* ACC100 PCI vendor & device IDs */
#define ACC100_VENDOR_ID           (0x8086)
#define ACC100_PF_DEVICE_ID        (0x0d5c)
#define ACC100_VF_DEVICE_ID        (0x0d5d)
#define VRB1_PF_DEVICE_ID          (0x57C0)
#define VRB2_PF_DEVICE_ID          (0x57C2)

/* Values used in writing to the registers */
#define ACC100_REG_IRQ_EN_ALL          0x1FF83FF  /* Enable all interrupts */

/* Number of Virtual Functions ACC100 supports */
#define ACC100_NUM_VFS                  16
#define ACC100_NUM_QGRPS                8
#define ACC100_NUM_AQS                  16

#define ACC100_GRP_ID_SHIFT    10 /* Queue Index Hierarchy */
#define ACC100_VF_ID_SHIFT     4  /* Queue Index Hierarchy */
#define ACC100_WORDS_IN_ARAM_SIZE (128 * 1024 / 4)

/* Mapping of signals for the available engines */
#define ACC100_SIG_UL_5G      0
#define ACC100_SIG_UL_5G_LAST 7
#define ACC100_SIG_DL_5G      13
#define ACC100_SIG_DL_5G_LAST 15
#define ACC100_SIG_UL_4G      16
#define ACC100_SIG_UL_4G_LAST 21
#define ACC100_SIG_DL_4G      27
#define ACC100_SIG_DL_4G_LAST 31
#define ACC100_NUM_ACCS       5

#define ACC100_EXT_MEM /* Default option with memory external to CPU */
#define ACC100_HARQ_OFFSET_THRESHOLD 1024

/* ACC100 Configuration */
#define ACC100_DDR_ECC_ENABLE
#define ACC100_CFG_DMA_ERROR    0x3D7
#define ACC100_CFG_AXI_CACHE    0x11
#define ACC100_CFG_QMGR_HI_P    0x0F0F
#define ACC100_CFG_PCI_AXI      0xC003
#define ACC100_CFG_PCI_BRIDGE   0x40006033
#define ACC100_QUAD_NUMS        4
#define ACC100_LANES_PER_QUAD   4
#define ACC100_PCIE_LANE_OFFSET 0x200
#define ACC100_PCIE_QUAD_OFFSET 0x2000
#define ACC100_PCS_EQ           0x6007
#define ACC100_ADAPT            0x8400
#define ACC100_RESET_HI         0x20100
#define ACC100_RESET_LO         0x20000
#define ACC100_RESET_HARD       0x1FF
#define ACC100_ENGINES_MAX      9
#define ACC100_GPEX_AXIMAP_NUM  17
#define ACC100_CLOCK_GATING_EN  0x30000
#define ACC100_FABRIC_MODE      0xB
/* DDR Size per VF - 512MB by default
 * Can be increased up to 4 GB with single PF/VF
 */
#define ACC100_HARQ_DDR         (512 * 1)
#define ACC100_PRQ_DDR_VER       0x10092020
#define ACC100_DDR_TRAINING_MAX (5000)
#define ACC100_HARQ_ALIGN_COMP   256

struct acc100_registry_addr {
	unsigned int dma_ring_dl5g_hi;
	unsigned int dma_ring_dl5g_lo;
	unsigned int dma_ring_ul5g_hi;
	unsigned int dma_ring_ul5g_lo;
	unsigned int dma_ring_dl4g_hi;
	unsigned int dma_ring_dl4g_lo;
	unsigned int dma_ring_ul4g_hi;
	unsigned int dma_ring_ul4g_lo;
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
	unsigned int depth_log0_offset;
	unsigned int depth_log1_offset;
	unsigned int qman_group_func;
	unsigned int ddr_range;
	unsigned int pmon_ctrl_a;
	unsigned int pmon_ctrl_b;
};

/* Structure holding registry addresses for PF */
static const struct acc100_registry_addr pf_reg_addr = {
	.dma_ring_dl5g_hi = HWPfDmaFec5GdlDescBaseHiRegVf,
	.dma_ring_dl5g_lo = HWPfDmaFec5GdlDescBaseLoRegVf,
	.dma_ring_ul5g_hi = HWPfDmaFec5GulDescBaseHiRegVf,
	.dma_ring_ul5g_lo = HWPfDmaFec5GulDescBaseLoRegVf,
	.dma_ring_dl4g_hi = HWPfDmaFec4GdlDescBaseHiRegVf,
	.dma_ring_dl4g_lo = HWPfDmaFec4GdlDescBaseLoRegVf,
	.dma_ring_ul4g_hi = HWPfDmaFec4GulDescBaseHiRegVf,
	.dma_ring_ul4g_lo = HWPfDmaFec4GulDescBaseLoRegVf,
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
	.depth_log0_offset = HWPfQmgrGrpDepthLog20Vf,
	.depth_log1_offset = HWPfQmgrGrpDepthLog21Vf,
	.qman_group_func = HWPfQmgrGrpFunction0,
	.ddr_range = HWPfDmaVfDdrBaseRw,
	.pmon_ctrl_a = HWPfPermonACntrlRegVf,
	.pmon_ctrl_b = HWPfPermonBCntrlRegVf,
};

/* Structure holding registry addresses for VF */
static const struct acc100_registry_addr vf_reg_addr = {
	.dma_ring_dl5g_hi = HWVfDmaFec5GdlDescBaseHiRegVf,
	.dma_ring_dl5g_lo = HWVfDmaFec5GdlDescBaseLoRegVf,
	.dma_ring_ul5g_hi = HWVfDmaFec5GulDescBaseHiRegVf,
	.dma_ring_ul5g_lo = HWVfDmaFec5GulDescBaseLoRegVf,
	.dma_ring_dl4g_hi = HWVfDmaFec4GdlDescBaseHiRegVf,
	.dma_ring_dl4g_lo = HWVfDmaFec4GdlDescBaseLoRegVf,
	.dma_ring_ul4g_hi = HWVfDmaFec4GulDescBaseHiRegVf,
	.dma_ring_ul4g_lo = HWVfDmaFec4GulDescBaseLoRegVf,
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
	.depth_log0_offset = HWVfQmgrGrpDepthLog20Vf,
	.depth_log1_offset = HWVfQmgrGrpDepthLog21Vf,
	.qman_group_func = HWVfQmgrGrpFunction0Vf,
	.ddr_range = HWVfDmaDdrBaseRangeRoVf,
	.pmon_ctrl_a = HWVfPmACntrlRegVf,
	.pmon_ctrl_b = HWVfPmBCntrlRegVf,
};

#endif /* _RTE_ACC100_PMD_H_ */
