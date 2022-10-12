/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef ACC200_PF_ENUM_H
#define ACC200_PF_ENUM_H

/*
 * ACC200 Register mapping on PF BAR0
 * This is automatically generated from RDL, format may change with new RDL
 * Release.
 * Variable names are as is
 */
enum {
	HWPfQmgrEgressQueuesTemplate          =  0x0007FC00,
	HWPfQmgrIngressAq                     =  0x00080000,
	HWPfQmgrDepthLog2Grp                  =  0x00A00200,
	HWPfQmgrTholdGrp                      =  0x00A00300,
	HWPfQmgrGrpTmplateReg0Indx            =  0x00A00600,
	HWPfQmgrGrpTmplateReg1Indx            =  0x00A00700,
	HWPfQmgrGrpTmplateReg2indx            =  0x00A00800,
	HWPfQmgrGrpTmplateReg3Indx            =  0x00A00900,
	HWPfQmgrGrpTmplateReg4Indx            =  0x00A00A00,
	HWPfQmgrVfBaseAddr                    =  0x00A01000,
	HWPfQmgrArbQDepthGrp                  =  0x00A02F00,
	HWPfQmgrGrpFunction0                  =  0x00A02F40,
	HWPfQmgrGrpFunction1                  =  0x00A02F44,
	HWPfQmgrGrpPriority                   =  0x00A02F48,
	HWPfQmgrAqEnableVf                    =  0x00A10000,
	HWPfQmgrRingSizeVf                    =  0x00A20004,
	HWPfQmgrGrpDepthLog20Vf               =  0x00A20008,
	HWPfQmgrGrpDepthLog21Vf               =  0x00A2000C,
	HWPfFabricM2iBufferReg                =  0x00B30000,
	HWPfFabricI2Mdma_weight               =  0x00B31044,
	HwPfFecUl5gIbDebugReg                 =  0x00B40200,
	HWPfFftConfig0                        =  0x00B58004,
	HWPfFftRamPageAccess                  =  0x00B5800C,
	HWPfFftRamOff                         =  0x00B58800,
	HWPfDmaConfig0Reg                     =  0x00B80000,
	HWPfDmaConfig1Reg                     =  0x00B80004,
	HWPfDmaQmgrAddrReg                    =  0x00B80008,
	HWPfDmaAxcacheReg                     =  0x00B80010,
	HWPfDmaAxiControl                     =  0x00B8002C,
	HWPfDmaQmanen                         =  0x00B80040,
	HWPfDma4gdlIbThld                     =  0x00B800CC,
	HWPfDmaCfgRrespBresp                  =  0x00B80814,
	HWPfDmaDescriptorSignatuture          =  0x00B80868,
	HWPfDmaErrorDetectionEn               =  0x00B80870,
	HWPfDmaFec5GulDescBaseLoRegVf         =  0x00B88020,
	HWPfDmaFec5GulDescBaseHiRegVf         =  0x00B88024,
	HWPfDmaFec5GulRespPtrLoRegVf          =  0x00B88028,
	HWPfDmaFec5GulRespPtrHiRegVf          =  0x00B8802C,
	HWPfDmaFec5GdlDescBaseLoRegVf         =  0x00B88040,
	HWPfDmaFec5GdlDescBaseHiRegVf         =  0x00B88044,
	HWPfDmaFec5GdlRespPtrLoRegVf          =  0x00B88048,
	HWPfDmaFec5GdlRespPtrHiRegVf          =  0x00B8804C,
	HWPfDmaFec4GulDescBaseLoRegVf         =  0x00B88060,
	HWPfDmaFec4GulDescBaseHiRegVf         =  0x00B88064,
	HWPfDmaFec4GulRespPtrLoRegVf          =  0x00B88068,
	HWPfDmaFec4GulRespPtrHiRegVf          =  0x00B8806C,
	HWPfDmaFec4GdlDescBaseLoRegVf         =  0x00B88080,
	HWPfDmaFec4GdlDescBaseHiRegVf         =  0x00B88084,
	HWPfDmaFec4GdlRespPtrLoRegVf          =  0x00B88088,
	HWPfDmaFec4GdlRespPtrHiRegVf          =  0x00B8808C,
	HWPDmaFftDescBaseLoRegVf              =  0x00B880A0,
	HWPDmaFftDescBaseHiRegVf              =  0x00B880A4,
	HWPDmaFftRespPtrLoRegVf               =  0x00B880A8,
	HWPDmaFftRespPtrHiRegVf               =  0x00B880AC,
	HWPfQosmonAEvalOverflow0              =  0x00B90008,
	HWPfPermonACntrlRegVf                 =  0x00B98000,
	HWPfQosmonBEvalOverflow0              =  0x00BA0008,
	HWPfPermonBCntrlRegVf                 =  0x00BA8000,
	HWPfPermonCCntrlRegVf                 =  0x00BB8000,
	HWPfHiInfoRingBaseLoRegPf             =  0x00C84014,
	HWPfHiInfoRingBaseHiRegPf             =  0x00C84018,
	HWPfHiInfoRingPointerRegPf            =  0x00C8401C,
	HWPfHiInfoRingIntWrEnRegPf            =  0x00C84020,
	HWPfHiBlockTransmitOnErrorEn          =  0x00C84038,
	HWPfHiCfgMsiIntWrEnRegPf              =  0x00C84040,
	HWPfHiMsixVectorMapperPf              =  0x00C84060,
	HWPfHiPfMode                          =  0x00C84108,
	HWPfHiClkGateHystReg                  =  0x00C8410C,
	HWPfHiMsiDropEnableReg                =  0x00C84114,
	HWPfHiSectionPowerGatingReq           =  0x00C84128,
	HWPfHiSectionPowerGatingAck           =  0x00C8412C,
};

/* TIP PF Interrupt numbers */
enum {
	ACC200_PF_INT_QMGR_AQ_OVERFLOW = 0,
	ACC200_PF_INT_DOORBELL_VF_2_PF = 1,
	ACC200_PF_INT_ILLEGAL_FORMAT = 2,
	ACC200_PF_INT_QMGR_DISABLED_ACCESS = 3,
	ACC200_PF_INT_QMGR_AQ_OVERTHRESHOLD = 4,
	ACC200_PF_INT_DMA_DL_DESC_IRQ = 5,
	ACC200_PF_INT_DMA_UL_DESC_IRQ = 6,
	ACC200_PF_INT_DMA_FFT_DESC_IRQ = 7,
	ACC200_PF_INT_DMA_UL5G_DESC_IRQ = 8,
	ACC200_PF_INT_DMA_DL5G_DESC_IRQ = 9,
	ACC200_PF_INT_DMA_MLD_DESC_IRQ = 10,
	ACC200_PF_INT_ARAM_ECC_1BIT_ERR = 11,
	ACC200_PF_INT_PARITY_ERR = 12,
	ACC200_PF_INT_QMGR_ERR = 13,
	ACC200_PF_INT_INT_REQ_OVERFLOW = 14,
	ACC200_PF_INT_APB_TIMEOUT = 15,
};

#endif /* ACC200_PF_ENUM_H */
