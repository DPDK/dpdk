/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef VRB1_PF_ENUM_H
#define VRB1_PF_ENUM_H

/*
 * VRB1 Register mapping on PF BAR0
 * This is automatically generated from RDL, format may change with new RDL
 * Release.
 * Variable names are as is
 */
enum {
	VRB1_PfQmgrEgressQueuesTemplate          =  0x0007FC00,
	VRB1_PfQmgrIngressAq                     =  0x00080000,
	VRB1_PfQmgrDepthLog2Grp                  =  0x00A00200,
	VRB1_PfQmgrTholdGrp                      =  0x00A00300,
	VRB1_PfQmgrGrpTmplateReg0Indx            =  0x00A00600,
	VRB1_PfQmgrGrpTmplateReg1Indx            =  0x00A00700,
	VRB1_PfQmgrGrpTmplateReg2indx            =  0x00A00800,
	VRB1_PfQmgrGrpTmplateReg3Indx            =  0x00A00900,
	VRB1_PfQmgrGrpTmplateReg4Indx            =  0x00A00A00,
	VRB1_PfQmgrVfBaseAddr                    =  0x00A01000,
	VRB1_PfQmgrArbQDepthGrp                  =  0x00A02F00,
	VRB1_PfQmgrGrpFunction0                  =  0x00A02F40,
	VRB1_PfQmgrGrpFunction1                  =  0x00A02F44,
	VRB1_PfQmgrGrpPriority                   =  0x00A02F48,
	VRB1_PfQmgrAqEnableVf                    =  0x00A10000,
	VRB1_PfQmgrRingSizeVf                    =  0x00A20004,
	VRB1_PfQmgrGrpDepthLog20Vf               =  0x00A20008,
	VRB1_PfQmgrGrpDepthLog21Vf               =  0x00A2000C,
	VRB1_PfFabricM2iBufferReg                =  0x00B30000,
	VRB1_PfFabricI2Mdma_weight               =  0x00B31044,
	VRB1_PfFecUl5gIbDebugReg                 =  0x00B40200,
	VRB1_PfFftConfig0                        =  0x00B58004,
	VRB1_PfFftRamPageAccess                  =  0x00B5800C,
	VRB1_PfFftRamOff                         =  0x00B58800,
	VRB1_PfDmaConfig0Reg                     =  0x00B80000,
	VRB1_PfDmaConfig1Reg                     =  0x00B80004,
	VRB1_PfDmaQmgrAddrReg                    =  0x00B80008,
	VRB1_PfDmaAxcacheReg                     =  0x00B80010,
	VRB1_PfDmaAxiControl                     =  0x00B8002C,
	VRB1_PfDmaQmanen                         =  0x00B80040,
	VRB1_PfDma4gdlIbThld                     =  0x00B800CC,
	VRB1_PfDmaCfgRrespBresp                  =  0x00B80814,
	VRB1_PfDmaDescriptorSignatuture          =  0x00B80868,
	VRB1_PfDmaErrorDetectionEn               =  0x00B80870,
	VRB1_PfDmaFec5GulDescBaseLoRegVf         =  0x00B88020,
	VRB1_PfDmaFec5GulDescBaseHiRegVf         =  0x00B88024,
	VRB1_PfDmaFec5GulRespPtrLoRegVf          =  0x00B88028,
	VRB1_PfDmaFec5GulRespPtrHiRegVf          =  0x00B8802C,
	VRB1_PfDmaFec5GdlDescBaseLoRegVf         =  0x00B88040,
	VRB1_PfDmaFec5GdlDescBaseHiRegVf         =  0x00B88044,
	VRB1_PfDmaFec5GdlRespPtrLoRegVf          =  0x00B88048,
	VRB1_PfDmaFec5GdlRespPtrHiRegVf          =  0x00B8804C,
	VRB1_PfDmaFec4GulDescBaseLoRegVf         =  0x00B88060,
	VRB1_PfDmaFec4GulDescBaseHiRegVf         =  0x00B88064,
	VRB1_PfDmaFec4GulRespPtrLoRegVf          =  0x00B88068,
	VRB1_PfDmaFec4GulRespPtrHiRegVf          =  0x00B8806C,
	VRB1_PfDmaFec4GdlDescBaseLoRegVf         =  0x00B88080,
	VRB1_PfDmaFec4GdlDescBaseHiRegVf         =  0x00B88084,
	VRB1_PfDmaFec4GdlRespPtrLoRegVf          =  0x00B88088,
	VRB1_PfDmaFec4GdlRespPtrHiRegVf          =  0x00B8808C,
	VRB1_PfDmaFftDescBaseLoRegVf             =  0x00B880A0,
	VRB1_PfDmaFftDescBaseHiRegVf             =  0x00B880A4,
	VRB1_PfDmaFftRespPtrLoRegVf              =  0x00B880A8,
	VRB1_PfDmaFftRespPtrHiRegVf              =  0x00B880AC,
	VRB1_PfQosmonAEvalOverflow0              =  0x00B90008,
	VRB1_PfPermonACntrlRegVf                 =  0x00B98000,
	VRB1_PfQosmonBEvalOverflow0              =  0x00BA0008,
	VRB1_PfPermonBCntrlRegVf                 =  0x00BA8000,
	VRB1_PfPermonCCntrlRegVf                 =  0x00BB8000,
	VRB1_PfHiInfoRingBaseLoRegPf             =  0x00C84014,
	VRB1_PfHiInfoRingBaseHiRegPf             =  0x00C84018,
	VRB1_PfHiInfoRingPointerRegPf            =  0x00C8401C,
	VRB1_PfHiInfoRingIntWrEnRegPf            =  0x00C84020,
	VRB1_PfHiBlockTransmitOnErrorEn          =  0x00C84038,
	VRB1_PfHiCfgMsiIntWrEnRegPf              =  0x00C84040,
	VRB1_PfHiMsixVectorMapperPf              =  0x00C84060,
	VRB1_PfHiPfMode                          =  0x00C84108,
	VRB1_PfHiClkGateHystReg                  =  0x00C8410C,
	VRB1_PfHiMsiDropEnableReg                =  0x00C84114,
	VRB1_PfHiSectionPowerGatingReq           =  0x00C84128,
	VRB1_PfHiSectionPowerGatingAck           =  0x00C8412C,
};

/* TIP PF Interrupt numbers */
enum {
	ACC_PF_INT_QMGR_AQ_OVERFLOW = 0,
	ACC_PF_INT_DOORBELL_VF_2_PF = 1,
	ACC_PF_INT_ILLEGAL_FORMAT = 2,
	ACC_PF_INT_QMGR_DISABLED_ACCESS = 3,
	ACC_PF_INT_QMGR_AQ_OVERTHRESHOLD = 4,
	ACC_PF_INT_DMA_DL_DESC_IRQ = 5,
	ACC_PF_INT_DMA_UL_DESC_IRQ = 6,
	ACC_PF_INT_DMA_FFT_DESC_IRQ = 7,
	ACC_PF_INT_DMA_UL5G_DESC_IRQ = 8,
	ACC_PF_INT_DMA_DL5G_DESC_IRQ = 9,
	ACC_PF_INT_DMA_MLD_DESC_IRQ = 10,
	ACC_PF_INT_ARAM_ECC_1BIT_ERR = 11,
	ACC_PF_INT_PARITY_ERR = 12,
	ACC_PF_INT_QMGR_ERR = 13,
	ACC_PF_INT_INT_REQ_OVERFLOW = 14,
	ACC_PF_INT_APB_TIMEOUT = 15,
};

#endif /* VRB1_PF_ENUM_H */
