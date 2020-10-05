/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef ACC100_VF_ENUM_H
#define ACC100_VF_ENUM_H

/*
 * ACC100 Register mapping on VF BAR0
 * This is automatically generated from RDL, format may change with new RDL
 */
enum {
	HWVfQmgrIngressAq             =  0x00000000,
	HWVfHiVfToPfDbellVf           =  0x00000800,
	HWVfHiPfToVfDbellVf           =  0x00000808,
	HWVfHiInfoRingBaseLoVf        =  0x00000810,
	HWVfHiInfoRingBaseHiVf        =  0x00000814,
	HWVfHiInfoRingPointerVf       =  0x00000818,
	HWVfHiInfoRingIntWrEnVf       =  0x00000820,
	HWVfHiInfoRingPf2VfWrEnVf     =  0x00000824,
	HWVfHiMsixVectorMapperVf      =  0x00000860,
	HWVfDmaFec5GulDescBaseLoRegVf =  0x00000920,
	HWVfDmaFec5GulDescBaseHiRegVf =  0x00000924,
	HWVfDmaFec5GulRespPtrLoRegVf  =  0x00000928,
	HWVfDmaFec5GulRespPtrHiRegVf  =  0x0000092C,
	HWVfDmaFec5GdlDescBaseLoRegVf =  0x00000940,
	HWVfDmaFec5GdlDescBaseHiRegVf =  0x00000944,
	HWVfDmaFec5GdlRespPtrLoRegVf  =  0x00000948,
	HWVfDmaFec5GdlRespPtrHiRegVf  =  0x0000094C,
	HWVfDmaFec4GulDescBaseLoRegVf =  0x00000960,
	HWVfDmaFec4GulDescBaseHiRegVf =  0x00000964,
	HWVfDmaFec4GulRespPtrLoRegVf  =  0x00000968,
	HWVfDmaFec4GulRespPtrHiRegVf  =  0x0000096C,
	HWVfDmaFec4GdlDescBaseLoRegVf =  0x00000980,
	HWVfDmaFec4GdlDescBaseHiRegVf =  0x00000984,
	HWVfDmaFec4GdlRespPtrLoRegVf  =  0x00000988,
	HWVfDmaFec4GdlRespPtrHiRegVf  =  0x0000098C,
	HWVfDmaDdrBaseRangeRoVf       =  0x000009A0,
	HWVfQmgrAqResetVf             =  0x00000E00,
	HWVfQmgrRingSizeVf            =  0x00000E04,
	HWVfQmgrGrpDepthLog20Vf       =  0x00000E08,
	HWVfQmgrGrpDepthLog21Vf       =  0x00000E0C,
	HWVfQmgrGrpFunction0Vf        =  0x00000E10,
	HWVfQmgrGrpFunction1Vf        =  0x00000E14,
	HWVfPmACntrlRegVf             =  0x00000F40,
	HWVfPmACountVf                =  0x00000F48,
	HWVfPmAKCntLoVf               =  0x00000F50,
	HWVfPmAKCntHiVf               =  0x00000F54,
	HWVfPmADeltaCntLoVf           =  0x00000F60,
	HWVfPmADeltaCntHiVf           =  0x00000F64,
	HWVfPmBCntrlRegVf             =  0x00000F80,
	HWVfPmBCountVf                =  0x00000F88,
	HWVfPmBKCntLoVf               =  0x00000F90,
	HWVfPmBKCntHiVf               =  0x00000F94,
	HWVfPmBDeltaCntLoVf           =  0x00000FA0,
	HWVfPmBDeltaCntHiVf           =  0x00000FA4
};

/* TIP VF Interrupt numbers */
enum {
	ACC100_VF_INT_QMGR_AQ_OVERFLOW = 0,
	ACC100_VF_INT_DOORBELL_VF_2_PF = 1,
	ACC100_VF_INT_DMA_DL_DESC_IRQ = 2,
	ACC100_VF_INT_DMA_UL_DESC_IRQ = 3,
	ACC100_VF_INT_DMA_MLD_DESC_IRQ = 4,
	ACC100_VF_INT_DMA_UL5G_DESC_IRQ = 5,
	ACC100_VF_INT_DMA_DL5G_DESC_IRQ = 6,
	ACC100_VF_INT_ILLEGAL_FORMAT = 7,
	ACC100_VF_INT_QMGR_DISABLED_ACCESS = 8,
	ACC100_VF_INT_QMGR_AQ_OVERTHRESHOLD = 9,
};

#endif /* ACC100_VF_ENUM_H */
