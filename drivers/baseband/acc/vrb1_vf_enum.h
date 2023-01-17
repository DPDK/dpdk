/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef VRB1_VF_ENUM_H
#define VRB1_VF_ENUM_H

/*
 * VRB1 Register mapping on VF BAR0
 * This is automatically generated from RDL, format may change with new RDL
 */
enum {
	VRB1_VfQmgrIngressAq             =  0x00000000,
	VRB1_VfHiVfToPfDbellVf           =  0x00000800,
	VRB1_VfHiPfToVfDbellVf           =  0x00000808,
	VRB1_VfHiInfoRingBaseLoVf        =  0x00000810,
	VRB1_VfHiInfoRingBaseHiVf        =  0x00000814,
	VRB1_VfHiInfoRingPointerVf       =  0x00000818,
	VRB1_VfHiInfoRingIntWrEnVf       =  0x00000820,
	VRB1_VfHiInfoRingPf2VfWrEnVf     =  0x00000824,
	VRB1_VfHiMsixVectorMapperVf      =  0x00000860,
	VRB1_VfDmaFec5GulDescBaseLoRegVf =  0x00000920,
	VRB1_VfDmaFec5GulDescBaseHiRegVf =  0x00000924,
	VRB1_VfDmaFec5GulRespPtrLoRegVf  =  0x00000928,
	VRB1_VfDmaFec5GulRespPtrHiRegVf  =  0x0000092C,
	VRB1_VfDmaFec5GdlDescBaseLoRegVf =  0x00000940,
	VRB1_VfDmaFec5GdlDescBaseHiRegVf =  0x00000944,
	VRB1_VfDmaFec5GdlRespPtrLoRegVf  =  0x00000948,
	VRB1_VfDmaFec5GdlRespPtrHiRegVf  =  0x0000094C,
	VRB1_VfDmaFec4GulDescBaseLoRegVf =  0x00000960,
	VRB1_VfDmaFec4GulDescBaseHiRegVf =  0x00000964,
	VRB1_VfDmaFec4GulRespPtrLoRegVf  =  0x00000968,
	VRB1_VfDmaFec4GulRespPtrHiRegVf  =  0x0000096C,
	VRB1_VfDmaFec4GdlDescBaseLoRegVf =  0x00000980,
	VRB1_VfDmaFec4GdlDescBaseHiRegVf =  0x00000984,
	VRB1_VfDmaFec4GdlRespPtrLoRegVf  =  0x00000988,
	VRB1_VfDmaFec4GdlRespPtrHiRegVf  =  0x0000098C,
	VRB1_VfDmaFftDescBaseLoRegVf     =  0x000009A0,
	VRB1_VfDmaFftDescBaseHiRegVf     =  0x000009A4,
	VRB1_VfDmaFftRespPtrLoRegVf      =  0x000009A8,
	VRB1_VfDmaFftRespPtrHiRegVf      =  0x000009AC,
	VRB1_VfQmgrAqResetVf             =  0x00000E00,
	VRB1_VfQmgrRingSizeVf            =  0x00000E04,
	VRB1_VfQmgrGrpDepthLog20Vf       =  0x00000E08,
	VRB1_VfQmgrGrpDepthLog21Vf       =  0x00000E0C,
	VRB1_VfQmgrGrpFunction0Vf        =  0x00000E10,
	VRB1_VfQmgrGrpFunction1Vf        =  0x00000E14,
	VRB1_VfPmACntrlRegVf             =  0x00000F40,
	VRB1_VfPmACountVf                =  0x00000F48,
	VRB1_VfPmAKCntLoVf               =  0x00000F50,
	VRB1_VfPmAKCntHiVf               =  0x00000F54,
	VRB1_VfPmADeltaCntLoVf           =  0x00000F60,
	VRB1_VfPmADeltaCntHiVf           =  0x00000F64,
	VRB1_VfPmBCntrlRegVf             =  0x00000F80,
	VRB1_VfPmBCountVf                =  0x00000F88,
	VRB1_VfPmBKCntLoVf               =  0x00000F90,
	VRB1_VfPmBKCntHiVf               =  0x00000F94,
	VRB1_VfPmBDeltaCntLoVf           =  0x00000FA0,
	VRB1_VfPmBDeltaCntHiVf           =  0x00000FA4,
	VRB1_VfPmCCntrlRegVf             =  0x00000FC0,
	VRB1_VfPmCCountVf                =  0x00000FC8,
	VRB1_VfPmCKCntLoVf               =  0x00000FD0,
	VRB1_VfPmCKCntHiVf               =  0x00000FD4,
	VRB1_VfPmCDeltaCntLoVf           =  0x00000FE0,
	VRB1_VfPmCDeltaCntHiVf           =  0x00000FE4
};

/* TIP VF Interrupt numbers */
enum {
	ACC_VF_INT_QMGR_AQ_OVERFLOW = 0,
	ACC_VF_INT_DOORBELL_PF_2_VF = 1,
	ACC_VF_INT_ILLEGAL_FORMAT = 2,
	ACC_VF_INT_QMGR_DISABLED_ACCESS = 3,
	ACC_VF_INT_QMGR_AQ_OVERTHRESHOLD = 4,
	ACC_VF_INT_DMA_DL_DESC_IRQ = 5,
	ACC_VF_INT_DMA_UL_DESC_IRQ = 6,
	ACC_VF_INT_DMA_FFT_DESC_IRQ = 7,
	ACC_VF_INT_DMA_UL5G_DESC_IRQ = 8,
	ACC_VF_INT_DMA_DL5G_DESC_IRQ = 9,
	ACC_VF_INT_DMA_MLD_DESC_IRQ = 10,
};

#endif /* VRB1_VF_ENUM_H */
