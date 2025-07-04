/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_P70_H_
#define _CFA_P70_H_

#include "sys_util.h"
#include "cfa_p70_hw.h"

#define BITS_TO_BYTES(n) (((n) + 7) / 8)
#define BYTES_TO_WORDS(n) (((n) + 3) / 4)

/* EM Lrec size */
#define CFA_P70_EM_LREC_SZ CFA_P70_EM_LREC_TOTAL_NUM_BITS
/* Encap header length */
#define CFA_P70_ACT_ENCAP_MIN_HDR_LEN 64
/* Max AR pointers per MCG record */
#define CFA_P70_ACT_MCG_MAX_AR_PTR 8
/* Max Key fields */
#define CFA_P70_KEY_FLD_ID_MAX CFA_P70_EM_KEY_LAYOUT_MAX_FLD

/* profiler ILT, l2ctxt remap, and profile remap are 32-bit accessed */
#define CFA_PROF_P7P0_ILT_NUM_WORDS                                            \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_PROF_ILT_DR_TOTAL_NUM_BITS)
#define CFA_PROF_P7P0_L2_CTXT_RMP_NUM_WORDS                                    \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_PROF_L2_CTXT_RMP_DR_TOTAL_NUM_BITS)
#define CFA_PROF_P7P0_PROFILE_RMP_NUM_WORDS                                    \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_PROF_PROFILE_RMP_DR_TOTAL_NUM_BITS)
/* profiler TCAM and L2 ctxt TCAM  are accessed via Wide-bus */
#define CFA_PROF_P7P0_PROFILE_TCAM_NUM_WORDS                                   \
	NUM_WORDS_ALIGN_128BIT(CFA_P70_PROF_PROFILE_TCAM_TOTAL_NUM_BITS)
#define CFA_PROF_P7P0_L2_CTXT_TCAM_NUM_WORDS                                   \
	NUM_WORDS_ALIGN_128BIT(CFA_P70_PROF_L2_CTXT_TCAM_TOTAL_NUM_BITS)
/* FKB are accessed via Wide-bus */
#define CFA_P70_EM_FKB_NUM_WORDS NUM_WORDS_ALIGN_128BIT(CFA_P70_EM_FKB_MAX_FLD)
#define CFA_P70_EM_FKB_NUM_ENTRIES 128

/* EM FKB Mask */
/* EM_FKB_MASK total num bits defined in CFA EAS section 3.3.9.2.2 EM Key */
#define CFA_P70_EM_FKB_MASK_TOTAL_NUM_BITS 896
#define CFA_P70_EM_FKB_MASK_NUM_WORDS                                          \
	NUM_WORDS_ALIGN_128BIT(CFA_P70_EM_FKB_MASK_TOTAL_NUM_BITS)
#define CFA_P70_EM_FKB_MASK_NUM_ENTRIES 128

#define CFA_P70_WC_TCAM_FKB_NUM_WORDS                                          \
	NUM_WORDS_ALIGN_128BIT(CFA_P70_WC_TCAM_FKB_MAX_FLD)
#define CFA_P70_WC_TCAM_FKB_NUM_ENTRIES 128
/* VNIC-SVIF Properties Table are accessed via Wide-bus */
#define CFA_ACT_P7P0_VSPT_NUM_WORDS                                            \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_ACT_VSPT_DR_TX_TOTAL_NUM_BITS)
#define CFA_P70_ACT_VEB_TCAM_NUM_WORDS                                         \
	NUM_WORDS_ALIGN_128BIT(CFA_P70_ACT_VEB_TCAM_RX_TOTAL_NUM_BITS)
#define CFA_P70_ACT_MIRROR_NUM_WORDS                                           \
	NUM_WORDS_ALIGN_128BIT(CFA_P70_ACT_MIRROR_TOTAL_NUM_BITS)
#define CFA_P7P0_ACT_VEB_RMP_NUM_WORDS                                         \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_ACT_VEB_RMP_TOTAL_NUM_BITS)
#define CFA_P7P0_ACT_LBT_NUM_WORDS                                             \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_ACT_LBT_DR_TOTAL_NUM_BITS)
#define CFA_P70_LKUP_EM_ENTRY_SIZE_IN_BITS 256
#define CFA_P70_LKUP_EM_MAX_ENTRIES 4
#define CFA_P70_LKUP_EM_MAX_ENTRY_SIZE_IN_BITS                                 \
	(CFA_P70_LKUP_EM_ENTRY_SIZE_IN_BITS * CFA_P70_LKUP_EM_MAX_ENTRIES)
/* Maximum EM key size in bits */
#define CFA_P70_LKUP_EM_DATA_SIZE_IN_BITS                                      \
	(CFA_P70_LKUP_EM_MAX_ENTRY_SIZE_IN_BITS - CFA_P70_EM_LREC_SZ)
#define CFA_P70_LKUP_WC_DATA_SIZE_IN_BITS 688
#define CFA_P70_LKUP_WC_DATA_SIZE_WITH_CTRL_INFO_IN_BITS 700
#define CFA_P70_LKUP_WC_DATA_SIZE                                              \
	(BITS_TO_BYTES(CFA_P70_LKUP_WC_DATA_SIZE_IN_BITS))
#define CFA_P70_LKUP_WC_MAX_DATA_SIZE                                          \
	(BITS_TO_BYTES(CFA_P70_LKUP_WC_DATA_SIZE_WITH_CTRL_INFO_IN_BITS))
#define CFA_P70_LKUP_WC_NUM_WORDS (BYTES_TO_WORDS(CFA_P70_LKUP_WC_DATA_SIZE))
#define CFA_P70_LKUP_WC_NUM_WORDS_PER_BANK (CFA_P70_LKUP_WC_NUM_WORDS / 2)
#define CFA_P70_LKUP_WC_LREC_DATA_SIZE                                         \
	(BITS_TO_BYTES(CFA_P70_WC_LREC_TOTAL_NUM_BITS))
#define CFA_P70_LKUP_WC_LREC_NUM_WORDS                                         \
	(BYTES_TO_WORDS(CFA_P70_LKUP_WC_LREC_DATA_SIZE))
#define CFA_P70_LKUP_WC_SLICE_LEN_WITH_CTRL_INFO 175
#define CFA_P70_LKUP_WC_SLICE_LEN 172
#define CFA_P70_LKUP_WC_TCAM_IDX_MASK 0x1fff
#define CFA_P70_LKUP_WC_ROW_IDX_SFT 2
#define CFA_P70_LKUP_WC_SLICE_IDX_MASK 0x3
#define CFA_P70_LKUP_WC_NUM_SLICES 4
#define CFA_P70_LKUP_WC_NUM_SLICES_PER_BANK 2
#define CFA_P70_LKUP_WC_TCAM_CTRL_172B_KEY 0
#define CFA_P70_LKUP_WC_TCAM_CTRL_344B_KEY 1
#define CFA_P70_LKUP_WC_TCAM_CTRL_688B_KEY 2
#define CFA_P70_LKUP_WC_TCAM_CTRL_MODE_SFT 29
#define CFA_P70_LKUP_WC_TCAM_CTRL_MODE_MASK 0x3
#define CFA_P70_LKUP_WC_TCAM_CTRL_VALID_SFT 31
#define CFA_P70_LKUP_WC_TCAM_CTRL_VALID_MASK 0x1
#define CFA_P70_LKUP_WC_TCAM_CTRL_NUM_BITS 3
#define CFA_P70_LKUP_WC_TCAM_CTRL_MODE_NUM_BITS 2
#define GET_NUM_SLICES_FROM_MODE(mode) (1 << (mode))
#define CFA_P70_LKUP_WC_SLICE_NUM_BYTES                                        \
	(BITS_TO_BYTES(CFA_P70_LKUP_WC_SLICE_LEN_WITH_CTRL_INFO))
#define CFA_P70_LKUP_WC_SLICE_NUM_WORDS                                        \
	(BYTES_TO_WORDS(CFA_P70_LKUP_WC_SLICE_NUM_BYTES))
#define CFA_P70_WC_TCAM_GET_NUM_SLICES_FROM_NUM_BYTES(n)                       \
	((((n) << 3) + CFA_P70_LKUP_WC_SLICE_LEN_WITH_CTRL_INFO - 1) /         \
	 CFA_P70_LKUP_WC_SLICE_LEN_WITH_CTRL_INFO)
#define CFA_MASK32(N) (((N) < 32) ? ((1U << (N)) - 1) : 0xffffffff)
#define CFA_P70_ECV_VTAG_ADD0_IMMED CFA_P70_ECV_VTAG_ADD0_IMMED_PRI0
#define CFA_P70_ECV_VTAG_PRI_MASK                                              \
	(~CFA_P70_ECV_VTAG_ADD0_IMMED &                                        \
	 CFA_MASK32(CFA_P70_ACT_ENC_ECV_VTAG_NUM_BITS))

#define CFA_P70_LKUP_EPOCH0_NUM_WORDS 1
#define CFA_P70_LKUP_EPOCH1_NUM_WORDS 1
#define CFA_P70_LKUP_EPOCH0_ENTRIES 4096
#define CFA_P70_LKUP_EPOCH1_ENTRIES 256

/* Field range check table register widths */
#define CFA_P70_FRC_PROF_NUM_WORDS                                             \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_LKUP_FRC_PROFILE_TOTAL_NUM_BITS)
#define CFA_P70_FRC_ENTRY_NUM_WORDS                                            \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_LKUP_FRC_RANGE_TOTAL_NUM_BITS)

/* Connection tracking table register widths */
#define CFA_P70_CT_STATE_NUM_WORDS                                             \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_LKUP_CT_STATE_TOTAL_NUM_BITS)
#define CFA_P70_CT_RULE_TCAM_NUM_WORDS                                         \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_LKUP_CT_RULE_TOTAL_NUM_BITS)
#define CFA_P70_CT_RULE_TCAM_RMP_NUM_WORDS                                     \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_LKUP_CT_RULE_RECORD_TOTAL_NUM_BITS)

/* Feature Chain table register widths */
#define CFA_P70_ACT_FC_TCAM_NUM_WORDS                                          \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_ACT_FC_TCAM_TOTAL_NUM_BITS)
#define CFA_P70_ACT_FC_TCAM_RMP_NUM_WORDS                                      \
	NUM_WORDS_ALIGN_32BIT(CFA_P70_ACT_FC_TCAM_RESULT_TOTAL_NUM_BITS)
/* Feature Context table register width */
#define CFA_P70_ACT_FC_NUM_WORDS                                               \
	NUM_WORDS_ALIGN_128BIT(CFA_P70_ACT_FC_RMP_DR_TOTAL_NUM_BITS)

/* Meter instance table register width */
#define CFA_P70_ACT_METER_NUM_WORDS                                            \
	NUM_WORDS_ALIGN_128BIT(CFA_P70_METERS_TOTAL_NUM_BITS)

/* Metadata Mask table register widths */
#define CFA_P70_METAMASK_PROF_NUM_WORDS 1
#define CFA_P70_METAMASK_LKUP_NUM_WORDS 1
#define CFA_P70_METAMASK_ACT_NUM_WORDS 1
#define MAX_METAMASK_PROF(chip_cfg) 8
#define MAX_METAMASK_LKUP(chip_cfg) 8
#define MAX_METAMASK_ACT(chip_cfg) 16

#define CFA_P70_VEB_TCAM_NUM_SLICES 1
#define CFA_P70_CT_TCAM_NUM_SLICES 1
#define CFA_P70_FC_TCAM_NUM_SLICES 1
#define CFA_P70_L2CTXT_TCAM_NUM_SLICES 1
#define CFA_P70_PROF_TCAM_NUM_SLICES 1

#endif /* _CFA_P70_H_ */
