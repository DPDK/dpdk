/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_P70_HW_H_
#define _CFA_P70_HW_H_

/* clang-format off */
#include "cfa_bld_p70_field_ids.h"


/**
 * Field code selection 1 for range checking (for idx 1 ...)
 */
#define CFA_P70_LKUP_FRC_PROFILE_FIELD_SEL_1_BITPOS 36
#define CFA_P70_LKUP_FRC_PROFILE_FIELD_SEL_1_NUM_BITS 4

/**
 * Mask of ranges to check against FIELD_SEL_1
 */
#define CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_1_BITPOS 20
#define CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_1_NUM_BITS 16

/**
 * Field code selection 0 for range checking
 */
#define CFA_P70_LKUP_FRC_PROFILE_FIELD_SEL_0_BITPOS 16
#define CFA_P70_LKUP_FRC_PROFILE_FIELD_SEL_0_NUM_BITS 4

/**
 * Mask of ranges to check against FIELD_SEL_0 The following shows the
 * FIELD_SEL code points:
 */
#define CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_BITPOS 0
#define CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_NUM_BITS 16
/**
 * Mask of ranges to check against FIELD_SEL_0 The following shows the
 * FIELD_SEL code points:
 */
enum cfa_p70_lkup_frc_profile_range_check_0 {
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_TL2_OVLAN_VID = 0,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_TL2_IVLAN_VID = 1,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_TL4_SRC = 2,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_TL4_DEST = 3,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_L2_OVLAN_VID = 4,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_L2_IVLAN_VID = 5,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_IP_LENGTH = 6,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_L4_SRC = 7,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_L4_DEST = 8,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_TUN_ID = 9,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_TUN_CTXT = 10,
	CFA_P70_LKUP_FRC_PROFILE_RANGE_CHECK_0_0 = 15,
};

/**
 * Total number of bits for LKUP_FRC_PROFILE
 */
#define CFA_P70_LKUP_FRC_PROFILE_TOTAL_NUM_BITS 40

/**
 * When 1, block rule searches and do host notify during background
 * visit
 */
#define CFA_P70_LKUP_CT_STATE_NOTIFY_BITPOS 13
#define CFA_P70_LKUP_CT_STATE_NOTIFY_NUM_BITS 1

/**
 * Next state to go to after host notify (only used when NOTIFY=1)
 */
#define CFA_P70_LKUP_CT_STATE_NOTIFY_STATE_BITPOS 8
#define CFA_P70_LKUP_CT_STATE_NOTIFY_STATE_NUM_BITS 5

/**
 * Default forwarding action (0=fwd, 1=miss, 2/3=copy)
 */
#define CFA_P70_LKUP_CT_STATE_ACTION_BITPOS 6
#define CFA_P70_LKUP_CT_STATE_ACTION_NUM_BITS 2

/**
 * Specifies timer (0=disabled, 1-3=timers 1-3)
 */
#define CFA_P70_LKUP_CT_STATE_TIMER_SELECT_BITPOS 4
#define CFA_P70_LKUP_CT_STATE_TIMER_SELECT_NUM_BITS 2

/**
 * Timer preload value for connections in this state
 */
#define CFA_P70_LKUP_CT_STATE_TIMER_PRELOAD_BITPOS 0
#define CFA_P70_LKUP_CT_STATE_TIMER_PRELOAD_NUM_BITS 4

/**
 * Total number of bits for LKUP_CT_STATE
 */
#define CFA_P70_LKUP_CT_STATE_TOTAL_NUM_BITS 14

/**
 * Rule only used if VALID=1 (for idx 1 ...)
 */
#define CFA_P70_LKUP_CT_RULE_VALID_BITPOS 38
#define CFA_P70_LKUP_CT_RULE_VALID_NUM_BITS 1

/**
 * Mask
 */
#define CFA_P70_LKUP_CT_RULE_MASK_BITPOS 19
#define CFA_P70_LKUP_CT_RULE_MASK_NUM_BITS 19

/**
 * Rule for packet (1) or background (0)
 */
#define CFA_P70_LKUP_CT_RULE_PKT_NOT_BG_BITPOS 18
#define CFA_P70_LKUP_CT_RULE_PKT_NOT_BG_NUM_BITS 1

/**
 * Current connection state
 */
#define CFA_P70_LKUP_CT_RULE_STATE_BITPOS 13
#define CFA_P70_LKUP_CT_RULE_STATE_NUM_BITS 5

/**
 * TCP packet flags
 */
#define CFA_P70_LKUP_CT_RULE_TCP_FLAGS_BITPOS 4
#define CFA_P70_LKUP_CT_RULE_TCP_FLAGS_NUM_BITS 9

/**
 * Packet protocol is TCP
 */
#define CFA_P70_LKUP_CT_RULE_PROT_IS_TCP_BITPOS 3
#define CFA_P70_LKUP_CT_RULE_PROT_IS_TCP_NUM_BITS 1

/**
 * Updating tcp_msb_loc
 */
#define CFA_P70_LKUP_CT_RULE_MSB_UPDT_BITPOS 2
#define CFA_P70_LKUP_CT_RULE_MSB_UPDT_NUM_BITS 1

/**
 * Packet flag error
 */
#define CFA_P70_LKUP_CT_RULE_FLAGS_FAILED_BITPOS 1
#define CFA_P70_LKUP_CT_RULE_FLAGS_FAILED_NUM_BITS 1

/**
 * Packet failed TCP window check If VALID=0, the rule is ignored during
 * searches. When VALID=1, MASK[18:0] provides a mask for bits 18:0. If
 * the mask bit is set to 0, the corresponding bit is ignored during
 * searches (does not need to match for the rule to match). During
 * background updates, all fields in the search key other than STATE are
 * always 0 (PKT_NOT_BG=0 and the other fields are unused). During
 * packet updates when PROT_IS_TCP=0, PKT_NOT_BG=1 and STATE is set to
 * the current state but the other fields will always be 0. If there is
 * a matching rule found, the record in LKUP_CT_RULE_RECORD for that
 * rule number is used.
 */
#define CFA_P70_LKUP_CT_RULE_WIN_FAILED_BITPOS 0
#define CFA_P70_LKUP_CT_RULE_WIN_FAILED_NUM_BITS 1

/**
 * Total number of bits for LKUP_CT_RULE
 */
#define CFA_P70_LKUP_CT_RULE_TOTAL_NUM_BITS 39

/**
 * Forward action (packet only): 0=fwd, 1=miss, 2/3=copy
 */
#define CFA_P70_LKUP_CT_RULE_RECORD_ACTION_BITPOS 7
#define CFA_P70_LKUP_CT_RULE_RECORD_ACTION_NUM_BITS 2

/**
 * Next state for the connection
 */
#define CFA_P70_LKUP_CT_RULE_RECORD_NEXT_STATE_BITPOS 2
#define CFA_P70_LKUP_CT_RULE_RECORD_NEXT_STATE_NUM_BITS 5

/**
 * Signals whether to send message to other CFA.k When SEND=0, no
 * message is sent. Otherwise, SEND[1] indicates that TCP_MSB_LOC in the
 * message is valid and SEND[0] that STATE is valid.
 */
#define CFA_P70_LKUP_CT_RULE_RECORD_SEND_BITPOS 0
#define CFA_P70_LKUP_CT_RULE_RECORD_SEND_NUM_BITS 2

/**
 * Total number of bits for LKUP_CT_RULE_RECORD
 */
#define CFA_P70_LKUP_CT_RULE_RECORD_TOTAL_NUM_BITS 9

/**
 * destination remap mode when enabled
 */
#define CFA_P70_ACT_VEB_RMP_MODE_BITPOS 6
#define CFA_P70_ACT_VEB_RMP_MODE_NUM_BITS 1
/**
 * destination remap mode when enabled
 */
enum cfa_p70_act_veb_rmp_mode {
	/* over write existing bitmap with entry */
	CFA_P70_ACT_VEB_RMP_MODE_OVRWRT = 0,
	/* or entry bit map with existing */
	CFA_P70_ACT_VEB_RMP_MODE_ORTGTHR = 1,
};

/**
 * enable remap the bitmap
 */
#define CFA_P70_ACT_VEB_RMP_ENABLE_BITPOS 5
#define CFA_P70_ACT_VEB_RMP_ENABLE_NUM_BITS 1

/**
 * destination bitmap #CAS_SW_REF
 * Action.CFA.VEB.Remap.tx.veb.remap.entry
 */
#define CFA_P70_ACT_VEB_RMP_BITMAP_BITPOS 0
#define CFA_P70_ACT_VEB_RMP_BITMAP_NUM_BITS 5

/**
 * Total number of bits for ACT_VEB_RMP
 */
#define CFA_P70_ACT_VEB_RMP_TOTAL_NUM_BITS 7

/**
 * Range low
 */
#define CFA_P70_LKUP_FRC_RANGE_RANGE_LO_BITPOS 16
#define CFA_P70_LKUP_FRC_RANGE_RANGE_LO_NUM_BITS 16

/**
 * Range high Field matches range when in [range_lo, range_hi]
 * (inclusive). A read/write to this register causes a read/write to the
 * LKUP_FRC_RANGE memory at address LKUP_FRC_RANGE_ADDR.
 */
#define CFA_P70_LKUP_FRC_RANGE_RANGE_HI_BITPOS 0
#define CFA_P70_LKUP_FRC_RANGE_RANGE_HI_NUM_BITS 16

/**
 * Total number of bits for LKUP_FRC_RANGE
 */
#define CFA_P70_LKUP_FRC_RANGE_TOTAL_NUM_BITS 32

/**
 * TCAM entry is valid (for idx 7 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_VALID_BITPOS 255
#define CFA_P70_PROF_L2_CTXT_TCAM_VALID_NUM_BITS 1

/**
 * spare bits (for idx 7 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_SPARE_BITPOS 253
#define CFA_P70_PROF_L2_CTXT_TCAM_SPARE_NUM_BITS 2

/**
 * Multi-pass cycle count (for idx 7 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_MPASS_CNT_BITPOS 251
#define CFA_P70_PROF_L2_CTXT_TCAM_MPASS_CNT_NUM_BITS 2

/**
 * Recycle count from prof_in (for idx 7 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_RCYC_BITPOS 247
#define CFA_P70_PROF_L2_CTXT_TCAM_RCYC_NUM_BITS 4

/**
 * loopback input from prof_in (for idx 7 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_LOOPBACK_BITPOS 246
#define CFA_P70_PROF_L2_CTXT_TCAM_LOOPBACK_NUM_BITS 1

/**
 * Source network port from prof_in (for idx 7 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_SPIF_BITPOS 244
#define CFA_P70_PROF_L2_CTXT_TCAM_SPIF_NUM_BITS 2

/**
 * Partition provided by input block (for idx 7 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_PARIF_BITPOS 239
#define CFA_P70_PROF_L2_CTXT_TCAM_PARIF_NUM_BITS 5

/**
 * Source network port or vnic (for idx 7 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_SVIF_BITPOS 228
#define CFA_P70_PROF_L2_CTXT_TCAM_SVIF_NUM_BITS 11

/**
 * Metadata provided by Input block
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_METADATA_BITPOS 196
#define CFA_P70_PROF_L2_CTXT_TCAM_METADATA_NUM_BITS 32

/**
 * L2 function
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_L2_FUNC_BITPOS 188
#define CFA_P70_PROF_L2_CTXT_TCAM_L2_FUNC_NUM_BITS 8

/**
 * ROCE Packet detected by the Parser (for idx 5 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_ROCE_BITPOS 187
#define CFA_P70_PROF_L2_CTXT_TCAM_ROCE_NUM_BITS 1

/**
 * Pure LLC Packet detected by the Parser. (for idx 5 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_PURE_LLC_BITPOS 186
#define CFA_P70_PROF_L2_CTXT_TCAM_PURE_LLC_NUM_BITS 1

/**
 * 5b enc Outer Tunnel Type (for idx 5 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_OT_HDR_TYPE_BITPOS 181
#define CFA_P70_PROF_L2_CTXT_TCAM_OT_HDR_TYPE_NUM_BITS 5

/**
 * 5b enc Tunnel Type The id_ctxt field is tunnel id or tunnel context
 * selected from outer tunnel header or tunnel header. (for idx 5 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_T_HDR_TYPE_BITPOS 176
#define CFA_P70_PROF_L2_CTXT_TCAM_T_HDR_TYPE_NUM_BITS 5

/**
 * FLDS Tunnel Status ID or Context. Each of these fields are from the
 * selected outer tunnel, tunnel, inner, or outermost L2 header
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_ID_CTXT_BITPOS 144
#define CFA_P70_PROF_L2_CTXT_TCAM_ID_CTXT_NUM_BITS 32

/**
 * Selected DMAC/SMAC
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_MAC0_BITPOS 96
#define CFA_P70_PROF_L2_CTXT_TCAM_MAC0_NUM_BITS 48

/**
 * Selected DMAC/SMAC
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_MAC1_BITPOS 48
#define CFA_P70_PROF_L2_CTXT_TCAM_MAC1_NUM_BITS 48

/**
 * 1+ VLAN tags present (for idx 1 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_VTAG_PRESENT_BITPOS 47
#define CFA_P70_PROF_L2_CTXT_TCAM_VTAG_PRESENT_NUM_BITS 1

/**
 * 2 VLAN tags present (for idx 1 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_TWO_VTAGS_BITPOS 46
#define CFA_P70_PROF_L2_CTXT_TCAM_TWO_VTAGS_NUM_BITS 1

/**
 * Outer VLAN VID (for idx 1 ...)
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_OVLAN_VID_BITPOS 34
#define CFA_P70_PROF_L2_CTXT_TCAM_OVLAN_VID_NUM_BITS 12

/**
 * Outer VLAN TPID, 3b encoded
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_OVLAN_TPID_SEL_BITPOS 31
#define CFA_P70_PROF_L2_CTXT_TCAM_OVLAN_TPID_SEL_NUM_BITS 3

/**
 * Inner VLAN VID
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_IVLAN_VID_BITPOS 19
#define CFA_P70_PROF_L2_CTXT_TCAM_IVLAN_VID_NUM_BITS 12

/**
 * Inner VLAN TPID, 3b encoded
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_IVLAN_TPID_SEL_BITPOS 16
#define CFA_P70_PROF_L2_CTXT_TCAM_IVLAN_TPID_SEL_NUM_BITS 3

/**
 * Ethertype. #CAS_SW_REF Profiler.l2ip.context.tcam.key #CAS_SW_REF
 * Profiler.l2ip.context.ipv6.tcam.key
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_ETYPE_BITPOS 0
#define CFA_P70_PROF_L2_CTXT_TCAM_ETYPE_NUM_BITS 16

/**
 * Total number of bits for PROF_L2_CTXT_TCAM
 */
#define CFA_P70_PROF_L2_CTXT_TCAM_TOTAL_NUM_BITS 256

/**
 * Valid(1)/Invalid(0) TCAM entry. (for idx 5 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_VALID_BITPOS 183
#define CFA_P70_PROF_PROFILE_TCAM_VALID_NUM_BITS 1

/**
 * spare bits. (for idx 5 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_SPARE_BITPOS 181
#define CFA_P70_PROF_PROFILE_TCAM_SPARE_NUM_BITS 2

/**
 * Loopback bit. (for idx 5 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_LOOPBACK_BITPOS 180
#define CFA_P70_PROF_PROFILE_TCAM_LOOPBACK_NUM_BITS 1

/**
 * Packet type directly from prof_in (for idx 5 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_PKT_TYPE_BITPOS 176
#define CFA_P70_PROF_PROFILE_TCAM_PKT_TYPE_NUM_BITS 4

/**
 * Recycle count from prof_in (for idx 5 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_RCYC_BITPOS 172
#define CFA_P70_PROF_PROFILE_TCAM_RCYC_NUM_BITS 4

/**
 * From L2 Context Lookup stage.
 */
#define CFA_P70_PROF_PROFILE_TCAM_METADATA_BITPOS 140
#define CFA_P70_PROF_PROFILE_TCAM_METADATA_NUM_BITS 32

/**
 * Aggregate error flag from Input stage. (for idx 4 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_AGG_ERROR_BITPOS 139
#define CFA_P70_PROF_PROFILE_TCAM_AGG_ERROR_NUM_BITS 1

/**
 * L2 function (for idx 4 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_L2_FUNC_BITPOS 131
#define CFA_P70_PROF_PROFILE_TCAM_L2_FUNC_NUM_BITS 8

/**
 * Profile function from L2 Context Lookup stage.
 */
#define CFA_P70_PROF_PROFILE_TCAM_PROF_FUNC_BITPOS 123
#define CFA_P70_PROF_PROFILE_TCAM_PROF_FUNC_NUM_BITS 8

/**
 * From FLDS Input General Status tunnel(1)/no tunnel(0) (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_HREC_NEXT_BITPOS 121
#define CFA_P70_PROF_PROFILE_TCAM_HREC_NEXT_NUM_BITS 2

/**
 * INT header type. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_INT_HDR_TYPE_BITPOS 119
#define CFA_P70_PROF_PROFILE_TCAM_INT_HDR_TYPE_NUM_BITS 2

/**
 * INT header group. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_INT_HDR_GROUP_BITPOS 117
#define CFA_P70_PROF_PROFILE_TCAM_INT_HDR_GROUP_NUM_BITS 2

/**
 * INT metadata is tail stamp. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_INT_IFA_TAIL_BITPOS 116
#define CFA_P70_PROF_PROFILE_TCAM_INT_IFA_TAIL_NUM_BITS 1

/**
 * resolved flds_otl2_hdr_valid. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL2_HDR_VALID_BITPOS 115
#define CFA_P70_PROF_PROFILE_TCAM_OTL2_HDR_VALID_NUM_BITS 1

/**
 * Outer Tunnel L2 header type. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL2_HDR_TYPE_BITPOS 113
#define CFA_P70_PROF_PROFILE_TCAM_OTL2_HDR_TYPE_NUM_BITS 2

/**
 * flds_otl2_dst_type remapped: UC(0)/MC(2)/BC(3) (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL2_UC_MC_BC_BITPOS 111
#define CFA_P70_PROF_PROFILE_TCAM_OTL2_UC_MC_BC_NUM_BITS 2

/**
 * 1+ VLAN tags present in Outer Tunnel L2 header (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL2_VTAG_PRESENT_BITPOS 110
#define CFA_P70_PROF_PROFILE_TCAM_OTL2_VTAG_PRESENT_NUM_BITS 1

/**
 * 2 VLAN tags present in Outer Tunnel L2 header (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL2_TWO_VTAGS_BITPOS 109
#define CFA_P70_PROF_PROFILE_TCAM_OTL2_TWO_VTAGS_NUM_BITS 1

/**
 * resolved flds_otl3_hdr_valid. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL3_HDR_VALID_BITPOS 108
#define CFA_P70_PROF_PROFILE_TCAM_OTL3_HDR_VALID_NUM_BITS 1

/**
 * flds_otl3_hdr_valid is stop_w_error. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL3_HDR_ERROR_BITPOS 107
#define CFA_P70_PROF_PROFILE_TCAM_OTL3_HDR_ERROR_NUM_BITS 1

/**
 * Outer Tunnel L3 header type directly from FLDS. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL3_HDR_TYPE_BITPOS 103
#define CFA_P70_PROF_PROFILE_TCAM_OTL3_HDR_TYPE_NUM_BITS 4

/**
 * Outer Tunnel L3 header is IPV4 or IPV6. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL3_HDR_ISIP_BITPOS 102
#define CFA_P70_PROF_PROFILE_TCAM_OTL3_HDR_ISIP_NUM_BITS 1

/**
 * resolved flds_otl4_hdr_valid. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL4_HDR_VALID_BITPOS 101
#define CFA_P70_PROF_PROFILE_TCAM_OTL4_HDR_VALID_NUM_BITS 1

/**
 * flds_otl4_hdr_valid is stop_w_error. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL4_HDR_ERROR_BITPOS 100
#define CFA_P70_PROF_PROFILE_TCAM_OTL4_HDR_ERROR_NUM_BITS 1

/**
 * Outer Tunnel L4 header type. (for idx 3 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL4_HDR_TYPE_BITPOS 96
#define CFA_P70_PROF_PROFILE_TCAM_OTL4_HDR_TYPE_NUM_BITS 4

/**
 * OTL4 header is UDP or TCP. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OTL4_HDR_IS_UDP_TCP_BITPOS 95
#define CFA_P70_PROF_PROFILE_TCAM_OTL4_HDR_IS_UDP_TCP_NUM_BITS 1

/**
 * resolved flds_ot_hdr_valid. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OT_HDR_VALID_BITPOS 94
#define CFA_P70_PROF_PROFILE_TCAM_OT_HDR_VALID_NUM_BITS 1

/**
 * flds_ot_hdr_valid is stop_w_error. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OT_HDR_ERROR_BITPOS 93
#define CFA_P70_PROF_PROFILE_TCAM_OT_HDR_ERROR_NUM_BITS 1

/**
 * Outer Tunnel header type. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OT_HDR_TYPE_BITPOS 88
#define CFA_P70_PROF_PROFILE_TCAM_OT_HDR_TYPE_NUM_BITS 5

/**
 * Outer Tunnel header flags. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_OT_HDR_FLAGS_BITPOS 80
#define CFA_P70_PROF_PROFILE_TCAM_OT_HDR_FLAGS_NUM_BITS 8

/**
 * resolved flds_tl2_hdr_valid. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL2_HDR_VALID_BITPOS 79
#define CFA_P70_PROF_PROFILE_TCAM_TL2_HDR_VALID_NUM_BITS 1

/**
 * Tunnel L2 header type directly from FLDS. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL2_HDR_TYPE_BITPOS 77
#define CFA_P70_PROF_PROFILE_TCAM_TL2_HDR_TYPE_NUM_BITS 2

/**
 * flds_tl2_dst_type remapped: UC(0)/MC(2)/BC(3) (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL2_UC_MC_BC_BITPOS 75
#define CFA_P70_PROF_PROFILE_TCAM_TL2_UC_MC_BC_NUM_BITS 2

/**
 * 1+ VLAN tags present in Tunnel L2 header (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL2_VTAG_PRESENT_BITPOS 74
#define CFA_P70_PROF_PROFILE_TCAM_TL2_VTAG_PRESENT_NUM_BITS 1

/**
 * 2 VLAN tags present in Tunnel L2 header (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL2_TWO_VTAGS_BITPOS 73
#define CFA_P70_PROF_PROFILE_TCAM_TL2_TWO_VTAGS_NUM_BITS 1

/**
 * resolved flds_tl3_hdr_valid. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL3_HDR_VALID_BITPOS 72
#define CFA_P70_PROF_PROFILE_TCAM_TL3_HDR_VALID_NUM_BITS 1

/**
 * flds_tl3_hdr_valid is stop_w_error. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL3_HDR_ERROR_BITPOS 71
#define CFA_P70_PROF_PROFILE_TCAM_TL3_HDR_ERROR_NUM_BITS 1

/**
 * Tunnel L3 header type directly from FLDS. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL3_HDR_TYPE_BITPOS 67
#define CFA_P70_PROF_PROFILE_TCAM_TL3_HDR_TYPE_NUM_BITS 4

/**
 * Tunnel L3 header is IPV4 or IPV6. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL3_HDR_ISIP_BITPOS 66
#define CFA_P70_PROF_PROFILE_TCAM_TL3_HDR_ISIP_NUM_BITS 1

/**
 * resolved flds_tl4_hdr_valid. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL4_HDR_VALID_BITPOS 65
#define CFA_P70_PROF_PROFILE_TCAM_TL4_HDR_VALID_NUM_BITS 1

/**
 * flds_tl4_hdr_valid is stop_w_error. (for idx 2 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL4_HDR_ERROR_BITPOS 64
#define CFA_P70_PROF_PROFILE_TCAM_TL4_HDR_ERROR_NUM_BITS 1

/**
 * Tunnel L4 header type directly from FLDS. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL4_HDR_TYPE_BITPOS 60
#define CFA_P70_PROF_PROFILE_TCAM_TL4_HDR_TYPE_NUM_BITS 4

/**
 * TL4 header is UDP or TCP. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TL4_HDR_IS_UDP_TCP_BITPOS 59
#define CFA_P70_PROF_PROFILE_TCAM_TL4_HDR_IS_UDP_TCP_NUM_BITS 1

/**
 * resolved flds_tun_hdr_valid. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TUN_HDR_VALID_BITPOS 58
#define CFA_P70_PROF_PROFILE_TCAM_TUN_HDR_VALID_NUM_BITS 1

/**
 * flds_tun_hdr_valid is stop_w_error. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TUN_HDR_ERROR_BITPOS 57
#define CFA_P70_PROF_PROFILE_TCAM_TUN_HDR_ERROR_NUM_BITS 1

/**
 * Tunnel header type directly from FLDS. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TUN_HDR_TYPE_BITPOS 52
#define CFA_P70_PROF_PROFILE_TCAM_TUN_HDR_TYPE_NUM_BITS 5

/**
 * Tunnel header flags directly from FLDS. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_TUN_HDR_FLAGS_BITPOS 44
#define CFA_P70_PROF_PROFILE_TCAM_TUN_HDR_FLAGS_NUM_BITS 8

/**
 * resolved flds_l2_hdr_valid. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_L2_HDR_VALID_BITPOS 43
#define CFA_P70_PROF_PROFILE_TCAM_L2_HDR_VALID_NUM_BITS 1

/**
 * flds_l2_hdr_valid is stop_w_error. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_L2_HDR_ERROR_BITPOS 42
#define CFA_P70_PROF_PROFILE_TCAM_L2_HDR_ERROR_NUM_BITS 1

/**
 * L2 header type directly from FLDS. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_L2_HDR_TYPE_BITPOS 40
#define CFA_P70_PROF_PROFILE_TCAM_L2_HDR_TYPE_NUM_BITS 2

/**
 * flds_l2_dst_type remapped: UC(0)/MC(2)/BC(3). (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_L2_UC_MC_BC_BITPOS 38
#define CFA_P70_PROF_PROFILE_TCAM_L2_UC_MC_BC_NUM_BITS 2

/**
 * 1+ VLAN tags present in inner L2 header. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_L2_VTAG_PRESENT_BITPOS 37
#define CFA_P70_PROF_PROFILE_TCAM_L2_VTAG_PRESENT_NUM_BITS 1

/**
 * 2 VLAN tags present in inner L2 header. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_L2_TWO_VTAGS_BITPOS 36
#define CFA_P70_PROF_PROFILE_TCAM_L2_TWO_VTAGS_NUM_BITS 1

/**
 * resolved flds_l3_hdr_valid. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_L3_HDR_VALID_BITPOS 35
#define CFA_P70_PROF_PROFILE_TCAM_L3_HDR_VALID_NUM_BITS 1

/**
 * flds_l3_hdr_valid is stop_w_error. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_TCAM_L3_HDR_ERROR_BITPOS 34
#define CFA_P70_PROF_PROFILE_TCAM_L3_HDR_ERROR_NUM_BITS 1

/**
 * L3 header type directly from FLDS.
 */
#define CFA_P70_PROF_PROFILE_TCAM_L3_HDR_TYPE_BITPOS 30
#define CFA_P70_PROF_PROFILE_TCAM_L3_HDR_TYPE_NUM_BITS 4

/**
 * L3 header is IPV4 or IPV6.
 */
#define CFA_P70_PROF_PROFILE_TCAM_L3_HDR_ISIP_BITPOS 29
#define CFA_P70_PROF_PROFILE_TCAM_L3_HDR_ISIP_NUM_BITS 1

/**
 * L3 header next protocol directly from FLDS.
 */
#define CFA_P70_PROF_PROFILE_TCAM_L3_PROT_BITPOS 21
#define CFA_P70_PROF_PROFILE_TCAM_L3_PROT_NUM_BITS 8

/**
 * resolved flds_l4_hdr_valid.
 */
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_VALID_BITPOS 20
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_VALID_NUM_BITS 1

/**
 * flds_l4_hdr_valid is stop_w_error.
 */
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_ERROR_BITPOS 19
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_ERROR_NUM_BITS 1

/**
 * L4 header type directly from FLDS.
 */
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_TYPE_BITPOS 15
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_TYPE_NUM_BITS 4

/**
 * L4 header is UDP or TCP.2
 */
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_IS_UDP_TCP_BITPOS 14
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_IS_UDP_TCP_NUM_BITS 1

/**
 * L4 header subtype directly from FLDS.
 */
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_SUBTYPE_BITPOS 11
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_SUBTYPE_NUM_BITS 3

/**
 * L4 header flags directly from FLDS.
 */
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_FLAGS_BITPOS 2
#define CFA_P70_PROF_PROFILE_TCAM_L4_HDR_FLAGS_NUM_BITS 9

/**
 * DCN present bits directly from FLDS. #CAS_SW_REF
 * Profiler.profile.lookup.tcam.key
 */
#define CFA_P70_PROF_PROFILE_TCAM_L4_DCN_PRESENT_BITPOS 0
#define CFA_P70_PROF_PROFILE_TCAM_L4_DCN_PRESENT_NUM_BITS 2

/**
 * Total number of bits for PROF_PROFILE_TCAM
 */
#define CFA_P70_PROF_PROFILE_TCAM_TOTAL_NUM_BITS 184

/**
 * Valid entry (for idx 2 ...)
 */
#define CFA_P70_ACT_VEB_TCAM_TX_VALID_BITPOS 79
#define CFA_P70_ACT_VEB_TCAM_TX_VALID_NUM_BITS 1

/**
 * PF Parif Number (for idx 2 ...)
 */
#define CFA_P70_ACT_VEB_TCAM_TX_PARIF_IN_BITPOS 74
#define CFA_P70_ACT_VEB_TCAM_TX_PARIF_IN_NUM_BITS 5

/**
 * Number of VLAN Tags. (for idx 2 ...)
 */
#define CFA_P70_ACT_VEB_TCAM_TX_NUM_VTAGS_BITPOS 72
#define CFA_P70_ACT_VEB_TCAM_TX_NUM_VTAGS_NUM_BITS 2

/**
 * Dest. MAC Address
 */
#define CFA_P70_ACT_VEB_TCAM_TX_DMAC_BITPOS 24
#define CFA_P70_ACT_VEB_TCAM_TX_DMAC_NUM_BITS 48

/**
 * Outer VLAN Tag ID
 */
#define CFA_P70_ACT_VEB_TCAM_TX_OVID_BITPOS 12
#define CFA_P70_ACT_VEB_TCAM_TX_OVID_NUM_BITS 12

/**
 * Inner VLAN Tag ID #CAS_SW_REF Action.CFA.VEB.TCAM.tx.veb.tcam.entry
 */
#define CFA_P70_ACT_VEB_TCAM_TX_IVID_BITPOS 0
#define CFA_P70_ACT_VEB_TCAM_TX_IVID_NUM_BITS 12

/**
 * Total number of bits for ACT_VEB_TCAM_TX
 */
#define CFA_P70_ACT_VEB_TCAM_TX_TOTAL_NUM_BITS 80

/**
 * Valid entry (for idx 2 ...)
 */
#define CFA_P70_ACT_VEB_TCAM_RX_VALID_BITPOS 79
#define CFA_P70_ACT_VEB_TCAM_RX_VALID_NUM_BITS 1

/**
 * spare (for idx 2 ...)
 */
#define CFA_P70_ACT_VEB_TCAM_RX_SPARE_BITPOS 78
#define CFA_P70_ACT_VEB_TCAM_RX_SPARE_NUM_BITS 1

/**
 * program to zero (for idx 2 ...)
 */
#define CFA_P70_ACT_VEB_TCAM_RX_PADDING_BITPOS 68
#define CFA_P70_ACT_VEB_TCAM_RX_PADDING_NUM_BITS 10

/**
 * DMAC is unicast address (for idx 2 ...)
 */
#define CFA_P70_ACT_VEB_TCAM_RX_UNICAST_BITPOS 67
#define CFA_P70_ACT_VEB_TCAM_RX_UNICAST_NUM_BITS 1

/**
 * DMAC is multicast address (for idx 2 ...)
 */
#define CFA_P70_ACT_VEB_TCAM_RX_MULTICAST_BITPOS 66
#define CFA_P70_ACT_VEB_TCAM_RX_MULTICAST_NUM_BITS 1

/**
 * DMAC is broadcast address (for idx 2 ...)
 */
#define CFA_P70_ACT_VEB_TCAM_RX_BROADCAST_BITPOS 65
#define CFA_P70_ACT_VEB_TCAM_RX_BROADCAST_NUM_BITS 1

/**
 * pfid
 */
#define CFA_P70_ACT_VEB_TCAM_RX_PFID_BITPOS 60
#define CFA_P70_ACT_VEB_TCAM_RX_PFID_NUM_BITS 5

/**
 * vfid (for idx 1 ...)
 */
#define CFA_P70_ACT_VEB_TCAM_RX_VFID_BITPOS 48
#define CFA_P70_ACT_VEB_TCAM_RX_VFID_NUM_BITS 12

/**
 * source mac #CAS_SW_REF AAction.CFA.VEB.TCAM.rx.veb.tcam.entry
 */
#define CFA_P70_ACT_VEB_TCAM_RX_SMAC_BITPOS 0
#define CFA_P70_ACT_VEB_TCAM_RX_SMAC_NUM_BITS 48

/**
 * Total number of bits for ACT_VEB_TCAM_RX
 */
#define CFA_P70_ACT_VEB_TCAM_RX_TOTAL_NUM_BITS 80

/**
 * Valid entry (for idx 1 ...)
 */
#define CFA_P70_ACT_FC_TCAM_FC_VALID_BITPOS 33
#define CFA_P70_ACT_FC_TCAM_FC_VALID_NUM_BITS 1

/**
 * Reserved (for idx 1 ...)
 */
#define CFA_P70_ACT_FC_TCAM_FC_RSVD_BITPOS 32
#define CFA_P70_ACT_FC_TCAM_FC_RSVD_NUM_BITS 1

/**
 * Updated metadata. #CAS_SW_REF Action.CFA.FC.TCAM.fc.tcam.meta.entry
 * #CAS_SW_REF Action.CFA.FC.TCAM.fc.tcam.l2ip.func.entry #CAS_SW_REF
 * Action.CFA.FC.TCAM.fc.tcam.l2.ctxt.entry #CAS_SW_REF
 * Action.CFA.FC.TCAM.fc.tcam.l2ipf.ctxt.entry
 */
#define CFA_P70_ACT_FC_TCAM_FC_METADATA_BITPOS 0
#define CFA_P70_ACT_FC_TCAM_FC_METADATA_NUM_BITS 32

/**
 * Total number of bits for ACT_FC_TCAM
 */
#define CFA_P70_ACT_FC_TCAM_TOTAL_NUM_BITS 34

/**
 * New metadata.
 */
#define CFA_P70_ACT_FC_RMP_DR_METADATA_BITPOS 40
#define CFA_P70_ACT_FC_RMP_DR_METADATA_NUM_BITS 32

/**
 * Metadata merge control mask.
 */
#define CFA_P70_ACT_FC_RMP_DR_METAMASK_BITPOS 8
#define CFA_P70_ACT_FC_RMP_DR_METAMASK_NUM_BITS 32

/**
 * New L2 function. #CAS_SW_REF Action.CFA.FC.Remap.fc.remap.entry
 */
#define CFA_P70_ACT_FC_RMP_DR_L2_FUNC_BITPOS 0
#define CFA_P70_ACT_FC_RMP_DR_L2_FUNC_NUM_BITS 8

/**
 * Total number of bits for ACT_FC_RMP_DR
 */
#define CFA_P70_ACT_FC_RMP_DR_TOTAL_NUM_BITS 72

/**
 * enables ilt metadata (for idx 3 ...)
 */
#define CFA_P70_PROF_ILT_DR_ILT_META_EN_BITPOS 104
#define CFA_P70_PROF_ILT_DR_ILT_META_EN_NUM_BITS 1

/**
 * meta profile register index (for idx 3 ...)
 */
#define CFA_P70_PROF_ILT_DR_META_PROF_BITPOS 101
#define CFA_P70_PROF_ILT_DR_META_PROF_NUM_BITS 3

/**
 * ilt metadata, used when ilt_meta_en is set
 */
#define CFA_P70_PROF_ILT_DR_METADATA_BITPOS 69
#define CFA_P70_PROF_ILT_DR_METADATA_NUM_BITS 32

/**
 * Partition (for idx 2 ...)
 */
#define CFA_P70_PROF_ILT_DR_PARIF_BITPOS 64
#define CFA_P70_PROF_ILT_DR_PARIF_NUM_BITS 5

/**
 * L2 function (for idx 1 ...)
 */
#define CFA_P70_PROF_ILT_DR_L2_FUNC_BITPOS 56
#define CFA_P70_PROF_ILT_DR_L2_FUNC_NUM_BITS 8

/**
 * When set cfa_meta opcode is allowed (for idx 1 ...)
 */
#define CFA_P70_PROF_ILT_DR_EN_BD_META_BITPOS 55
#define CFA_P70_PROF_ILT_DR_EN_BD_META_NUM_BITS 1

/**
 * When set act_rec_ptr is set to cfa_action if it is non-zero.
 * Otherwise act_rec_ptr is set to act_rec_ptr from this table. (for idx
 * 1 ...)
 */
#define CFA_P70_PROF_ILT_DR_EN_BD_ACTION_BITPOS 54
#define CFA_P70_PROF_ILT_DR_EN_BD_ACTION_NUM_BITS 1

/**
 * When set destination is set to destination from this table. Otherwise
 * it is set to est_dest. (for idx 1 ...)
 */
#define CFA_P70_PROF_ILT_DR_EN_ILT_DEST_BITPOS 53
#define CFA_P70_PROF_ILT_DR_EN_ILT_DEST_NUM_BITS 1

/**
 * ILT opcode (for idx 1 ...)
 */
#define CFA_P70_PROF_ILT_DR_ILT_FWD_OP_BITPOS 50
#define CFA_P70_PROF_ILT_DR_ILT_FWD_OP_NUM_BITS 3
/**
 * ILT opcode (for idx 1 ...)
 */
enum cfa_p70_prof_ilt_dr_ilt_fwd_op {
	/* cfa is bypassed */
	CFA_P70_PROF_ILT_DR_ILT_FWD_OP_BYPASS_CFA = 0,
	/* cfa is bypassed if packet is ROCE */
	CFA_P70_PROF_ILT_DR_ILT_FWD_OP_BYPASS_CFA_ROCE = 1,
	/* profiler and lookup blocks are bypassed */
	CFA_P70_PROF_ILT_DR_ILT_FWD_OP_BYPASS_LKUP = 2,
	/* packet proceeds to L2 Context Stage */
	CFA_P70_PROF_ILT_DR_ILT_FWD_OP_NORMAL_FLOW = 3,
	/* mark packet for drop */
	CFA_P70_PROF_ILT_DR_ILT_FWD_OP_DROP = 4,
};

/**
 * action hint used with act_rec_ptr (for idx 1 ...)
 */
#define CFA_P70_PROF_ILT_DR_ILT_ACT_HINT_BITPOS 48
#define CFA_P70_PROF_ILT_DR_ILT_ACT_HINT_NUM_BITS 2

/**
 * table scope used with act_rec_ptr (for idx 1 ...)
 */
#define CFA_P70_PROF_ILT_DR_ILT_SCOPE_BITPOS 43
#define CFA_P70_PROF_ILT_DR_ILT_SCOPE_NUM_BITS 5

/**
 * Default act_rec_ptr or explicit on Lookup Bypass.
 */
#define CFA_P70_PROF_ILT_DR_ILT_ACT_REC_PTR_BITPOS 17
#define CFA_P70_PROF_ILT_DR_ILT_ACT_REC_PTR_NUM_BITS 26

/**
 * used for destination #CAS_SW_REF Profiler.input.lookup.table.entry
 */
#define CFA_P70_PROF_ILT_DR_ILT_DESTINATION_BITPOS 0
#define CFA_P70_PROF_ILT_DR_ILT_DESTINATION_NUM_BITS 17

/**
 * Total number of bits for PROF_ILT_DR
 */
#define CFA_P70_PROF_ILT_DR_TOTAL_NUM_BITS 105

/**
 * Normal operation. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_PL_BYP_LKUP_EN_BITPOS 42
#define CFA_P70_PROF_PROFILE_RMP_DR_PL_BYP_LKUP_EN_NUM_BITS 1

/**
 * Enable search in EM database. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_EM_SEARCH_EN_BITPOS 41
#define CFA_P70_PROF_PROFILE_RMP_DR_EM_SEARCH_EN_NUM_BITS 1

/**
 * ID to differentiate common EM keys. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_EM_PROFILE_ID_BITPOS 33
#define CFA_P70_PROF_PROFILE_RMP_DR_EM_PROFILE_ID_NUM_BITS 8

/**
 * Exact match key template select.
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_EM_KEY_ID_BITPOS 26
#define CFA_P70_PROF_PROFILE_RMP_DR_EM_KEY_ID_NUM_BITS 7

/**
 * Exact Match Lookup table scope.
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_EM_SCOPE_BITPOS 21
#define CFA_P70_PROF_PROFILE_RMP_DR_EM_SCOPE_NUM_BITS 5

/**
 * Enable search in TCAM database.
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_TCAM_SEARCH_EN_BITPOS 20
#define CFA_P70_PROF_PROFILE_RMP_DR_TCAM_SEARCH_EN_NUM_BITS 1

/**
 * ID to differentiate common TCAM keys.
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_TCAM_PROFILE_ID_BITPOS 12
#define CFA_P70_PROF_PROFILE_RMP_DR_TCAM_PROFILE_ID_NUM_BITS 8

/**
 * TCAM key template select.
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_TCAM_KEY_ID_BITPOS 5
#define CFA_P70_PROF_PROFILE_RMP_DR_TCAM_KEY_ID_NUM_BITS 7

/**
 * Wild-card TCAM Lookup table scope.
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_TCAM_SCOPE_BITPOS 0
#define CFA_P70_PROF_PROFILE_RMP_DR_TCAM_SCOPE_NUM_BITS 5

/**
 * Total number of bits for PROF_PROFILE_RMP_DR
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_TOTAL_NUM_BITS 43

/**
 * Bypass operation. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_PL_BYP_LKUP_EN_BITPOS 42
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_PL_BYP_LKUP_EN_NUM_BITS 1

/**
 * Reserved for future use. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_RESERVED_BITPOS 36
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_RESERVED_NUM_BITS 6

/**
 * Bypass operations. (for idx 1 ...)
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_BYPASS_OP_BITPOS 33
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_BYPASS_OP_NUM_BITS 3
/**
 * Bypass operations. (for idx 1 ...)
 */
enum cfa_p70_prof_profile_rmp_dr_byp_bypass_op {
	/* cfa is bypassed */
	CFA_P70_PROF_PROFILE_RMP_DR_BYP_BYPASS_OP_BYPASS_CFA = 0,
	/* Byass lookup use act_record_ptr from this table. */
	CFA_P70_PROF_PROFILE_RMP_DR_BYP_BYPASS_OP_BYPASS_LKUP = 1,
	/* Byass lookup use Partition Default Action Record Pointer Table */
	CFA_P70_PROF_PROFILE_RMP_DR_BYP_BYPASS_OP_BYPASS_DEFAULT = 2,
	/* Byass lookup use Partition Error Action Record Pointer Table. */
	CFA_P70_PROF_PROFILE_RMP_DR_BYP_BYPASS_OP_BYPASS_ERROR = 3,
	/* set the drop flag. */
	CFA_P70_PROF_PROFILE_RMP_DR_BYP_BYPASS_OP_DROP = 4,
};

/**
 * action hint used with plact_rec_ptr
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_PL_ACT_HINT_BITPOS 31
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_PL_ACT_HINT_NUM_BITS 2

/**
 * table scope used with pl_act_rec_ptr
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_PL_SCOPE_BITPOS 26
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_PL_SCOPE_NUM_BITS 5

/**
 * Used for BYPASS_LKUP. #CAS_SW_REF Profiler.profile.remap.entry.build
 * #CAS_SW_REF Profiler.profile.remap.entry.bypass.cfa #CAS_SW_REF
 * Profiler.profile.remap.entry.bypass.lkup #CAS_SW_REF
 * Profiler.profile.remap.entry.other
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_PL_ACT_REC_PTR_BITPOS 0
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_PL_ACT_REC_PTR_NUM_BITS 26

/**
 * Total number of bits for PROF_PROFILE_RMP_DR_BYP
 */
#define CFA_P70_PROF_PROFILE_RMP_DR_BYP_TOTAL_NUM_BITS 43

/**
 * VLAN TPID anti-spoofing control.
 */
#define CFA_P70_ACT_VSPT_DR_TX_TPID_AS_CTL_BITPOS 29
#define CFA_P70_ACT_VSPT_DR_TX_TPID_AS_CTL_NUM_BITS 2
/**
 * VLAN TPID anti-spoofing control.
 */
enum cfa_p70_act_vspt_dr_tx_tpid_as_ctl {
	CFA_P70_ACT_VSPT_DR_TX_TPID_IGNORE = 0,
	CFA_P70_ACT_VSPT_DR_TX_TPID_DEFAULT = 1,
	CFA_P70_ACT_VSPT_DR_TX_TPID_DROP = 2,
};

/**
 * VLAN allowed TPID bit map.
 */
#define CFA_P70_ACT_VSPT_DR_TX_ALWD_TPID_BITPOS 21
#define CFA_P70_ACT_VSPT_DR_TX_ALWD_TPID_NUM_BITS 8

/**
 * VLAN encoded default TPID.
 */
#define CFA_P70_ACT_VSPT_DR_TX_DFLT_TPID_BITPOS 18
#define CFA_P70_ACT_VSPT_DR_TX_DFLT_TPID_NUM_BITS 3

/**
 * VLAN PRIority anti-spoofing control.
 */
#define CFA_P70_ACT_VSPT_DR_TX_PRI_AS_CTL_BITPOS 16
#define CFA_P70_ACT_VSPT_DR_TX_PRI_AS_CTL_NUM_BITS 2
/**
 * VLAN PRIority anti-spoofing control.
 */
enum cfa_p70_act_vspt_dr_tx_pri_as_ctl {
	CFA_P70_ACT_VSPT_DR_TX_PRI_IGNORE = 0,
	CFA_P70_ACT_VSPT_DR_TX_PRI_DEFAULT = 1,
	CFA_P70_ACT_VSPT_DR_TX_PRI_DROP = 2,
};

/**
 * VLAN allowed PRIority bit map.
 */
#define CFA_P70_ACT_VSPT_DR_TX_ALWD_PRI_BITPOS 8
#define CFA_P70_ACT_VSPT_DR_TX_ALWD_PRI_NUM_BITS 8

/**
 * VLAN default PRIority.
 */
#define CFA_P70_ACT_VSPT_DR_TX_DFLT_PRI_BITPOS 5
#define CFA_P70_ACT_VSPT_DR_TX_DFLT_PRI_NUM_BITS 3

/**
 * Mirror destination (1..31) or 5'h0=NO_MIRROR #CAS_SW_REF
 * Action.CFA.DEST.SVIF.Property.Tables.tx.svif.property.entry
 */
#define CFA_P70_ACT_VSPT_DR_TX_MIR_BITPOS 0
#define CFA_P70_ACT_VSPT_DR_TX_MIR_NUM_BITS 5

/**
 * Total number of bits for ACT_VSPT_DR_TX
 */
#define CFA_P70_ACT_VSPT_DR_TX_TOTAL_NUM_BITS 31

/**
 * Reserved for future use.
 */
#define CFA_P70_ACT_VSPT_DR_RX_RSVD_BITPOS 24
#define CFA_P70_ACT_VSPT_DR_RX_RSVD_NUM_BITS 7

/**
 * Output metadata format select.
 */
#define CFA_P70_ACT_VSPT_DR_RX_METAFMT_BITPOS 22
#define CFA_P70_ACT_VSPT_DR_RX_METAFMT_NUM_BITS 2
/**
 * Output metadata format select.
 */
enum cfa_p70_act_vspt_dr_rx_metafmt {
	CFA_P70_ACT_VSPT_DR_RX_METAFMT_ACT_REC_PTR = 0,
	CFA_P70_ACT_VSPT_DR_RX_METAFMT_TUNNEL_ID = 1,
	CFA_P70_ACT_VSPT_DR_RX_METAFMT_CSTM_HDR_DATA = 2,
	CFA_P70_ACT_VSPT_DR_RX_METAFMT_HDR_OFFSETS = 3,
};

/**
 * Function ID: 4 bit PF and 12 bit VID (VNIC ID)
 */
#define CFA_P70_ACT_VSPT_DR_RX_FID_BITPOS 5
#define CFA_P70_ACT_VSPT_DR_RX_FID_NUM_BITS 17

/**
 * Mirror destination (1..31) or 5'h0=NO_MIRROR #CAS_SW_REF
 * Action.CFA.DEST.SVIF.Property.Tables.rx.destination.property.entry
 */
#define CFA_P70_ACT_VSPT_DR_RX_MIR_BITPOS 0
#define CFA_P70_ACT_VSPT_DR_RX_MIR_NUM_BITS 5

/**
 * Total number of bits for ACT_VSPT_DR_RX
 */
#define CFA_P70_ACT_VSPT_DR_RX_TOTAL_NUM_BITS 31

/**
 * LAG destination bit map.
 */
#define CFA_P70_ACT_LBT_DR_DST_BMP_BITPOS 0
#define CFA_P70_ACT_LBT_DR_DST_BMP_NUM_BITS 5

/**
 * Total number of bits for ACT_LBT_DR
 */
#define CFA_P70_ACT_LBT_DR_TOTAL_NUM_BITS 5

/**
 * Preserve incoming partition, don't remap (for idx 3 ...)
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_PRSV_PARIF_BITPOS 126
#define CFA_P70_PROF_L2_CTXT_RMP_DR_PRSV_PARIF_NUM_BITS 1

/**
 * Partition, replaces parif from input block (for idx 3 ...)
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_PARIF_BITPOS 121
#define CFA_P70_PROF_L2_CTXT_RMP_DR_PARIF_NUM_BITS 5

/**
 * Preserve incoming L2_CTXT (for idx 3 ...)
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_PRSV_L2IP_CTXT_BITPOS 120
#define CFA_P70_PROF_L2_CTXT_RMP_DR_PRSV_L2IP_CTXT_NUM_BITS 1

/**
 * L2 logical id which may be used in EM and WC Lookups. (for idx 3 ...)
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_CTXT_BITPOS 109
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_CTXT_NUM_BITS 11

/**
 * Preserve incoming PROF_FUNC (for idx 3 ...)
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_PRSV_PROF_FUNC_BITPOS 108
#define CFA_P70_PROF_L2_CTXT_RMP_DR_PRSV_PROF_FUNC_NUM_BITS 1

/**
 * Allow Profile TCAM Lookup Table to be logically partitioned. (for idx
 * 3 ...)
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_PROF_FUNC_BITPOS 100
#define CFA_P70_PROF_L2_CTXT_RMP_DR_PROF_FUNC_NUM_BITS 8

/**
 * Context operation code. (for idx 3 ...)
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_CTXT_OPCODE_BITPOS 98
#define CFA_P70_PROF_L2_CTXT_RMP_DR_CTXT_OPCODE_NUM_BITS 2
/**
 * Context operation code. (for idx 3 ...)
 */
enum cfa_p70_prof_l2_ctxt_rmp_dr_ctxt_opcode {
	/* def_ctxt_data provides destination */
	CFA_P70_PROF_L2_CTXT_RMP_DR_CTXT_OPCODE_BYPASS_CFA = 0,
	/* def_ctxt_data provides act_rec_ptr */
	CFA_P70_PROF_L2_CTXT_RMP_DR_CTXT_OPCODE_BYPASS_LKUP = 1,
	/* continue normal flow */
	CFA_P70_PROF_L2_CTXT_RMP_DR_CTXT_OPCODE_NORMAL_FLOW = 2,
	/* mark packet for drop */
	CFA_P70_PROF_L2_CTXT_RMP_DR_CTXT_OPCODE_DROP = 3,
};

/**
 * Enables remap of meta_data from Input block (for idx 3 ...)
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_META_ENB_BITPOS 97
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_META_ENB_NUM_BITS 1

/**
 * l2ip_meta_prof[2:0] = l2ip_meta[34:32], l2ip_meta_data[31:0] =
 * l2ip_meta[31:0]
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_META_BITPOS 62
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_META_NUM_BITS 35

/**
 * Enables remap of action record pointer from Input block (for idx 1
 * ...)
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_ACT_ENB_BITPOS 61
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_ACT_ENB_NUM_BITS 1

/**
 * l2ip_act_hint[1:0] = l2ip_act_data[32:31], l2ip_act_scope[4:0] =
 * l2ip_act_data[30:26], l2ip_act_rec_ptr[25:0] = l2ip_act_data[25:0]
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_ACT_DATA_BITPOS 28
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_ACT_DATA_NUM_BITS 33

/**
 * Enables remap of ring_table_idx
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_RFS_ENB_BITPOS 27
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_RFS_ENB_NUM_BITS 1

/**
 * ring_table_idx[8:0] = l2ip_rfs_data[8:0] (rx only)
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_RFS_DATA_BITPOS 18
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_RFS_DATA_NUM_BITS 9

/**
 * Enables remap of destination from input block
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_DEST_ENB_BITPOS 17
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_DEST_ENB_NUM_BITS 1

/**
 * destination[16:0] = l2ip_dest_data[16:0] #CAS_SW_REF
 * Profiler.l2ip.context.remap.table
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_DEST_DATA_BITPOS 0
#define CFA_P70_PROF_L2_CTXT_RMP_DR_L2IP_DEST_DATA_NUM_BITS 17

/**
 * Total number of bits for PROF_L2_CTXT_RMP_DR
 */
#define CFA_P70_PROF_L2_CTXT_RMP_DR_TOTAL_NUM_BITS 127

/**
 * FC TCAM Search Result.
 */
#define CFA_P70_ACT_FC_TCAM_RESULT_SEARCH_RESULT_BITPOS 0
#define CFA_P70_ACT_FC_TCAM_RESULT_SEARCH_RESULT_NUM_BITS 6

/**
 * Unused Field.
 */
#define CFA_P70_ACT_FC_TCAM_RESULT_UNUSED_0_BITPOS 6
#define CFA_P70_ACT_FC_TCAM_RESULT_UNUSED_0_NUM_BITS 25

/**
 * FC TCAM Search Hit.
 */
#define CFA_P70_ACT_FC_TCAM_RESULT_SEARCH_HIT_BITPOS 31
#define CFA_P70_ACT_FC_TCAM_RESULT_SEARCH_HIT_NUM_BITS 1

/**
 * Total number of bits for ACT_FC_TCAM_RESULT
 */
#define CFA_P70_ACT_FC_TCAM_RESULT_TOTAL_NUM_BITS 32

/**
 * Unused Field.
 */
#define CFA_P70_ACT_MIRROR_UNUSED_0_BITPOS 0
#define CFA_P70_ACT_MIRROR_UNUSED_0_NUM_BITS 21
#define CFA_P70_ACT_MIRROR_RELATIVE_BITPOS 21
#define CFA_P70_ACT_MIRROR_RELATIVE_NUM_BITS 1
/**
 * RELATIVE
 */
enum cfa_p70_act_mirror_relative {
	/* act_rec_ptr field is absolute. */
	CFA_P70_ACT_MIRROR_RELATIVE_ABSOLUTE  = 0,
	/*
	 * act_rec_ptr field is relative to the original action record pointer.
	 */
	CFA_P70_ACT_MIRROR_RELATIVE_RELATIVE  = 1,
};

/**
 * micr1_act_hint[1:0] - action hint used with act_rec_ptr.
 */
#define CFA_P70_ACT_MIRROR_HINT_BITPOS 22
#define CFA_P70_ACT_MIRROR_HINT_NUM_BITS 2

/**
 * Sampling mode.
 */
#define CFA_P70_ACT_MIRROR_SAMP_BITPOS 24
#define CFA_P70_ACT_MIRROR_SAMP_NUM_BITS 2
/**
 * Sampling mode.
 */
enum cfa_p70_act_mirror_samp {
	/* PRNG based. */
	CFA_P70_ACT_MIRROR_SAMP_STAT  = 0,
	/* packet count based. */
	CFA_P70_ACT_MIRROR_SAMP_PACKET  = 1,
	/* packet count w/jitter based. */
	CFA_P70_ACT_MIRROR_SAMP_JITTER  = 2,
	/* timer based. */
	CFA_P70_ACT_MIRROR_SAMP_TIMER  = 3,
};

/**
 * Truncation mode.
 */
#define CFA_P70_ACT_MIRROR_TRUNC_BITPOS 26
#define CFA_P70_ACT_MIRROR_TRUNC_NUM_BITS 2
/**
 * Truncation mode.
 */
enum cfa_p70_act_mirror_trunc {
	/* No Truncation. */
	CFA_P70_ACT_MIRROR_TRUNC_DISABLED  = 0,
	/* RFFU. */
	CFA_P70_ACT_MIRROR_TRUNC_RSVD  = 1,
	/* mirror copy will restrict outermost tunnel payload to 128B. */
	CFA_P70_ACT_MIRROR_TRUNC_B128  = 2,
	/* mirror copy will restrict outermost tunnel payload to 256B. */
	CFA_P70_ACT_MIRROR_TRUNC_B256  = 3,
};
#define CFA_P70_ACT_MIRROR_IGN_DROP_BITPOS 28
#define CFA_P70_ACT_MIRROR_IGN_DROP_NUM_BITS 1
/**
 * IGN_DROP
 */
enum cfa_p70_act_mirror_ign_drop {
	/*
	 * Honor Drop When set the mirror copy is made regardless if the initial
	 * action is to drop the packet or not.
	 */
	CFA_P70_ACT_MIRROR_IGN_DROP_HONOR  = 0,
	/* Ignore Drop */
	CFA_P70_ACT_MIRROR_IGN_DROP_IGNORE  = 1,
};
#define CFA_P70_ACT_MIRROR_MODE_BITPOS 29
#define CFA_P70_ACT_MIRROR_MODE_NUM_BITS 2
/**
 * MODE
 */
enum cfa_p70_act_mirror_mode {
	/* No Copy. */
	CFA_P70_ACT_MIRROR_MODE_DISABLED  = 0,
	/* Override AR. */
	CFA_P70_ACT_MIRROR_MODE_OVERRIDE  = 1,
	/* Ingress Copy. */
	CFA_P70_ACT_MIRROR_MODE_INGRESS  = 2,
	/* Egress Copy. */
	CFA_P70_ACT_MIRROR_MODE_EGRESS  = 3,
};
#define CFA_P70_ACT_MIRROR_COND_BITPOS 31
#define CFA_P70_ACT_MIRROR_COND_NUM_BITS 1
/**
 * COND
 */
enum cfa_p70_act_mirror_cond {
	/* mirror is only processed if Lookup copy bit is set */
	CFA_P70_ACT_MIRROR_COND_UNCONDITIONAL  = 0,
	/* mirror is processed unconditionally. */
	CFA_P70_ACT_MIRROR_COND_CONDITIONAL  = 1,
};

/**
 * Mirror Destination 1 Action Record Pointer.
 */
#define CFA_P70_ACT_MIRROR_AR_PTR_BITPOS 32
#define CFA_P70_ACT_MIRROR_AR_PTR_NUM_BITS 26

/**
 * Mirror Destination 1 Sampling Configuration.
 */
#define CFA_P70_ACT_MIRROR_SAMP_CFG_BITPOS 64
#define CFA_P70_ACT_MIRROR_SAMP_CFG_NUM_BITS 32

/**
 * Total number of bits for ACT_MIRROR
 */
#define CFA_P70_ACT_MIRROR_TOTAL_NUM_BITS 96

/**
 * This is the new medadata that is merged with the existing packet
 * metadata, based on the profile selected by META_PROF.
 */
#define CFA_P70_WC_LREC_METADATA_BITPOS 5
#define CFA_P70_WC_LREC_METADATA_NUM_BITS 32

/**
 * Specifies one of 8 metadata profile masks to use when merging the
 * input metadata with the LREC metadata for recycling.
 */
#define CFA_P70_WC_LREC_META_PROF_BITPOS 37
#define CFA_P70_WC_LREC_META_PROF_NUM_BITS 3

/**
 * When a packet is recycled to the Profile TCAM, this value is used as
 * the PROF_FUNC field in the TCAM search.
 */
#define CFA_P70_WC_LREC_PROF_FUNC_BITPOS 40
#define CFA_P70_WC_LREC_PROF_FUNC_NUM_BITS 8

/**
 * Indicates whether the packet will be recycled to the L2 Context TCAM,
 * the Profile TCAM. When to the Profile TCAM, PROF_FUNC is used for the
 * search key.
 */
#define CFA_P70_WC_LREC_RECYCLE_DEST_BITPOS 48
#define CFA_P70_WC_LREC_RECYCLE_DEST_NUM_BITS 1

/**
 * Flow counter pointer.
 */
#define CFA_P70_WC_LREC_FC_PTR_BITPOS 0
#define CFA_P70_WC_LREC_FC_PTR_NUM_BITS 28

/**
 * Flow counter type.
 */
#define CFA_P70_WC_LREC_FC_TYPE_BITPOS 28
#define CFA_P70_WC_LREC_FC_TYPE_NUM_BITS 2

/**
 * Flow counter op.
 */
#define CFA_P70_WC_LREC_FC_OP_BITPOS 30
#define CFA_P70_WC_LREC_FC_OP_NUM_BITS 1
/**
 * Enumeration definition for field 'fc_op'
 */
enum cfa_p70_wc_lrec_fc_op {
	/* ingress */
	CFA_P70_WC_LREC_FC_OP_INGRESS = 0,
	/* egress */
	CFA_P70_WC_LREC_FC_OP_EGRESS = 1,
};

/**
 * When not present, a value of 0 is used which disables ECMP. The final
 * action record location is: ! ACT_REC_PTR += (ECMP_HASH % PATHS_M1 +
 * 1)) * ACT_REC_SIZE
 */
#define CFA_P70_WC_LREC_PATHS_M1_BITPOS 31
#define CFA_P70_WC_LREC_PATHS_M1_NUM_BITS 4

/**
 * Specifies the size in 32B units of the action memory allocated for
 * each ECMP path.
 */
#define CFA_P70_WC_LREC_ACT_REC_SIZE_BITPOS 35
#define CFA_P70_WC_LREC_ACT_REC_SIZE_NUM_BITS 5

/**
 * This field is used in flow steering applications such as Linux RFS.
 * This field is used in conjunction with the VNIC destination in the
 * action record on RX to steer the packet to a specific ring.
 */
#define CFA_P70_WC_LREC_RING_TABLE_IDX_BITPOS 40
#define CFA_P70_WC_LREC_RING_TABLE_IDX_NUM_BITS 9

/**
 * This field provides a destination for the packet, which goes directly
 * to the output of CFA.
 */
#define CFA_P70_WC_LREC_DESTINATION_BITPOS 49
#define CFA_P70_WC_LREC_DESTINATION_NUM_BITS 17

/**
 * This is the action record pointer. This value points into the current
 * scope action table. Not that when ACT_REC_SIZE and PATHS_M1 are
 * preset and PATHS_M1 != 0, the value may be modified using this as the
 * base pointer for ECMP.
 */
#define CFA_P70_WC_LREC_ACT_REC_PTR_BITPOS 49
#define CFA_P70_WC_LREC_ACT_REC_PTR_NUM_BITS 26

/**
 * This value provides a hit of the action record size to the Action
 * block.
 */
#define CFA_P70_WC_LREC_ACT_HINT_BITPOS 75
#define CFA_P70_WC_LREC_ACT_HINT_NUM_BITS 2

/**
 * When both WC and EM have a hit, the one with the higher STRENGTH is
 * used. If the STRENGTHs are equal, the LKUP_TIE_BREAKER register bit
 * determines the winner. (0=WC, 1=EM)
 */
#define CFA_P70_WC_LREC_STRENGTH_BITPOS 77
#define CFA_P70_WC_LREC_STRENGTH_NUM_BITS 2

/**
 * This field defines the format for the LREC and the basic thing that
 * will be done with the packet.
 */
#define CFA_P70_WC_LREC_OPCODE_BITPOS 79
#define CFA_P70_WC_LREC_OPCODE_NUM_BITS 4
/**
 * Enumeration definition for field 'opcode'
 */
enum cfa_p70_wc_lrec_opcode {
	/*
	 * This value means the packet will go to the action block for edit
	 * processing and that no RFS will be specified for the packet.
	 */
	CFA_P70_WC_LREC_OPCODE_NORMAL = 0,
	/*
	 * This value means the packet will go to the action block for edit
	 * processing and that RFS will be specified for the packet.
	 */
	CFA_P70_WC_LREC_OPCODE_NORMAL_RFS = 1,
	/*
	 * This value means the packet will go directly to the output, bypassing
	 * the action block and that no RFS will be specified for the packet.
	 */
	CFA_P70_WC_LREC_OPCODE_FAST = 2,
	/*
	 * This value means the packet will go directly to the output, bypassing
	 * the action block and that RFS will be specified for the packet.
	 */
	CFA_P70_WC_LREC_OPCODE_FAST_RFS = 3,
	/*
	 * This value Recycles the packet to the Profiler and provides LREC
	 * fields that determine the fields returned to the Profiler for further
	 * processing.
	 */
	CFA_P70_WC_LREC_OPCODE_RECYCLE = 8,
};

/**
 * In addition to requiring VALID=1, the bits indexed by epoch1 must be
 * set to '1' in the EPOCH1_MASK table, or the LREC is invalid. This is
 * used to invalidate rules as a group.
 */
#define CFA_P70_WC_LREC_EPOCH1_BITPOS 83
#define CFA_P70_WC_LREC_EPOCH1_NUM_BITS 6

/**
 * In addition to requiring VALID=1, the bits indexed by epoch0 must be
 * set to '1' in the EPOCH0_MASK table, or the LREC is invalid. This is
 * used to invalidate rules as a group.
 */
#define CFA_P70_WC_LREC_EPOCH0_BITPOS 89
#define CFA_P70_WC_LREC_EPOCH0_NUM_BITS 12

/**
 * Record size in 32B words minus 1 (ignored by hardware).
 */
#define CFA_P70_WC_LREC_REC_SIZE_BITPOS 101
#define CFA_P70_WC_LREC_REC_SIZE_NUM_BITS 2

/**
 * When set to '0', the LREC is not valid.
 */
#define CFA_P70_WC_LREC_VALID_BITPOS 103
#define CFA_P70_WC_LREC_VALID_NUM_BITS 1

/**
 * Total number of bits for wc_lrec
 */
#define CFA_P70_WC_LREC_TOTAL_NUM_BITS 104

/**
 * This value provides a base pointer to the LKUP_FRC_RANGE memory. Each
 * packet can have up to 16 ranges. A value of 16'hFFFF disables FRC.
 */
#define CFA_P70_EM_LREC_RANGE_IDX_BITPOS 0
#define CFA_P70_EM_LREC_RANGE_IDX_NUM_BITS 16

/**
 * Selects one of 16 profiles for FRC in the LKUP_RANGE_PROFILE table,
 * which specifies 2 packet fields to range check and gives a mask of 16
 * ranges determined by range_index.
 */
#define CFA_P70_EM_LREC_RANGE_PROFILE_BITPOS 16
#define CFA_P70_EM_LREC_RANGE_PROFILE_NUM_BITS 4

/**
 * Current timer value for the connection.
 */
#define CFA_P70_EM_LREC_CREC_TIMER_VALUE_BITPOS 20
#define CFA_P70_EM_LREC_CREC_TIMER_VALUE_NUM_BITS 4

/**
 * Current state of the connection.
 */
#define CFA_P70_EM_LREC_CREC_STATE_BITPOS 24
#define CFA_P70_EM_LREC_CREC_STATE_NUM_BITS 5

/**
 * Set to one by hardware whenever a notify of a valid tcp_msb_opp has
 * been written into the connection record. Software can also initialize
 * this to one if it initializes tcp_msb_opp to a valid value.
 */
#define CFA_P70_EM_LREC_CREC_TCP_MSB_OPP_INIT_BITPOS 29
#define CFA_P70_EM_LREC_CREC_TCP_MSB_OPP_INIT_NUM_BITS 1

/**
 * Bits 31:14 of seq# or ack# as seen in packets on the opposite path.
 */
#define CFA_P70_EM_LREC_CREC_TCP_MSB_OPP_BITPOS 30
#define CFA_P70_EM_LREC_CREC_TCP_MSB_OPP_NUM_BITS 18

/**
 * Bits 31:14 of seq# or ack# as seen in packets on the local path.
 */
#define CFA_P70_EM_LREC_CREC_TCP_MSB_LOC_BITPOS 48
#define CFA_P70_EM_LREC_CREC_TCP_MSB_LOC_NUM_BITS 18

/**
 * Window size is 1<<TCP_WIN. A value of 0 disables window checks. Only
 * modified by SW.
 */
#define CFA_P70_EM_LREC_CREC_TCP_WIN_BITPOS 66
#define CFA_P70_EM_LREC_CREC_TCP_WIN_NUM_BITS 5

/**
 * Enables update of TCP_MSB_LOC when '1'. Only modified by SW.
 */
#define CFA_P70_EM_LREC_CREC_TCP_UPDT_EN_BITPOS 71
#define CFA_P70_EM_LREC_CREC_TCP_UPDT_EN_NUM_BITS 1

/**
 * Direction of tracked connection. Only modified by SW.
 */
#define CFA_P70_EM_LREC_CREC_TCP_DIR_BITPOS 72
#define CFA_P70_EM_LREC_CREC_TCP_DIR_NUM_BITS 1
/**
 * Enumeration definition for field 'crec_tcp_dir'
 */
enum cfa_p70_em_lrec_crec_tcp_dir {
	/* RX */
	CFA_P70_EM_LREC_CREC_TCP_DIR_RX = 0,
	/* TX */
	CFA_P70_EM_LREC_CREC_TCP_DIR_TX = 1,
};

/**
 * This is the new medadata that is merged with the existing packet
 * metadata, based on the profile selected by META_PROF.
 */
#define CFA_P70_EM_LREC_METADATA_BITPOS 29
#define CFA_P70_EM_LREC_METADATA_NUM_BITS 32

/**
 * When a packet is recycled to the Profile TCAM, this value is used as
 * the PROF_FUNC field in the TCAM search.
 */
#define CFA_P70_EM_LREC_PROF_FUNC_BITPOS 61
#define CFA_P70_EM_LREC_PROF_FUNC_NUM_BITS 8

/**
 * Specifies one of 8 metadata profile masks to use when merging the
 * input metadata with the LREC metadata for recycling.
 */
#define CFA_P70_EM_LREC_META_PROF_BITPOS 69
#define CFA_P70_EM_LREC_META_PROF_NUM_BITS 3

/**
 * Indicates whether the packet will be recycled to the L2 Context TCAM,
 * the Profile TCAM. When to the Profile TCAM, PROF_FUNC is used for the
 * search key.
 */
#define CFA_P70_EM_LREC_RECYCLE_DEST_BITPOS 72
#define CFA_P70_EM_LREC_RECYCLE_DEST_NUM_BITS 1

/**
 * Flow counter pointer.
 */
#define CFA_P70_EM_LREC_FC_PTR_BITPOS 24
#define CFA_P70_EM_LREC_FC_PTR_NUM_BITS 28

/**
 * Flow counter type.
 */
#define CFA_P70_EM_LREC_FC_TYPE_BITPOS 52
#define CFA_P70_EM_LREC_FC_TYPE_NUM_BITS 2

/**
 * Flow counter op.
 */
#define CFA_P70_EM_LREC_FC_OP_BITPOS 54
#define CFA_P70_EM_LREC_FC_OP_NUM_BITS 1
/**
 * Enumeration definition for field 'fc_op'
 */
enum cfa_p70_em_lrec_fc_op {
	/* ingress */
	CFA_P70_EM_LREC_FC_OP_INGRESS = 0,
	/* egress */
	CFA_P70_EM_LREC_FC_OP_EGRESS = 1,
};

/**
 * When not present, a value of 0 is used which disables ECMP. The final
 * action record location is: ! ACT_REC_PTR += (ECMP_HASH % PATHS_M1 +
 * 1)) * ACT_REC_SIZE
 */
#define CFA_P70_EM_LREC_PATHS_M1_BITPOS 55
#define CFA_P70_EM_LREC_PATHS_M1_NUM_BITS 4

/**
 * Specifies the size in 32B units of the action memory allocated for
 * each ECMP path.
 */
#define CFA_P70_EM_LREC_ACT_REC_SIZE_BITPOS 59
#define CFA_P70_EM_LREC_ACT_REC_SIZE_NUM_BITS 5

/**
 * This field is used in flow steering applications such as Linux RFS.
 * This field is used in conjunction with the VNIC destination in the
 * action record on RX to steer the packet to a specific ring.
 */
#define CFA_P70_EM_LREC_RING_TABLE_IDX_BITPOS 64
#define CFA_P70_EM_LREC_RING_TABLE_IDX_NUM_BITS 9

/**
 * This field provides a destination for the packet, which goes directly
 * to the output of CFA.
 */
#define CFA_P70_EM_LREC_DESTINATION_BITPOS 73
#define CFA_P70_EM_LREC_DESTINATION_NUM_BITS 17

/**
 * This is the action record pointer. This value points into the current
 * scope action table. Not that when ACT_REC_SIZE and PATHS_M1 are
 * preset and PATHS_M1 != 0, the value may be modified using this as the
 * base pointer for ECMP.
 */
#define CFA_P70_EM_LREC_ACT_REC_PTR_BITPOS 73
#define CFA_P70_EM_LREC_ACT_REC_PTR_NUM_BITS 26

/**
 * This value provides a hit of the action record size to the Action
 * block.
 */
#define CFA_P70_EM_LREC_ACT_HINT_BITPOS 99
#define CFA_P70_EM_LREC_ACT_HINT_NUM_BITS 2

/**
 * When both WC and EM have a hit, the one with the higher STRENGTH is
 * used. If the STRENGTHs are equal, the LKUP_TIE_BREAKER register bit
 * determines the winner. (0=WC, 1=EM)
 */
#define CFA_P70_EM_LREC_STRENGTH_BITPOS 101
#define CFA_P70_EM_LREC_STRENGTH_NUM_BITS 2

/**
 * This field defines the format for the LREC and the basic thing that
 * will be done with the packet.
 */
#define CFA_P70_EM_LREC_OPCODE_BITPOS 103
#define CFA_P70_EM_LREC_OPCODE_NUM_BITS 4
/**
 * Enumeration definition for field 'opcode'
 */
enum cfa_p70_em_lrec_opcode {
	/*
	 * This value means the packet will go to the action block for edit
	 * processing and that no RFS will be specified for the packet.
	 */
	CFA_P70_EM_LREC_OPCODE_NORMAL = 0,
	/*
	 * This value means the packet will go to the action block for edit
	 * processing and that RFS will be specified for the packet.
	 */
	CFA_P70_EM_LREC_OPCODE_NORMAL_RFS = 1,
	/*
	 * This value means the packet will go directly to the output, bypassing
	 * the action block and that no RFS will be specified for the packet.
	 */
	CFA_P70_EM_LREC_OPCODE_FAST = 2,
	/*
	 * This value means the packet will go directly to the output, bypassing
	 * the action block and that RFS will be specified for the packet.
	 */
	CFA_P70_EM_LREC_OPCODE_FAST_RFS = 3,
	/*
	 * This means the packet will go to the action block, but will have
	 * connection tracking affect the action, but no RFS. Connection
	 * tracking determines the ACTION, which is forward, miss, or copy. The
	 * default action record pointer is used when ACTION=miss.
	 */
	CFA_P70_EM_LREC_OPCODE_CT_MISS_DEF = 4,
	/*
	 * This means the packet will go to the action block, but will have
	 * connection tracking affect the action, but no RFS. Connection
	 * tracking determines the ACTION, which is forward, miss, or copy. The
	 * default action record pointer is used when ACTION=forward or
	 * ACTION=copy.
	 */
	CFA_P70_EM_LREC_OPCODE_CT_HIT_DEF = 6,
	/*
	 * This value Recycles the packet to the Profiler and provides LREC
	 * fields that determine the fields returned to the Profiler for further
	 * processing.
	 */
	CFA_P70_EM_LREC_OPCODE_RECYCLE = 8,
};

/**
 * In addition to requiring VALID=1, the bits indexed by epoch1 must be
 * set to '1' in the EPOCH1_MASK table, or the LREC is invalid. This is
 * used to invalidate rules as a group.
 */
#define CFA_P70_EM_LREC_EPOCH1_BITPOS 107
#define CFA_P70_EM_LREC_EPOCH1_NUM_BITS 6

/**
 * In addition to requiring VALID=1, the bits indexed by epoch0 must be
 * set to '1' in the EPOCH0_MASK table, or the LREC is invalid. This is
 * used to invalidate rules as a group.
 */
#define CFA_P70_EM_LREC_EPOCH0_BITPOS 113
#define CFA_P70_EM_LREC_EPOCH0_NUM_BITS 12

/**
 * Record size in 32B words minus 1 (ignored by hardware).
 */
#define CFA_P70_EM_LREC_REC_SIZE_BITPOS 125
#define CFA_P70_EM_LREC_REC_SIZE_NUM_BITS 2

/**
 * When set to '0', the LREC is not valid.
 */
#define CFA_P70_EM_LREC_VALID_BITPOS 127
#define CFA_P70_EM_LREC_VALID_NUM_BITS 1

/**
 * Total number of bits for em_lrec
 */
#define CFA_P70_EM_LREC_TOTAL_NUM_BITS 128

/**
 * This entry points to the entry associated with this bucket area. If
 * this value is zero, then the bucket area is not valid and should be
 * skipped.
 */
#define CFA_P70_EM_BUCKET_BIN0_ENTRY_BITPOS 0
#define CFA_P70_EM_BUCKET_BIN0_ENTRY_NUM_BITS 26

/**
 * This field holds the upper 12 bits of the 36b spooky hash of the key.
 * This part of the bucket area must match for the entry associated with
 * the bucket area to be read.
 */
#define CFA_P70_EM_BUCKET_BIN0_HASH_MSBS_BITPOS 26
#define CFA_P70_EM_BUCKET_BIN0_HASH_MSBS_NUM_BITS 12

/**
 * This entry points to the entry associated with this bucket area. If
 * this value is zero, then the bucket area is not valid and should be
 * skipped.
 */
#define CFA_P70_EM_BUCKET_BIN1_ENTRY_BITPOS 38
#define CFA_P70_EM_BUCKET_BIN1_ENTRY_NUM_BITS 26

/**
 * This field holds the upper 12 bits of the 36b spooky hash of the key.
 * This part of the bucket area must match for the entry associated with
 * the bucket area to be read.
 */
#define CFA_P70_EM_BUCKET_BIN1_HASH_MSBS_BITPOS 64
#define CFA_P70_EM_BUCKET_BIN1_HASH_MSBS_NUM_BITS 12

/**
 * This entry points to the entry associated with this bucket area. If
 * this value is zero, then the bucket area is not valid and should be
 * skipped.
 */
#define CFA_P70_EM_BUCKET_BIN2_ENTRY_BITPOS 76
#define CFA_P70_EM_BUCKET_BIN2_ENTRY_NUM_BITS 26

/**
 * This field holds the upper 12 bits of the 36b spooky hash of the key.
 * This part of the bucket area must match for the entry associated with
 * the bucket area to be read.
 */
#define CFA_P70_EM_BUCKET_BIN2_HASH_MSBS_BITPOS 102
#define CFA_P70_EM_BUCKET_BIN2_HASH_MSBS_NUM_BITS 12

/**
 * This entry points to the entry associated with this bucket area. If
 * this value is zero, then the bucket area is not valid and should be
 * skipped.
 */
#define CFA_P70_EM_BUCKET_BIN3_ENTRY_BITPOS 114
#define CFA_P70_EM_BUCKET_BIN3_ENTRY_NUM_BITS 26

/**
 * This field holds the upper 12 bits of the 36b spooky hash of the key.
 * This part of the bucket area must match for the entry associated with
 * the bucket area to be read.
 */
#define CFA_P70_EM_BUCKET_BIN3_HASH_MSBS_BITPOS 140
#define CFA_P70_EM_BUCKET_BIN3_HASH_MSBS_NUM_BITS 12

/**
 * This entry points to the entry associated with this bucket area. If
 * this value is zero, then the bucket area is not valid and should be
 * skipped.
 */
#define CFA_P70_EM_BUCKET_BIN4_ENTRY_BITPOS 152
#define CFA_P70_EM_BUCKET_BIN4_ENTRY_NUM_BITS 26

/**
 * This field holds the upper 12 bits of the 36b spooky hash of the key.
 * This part of the bucket area must match for the entry associated with
 * the bucket area to be read.
 */
#define CFA_P70_EM_BUCKET_BIN4_HASH_MSBS_BITPOS 178
#define CFA_P70_EM_BUCKET_BIN4_HASH_MSBS_NUM_BITS 12

/**
 * This entry points to the entry associated with this bucket area. If
 * this value is zero, then the bucket area is not valid and should be
 * skipped.
 */
#define CFA_P70_EM_BUCKET_BIN5_ENTRY_BITPOS 190
#define CFA_P70_EM_BUCKET_BIN5_ENTRY_NUM_BITS 26

/**
 * This field holds the upper 12 bits of the 36b spooky hash of the key.
 * This part of the bucket area must match for the entry associated with
 * the bucket area to be read.
 */
#define CFA_P70_EM_BUCKET_BIN5_HASH_MSBS_BITPOS 216
#define CFA_P70_EM_BUCKET_BIN5_HASH_MSBS_NUM_BITS 12

/**
 * This value points to the next bucket in the chain. When set to 0, the
 * next bucket visit for a background thread is to the starting bucket
 * for the thread.
 */
#define CFA_P70_EM_BUCKET_CHAIN_POINTER_BITPOS 228
#define CFA_P70_EM_BUCKET_CHAIN_POINTER_NUM_BITS 26

/**
 * If this value is '1', then the pointer value must be valid and will
 * be followed if a key match is not found in any bin in the current
 * bucket.
 */
#define CFA_P70_EM_BUCKET_CHAIN_VALID_BITPOS 254
#define CFA_P70_EM_BUCKET_CHAIN_VALID_NUM_BITS 1

/**
 * Total number of bits for em_bucket
 */
#define CFA_P70_EM_BUCKET_TOTAL_NUM_BITS 255

/**
 * The type field identifies the format of the action record to the
 * hardware.
 */
#define CFA_P70_COMPACT_ACTION_TYPE_BITPOS 0
#define CFA_P70_COMPACT_ACTION_TYPE_NUM_BITS 3
/**
 * Enumeration definition for field 'type'
 */
enum cfa_p70_compact_action_type {
	/*
	 * Compact Action Record. The compact action record uses relative
	 * pointers to access needed data. This keeps the compact action record
	 * down to 64b.
	 */
	CFA_P70_COMPACT_ACTION_TYPE_COMPACT_ACTION = 0,
};

/**
 * When this value is '1', the packet will be dropped.
 */
#define CFA_P70_COMPACT_ACTION_DROP_BITPOS 3
#define CFA_P70_COMPACT_ACTION_DROP_NUM_BITS 1

/**
 * This value controls how the VLAN Delete/Report edit works.
 */
#define CFA_P70_COMPACT_ACTION_VLAN_DELETE_BITPOS 4
#define CFA_P70_COMPACT_ACTION_VLAN_DELETE_NUM_BITS 2
/**
 * Enumeration definition for field 'vlan_delete'
 */
enum cfa_p70_compact_action_vlan_delete {
	/* The VLAN tag is left alone. */
	CFA_P70_COMPACT_ACTION_VLAN_DELETE_DISABLED = 0,
	/* Strip/Report the outer VLAN tag. Leave the inner VLAN tag. */
	CFA_P70_COMPACT_ACTION_VLAN_DELETE_OUTER = 1,
	/*
	 * Strip both the outer and inner VLAN tag. Report the inner VLAN tag.
	 */
	CFA_P70_COMPACT_ACTION_VLAN_DELETE_BOTH = 2,
	/*
	 * If the outer VID != 0, strip and pass the outer VLAG tag and leave
	 * the inner VLAN tag. If outer VID == 0, then strip both VLAN tags and
	 * report the inner VLAN tag.
	 */
	CFA_P70_COMPACT_ACTION_VLAN_DELETE_COND = 3,
};

/**
 * This value specifies the port destination mask for TX path and is the
 * index into the VNIC Properties Table for the RX path.
 */
#define CFA_P70_COMPACT_ACTION_DEST_BITPOS 6
#define CFA_P70_COMPACT_ACTION_DEST_NUM_BITS 7
#define CFA_P70_COMPACT_ACTION_DEST_OP_BITPOS 17
#define CFA_P70_COMPACT_ACTION_DEST_OP_NUM_BITS 2
/**
 * Enumeration definition for field 'dest_op'
 */
enum cfa_p70_compact_action_dest_op {
	/* Use the dest field from the Action Record. */
	CFA_P70_COMPACT_ACTION_DEST_OP_NORMAL = 0,
	/*
	 * This value specifies that the default destination as determined by
	 * the Profiler/Lookup/MCG stages and passed into the Action Record
	 * Fetch should be used instead of the destination from the Action
	 * Record. For example this can be useful for applications where actions
	 * are desired on a packet but the destination is to be taken solely
	 * from the Profiler Input Lookup Table.
	 */
	CFA_P70_COMPACT_ACTION_DEST_OP_DEFAULT = 1,
	/*
	 * This value specifies that the lower order bits of the metadata should
	 * be used instead of the destination from the Action Record.
	 */
	CFA_P70_COMPACT_ACTION_DEST_OP_METADATA = 2,
};

/**
 * This field controls the decapsulation function for the action.
 */
#define CFA_P70_COMPACT_ACTION_DECAP_BITPOS 19
#define CFA_P70_COMPACT_ACTION_DECAP_NUM_BITS 5
/**
 * Enumeration definition for field 'decap'
 */
enum cfa_p70_compact_action_decap {
	/* Do nothing. */
	CFA_P70_COMPACT_ACTION_DECAP_DISABLE = 0,
	/* Decap the outer VLAN tag */
	CFA_P70_COMPACT_ACTION_DECAP_OVLAN = 1,
	/* Decap all the VLAN tags */
	CFA_P70_COMPACT_ACTION_DECAP_ALL_VLAN = 2,
	/* Decap through Tunnel L2 header */
	CFA_P70_COMPACT_ACTION_DECAP_TO_TL2 = 3,
	/* Decap 1 MPLS label (does not delete outer L2) */
	CFA_P70_COMPACT_ACTION_DECAP_1MPLS = 4,
	/* Decap 1 MPLS label and outer L2 */
	CFA_P70_COMPACT_ACTION_DECAP_1MPLS_OL2 = 5,
	/* Decap 2 MPLS labels (does not delete outer L2) */
	CFA_P70_COMPACT_ACTION_DECAP_2MPLS = 6,
	/* Decap 2 MPLS labels and outer L2 */
	CFA_P70_COMPACT_ACTION_DECAP_2MPLS_OL2 = 7,
	/* Decap through Tunnel L3 header */
	CFA_P70_COMPACT_ACTION_DECAP_TO_TL3 = 8,
	/* Decap through Tunnel L4 header */
	CFA_P70_COMPACT_ACTION_DECAP_TO_TL4 = 9,
	/* Decap through Tunnel header */
	CFA_P70_COMPACT_ACTION_DECAP_TO_T = 10,
	/* Decap through Inner L2 */
	CFA_P70_COMPACT_ACTION_DECAP_TO_L2 = 11,
	/* Decap through Inner L3 */
	CFA_P70_COMPACT_ACTION_DECAP_TO_L3 = 12,
	/* Decap through inner L4 */
	CFA_P70_COMPACT_ACTION_DECAP_TO_L4 = 13,
	/* Shift tunnel->inner (single shift) */
	CFA_P70_COMPACT_ACTION_DECAP_SHIFT_SINGLE = 14,
	/* Un-parse (treat header as payload) */
	CFA_P70_COMPACT_ACTION_DECAP_UNPARSE = 15,
	/* Shift outer tunnel->inner (double shift) */
	CFA_P70_COMPACT_ACTION_DECAP_SHIFT_DOUBLE = 18,
	/* Decap through Outer Tunnel L2 header */
	CFA_P70_COMPACT_ACTION_DECAP_TO_OL2 = 20,
	/* Decap through Outer Tunnel L3 header */
	CFA_P70_COMPACT_ACTION_DECAP_TO_OL3 = 21,
	/* Decap through Outer Tunnel L4 header */
	CFA_P70_COMPACT_ACTION_DECAP_TO_OL4 = 22,
	/* Decap through Outer Tunnel header */
	CFA_P70_COMPACT_ACTION_DECAP_TO_OT = 23,
};

/**
 * The mirroring value selects one of 31 mirror destinations for the
 * packet. A value of zero means that there is not Action Record
 * mirroring for the packet.
 */
#define CFA_P70_COMPACT_ACTION_MIRRORING_BITPOS 24
#define CFA_P70_COMPACT_ACTION_MIRRORING_NUM_BITS 5

/**
 * This value points to one of the 1024 meter entries. If the meter has
 * scope verification enabled, then the scope in the meter table entry
 * must match the scope of this action record.
 */
#define CFA_P70_COMPACT_ACTION_METER_PTR_BITPOS 29
#define CFA_P70_COMPACT_ACTION_METER_PTR_NUM_BITS 10

/**
 * This is the offset to the statistic structure in 8B units from the
 * start of the Action Record. A value of zero will disable the
 * statistics action.
 */
#define CFA_P70_COMPACT_ACTION_STAT0_OFF_BITPOS 39
#define CFA_P70_COMPACT_ACTION_STAT0_OFF_NUM_BITS 3

/**
 * This value controls the packet size that is used for counted stats.
 */
#define CFA_P70_COMPACT_ACTION_STAT0_OP_BITPOS 42
#define CFA_P70_COMPACT_ACTION_STAT0_OP_NUM_BITS 1
/**
 * Enumeration definition for field 'stat0_op'
 */
enum cfa_p70_compact_action_stat0_op {
	/* Statistics count reflects packet at 'ingress' to CFA. */
	CFA_P70_COMPACT_ACTION_STAT0_OP_INGRESS = 0,
	/* Statistics count reflects packet at 'egress' from CFA. */
	CFA_P70_COMPACT_ACTION_STAT0_OP_EGRESS = 1,
};

/**
 * Selects counter type. In all cases, fields are packet little endian
 * in the action memory.
 */
#define CFA_P70_COMPACT_ACTION_STAT0_CTR_TYPE_BITPOS 43
#define CFA_P70_COMPACT_ACTION_STAT0_CTR_TYPE_NUM_BITS 2
/**
 * Enumeration definition for field 'stat0_ctr_type'
 */
enum cfa_p70_compact_action_stat0_ctr_type {
	/* Forward packet count(64b)/byte count(64b) */
	CFA_P70_COMPACT_ACTION_STAT0_CTR_TYPE_B16 = 0,
	/*
	 * Forward packet count(64b)/byte count(64b) timestamp(32b) TCP
	 * Flags(16b) reserved(23b)
	 */
	CFA_P70_COMPACT_ACTION_STAT0_CTR_TYPE_B24 = 1,
	/*
	 * Forward packet count(64b)/byte count(64b) Meter (drop or red) packet
	 * count(64b)/byte count(64b)
	 */
	CFA_P70_COMPACT_ACTION_STAT0_CTR_TYPE_B32A = 2,
	/*
	 * Forward packet count(64b)/byte count(64b) Meter timestamp(32b) TCP
	 * Flags(16b) reserved(6b) (drop or red) packet count(38b)/byte
	 * count(42b)
	 */
	CFA_P70_COMPACT_ACTION_STAT0_CTR_TYPE_B32B = 3,
};

/**
 * This is an offset to the modification record. This is the offset in
 * 8B units from the start of the Action Record to get to dependent
 * record data. A value of zero indicates no additional actions.
 */
#define CFA_P70_COMPACT_ACTION_MOD_OFF_BITPOS 45
#define CFA_P70_COMPACT_ACTION_MOD_OFF_NUM_BITS 5

/**
 * This is an offset to the encapsulation record. This is the offset in
 * 8B units from the start of the Action Record to get to dependent
 * record data. A value of zero indicates no additional actions.
 */
#define CFA_P70_COMPACT_ACTION_ENC_OFF_BITPOS 50
#define CFA_P70_COMPACT_ACTION_ENC_OFF_NUM_BITS 6

/**
 * This is an offset to the source record. This is the offset in 8B
 * units from the start of the Action Record to get to dependent record
 * data. A value of zero indicates no additional actions.
 */
#define CFA_P70_COMPACT_ACTION_SRC_OFF_BITPOS 56
#define CFA_P70_COMPACT_ACTION_SRC_OFF_NUM_BITS 4
#define CFA_P70_COMPACT_ACTION_UNUSED_0_BITPOS 60
#define CFA_P70_COMPACT_ACTION_UNUSED_0_NUM_BITS 4

/**
 * Total number of bits for compact_action
 */
#define CFA_P70_COMPACT_ACTION_TOTAL_NUM_BITS 64

/**
 * The type field identifies the format of the action record to the
 * hardware.
 */
#define CFA_P70_FULL_ACTION_TYPE_BITPOS 0
#define CFA_P70_FULL_ACTION_TYPE_NUM_BITS 3
/**
 * Enumeration definition for field 'type'
 */
enum cfa_p70_full_action_type {
	/*
	 * Full Action Record. The full action record uses full pointers to
	 * access needed data. It also allows access to all the action features.
	 * The Full Action record is 192b.
	 */
	CFA_P70_FULL_ACTION_TYPE_FULL_ACTION = 1,
};

/**
 * When this value is '1', the packet will be dropped.
 */
#define CFA_P70_FULL_ACTION_DROP_BITPOS 3
#define CFA_P70_FULL_ACTION_DROP_NUM_BITS 1

/**
 * This value controls how the VLAN Delete/Report edit works.
 */
#define CFA_P70_FULL_ACTION_VLAN_DELETE_BITPOS 4
#define CFA_P70_FULL_ACTION_VLAN_DELETE_NUM_BITS 2
/**
 * Enumeration definition for field 'vlan_delete'
 */
enum cfa_p70_full_action_vlan_delete {
	/* The VLAN tag is left alone. */
	CFA_P70_FULL_ACTION_VLAN_DELETE_DISABLED = 0,
	/* Strip/Report the outer VLAN tag. Leave the inner VLAN tag. */
	CFA_P70_FULL_ACTION_VLAN_DELETE_OUTER = 1,
	/*
	 * Strip both the outer and inner VLAN tag. Report the inner VLAN tag.
	 */
	CFA_P70_FULL_ACTION_VLAN_DELETE_BOTH = 2,
	/*
	 * If the outer VID != 0, strip and pass the outer VLAG tag and leave
	 * the inner VLAN tag. If outer VID == 0, then strip both VLAN tags and
	 * report the inner VLAN tag.
	 */
	CFA_P70_FULL_ACTION_VLAN_DELETE_COND = 3,
};

/**
 * This value specifies the port destination mask for TX path and is the
 * index into the VNIC Properties Table for the RX path.
 */
#define CFA_P70_FULL_ACTION_DEST_BITPOS 6
#define CFA_P70_FULL_ACTION_DEST_NUM_BITS 7
#define CFA_P70_FULL_ACTION_DEST_OP_BITPOS 17
#define CFA_P70_FULL_ACTION_DEST_OP_NUM_BITS 2
/**
 * Enumeration definition for field 'dest_op'
 */
enum cfa_p70_full_action_dest_op {
	/* Use the dest field from the Action Record. */
	CFA_P70_FULL_ACTION_DEST_OP_NORMAL = 0,
	/*
	 * This value specifies that the default destination as determined by
	 * the Profiler/Lookup/MCG stages and passed into the Action Record
	 * Fetch should be used instead of the destination from the Action
	 * Record. For example this can be useful for applications where actions
	 * are desired on a packet but the destination is to be taken solely
	 * from the Profiler Input Lookup Table.
	 */
	CFA_P70_FULL_ACTION_DEST_OP_DEFAULT = 1,
	/*
	 * This value specifies that the lower order bits of the metadata should
	 * be used instead of the destination from the Action Record.
	 */
	CFA_P70_FULL_ACTION_DEST_OP_METADATA = 2,
};

/**
 * This field controls the decapsulation function for the action.
 */
#define CFA_P70_FULL_ACTION_DECAP_BITPOS 19
#define CFA_P70_FULL_ACTION_DECAP_NUM_BITS 5
/**
 * Enumeration definition for field 'decap'
 */
enum cfa_p70_full_action_decap {
	/* Do nothing. */
	CFA_P70_FULL_ACTION_DECAP_DISABLE = 0,
	/* Decap the outer VLAN tag */
	CFA_P70_FULL_ACTION_DECAP_OVLAN = 1,
	/* Decap all the VLAN tags */
	CFA_P70_FULL_ACTION_DECAP_ALL_VLAN = 2,
	/* Decap through Tunnel L2 header */
	CFA_P70_FULL_ACTION_DECAP_TO_TL2 = 3,
	/* Decap 1 MPLS label (does not delete outer L2) */
	CFA_P70_FULL_ACTION_DECAP_1MPLS = 4,
	/* Decap 1 MPLS label and outer L2 */
	CFA_P70_FULL_ACTION_DECAP_1MPLS_OL2 = 5,
	/* Decap 2 MPLS labels (does not delete outer L2) */
	CFA_P70_FULL_ACTION_DECAP_2MPLS = 6,
	/* Decap 2 MPLS labels and outer L2 */
	CFA_P70_FULL_ACTION_DECAP_2MPLS_OL2 = 7,
	/* Decap through Tunnel L3 header */
	CFA_P70_FULL_ACTION_DECAP_TO_TL3 = 8,
	/* Decap through Tunnel L4 header */
	CFA_P70_FULL_ACTION_DECAP_TO_TL4 = 9,
	/* Decap through Tunnel header */
	CFA_P70_FULL_ACTION_DECAP_TO_T = 10,
	/* Decap through Inner L2 */
	CFA_P70_FULL_ACTION_DECAP_TO_L2 = 11,
	/* Decap through Inner L3 */
	CFA_P70_FULL_ACTION_DECAP_TO_L3 = 12,
	/* Decap through inner L4 */
	CFA_P70_FULL_ACTION_DECAP_TO_L4 = 13,
	/* Shift tunnel->inner (single shift) */
	CFA_P70_FULL_ACTION_DECAP_SHIFT_SINGLE = 14,
	/* Un-parse (treat header as payload) */
	CFA_P70_FULL_ACTION_DECAP_UNPARSE = 15,
	/* Shift outer tunnel->inner (double shift) */
	CFA_P70_FULL_ACTION_DECAP_SHIFT_DOUBLE = 18,
	/* Decap through Outer Tunnel L2 header */
	CFA_P70_FULL_ACTION_DECAP_TO_OL2 = 20,
	/* Decap through Outer Tunnel L3 header */
	CFA_P70_FULL_ACTION_DECAP_TO_OL3 = 21,
	/* Decap through Outer Tunnel L4 header */
	CFA_P70_FULL_ACTION_DECAP_TO_OL4 = 22,
	/* Decap through Outer Tunnel header */
	CFA_P70_FULL_ACTION_DECAP_TO_OT = 23,
};

/**
 * The mirroring value selects one of 31 mirror destinations for the
 * packet. A value of zero means that there is not Action Record
 * mirroring for the packet.
 */
#define CFA_P70_FULL_ACTION_MIRRORING_BITPOS 24
#define CFA_P70_FULL_ACTION_MIRRORING_NUM_BITS 5

/**
 * This value points to one of the 1024 meter entries. If the meter has
 * scope verification enabled, then the scope in the meter table entry
 * must match the scope of this action record.
 */
#define CFA_P70_FULL_ACTION_METER_PTR_BITPOS 29
#define CFA_P70_FULL_ACTION_METER_PTR_NUM_BITS 10

/**
 * This is the pointer to the statistic structure in 8B units A value of
 * zero will disable the statistics action.
 */
#define CFA_P70_FULL_ACTION_STAT0_PTR_BITPOS 39
#define CFA_P70_FULL_ACTION_STAT0_PTR_NUM_BITS 28

/**
 * This value controls the packet size that is used for counted stats.
 */
#define CFA_P70_FULL_ACTION_STAT0_OP_BITPOS 67
#define CFA_P70_FULL_ACTION_STAT0_OP_NUM_BITS 1
/**
 * Enumeration definition for field 'stat0_op'
 */
enum cfa_p70_full_action_stat0_op {
	/* Statistics count reflects packet at 'ingress' to CFA. */
	CFA_P70_FULL_ACTION_STAT0_OP_INGRESS = 0,
	/* Statistics count reflects packet at 'egress' from CFA. */
	CFA_P70_FULL_ACTION_STAT0_OP_EGRESS = 1,
};

/**
 * Selects counter type. In all cases, fields are packet little endian
 * in the action memory.
 */
#define CFA_P70_FULL_ACTION_STAT0_CTR_TYPE_BITPOS 68
#define CFA_P70_FULL_ACTION_STAT0_CTR_TYPE_NUM_BITS 2
/**
 * Enumeration definition for field 'stat0_ctr_type'
 */
enum cfa_p70_full_action_stat0_ctr_type {
	/* Forward packet count(64b)/byte count(64b) */
	CFA_P70_FULL_ACTION_STAT0_CTR_TYPE_B16 = 0,
	/*
	 * Forward packet count(64b)/byte count(64b) timestamp(32b) TCP
	 * Flags(16b) reserved(23b)
	 */
	CFA_P70_FULL_ACTION_STAT0_CTR_TYPE_B24 = 1,
	/*
	 * Forward packet count(64b)/byte count(64b) Meter (drop or red) packet
	 * count(64b)/byte count(64b)
	 */
	CFA_P70_FULL_ACTION_STAT0_CTR_TYPE_B32A = 2,
	/*
	 * Forward packet count(64b)/byte count(64b) Meter timestamp(32b) TCP
	 * Flags(16b) reserved(6b) (drop or red) packet count(38b)/byte
	 * count(42b)
	 */
	CFA_P70_FULL_ACTION_STAT0_CTR_TYPE_B32B = 3,
};

/**
 * This is the pointer to the statistic structure in 8B units A value of
 * zero will disable the statistics action.
 */
#define CFA_P70_FULL_ACTION_STAT1_PTR_BITPOS 70
#define CFA_P70_FULL_ACTION_STAT1_PTR_NUM_BITS 28

/**
 * This value controls the packet size that is used for counted stats.
 */
#define CFA_P70_FULL_ACTION_STAT1_OP_BITPOS 98
#define CFA_P70_FULL_ACTION_STAT1_OP_NUM_BITS 1
/**
 * Enumeration definition for field 'stat1_op'
 */
enum cfa_p70_full_action_stat1_op {
	/* Statistics count reflects packet at 'ingress' to CFA. */
	CFA_P70_FULL_ACTION_STAT1_OP_INGRESS = 0,
	/* Statistics count reflects packet at 'egress' from CFA. */
	CFA_P70_FULL_ACTION_STAT1_OP_EGRESS = 1,
};

/**
 * Selects counter type. In all cases, fields are packet little endian
 * in the action memory.
 */
#define CFA_P70_FULL_ACTION_STAT1_CTR_TYPE_BITPOS 99
#define CFA_P70_FULL_ACTION_STAT1_CTR_TYPE_NUM_BITS 2
/**
 * Enumeration definition for field 'stat1_ctr_type'
 */
enum cfa_p70_full_action_stat1_ctr_type {
	/* Forward packet count(64b)/byte count(64b) */
	CFA_P70_FULL_ACTION_STAT1_CTR_TYPE_B16 = 0,
	/*
	 * Forward packet count(64b)/byte count(64b) timestamp(32b) TCP
	 * Flags(16b) reserved(23b)
	 */
	CFA_P70_FULL_ACTION_STAT1_CTR_TYPE_B24 = 1,
	/*
	 * Forward packet count(64b)/byte count(64b) Meter (drop or red) packet
	 * count(64b)/byte count(64b)
	 */
	CFA_P70_FULL_ACTION_STAT1_CTR_TYPE_B32A = 2,
	/*
	 * Forward packet count(64b)/byte count(64b) Meter timestamp(32b) TCP
	 * Flags(16b) reserved(6b) (drop or red) packet count(38b)/byte
	 * count(42b)
	 */
	CFA_P70_FULL_ACTION_STAT1_CTR_TYPE_B32B = 3,
};

/**
 * This is a pointer to the modification record. This is a pointer in 8B
 * units directly to dependent record data. A value of zero indicates no
 * additional actions.
 */
#define CFA_P70_FULL_ACTION_MOD_PTR_BITPOS 101
#define CFA_P70_FULL_ACTION_MOD_PTR_NUM_BITS 28

/**
 * This is a pointer to the encapsulation record. This is a pointer in
 * 8B units directly to dependent record data. A value of zero indicates
 * no additional actions.
 */
#define CFA_P70_FULL_ACTION_ENC_PTR_BITPOS 129
#define CFA_P70_FULL_ACTION_ENC_PTR_NUM_BITS 28

/**
 * This is a pointer to the source record. This is a pointer in 8B units
 * directly to dependent record data. A value of zero indicates no
 * additional actions.
 */
#define CFA_P70_FULL_ACTION_SRC_PTR_BITPOS 157
#define CFA_P70_FULL_ACTION_SRC_PTR_NUM_BITS 28
#define CFA_P70_FULL_ACTION_UNUSED_0_BITPOS 185
#define CFA_P70_FULL_ACTION_UNUSED_0_NUM_BITS 7

/**
 * Total number of bits for full_action
 */
#define CFA_P70_FULL_ACTION_TOTAL_NUM_BITS 192

/**
 * The type field identifies the format of the action record to the
 * hardware.
 */
#define CFA_P70_MCG_ACTION_TYPE_BITPOS 0
#define CFA_P70_MCG_ACTION_TYPE_NUM_BITS 3
/**
 * Enumeration definition for field 'type'
 */
enum cfa_p70_mcg_action_type {
	/*
	 * Multicast Group Action Record. This action is used to send the packet
	 * to multiple destinations. The MGC Action record is 256b.
	 */
	CFA_P70_MCG_ACTION_TYPE_MCG_ACTION = 4,
};

/**
 * When this bit is set to '1', source knockout will be supported for
 * the MCG record. This value also applies to any chained subsequent MCG
 * records. This is applied on the RX CFA only.
 */
#define CFA_P70_MCG_ACTION_SRC_KO_EN_BITPOS 3
#define CFA_P70_MCG_ACTION_SRC_KO_EN_NUM_BITS 1
#define CFA_P70_MCG_ACTION_UNUSED_0_BITPOS 4
#define CFA_P70_MCG_ACTION_UNUSED_0_NUM_BITS 2

/**
 * This is a pointer to the next MGC Subsequent Entries Record. The
 * Subsequent Entries MGC record must be on a 32B boundary. A value of
 * zero indicates that there are not additional MGC Subsequent Entries
 * record.
 */
#define CFA_P70_MCG_ACTION_NEXT_PTR_BITPOS 6
#define CFA_P70_MCG_ACTION_NEXT_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_ACTION_PTR0_ACT_HINT_BITPOS 32
#define CFA_P70_MCG_ACTION_PTR0_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_ACTION_PTR0_ACT_REC_PTR_BITPOS 34
#define CFA_P70_MCG_ACTION_PTR0_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_ACTION_PTR1_ACT_HINT_BITPOS 60
#define CFA_P70_MCG_ACTION_PTR1_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_ACTION_PTR1_ACT_REC_PTR_BITPOS 62
#define CFA_P70_MCG_ACTION_PTR1_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_ACTION_PTR2_ACT_HINT_BITPOS 88
#define CFA_P70_MCG_ACTION_PTR2_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_ACTION_PTR2_ACT_REC_PTR_BITPOS 90
#define CFA_P70_MCG_ACTION_PTR2_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_ACTION_PTR3_ACT_HINT_BITPOS 116
#define CFA_P70_MCG_ACTION_PTR3_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_ACTION_PTR3_ACT_REC_PTR_BITPOS 118
#define CFA_P70_MCG_ACTION_PTR3_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_ACTION_PTR4_ACT_HINT_BITPOS 144
#define CFA_P70_MCG_ACTION_PTR4_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_ACTION_PTR4_ACT_REC_PTR_BITPOS 146
#define CFA_P70_MCG_ACTION_PTR4_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_ACTION_PTR5_ACT_HINT_BITPOS 172
#define CFA_P70_MCG_ACTION_PTR5_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_ACTION_PTR5_ACT_REC_PTR_BITPOS 174
#define CFA_P70_MCG_ACTION_PTR5_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_ACTION_PTR6_ACT_HINT_BITPOS 200
#define CFA_P70_MCG_ACTION_PTR6_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_ACTION_PTR6_ACT_REC_PTR_BITPOS 202
#define CFA_P70_MCG_ACTION_PTR6_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_ACTION_PTR7_ACT_HINT_BITPOS 228
#define CFA_P70_MCG_ACTION_PTR7_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_ACTION_PTR7_ACT_REC_PTR_BITPOS 230
#define CFA_P70_MCG_ACTION_PTR7_ACT_REC_PTR_NUM_BITS 26

/**
 * Total number of bits for mcg_action
 */
#define CFA_P70_MCG_ACTION_TOTAL_NUM_BITS 256

/**
 * The type field identifies the format of the action record to the
 * hardware.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_TYPE_BITPOS 0
#define CFA_P70_MCG_SUBSEQ_ACTION_TYPE_NUM_BITS 3
/**
 * Enumeration definition for field 'type'
 */
enum cfa_p70_mcg_subseq_action_type {
	/*
	 * Multicast Group Action Record. This action is used to send the packet
	 * to multiple destinations. The MGC Action record is 256b.
	 */
	CFA_P70_MCG_SUBSEQ_ACTION_TYPE_MCG_ACTION = 4,
};
#define CFA_P70_MCG_SUBSEQ_ACTION_UNUSED_0_BITPOS 3
#define CFA_P70_MCG_SUBSEQ_ACTION_UNUSED_0_NUM_BITS 3

/**
 * This is a pointer to the next MGC Subsequent Entries Record. The
 * Subsequent Entries MGC record must be on a 32B boundary. A value of
 * zero indicates that there are not additional MGC Subsequent Entries
 * record.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_NEXT_PTR_BITPOS 6
#define CFA_P70_MCG_SUBSEQ_ACTION_NEXT_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR0_ACT_HINT_BITPOS 32
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR0_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR0_ACT_REC_PTR_BITPOS 34
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR0_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR1_ACT_HINT_BITPOS 60
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR1_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR1_ACT_REC_PTR_BITPOS 62
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR1_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR2_ACT_HINT_BITPOS 88
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR2_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR2_ACT_REC_PTR_BITPOS 90
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR2_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR3_ACT_HINT_BITPOS 116
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR3_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR3_ACT_REC_PTR_BITPOS 118
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR3_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR4_ACT_HINT_BITPOS 144
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR4_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR4_ACT_REC_PTR_BITPOS 146
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR4_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR5_ACT_HINT_BITPOS 172
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR5_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR5_ACT_REC_PTR_BITPOS 174
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR5_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR6_ACT_HINT_BITPOS 200
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR6_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR6_ACT_REC_PTR_BITPOS 202
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR6_ACT_REC_PTR_NUM_BITS 26

/**
 * This is the prefetch hint that corresponds to this action record
 * pointer. This value will index into the hint table for the current
 * scope to determines the actual prefetch size.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR7_ACT_HINT_BITPOS 228
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR7_ACT_HINT_NUM_BITS 2

/**
 * This is an individual action record pointer for an MGC entry. This
 * points to a action record for this particular MGC member. If this
 * pointer is zero, then it will not be followed.
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR7_ACT_REC_PTR_BITPOS 230
#define CFA_P70_MCG_SUBSEQ_ACTION_PTR7_ACT_REC_PTR_NUM_BITS 26

/**
 * Total number of bits for mcg_subseq_action
 */
#define CFA_P70_MCG_SUBSEQ_ACTION_TOTAL_NUM_BITS 256

/**
 * Current committed token bucket count.
 */
#define CFA_P70_METERS_BKT_C_BITPOS 0
#define CFA_P70_METERS_BKT_C_NUM_BITS 27

/**
 * Current excess token bucket count.
 */
#define CFA_P70_METERS_BKT_E_BITPOS 27
#define CFA_P70_METERS_BKT_E_NUM_BITS 27

/**
 * Meter Valid
 */
#define CFA_P70_METERS_FLAGS_MTR_VAL_BITPOS 54
#define CFA_P70_METERS_FLAGS_MTR_VAL_NUM_BITS 1

/**
 * ECN Remap Enable
 */
#define CFA_P70_METERS_FLAGS_ECN_RMP_EN_BITPOS 55
#define CFA_P70_METERS_FLAGS_ECN_RMP_EN_NUM_BITS 1

/**
 * Coupling Flag. Indicates that tokens being added to the committed
 * bucket should be diverted to the excess bucket when the committed
 * bucket is full. This bit is ignored when RFC2698=1
 */
#define CFA_P70_METERS_FLAGS_CF_BITPOS 56
#define CFA_P70_METERS_FLAGS_CF_NUM_BITS 1

/**
 * Packet Mode. When set packet length is ignored and a global value is
 * used instead.
 */
#define CFA_P70_METERS_FLAGS_PM_BITPOS 57
#define CFA_P70_METERS_FLAGS_PM_NUM_BITS 1

/**
 * RFC2698 Enable - Indicates if BOTH buckets must have sufficient
 * tokens to color a packet green per RFC2698, as opposed to just the
 * committed bucket.
 */
#define CFA_P70_METERS_FLAGS_RFC2698_BITPOS 58
#define CFA_P70_METERS_FLAGS_RFC2698_NUM_BITS 1

/**
 * Committed Bucket Strict Mode. If set, a packet conforms to the
 * committed bucket only if the number of tokens is greater than or
 * equal to the packet length. When not set meter conformance is
 * independent of packet size and requires only that the token count is
 * non-negative.
 */
#define CFA_P70_METERS_FLAGS_CBSM_BITPOS 59
#define CFA_P70_METERS_FLAGS_CBSM_NUM_BITS 1

/**
 * Excess Bucket Strict Mode. If set, a packet conforms to the excess
 * bucket only if the number of tokens is greater than or equal to the
 * packet length. When not set, meter conformance is independent of
 * packet size and requires only that the token count is non-negative.
 */
#define CFA_P70_METERS_FLAGS_EBSM_BITPOS 60
#define CFA_P70_METERS_FLAGS_EBSM_NUM_BITS 1

/**
 * Committed Bucket No Decrement. If set, tokens are never decremented
 * from the committed bucket, even when the packet is Green.
 */
#define CFA_P70_METERS_FLAGS_CBND_BITPOS 61
#define CFA_P70_METERS_FLAGS_CBND_NUM_BITS 1

/**
 * Excess Bucket No Decrement. If set, tokens are never decremented from
 * the excess bucket, even when the packet is Green.
 */
#define CFA_P70_METERS_FLAGS_EBND_BITPOS 62
#define CFA_P70_METERS_FLAGS_EBND_NUM_BITS 1

/**
 * Committed Burst Size. Expressed in bytes in a normalized floating
 * point format.
 */
#define CFA_P70_METERS_CBS_BITPOS 63
#define CFA_P70_METERS_CBS_NUM_BITS 12

/**
 * Excess Burst Size. Expressed in bytes in a normalized floating point
 * format.
 */
#define CFA_P70_METERS_EBS_BITPOS 75
#define CFA_P70_METERS_EBS_NUM_BITS 12

/**
 * Committed Information Rate. A rate expressed in bytes per clock cycle
 * in a normalized floating point format.
 */
#define CFA_P70_METERS_CIR_BITPOS 87
#define CFA_P70_METERS_CIR_NUM_BITS 17

/**
 * Excess Information Rate. A rate expressed in bytes per clock cycle in
 * a normalized floating point format.
 */
#define CFA_P70_METERS_EIR_BITPOS 104
#define CFA_P70_METERS_EIR_NUM_BITS 17

/**
 * This is the scope whose action records will be allowed to reference
 * this meter if the enable bit is '1'.
 */
#define CFA_P70_METERS_PROTECTION_SCOPE_BITPOS 121
#define CFA_P70_METERS_PROTECTION_SCOPE_NUM_BITS 5

/**
 * Reserved.
 */
#define CFA_P70_METERS_PROTECTION_RSVD_BITPOS 126
#define CFA_P70_METERS_PROTECTION_RSVD_NUM_BITS 1

/**
 * When this bit is '1', the meter will be protected from any scope
 * action other than the one in the scope field.
 */
#define CFA_P70_METERS_PROTECTION_ENABLE_BITPOS 127
#define CFA_P70_METERS_PROTECTION_ENABLE_NUM_BITS 1

/**
 * Total number of bits for meters
 */
#define CFA_P70_METERS_TOTAL_NUM_BITS 128

/**
 * Field length definitions for fkb
 */
#define CFA_P70_FKB_PROF_ID_NUM_BITS 8
#define CFA_P70_FKB_L2CTXT_NUM_BITS 11
#define CFA_P70_FKB_L2FUNC_NUM_BITS 8
#define CFA_P70_FKB_PARIF_NUM_BITS 2
#define CFA_P70_FKB_SPIF_NUM_BITS 2
#define CFA_P70_FKB_SVIF_NUM_BITS 6
#define CFA_P70_FKB_LCOS_NUM_BITS 3
#define CFA_P70_FKB_META_HI_NUM_BITS 16
#define CFA_P70_FKB_META_LO_NUM_BITS 16
#define CFA_P70_FKB_RCYC_CNT_NUM_BITS 4
#define CFA_P70_FKB_LOOPBACK_NUM_BITS 1
#define CFA_P70_FKB_OTL2_TYPE_NUM_BITS 2
#define CFA_P70_FKB_OTL2_DMAC_NUM_BITS 48
#define CFA_P70_FKB_OTL2_SMAC_NUM_BITS 48
#define CFA_P70_FKB_OTL2_DT_NUM_BITS 2
#define CFA_P70_FKB_OTL2_SA_NUM_BITS 1
#define CFA_P70_FKB_OTL2_NVT_NUM_BITS 2
#define CFA_P70_FKB_OTL2_OVP_NUM_BITS 3
#define CFA_P70_FKB_OTL2_OVD_NUM_BITS 1
#define CFA_P70_FKB_OTL2_OVV_NUM_BITS 12
#define CFA_P70_FKB_OTL2_OVT_NUM_BITS 3
#define CFA_P70_FKB_OTL2_IVP_NUM_BITS 3
#define CFA_P70_FKB_OTL2_IVD_NUM_BITS 1
#define CFA_P70_FKB_OTL2_IVV_NUM_BITS 12
#define CFA_P70_FKB_OTL2_IVT_NUM_BITS 3
#define CFA_P70_FKB_OTL2_ETYPE_NUM_BITS 16
#define CFA_P70_FKB_OTL3_TYPE_NUM_BITS 4
#define CFA_P70_FKB_OTL3_SIP3_NUM_BITS 32
#define CFA_P70_FKB_OTL3_SIP2_NUM_BITS 32
#define CFA_P70_FKB_OTL3_SIP1_NUM_BITS 32
#define CFA_P70_FKB_OTL3_SIP0_NUM_BITS 32
#define CFA_P70_FKB_OTL3_DIP3_NUM_BITS 32
#define CFA_P70_FKB_OTL3_DIP2_NUM_BITS 32
#define CFA_P70_FKB_OTL3_DIP1_NUM_BITS 32
#define CFA_P70_FKB_OTL3_DIP0_NUM_BITS 32
#define CFA_P70_FKB_OTL3_TTL_NUM_BITS 8
#define CFA_P70_FKB_OTL3_PROT_NUM_BITS 8
/**
 * CFA_P70_FKB_OTL3_FID bit length is not fixed
 * So the CFA_P70_FKB_OTL3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_FKB_OTL3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_FKB_OTL3_QOS_NUM_BITS 8
#define CFA_P70_FKB_OTL3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_FKB_OTL3_IEH_SEP_NUM_BITS 1
#define CFA_P70_FKB_OTL3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_FKB_OTL3_IEH_DEST_NUM_BITS 1
#define CFA_P70_FKB_OTL3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_FKB_OTL3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_FKB_OTL3_IEH_HOP_NUM_BITS 1
#define CFA_P70_FKB_OTL3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_FKB_OTL3_DF_NUM_BITS 1
#define CFA_P70_FKB_OTL3_L3ERR_NUM_BITS 4
#define CFA_P70_FKB_OTL4_TYPE_NUM_BITS 4
#define CFA_P70_FKB_OTL4_SRC_NUM_BITS 16
#define CFA_P70_FKB_OTL4_DST_NUM_BITS 16
#define CFA_P70_FKB_OTL4_FLAGS_NUM_BITS 9
#define CFA_P70_FKB_OTL4_SEQ_NUM_BITS 32
#define CFA_P70_FKB_OTL4_PA_NUM_BITS 1
#define CFA_P70_FKB_OTL4_OPT_NUM_BITS 1
#define CFA_P70_FKB_OTL4_TCPTS_NUM_BITS 1
#define CFA_P70_FKB_OTL4_ERR_NUM_BITS 4
#define CFA_P70_FKB_OT_TYPE_NUM_BITS 5
#define CFA_P70_FKB_OT_FLAGS_NUM_BITS 8
#define CFA_P70_FKB_OT_IDS_NUM_BITS 24
#define CFA_P70_FKB_OT_ID_NUM_BITS 32
#define CFA_P70_FKB_OT_CTXTS_NUM_BITS 24
#define CFA_P70_FKB_OT_CTXT_NUM_BITS 32
#define CFA_P70_FKB_OT_QOS_NUM_BITS 3
#define CFA_P70_FKB_OT_ERR_NUM_BITS 4
#define CFA_P70_FKB_TL2_TYPE_NUM_BITS 2
#define CFA_P70_FKB_TL2_DMAC_NUM_BITS 48
#define CFA_P70_FKB_TL2_SMAC_NUM_BITS 48
#define CFA_P70_FKB_TL2_DT_NUM_BITS 2
#define CFA_P70_FKB_TL2_SA_NUM_BITS 1
#define CFA_P70_FKB_TL2_NVT_NUM_BITS 2
#define CFA_P70_FKB_TL2_OVP_NUM_BITS 3
#define CFA_P70_FKB_TL2_OVD_NUM_BITS 1
#define CFA_P70_FKB_TL2_OVV_NUM_BITS 12
#define CFA_P70_FKB_TL2_OVT_NUM_BITS 3
#define CFA_P70_FKB_TL2_IVP_NUM_BITS 3
#define CFA_P70_FKB_TL2_IVD_NUM_BITS 1
#define CFA_P70_FKB_TL2_IVV_NUM_BITS 12
#define CFA_P70_FKB_TL2_IVT_NUM_BITS 3
#define CFA_P70_FKB_TL2_ETYPE_NUM_BITS 16
#define CFA_P70_FKB_TL3_TYPE_NUM_BITS 4
#define CFA_P70_FKB_TL3_SIP3_NUM_BITS 32
#define CFA_P70_FKB_TL3_SIP2_NUM_BITS 32
#define CFA_P70_FKB_TL3_SIP1_NUM_BITS 32
#define CFA_P70_FKB_TL3_SIP0_NUM_BITS 32
#define CFA_P70_FKB_TL3_DIP3_NUM_BITS 32
#define CFA_P70_FKB_TL3_DIP2_NUM_BITS 32
#define CFA_P70_FKB_TL3_DIP1_NUM_BITS 32
#define CFA_P70_FKB_TL3_DIP0_NUM_BITS 32
#define CFA_P70_FKB_TL3_TTL_NUM_BITS 8
#define CFA_P70_FKB_TL3_PROT_NUM_BITS 8
/**
 * CFA_P70_FKB_TL3_FID bit length is not fixed
 * So the CFA_P70_FKB_TL3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_FKB_TL3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_FKB_TL3_QOS_NUM_BITS 8
#define CFA_P70_FKB_TL3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_FKB_TL3_IEH_SEP_NUM_BITS 1
#define CFA_P70_FKB_TL3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_FKB_TL3_IEH_DEST_NUM_BITS 1
#define CFA_P70_FKB_TL3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_FKB_TL3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_FKB_TL3_IEH_HOP_NUM_BITS 1
#define CFA_P70_FKB_TL3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_FKB_TL3_DF_NUM_BITS 1
#define CFA_P70_FKB_TL3_L3ERR_NUM_BITS 4
#define CFA_P70_FKB_TL4_TYPE_NUM_BITS 4
#define CFA_P70_FKB_TL4_SRC_NUM_BITS 16
#define CFA_P70_FKB_TL4_DST_NUM_BITS 16
#define CFA_P70_FKB_TL4_FLAGS_NUM_BITS 9
#define CFA_P70_FKB_TL4_SEQ_NUM_BITS 32
#define CFA_P70_FKB_TL4_PA_NUM_BITS 1
#define CFA_P70_FKB_TL4_OPT_NUM_BITS 1
#define CFA_P70_FKB_TL4_TCPTS_NUM_BITS 1
#define CFA_P70_FKB_TL4_ERR_NUM_BITS 4
#define CFA_P70_FKB_T_TYPE_NUM_BITS 5
#define CFA_P70_FKB_T_FLAGS_NUM_BITS 8
#define CFA_P70_FKB_T_IDS_NUM_BITS 24
#define CFA_P70_FKB_T_ID_NUM_BITS 32
#define CFA_P70_FKB_T_CTXTS_NUM_BITS 24
#define CFA_P70_FKB_T_CTXT_NUM_BITS 32
#define CFA_P70_FKB_T_QOS_NUM_BITS 3
#define CFA_P70_FKB_T_ERR_NUM_BITS 4
#define CFA_P70_FKB_L2_TYPE_NUM_BITS 2
#define CFA_P70_FKB_L2_DMAC_NUM_BITS 48
#define CFA_P70_FKB_L2_SMAC_NUM_BITS 48
#define CFA_P70_FKB_L2_DT_NUM_BITS 2
#define CFA_P70_FKB_L2_SA_NUM_BITS 1
#define CFA_P70_FKB_L2_NVT_NUM_BITS 2
#define CFA_P70_FKB_L2_OVP_NUM_BITS 3
#define CFA_P70_FKB_L2_OVD_NUM_BITS 1
#define CFA_P70_FKB_L2_OVV_NUM_BITS 12
#define CFA_P70_FKB_L2_OVT_NUM_BITS 3
#define CFA_P70_FKB_L2_IVP_NUM_BITS 3
#define CFA_P70_FKB_L2_IVD_NUM_BITS 1
#define CFA_P70_FKB_L2_IVV_NUM_BITS 12
#define CFA_P70_FKB_L2_IVT_NUM_BITS 3
#define CFA_P70_FKB_L2_ETYPE_NUM_BITS 16
#define CFA_P70_FKB_L3_TYPE_NUM_BITS 4
#define CFA_P70_FKB_L3_SIP3_NUM_BITS 32
#define CFA_P70_FKB_L3_SIP2_NUM_BITS 32
#define CFA_P70_FKB_L3_SIP1_NUM_BITS 32
#define CFA_P70_FKB_L3_SIP0_NUM_BITS 32
#define CFA_P70_FKB_L3_DIP3_NUM_BITS 32
#define CFA_P70_FKB_L3_DIP2_NUM_BITS 32
#define CFA_P70_FKB_L3_DIP1_NUM_BITS 32
#define CFA_P70_FKB_L3_DIP0_NUM_BITS 32
#define CFA_P70_FKB_L3_TTL_NUM_BITS 8
#define CFA_P70_FKB_L3_PROT_NUM_BITS 8
/**
 * CFA_P70_FKB_L3_FID bit length is not fixed
 * So the CFA_P70_FKB_L3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_FKB_L3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_FKB_L3_QOS_NUM_BITS 8
#define CFA_P70_FKB_L3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_FKB_L3_IEH_SEP_NUM_BITS 1
#define CFA_P70_FKB_L3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_FKB_L3_IEH_DEST_NUM_BITS 1
#define CFA_P70_FKB_L3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_FKB_L3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_FKB_L3_IEH_HOP_NUM_BITS 1
#define CFA_P70_FKB_L3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_FKB_L3_DF_NUM_BITS 1
#define CFA_P70_FKB_L3_L3ERR_NUM_BITS 4
#define CFA_P70_FKB_L4_TYPE_NUM_BITS 4
#define CFA_P70_FKB_L4_SRC_NUM_BITS 16
#define CFA_P70_FKB_L4_DST_NUM_BITS 16
#define CFA_P70_FKB_L4_FLAGS_NUM_BITS 9
#define CFA_P70_FKB_L4_SEQ_NUM_BITS 32
#define CFA_P70_FKB_L4_ACK_NUM_BITS 32
#define CFA_P70_FKB_L4_WIN_NUM_BITS 16
#define CFA_P70_FKB_L4_PA_NUM_BITS 1
#define CFA_P70_FKB_L4_OPT_NUM_BITS 1
#define CFA_P70_FKB_L4_TCPTS_NUM_BITS 1
#define CFA_P70_FKB_L4_TSVAL_NUM_BITS 32
#define CFA_P70_FKB_L4_TXECR_NUM_BITS 32
#define CFA_P70_FKB_L4_ERR_NUM_BITS 4

/**
 * Field length definitions for wc tcam fkb
 */
#define CFA_P70_WC_TCAM_FKB_PROF_ID_NUM_BITS 8
#define CFA_P70_WC_TCAM_FKB_L2CTXT_NUM_BITS 11
#define CFA_P70_WC_TCAM_FKB_L2FUNC_NUM_BITS 8
#define CFA_P70_WC_TCAM_FKB_PARIF_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_SPIF_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_SVIF_NUM_BITS 6
#define CFA_P70_WC_TCAM_FKB_LCOS_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_META_HI_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_META_LO_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_RCYC_CNT_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_LOOPBACK_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL2_TYPE_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_OTL2_DMAC_NUM_BITS 48
#define CFA_P70_WC_TCAM_FKB_OTL2_SMAC_NUM_BITS 48
#define CFA_P70_WC_TCAM_FKB_OTL2_DT_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_OTL2_SA_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL2_NVT_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_OTL2_OVP_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_OTL2_OVD_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL2_OVV_NUM_BITS 12
#define CFA_P70_WC_TCAM_FKB_OTL2_OVT_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_OTL2_IVP_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_OTL2_IVD_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL2_IVV_NUM_BITS 12
#define CFA_P70_WC_TCAM_FKB_OTL2_IVT_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_OTL2_ETYPE_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_OTL3_TYPE_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_OTL3_SIP3_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OTL3_SIP2_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OTL3_SIP1_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OTL3_SIP0_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OTL3_DIP3_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OTL3_DIP2_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OTL3_DIP1_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OTL3_DIP0_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OTL3_TTL_NUM_BITS 8
#define CFA_P70_WC_TCAM_FKB_OTL3_PROT_NUM_BITS 8
/**
 * CFA_P70_WC_TCAM_FKB_OTL3_FID bit length is not fixed
 * So the CFA_P70_WC_TCAM_FKB_OTL3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_WC_TCAM_FKB_OTL3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_WC_TCAM_FKB_OTL3_QOS_NUM_BITS 8
#define CFA_P70_WC_TCAM_FKB_OTL3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL3_IEH_SEP_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL3_IEH_DEST_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL3_IEH_HOP_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL3_DF_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL3_L3ERR_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_OTL4_TYPE_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_OTL4_SRC_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_OTL4_DST_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_OTL4_FLAGS_NUM_BITS 9
#define CFA_P70_WC_TCAM_FKB_OTL4_SEQ_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OTL4_PA_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL4_OPT_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL4_TCPTS_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_OTL4_ERR_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_OT_TYPE_NUM_BITS 5
#define CFA_P70_WC_TCAM_FKB_OT_FLAGS_NUM_BITS 8
#define CFA_P70_WC_TCAM_FKB_OT_IDS_NUM_BITS 24
#define CFA_P70_WC_TCAM_FKB_OT_ID_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OT_CTXTS_NUM_BITS 24
#define CFA_P70_WC_TCAM_FKB_OT_CTXT_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_OT_QOS_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_OT_ERR_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_TL2_TYPE_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_TL2_DMAC_NUM_BITS 48
#define CFA_P70_WC_TCAM_FKB_TL2_SMAC_NUM_BITS 48
#define CFA_P70_WC_TCAM_FKB_TL2_DT_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_TL2_SA_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL2_NVT_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_TL2_OVP_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_TL2_OVD_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL2_OVV_NUM_BITS 12
#define CFA_P70_WC_TCAM_FKB_TL2_OVT_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_TL2_IVP_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_TL2_IVD_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL2_IVV_NUM_BITS 12
#define CFA_P70_WC_TCAM_FKB_TL2_IVT_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_TL2_ETYPE_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_TL3_TYPE_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_TL3_SIP3_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_TL3_SIP2_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_TL3_SIP1_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_TL3_SIP0_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_TL3_DIP3_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_TL3_DIP2_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_TL3_DIP1_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_TL3_DIP0_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_TL3_TTL_NUM_BITS 8
#define CFA_P70_WC_TCAM_FKB_TL3_PROT_NUM_BITS 8
/**
 * CFA_P70_WC_TCAM_FKB_TL3_FID bit length is not fixed
 * So the CFA_P70_WC_TCAM_FKB_TL3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_WC_TCAM_FKB_TL3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_WC_TCAM_FKB_TL3_QOS_NUM_BITS 8
#define CFA_P70_WC_TCAM_FKB_TL3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL3_IEH_SEP_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL3_IEH_DEST_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL3_IEH_HOP_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL3_DF_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL3_L3ERR_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_TL4_TYPE_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_TL4_SRC_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_TL4_DST_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_TL4_FLAGS_NUM_BITS 9
#define CFA_P70_WC_TCAM_FKB_TL4_SEQ_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_TL4_PA_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL4_OPT_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL4_TCPTS_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_TL4_ERR_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_T_TYPE_NUM_BITS 5
#define CFA_P70_WC_TCAM_FKB_T_FLAGS_NUM_BITS 8
#define CFA_P70_WC_TCAM_FKB_T_IDS_NUM_BITS 24
#define CFA_P70_WC_TCAM_FKB_T_ID_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_T_CTXTS_NUM_BITS 24
#define CFA_P70_WC_TCAM_FKB_T_CTXT_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_T_QOS_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_T_ERR_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_L2_TYPE_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_L2_DMAC_NUM_BITS 48
#define CFA_P70_WC_TCAM_FKB_L2_SMAC_NUM_BITS 48
#define CFA_P70_WC_TCAM_FKB_L2_DT_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_L2_SA_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L2_NVT_NUM_BITS 2
#define CFA_P70_WC_TCAM_FKB_L2_OVP_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_L2_OVD_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L2_OVV_NUM_BITS 12
#define CFA_P70_WC_TCAM_FKB_L2_OVT_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_L2_IVP_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_L2_IVD_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L2_IVV_NUM_BITS 12
#define CFA_P70_WC_TCAM_FKB_L2_IVT_NUM_BITS 3
#define CFA_P70_WC_TCAM_FKB_L2_ETYPE_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_L3_TYPE_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_L3_SIP3_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L3_SIP2_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L3_SIP1_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L3_SIP0_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L3_DIP3_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L3_DIP2_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L3_DIP1_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L3_DIP0_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L3_TTL_NUM_BITS 8
#define CFA_P70_WC_TCAM_FKB_L3_PROT_NUM_BITS 8
/**
 * CFA_P70_WC_TCAM_FKB_L3_FID bit length is not fixed
 * So the CFA_P70_WC_TCAM_FKB_L3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_WC_TCAM_FKB_L3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_WC_TCAM_FKB_L3_QOS_NUM_BITS 8
#define CFA_P70_WC_TCAM_FKB_L3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L3_IEH_SEP_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L3_IEH_DEST_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L3_IEH_HOP_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L3_DF_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L3_L3ERR_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_L4_TYPE_NUM_BITS 4
#define CFA_P70_WC_TCAM_FKB_L4_SRC_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_L4_DST_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_L4_FLAGS_NUM_BITS 9
#define CFA_P70_WC_TCAM_FKB_L4_SEQ_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L4_ACK_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L4_WIN_NUM_BITS 16
#define CFA_P70_WC_TCAM_FKB_L4_PA_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L4_OPT_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L4_TCPTS_NUM_BITS 1
#define CFA_P70_WC_TCAM_FKB_L4_TSVAL_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L4_TXECR_NUM_BITS 32
#define CFA_P70_WC_TCAM_FKB_L4_ERR_NUM_BITS 4

/**
 * Field length definitions for em fkb
 */
#define CFA_P70_EM_FKB_PROF_ID_NUM_BITS 8
#define CFA_P70_EM_FKB_L2CTXT_NUM_BITS 11
#define CFA_P70_EM_FKB_L2FUNC_NUM_BITS 8
#define CFA_P70_EM_FKB_PARIF_NUM_BITS 2
#define CFA_P70_EM_FKB_SPIF_NUM_BITS 2
#define CFA_P70_EM_FKB_SVIF_NUM_BITS 6
#define CFA_P70_EM_FKB_LCOS_NUM_BITS 3
#define CFA_P70_EM_FKB_META_HI_NUM_BITS 16
#define CFA_P70_EM_FKB_META_LO_NUM_BITS 16
#define CFA_P70_EM_FKB_RCYC_CNT_NUM_BITS 4
#define CFA_P70_EM_FKB_LOOPBACK_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL2_TYPE_NUM_BITS 2
#define CFA_P70_EM_FKB_OTL2_DMAC_NUM_BITS 48
#define CFA_P70_EM_FKB_OTL2_SMAC_NUM_BITS 48
#define CFA_P70_EM_FKB_OTL2_DT_NUM_BITS 2
#define CFA_P70_EM_FKB_OTL2_SA_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL2_NVT_NUM_BITS 2
#define CFA_P70_EM_FKB_OTL2_OVP_NUM_BITS 3
#define CFA_P70_EM_FKB_OTL2_OVD_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL2_OVV_NUM_BITS 12
#define CFA_P70_EM_FKB_OTL2_OVT_NUM_BITS 3
#define CFA_P70_EM_FKB_OTL2_IVP_NUM_BITS 3
#define CFA_P70_EM_FKB_OTL2_IVD_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL2_IVV_NUM_BITS 12
#define CFA_P70_EM_FKB_OTL2_IVT_NUM_BITS 3
#define CFA_P70_EM_FKB_OTL2_ETYPE_NUM_BITS 16
#define CFA_P70_EM_FKB_OTL3_TYPE_NUM_BITS 4
#define CFA_P70_EM_FKB_OTL3_SIP3_NUM_BITS 32
#define CFA_P70_EM_FKB_OTL3_SIP2_NUM_BITS 32
#define CFA_P70_EM_FKB_OTL3_SIP1_NUM_BITS 32
#define CFA_P70_EM_FKB_OTL3_SIP0_NUM_BITS 32
#define CFA_P70_EM_FKB_OTL3_DIP3_NUM_BITS 32
#define CFA_P70_EM_FKB_OTL3_DIP2_NUM_BITS 32
#define CFA_P70_EM_FKB_OTL3_DIP1_NUM_BITS 32
#define CFA_P70_EM_FKB_OTL3_DIP0_NUM_BITS 32
#define CFA_P70_EM_FKB_OTL3_TTL_NUM_BITS 8
#define CFA_P70_EM_FKB_OTL3_PROT_NUM_BITS 8
/**
 * CFA_P70_EM_FKB_OTL3_FID bit length is not fixed
 * So the CFA_P70_EM_FKB_OTL3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_EM_FKB_OTL3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_EM_FKB_OTL3_QOS_NUM_BITS 8
#define CFA_P70_EM_FKB_OTL3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL3_IEH_SEP_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL3_IEH_DEST_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL3_IEH_HOP_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL3_DF_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL3_L3ERR_NUM_BITS 4
#define CFA_P70_EM_FKB_OTL4_TYPE_NUM_BITS 4
#define CFA_P70_EM_FKB_OTL4_SRC_NUM_BITS 16
#define CFA_P70_EM_FKB_OTL4_DST_NUM_BITS 16
#define CFA_P70_EM_FKB_OTL4_FLAGS_NUM_BITS 9
#define CFA_P70_EM_FKB_OTL4_SEQ_NUM_BITS 32
#define CFA_P70_EM_FKB_OTL4_PA_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL4_OPT_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL4_TCPTS_NUM_BITS 1
#define CFA_P70_EM_FKB_OTL4_ERR_NUM_BITS 4
#define CFA_P70_EM_FKB_OT_TYPE_NUM_BITS 5
#define CFA_P70_EM_FKB_OT_FLAGS_NUM_BITS 8
#define CFA_P70_EM_FKB_OT_IDS_NUM_BITS 24
#define CFA_P70_EM_FKB_OT_ID_NUM_BITS 32
#define CFA_P70_EM_FKB_OT_CTXTS_NUM_BITS 24
#define CFA_P70_EM_FKB_OT_CTXT_NUM_BITS 32
#define CFA_P70_EM_FKB_OT_QOS_NUM_BITS 3
#define CFA_P70_EM_FKB_OT_ERR_NUM_BITS 4
#define CFA_P70_EM_FKB_TL2_TYPE_NUM_BITS 2
#define CFA_P70_EM_FKB_TL2_DMAC_NUM_BITS 48
#define CFA_P70_EM_FKB_TL2_SMAC_NUM_BITS 48
#define CFA_P70_EM_FKB_TL2_DT_NUM_BITS 2
#define CFA_P70_EM_FKB_TL2_SA_NUM_BITS 1
#define CFA_P70_EM_FKB_TL2_NVT_NUM_BITS 2
#define CFA_P70_EM_FKB_TL2_OVP_NUM_BITS 3
#define CFA_P70_EM_FKB_TL2_OVD_NUM_BITS 1
#define CFA_P70_EM_FKB_TL2_OVV_NUM_BITS 12
#define CFA_P70_EM_FKB_TL2_OVT_NUM_BITS 3
#define CFA_P70_EM_FKB_TL2_IVP_NUM_BITS 3
#define CFA_P70_EM_FKB_TL2_IVD_NUM_BITS 1
#define CFA_P70_EM_FKB_TL2_IVV_NUM_BITS 12
#define CFA_P70_EM_FKB_TL2_IVT_NUM_BITS 3
#define CFA_P70_EM_FKB_TL2_ETYPE_NUM_BITS 16
#define CFA_P70_EM_FKB_TL3_TYPE_NUM_BITS 4
#define CFA_P70_EM_FKB_TL3_SIP3_NUM_BITS 32
#define CFA_P70_EM_FKB_TL3_SIP2_NUM_BITS 32
#define CFA_P70_EM_FKB_TL3_SIP1_NUM_BITS 32
#define CFA_P70_EM_FKB_TL3_SIP0_NUM_BITS 32
#define CFA_P70_EM_FKB_TL3_DIP3_NUM_BITS 32
#define CFA_P70_EM_FKB_TL3_DIP2_NUM_BITS 32
#define CFA_P70_EM_FKB_TL3_DIP1_NUM_BITS 32
#define CFA_P70_EM_FKB_TL3_DIP0_NUM_BITS 32
#define CFA_P70_EM_FKB_TL3_TTL_NUM_BITS 8
#define CFA_P70_EM_FKB_TL3_PROT_NUM_BITS 8
/**
 * CFA_P70_EM_FKB_TL3_FID bit length is not fixed
 * So the CFA_P70_EM_FKB_TL3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_EM_FKB_TL3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_EM_FKB_TL3_QOS_NUM_BITS 8
#define CFA_P70_EM_FKB_TL3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_EM_FKB_TL3_IEH_SEP_NUM_BITS 1
#define CFA_P70_EM_FKB_TL3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_EM_FKB_TL3_IEH_DEST_NUM_BITS 1
#define CFA_P70_EM_FKB_TL3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_EM_FKB_TL3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_EM_FKB_TL3_IEH_HOP_NUM_BITS 1
#define CFA_P70_EM_FKB_TL3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_EM_FKB_TL3_DF_NUM_BITS 1
#define CFA_P70_EM_FKB_TL3_L3ERR_NUM_BITS 4
#define CFA_P70_EM_FKB_TL4_TYPE_NUM_BITS 4
#define CFA_P70_EM_FKB_TL4_SRC_NUM_BITS 16
#define CFA_P70_EM_FKB_TL4_DST_NUM_BITS 16
#define CFA_P70_EM_FKB_TL4_FLAGS_NUM_BITS 9
#define CFA_P70_EM_FKB_TL4_SEQ_NUM_BITS 32
#define CFA_P70_EM_FKB_TL4_PA_NUM_BITS 1
#define CFA_P70_EM_FKB_TL4_OPT_NUM_BITS 1
#define CFA_P70_EM_FKB_TL4_TCPTS_NUM_BITS 1
#define CFA_P70_EM_FKB_TL4_ERR_NUM_BITS 4
#define CFA_P70_EM_FKB_T_TYPE_NUM_BITS 5
#define CFA_P70_EM_FKB_T_FLAGS_NUM_BITS 8
#define CFA_P70_EM_FKB_T_IDS_NUM_BITS 24
#define CFA_P70_EM_FKB_T_ID_NUM_BITS 32
#define CFA_P70_EM_FKB_T_CTXTS_NUM_BITS 24
#define CFA_P70_EM_FKB_T_CTXT_NUM_BITS 32
#define CFA_P70_EM_FKB_T_QOS_NUM_BITS 3
#define CFA_P70_EM_FKB_T_ERR_NUM_BITS 4
#define CFA_P70_EM_FKB_L2_TYPE_NUM_BITS 2
#define CFA_P70_EM_FKB_L2_DMAC_NUM_BITS 48
#define CFA_P70_EM_FKB_L2_SMAC_NUM_BITS 48
#define CFA_P70_EM_FKB_L2_DT_NUM_BITS 2
#define CFA_P70_EM_FKB_L2_SA_NUM_BITS 1
#define CFA_P70_EM_FKB_L2_NVT_NUM_BITS 2
#define CFA_P70_EM_FKB_L2_OVP_NUM_BITS 3
#define CFA_P70_EM_FKB_L2_OVD_NUM_BITS 1
#define CFA_P70_EM_FKB_L2_OVV_NUM_BITS 12
#define CFA_P70_EM_FKB_L2_OVT_NUM_BITS 3
#define CFA_P70_EM_FKB_L2_IVP_NUM_BITS 3
#define CFA_P70_EM_FKB_L2_IVD_NUM_BITS 1
#define CFA_P70_EM_FKB_L2_IVV_NUM_BITS 12
#define CFA_P70_EM_FKB_L2_IVT_NUM_BITS 3
#define CFA_P70_EM_FKB_L2_ETYPE_NUM_BITS 16
#define CFA_P70_EM_FKB_L3_TYPE_NUM_BITS 4
#define CFA_P70_EM_FKB_L3_SIP3_NUM_BITS 32
#define CFA_P70_EM_FKB_L3_SIP2_NUM_BITS 32
#define CFA_P70_EM_FKB_L3_SIP1_NUM_BITS 32
#define CFA_P70_EM_FKB_L3_SIP0_NUM_BITS 32
#define CFA_P70_EM_FKB_L3_DIP3_NUM_BITS 32
#define CFA_P70_EM_FKB_L3_DIP2_NUM_BITS 32
#define CFA_P70_EM_FKB_L3_DIP1_NUM_BITS 32
#define CFA_P70_EM_FKB_L3_DIP0_NUM_BITS 32
#define CFA_P70_EM_FKB_L3_TTL_NUM_BITS 8
#define CFA_P70_EM_FKB_L3_PROT_NUM_BITS 8
/**
 * CFA_P70_EM_FKB_L3_FID bit length is not fixed
 * So the CFA_P70_EM_FKB_L3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_EM_FKB_L3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_EM_FKB_L3_QOS_NUM_BITS 8
#define CFA_P70_EM_FKB_L3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_EM_FKB_L3_IEH_SEP_NUM_BITS 1
#define CFA_P70_EM_FKB_L3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_EM_FKB_L3_IEH_DEST_NUM_BITS 1
#define CFA_P70_EM_FKB_L3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_EM_FKB_L3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_EM_FKB_L3_IEH_HOP_NUM_BITS 1
#define CFA_P70_EM_FKB_L3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_EM_FKB_L3_DF_NUM_BITS 1
#define CFA_P70_EM_FKB_L3_L3ERR_NUM_BITS 4
#define CFA_P70_EM_FKB_L4_TYPE_NUM_BITS 4
#define CFA_P70_EM_FKB_L4_SRC_NUM_BITS 16
#define CFA_P70_EM_FKB_L4_DST_NUM_BITS 16
#define CFA_P70_EM_FKB_L4_FLAGS_NUM_BITS 9
#define CFA_P70_EM_FKB_L4_SEQ_NUM_BITS 32
#define CFA_P70_EM_FKB_L4_ACK_NUM_BITS 32
#define CFA_P70_EM_FKB_L4_WIN_NUM_BITS 16
#define CFA_P70_EM_FKB_L4_PA_NUM_BITS 1
#define CFA_P70_EM_FKB_L4_OPT_NUM_BITS 1
#define CFA_P70_EM_FKB_L4_TCPTS_NUM_BITS 1
#define CFA_P70_EM_FKB_L4_TSVAL_NUM_BITS 32
#define CFA_P70_EM_FKB_L4_TXECR_NUM_BITS 32
#define CFA_P70_EM_FKB_L4_ERR_NUM_BITS 4

/**
 * Field length definitions for em key layout
 */
#define CFA_P70_EM_KL_RANGE_IDX_NUM_BITS 16
#define CFA_P70_EM_KL_RANGE_PROFILE_NUM_BITS 4
#define CFA_P70_EM_KL_CREC_TIMER_VALUE_NUM_BITS 4
#define CFA_P70_EM_KL_CREC_STATE_NUM_BITS 5
#define CFA_P70_EM_KL_CREC_TCP_MSB_OPP_INIT_NUM_BITS 1
#define CFA_P70_EM_KL_CREC_TCP_MSB_OPP_NUM_BITS 18
#define CFA_P70_EM_KL_CREC_TCP_MSB_LOC_NUM_BITS 18
#define CFA_P70_EM_KL_CREC_TCP_WIN_NUM_BITS 5
#define CFA_P70_EM_KL_CREC_TCP_UPDT_EN_NUM_BITS 1
#define CFA_P70_EM_KL_CREC_TCP_DIR_NUM_BITS 1
#define CFA_P70_EM_KL_METADATA_NUM_BITS 32
#define CFA_P70_EM_KL_PROF_FUNC_NUM_BITS 8
#define CFA_P70_EM_KL_META_PROF_NUM_BITS 3
#define CFA_P70_EM_KL_RECYCLE_DEST_NUM_BITS 1
#define CFA_P70_EM_KL_FC_PTR_NUM_BITS 28
#define CFA_P70_EM_KL_FC_TYPE_NUM_BITS 2
#define CFA_P70_EM_KL_FC_OP_NUM_BITS 1
#define CFA_P70_EM_KL_PATHS_M1_NUM_BITS 4
#define CFA_P70_EM_KL_ACT_REC_SIZE_NUM_BITS 5
#define CFA_P70_EM_KL_RING_TABLE_IDX_NUM_BITS 9
#define CFA_P70_EM_KL_DESTINATION_NUM_BITS 17
#define CFA_P70_EM_KL_ACT_REC_PTR_NUM_BITS 26
#define CFA_P70_EM_KL_ACT_HINT_NUM_BITS 2
#define CFA_P70_EM_KL_STRENGTH_NUM_BITS 2
#define CFA_P70_EM_KL_OPCODE_NUM_BITS 4
#define CFA_P70_EM_KL_EPOCH1_NUM_BITS 6
#define CFA_P70_EM_KL_EPOCH0_NUM_BITS 12
#define CFA_P70_EM_KL_REC_SIZE_NUM_BITS 2
#define CFA_P70_EM_KL_VALID_NUM_BITS 1
#define CFA_P70_EM_KL_PROF_ID_NUM_BITS 8
#define CFA_P70_EM_KL_L2CTXT_NUM_BITS 11
#define CFA_P70_EM_KL_L2FUNC_NUM_BITS 8
#define CFA_P70_EM_KL_PARIF_NUM_BITS 2
#define CFA_P70_EM_KL_SPIF_NUM_BITS 2
#define CFA_P70_EM_KL_SVIF_NUM_BITS 6
#define CFA_P70_EM_KL_LCOS_NUM_BITS 3
#define CFA_P70_EM_KL_META_HI_NUM_BITS 16
#define CFA_P70_EM_KL_META_LO_NUM_BITS 16
#define CFA_P70_EM_KL_RCYC_CNT_NUM_BITS 4
#define CFA_P70_EM_KL_LOOPBACK_NUM_BITS 1
#define CFA_P70_EM_KL_OTL2_TYPE_NUM_BITS 2
#define CFA_P70_EM_KL_OTL2_DMAC_NUM_BITS 48
#define CFA_P70_EM_KL_OTL2_SMAC_NUM_BITS 48
#define CFA_P70_EM_KL_OTL2_DT_NUM_BITS 2
#define CFA_P70_EM_KL_OTL2_SA_NUM_BITS 1
#define CFA_P70_EM_KL_OTL2_NVT_NUM_BITS 2
#define CFA_P70_EM_KL_OTL2_OVP_NUM_BITS 3
#define CFA_P70_EM_KL_OTL2_OVD_NUM_BITS 1
#define CFA_P70_EM_KL_OTL2_OVV_NUM_BITS 12
#define CFA_P70_EM_KL_OTL2_OVT_NUM_BITS 3
#define CFA_P70_EM_KL_OTL2_IVP_NUM_BITS 3
#define CFA_P70_EM_KL_OTL2_IVD_NUM_BITS 1
#define CFA_P70_EM_KL_OTL2_IVV_NUM_BITS 12
#define CFA_P70_EM_KL_OTL2_IVT_NUM_BITS 3
#define CFA_P70_EM_KL_OTL2_ETYPE_NUM_BITS 16
#define CFA_P70_EM_KL_OTL3_TYPE_NUM_BITS 4
#define CFA_P70_EM_KL_OTL3_SIP3_NUM_BITS 32
#define CFA_P70_EM_KL_OTL3_SIP2_NUM_BITS 32
#define CFA_P70_EM_KL_OTL3_SIP1_NUM_BITS 32
#define CFA_P70_EM_KL_OTL3_SIP0_NUM_BITS 32
#define CFA_P70_EM_KL_OTL3_DIP3_NUM_BITS 32
#define CFA_P70_EM_KL_OTL3_DIP2_NUM_BITS 32
#define CFA_P70_EM_KL_OTL3_DIP1_NUM_BITS 32
#define CFA_P70_EM_KL_OTL3_DIP0_NUM_BITS 32
#define CFA_P70_EM_KL_OTL3_TTL_NUM_BITS 8
#define CFA_P70_EM_KL_OTL3_PROT_NUM_BITS 8
/**
 * CFA_P70_EM_KL_OTL3_FID bit length is not fixed
 * So the CFA_P70_EM_KL_OTL3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_EM_KL_OTL3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_EM_KL_OTL3_QOS_NUM_BITS 8
#define CFA_P70_EM_KL_OTL3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_EM_KL_OTL3_IEH_SEP_NUM_BITS 1
#define CFA_P70_EM_KL_OTL3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_EM_KL_OTL3_IEH_DEST_NUM_BITS 1
#define CFA_P70_EM_KL_OTL3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_EM_KL_OTL3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_EM_KL_OTL3_IEH_HOP_NUM_BITS 1
#define CFA_P70_EM_KL_OTL3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_EM_KL_OTL3_DF_NUM_BITS 1
#define CFA_P70_EM_KL_OTL3_L3ERR_NUM_BITS 4
#define CFA_P70_EM_KL_OTL4_TYPE_NUM_BITS 4
#define CFA_P70_EM_KL_OTL4_SRC_NUM_BITS 16
#define CFA_P70_EM_KL_OTL4_DST_NUM_BITS 16
#define CFA_P70_EM_KL_OTL4_FLAGS_NUM_BITS 9
#define CFA_P70_EM_KL_OTL4_SEQ_NUM_BITS 32
#define CFA_P70_EM_KL_OTL4_PA_NUM_BITS 1
#define CFA_P70_EM_KL_OTL4_OPT_NUM_BITS 1
#define CFA_P70_EM_KL_OTL4_TCPTS_NUM_BITS 1
#define CFA_P70_EM_KL_OTL4_ERR_NUM_BITS 4
#define CFA_P70_EM_KL_OT_TYPE_NUM_BITS 5
#define CFA_P70_EM_KL_OT_FLAGS_NUM_BITS 8
#define CFA_P70_EM_KL_OT_IDS_NUM_BITS 24
#define CFA_P70_EM_KL_OT_ID_NUM_BITS 32
#define CFA_P70_EM_KL_OT_CTXTS_NUM_BITS 24
#define CFA_P70_EM_KL_OT_CTXT_NUM_BITS 32
#define CFA_P70_EM_KL_OT_QOS_NUM_BITS 3
#define CFA_P70_EM_KL_OT_ERR_NUM_BITS 4
#define CFA_P70_EM_KL_TL2_TYPE_NUM_BITS 2
#define CFA_P70_EM_KL_TL2_DMAC_NUM_BITS 48
#define CFA_P70_EM_KL_TL2_SMAC_NUM_BITS 48
#define CFA_P70_EM_KL_TL2_DT_NUM_BITS 2
#define CFA_P70_EM_KL_TL2_SA_NUM_BITS 1
#define CFA_P70_EM_KL_TL2_NVT_NUM_BITS 2
#define CFA_P70_EM_KL_TL2_OVP_NUM_BITS 3
#define CFA_P70_EM_KL_TL2_OVD_NUM_BITS 1
#define CFA_P70_EM_KL_TL2_OVV_NUM_BITS 12
#define CFA_P70_EM_KL_TL2_OVT_NUM_BITS 3
#define CFA_P70_EM_KL_TL2_IVP_NUM_BITS 3
#define CFA_P70_EM_KL_TL2_IVD_NUM_BITS 1
#define CFA_P70_EM_KL_TL2_IVV_NUM_BITS 12
#define CFA_P70_EM_KL_TL2_IVT_NUM_BITS 3
#define CFA_P70_EM_KL_TL2_ETYPE_NUM_BITS 16
#define CFA_P70_EM_KL_TL3_TYPE_NUM_BITS 4
#define CFA_P70_EM_KL_TL3_SIP3_NUM_BITS 32
#define CFA_P70_EM_KL_TL3_SIP2_NUM_BITS 32
#define CFA_P70_EM_KL_TL3_SIP1_NUM_BITS 32
#define CFA_P70_EM_KL_TL3_SIP0_NUM_BITS 32
#define CFA_P70_EM_KL_TL3_DIP3_NUM_BITS 32
#define CFA_P70_EM_KL_TL3_DIP2_NUM_BITS 32
#define CFA_P70_EM_KL_TL3_DIP1_NUM_BITS 32
#define CFA_P70_EM_KL_TL3_DIP0_NUM_BITS 32
#define CFA_P70_EM_KL_TL3_TTL_NUM_BITS 8
#define CFA_P70_EM_KL_TL3_PROT_NUM_BITS 8
/**
 * CFA_P70_EM_KL_TL3_FID bit length is not fixed
 * So the CFA_P70_EM_KL_TL3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_EM_KL_TL3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_EM_KL_TL3_QOS_NUM_BITS 8
#define CFA_P70_EM_KL_TL3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_EM_KL_TL3_IEH_SEP_NUM_BITS 1
#define CFA_P70_EM_KL_TL3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_EM_KL_TL3_IEH_DEST_NUM_BITS 1
#define CFA_P70_EM_KL_TL3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_EM_KL_TL3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_EM_KL_TL3_IEH_HOP_NUM_BITS 1
#define CFA_P70_EM_KL_TL3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_EM_KL_TL3_DF_NUM_BITS 1
#define CFA_P70_EM_KL_TL3_L3ERR_NUM_BITS 4
#define CFA_P70_EM_KL_TL4_TYPE_NUM_BITS 4
#define CFA_P70_EM_KL_TL4_SRC_NUM_BITS 16
#define CFA_P70_EM_KL_TL4_DST_NUM_BITS 16
#define CFA_P70_EM_KL_TL4_FLAGS_NUM_BITS 9
#define CFA_P70_EM_KL_TL4_SEQ_NUM_BITS 32
#define CFA_P70_EM_KL_TL4_PA_NUM_BITS 1
#define CFA_P70_EM_KL_TL4_OPT_NUM_BITS 1
#define CFA_P70_EM_KL_TL4_TCPTS_NUM_BITS 1
#define CFA_P70_EM_KL_TL4_ERR_NUM_BITS 4
#define CFA_P70_EM_KL_T_TYPE_NUM_BITS 5
#define CFA_P70_EM_KL_T_FLAGS_NUM_BITS 8
#define CFA_P70_EM_KL_T_IDS_NUM_BITS 24
#define CFA_P70_EM_KL_T_ID_NUM_BITS 32
#define CFA_P70_EM_KL_T_CTXTS_NUM_BITS 24
#define CFA_P70_EM_KL_T_CTXT_NUM_BITS 32
#define CFA_P70_EM_KL_T_QOS_NUM_BITS 3
#define CFA_P70_EM_KL_T_ERR_NUM_BITS 4
#define CFA_P70_EM_KL_L2_TYPE_NUM_BITS 2
#define CFA_P70_EM_KL_L2_DMAC_NUM_BITS 48
#define CFA_P70_EM_KL_L2_SMAC_NUM_BITS 48
#define CFA_P70_EM_KL_L2_DT_NUM_BITS 2
#define CFA_P70_EM_KL_L2_SA_NUM_BITS 1
#define CFA_P70_EM_KL_L2_NVT_NUM_BITS 2
#define CFA_P70_EM_KL_L2_OVP_NUM_BITS 3
#define CFA_P70_EM_KL_L2_OVD_NUM_BITS 1
#define CFA_P70_EM_KL_L2_OVV_NUM_BITS 12
#define CFA_P70_EM_KL_L2_OVT_NUM_BITS 3
#define CFA_P70_EM_KL_L2_IVP_NUM_BITS 3
#define CFA_P70_EM_KL_L2_IVD_NUM_BITS 1
#define CFA_P70_EM_KL_L2_IVV_NUM_BITS 12
#define CFA_P70_EM_KL_L2_IVT_NUM_BITS 3
#define CFA_P70_EM_KL_L2_ETYPE_NUM_BITS 16
#define CFA_P70_EM_KL_L3_TYPE_NUM_BITS 4
#define CFA_P70_EM_KL_L3_SIP3_NUM_BITS 32
#define CFA_P70_EM_KL_L3_SIP2_NUM_BITS 32
#define CFA_P70_EM_KL_L3_SIP1_NUM_BITS 32
#define CFA_P70_EM_KL_L3_SIP0_NUM_BITS 32
#define CFA_P70_EM_KL_L3_DIP3_NUM_BITS 32
#define CFA_P70_EM_KL_L3_DIP2_NUM_BITS 32
#define CFA_P70_EM_KL_L3_DIP1_NUM_BITS 32
#define CFA_P70_EM_KL_L3_DIP0_NUM_BITS 32
#define CFA_P70_EM_KL_L3_TTL_NUM_BITS 8
#define CFA_P70_EM_KL_L3_PROT_NUM_BITS 8
/**
 * CFA_P70_EM_KL_L3_FID bit length is not fixed
 * So the CFA_P70_EM_KL_L3_FID_NUMBITS macro is defined with arguments
 */
#define CFA_P70_EM_KL_L3_FID_NUM_BITS(COND) ((COND) ? 16 : 20)
#define CFA_P70_EM_KL_L3_QOS_NUM_BITS 8
#define CFA_P70_EM_KL_L3_IEH_NONEXT_NUM_BITS 1
#define CFA_P70_EM_KL_L3_IEH_SEP_NUM_BITS 1
#define CFA_P70_EM_KL_L3_IEH_AUTH_NUM_BITS 1
#define CFA_P70_EM_KL_L3_IEH_DEST_NUM_BITS 1
#define CFA_P70_EM_KL_L3_IEH_FRAG_NUM_BITS 1
#define CFA_P70_EM_KL_L3_IEH_RTHDR_NUM_BITS 1
#define CFA_P70_EM_KL_L3_IEH_HOP_NUM_BITS 1
#define CFA_P70_EM_KL_L3_IEH_1FRAG_NUM_BITS 1
#define CFA_P70_EM_KL_L3_DF_NUM_BITS 1
#define CFA_P70_EM_KL_L3_L3ERR_NUM_BITS 4
#define CFA_P70_EM_KL_L4_TYPE_NUM_BITS 4
#define CFA_P70_EM_KL_L4_SRC_NUM_BITS 16
#define CFA_P70_EM_KL_L4_DST_NUM_BITS 16
#define CFA_P70_EM_KL_L4_FLAGS_NUM_BITS 9
#define CFA_P70_EM_KL_L4_SEQ_NUM_BITS 32
#define CFA_P70_EM_KL_L4_ACK_NUM_BITS 32
#define CFA_P70_EM_KL_L4_WIN_NUM_BITS 16
#define CFA_P70_EM_KL_L4_PA_NUM_BITS 1
#define CFA_P70_EM_KL_L4_OPT_NUM_BITS 1
#define CFA_P70_EM_KL_L4_TCPTS_NUM_BITS 1
#define CFA_P70_EM_KL_L4_TSVAL_NUM_BITS 32
#define CFA_P70_EM_KL_L4_TXECR_NUM_BITS 32
#define CFA_P70_EM_KL_L4_ERR_NUM_BITS 4

/**
 * Field length definitions for action
 */
#define CFA_P70_ACT_TYPE_NUM_BITS 3
#define CFA_P70_ACT_DROP_NUM_BITS 1
#define CFA_P70_ACT_VLAN_DELETE_NUM_BITS 2
#define CFA_P70_ACT_DEST_NUM_BITS 7
#define CFA_P70_ACT_DEST_OP_NUM_BITS 2
#define CFA_P70_ACT_DECAP_NUM_BITS 5
#define CFA_P70_ACT_MIRRORING_NUM_BITS 5
#define CFA_P70_ACT_METER_PTR_NUM_BITS 10
#define CFA_P70_ACT_STAT0_OFF_NUM_BITS 3
#define CFA_P70_ACT_STAT0_OP_NUM_BITS 1
#define CFA_P70_ACT_STAT0_CTR_TYPE_NUM_BITS 2
#define CFA_P70_ACT_MOD_OFF_NUM_BITS 5
#define CFA_P70_ACT_ENC_OFF_NUM_BITS 6
#define CFA_P70_ACT_SRC_OFF_NUM_BITS 4
#define CFA_P70_ACT_COMPACT_RSVD_0_NUM_BITS 4
#define CFA_P70_ACT_STAT0_PTR_NUM_BITS 28
#define CFA_P70_ACT_STAT1_PTR_NUM_BITS 28
#define CFA_P70_ACT_STAT1_OP_NUM_BITS 1
#define CFA_P70_ACT_STAT1_CTR_TYPE_NUM_BITS 2
#define CFA_P70_ACT_MOD_PTR_NUM_BITS 28
#define CFA_P70_ACT_ENC_PTR_NUM_BITS 28
#define CFA_P70_ACT_SRC_PTR_NUM_BITS 28
#define CFA_P70_ACT_FULL_RSVD_0_NUM_BITS 7
#define CFA_P70_ACT_SRC_KO_EN_NUM_BITS 1
#define CFA_P70_ACT_MCG_RSVD_0_NUM_BITS 2
#define CFA_P70_ACT_NEXT_PTR_NUM_BITS 26
#define CFA_P70_ACT_PTR0_ACT_HINT_NUM_BITS 2
#define CFA_P70_ACT_PTR0_ACT_REC_PTR_NUM_BITS 26
#define CFA_P70_ACT_PTR1_ACT_HINT_NUM_BITS 2
#define CFA_P70_ACT_PTR1_ACT_REC_PTR_NUM_BITS 26
#define CFA_P70_ACT_PTR2_ACT_HINT_NUM_BITS 2
#define CFA_P70_ACT_PTR2_ACT_REC_PTR_NUM_BITS 26
#define CFA_P70_ACT_PTR3_ACT_HINT_NUM_BITS 2
#define CFA_P70_ACT_PTR3_ACT_REC_PTR_NUM_BITS 26
#define CFA_P70_ACT_PTR4_ACT_HINT_NUM_BITS 2
#define CFA_P70_ACT_PTR4_ACT_REC_PTR_NUM_BITS 26
#define CFA_P70_ACT_PTR5_ACT_HINT_NUM_BITS 2
#define CFA_P70_ACT_PTR5_ACT_REC_PTR_NUM_BITS 26
#define CFA_P70_ACT_PTR6_ACT_HINT_NUM_BITS 2
#define CFA_P70_ACT_PTR6_ACT_REC_PTR_NUM_BITS 26
#define CFA_P70_ACT_PTR7_ACT_HINT_NUM_BITS 2
#define CFA_P70_ACT_PTR7_ACT_REC_PTR_NUM_BITS 26
#define CFA_P70_ACT_MCG_SUBSEQ_RSVD_0_NUM_BITS 3
#define CFA_P70_ACT_MOD_MODIFY_ACT_HDR_NUM_BITS 16
#define CFA_P70_ACT_MOD_MD_UPDT_DATA_NUM_BITS 32
#define CFA_P70_ACT_MOD_MD_UPDT_PROF_NUM_BITS 4

/**
 * Enumeration definition for field 'md_op'
 */
enum cfa_p70_md_op {
	/*
	 * Normal Metadata update: ! md = (md & ~md_prof.mask) | (md_prof.mask &
	 * md_data)
	 */
	CFA_P70_MD_OP_NORMAL = 0,
	/*
	 * L2 Hash Metadata update: ! md = (md & ~md_prof.mask) | (md_prof.mask
	 * & hash_l2(seed,packet))
	 */
	CFA_P70_MD_OP_L2_HASH = 1,
	/*
	 * L4 Hash Metadata update: ! md = (md & ~ md_prof.mask) | (md_prof.mask
	 * & hash_l3l4(seed,packet))
	 */
	CFA_P70_MD_OP_L4_HASH = 2,
	/*
	 * SVIF insert Metadata update: ! md = (md & ~ md_prof.mask) |
	 * (md_prof.mask & zero_extend(svif))
	 */
	CFA_P70_MD_OP_SVIF = 3,
};
#define CFA_P70_ACT_MOD_MD_UPDT_OP_NUM_BITS 2
#define CFA_P70_ACT_MOD_MD_UPDT_RSVD_0_NUM_BITS 10
#define CFA_P70_ACT_MOD_MD_UPDT_TOP_NUM_BITS 48
#define CFA_P70_ACT_MOD_RM_OVLAN_NUM_BITS 32
#define CFA_P70_ACT_MOD_RM_IVLAN_NUM_BITS 32
#define CFA_P70_ACT_MOD_RPL_IVLAN_NUM_BITS 32
#define CFA_P70_ACT_MOD_RPL_OVLAN_NUM_BITS 32
#define CFA_P70_ACT_MOD_TTL_UPDT_OP_NUM_BITS 15
#define CFA_P70_ACT_MOD_TTL_UPDT_ALT_VID_NUM_BITS 12
#define CFA_P70_ACT_MOD_TTL_UPDT_ALT_PFID_NUM_BITS 5
#define CFA_P70_ACT_MOD_TTL_UPDT_TOP_NUM_BITS 32
#define CFA_P70_ACT_MOD_TNL_MODIFY_DEL_NUM_BITS 16
#define CFA_P70_ACT_MOD_TNL_MODIFY_8B_NEW_PROT_NUM_BITS 8
#define CFA_P70_ACT_MOD_TNL_MODIFY_8B_EXIST_PROT_NUM_BITS 8
#define CFA_P70_ACT_MOD_TNL_MODIFY_8B_VEC_NUM_BITS 16
#define CFA_P70_ACT_MOD_TNL_MODIFY_8B_TOP_NUM_BITS 32
#define CFA_P70_ACT_MOD_TNL_MODIFY_16B_NEW_PROT_NUM_BITS 16
#define CFA_P70_ACT_MOD_TNL_MODIFY_16B_EXIST_PROT_NUM_BITS 16
#define CFA_P70_ACT_MOD_TNL_MODIFY_16B_VEC_NUM_BITS 16
#define CFA_P70_ACT_MOD_TNL_MODIFY_16B_TOP_NUM_BITS 48
#define CFA_P70_ACT_MOD_UPDT_FIELD_DATA0_NUM_BITS 32
#define CFA_P70_ACT_MOD_UPDT_FIELD_VEC_RSVD_NUM_BITS 15
#define CFA_P70_ACT_MOD_UPDT_FIELD_VEC_KID_NUM_BITS 1
#define CFA_P70_ACT_MOD_UPDT_FIELD_TOP_NUM_BITS 48
#define CFA_P70_ACT_MOD_SMAC_NUM_BITS 48
#define CFA_P70_ACT_MOD_DMAC_NUM_BITS 48
#define CFA_P70_ACT_MOD_SIPV6_NUM_BITS 128
#define CFA_P70_ACT_MOD_DIPV6_NUM_BITS 128
#define CFA_P70_ACT_MOD_SIPV4_NUM_BITS 32
#define CFA_P70_ACT_MOD_DIPV4_NUM_BITS 32
#define CFA_P70_ACT_MOD_SPORT_NUM_BITS 16
#define CFA_P70_ACT_MOD_DPORT_NUM_BITS 16

/**
 * Enumeration definition for field 'ecv_tnl'
 */
enum cfa_p70_ecv_tnl {
	/* No tunnel header will be added. */
	CFA_P70_ECV_TNL_NOP = 0,
	/*
	 * Generic full header will be added after inserted L2, L3, or L4
	 * header. The first byte of the tunnel body will be the length of the
	 * inserted tunnel.
	 */
	CFA_P70_ECV_TNL_GENERIC = 1,
	/* VXLAN tunnel header will be added. */
	CFA_P70_ECV_TNL_VXLAN = 2,
	/* NGE (VXLAN2) Header will be added. */
	CFA_P70_ECV_TNL_NGE = 3,
	/* NVGRE Header will be added. */
	CFA_P70_ECV_TNL_NVGRE = 4,
	/* GRE Header will be added. */
	CFA_P70_ECV_TNL_GRE = 5,
	/*
	 * Generic header after existing L4 header will be added. The first byte
	 * of the tunnel body will be the length of the inserted tunnel.
	 */
	CFA_P70_ECV_TNL_GENERIC_L4 = 6,
	/*
	 * Generic header after existing tunnel will be added. The first byte of
	 * the tunnel body will be the length of the inserted tunnel.
	 */
	CFA_P70_ECV_TNL_GENERIC_TUN = 7,
};
#define CFA_P70_ACT_ENC_ECV_TNL_NUM_BITS 3

/**
 * Enumeration definition for field 'ecv_l4'
 */
enum cfa_p70_ecv_l4 {
	/* No L4 Header */
	CFA_P70_ECV_L4_NOP = 0,
	/* No L4 Header */
	CFA_P70_ECV_L4_NOP1 = 1,
	/* No L4 Header */
	CFA_P70_ECV_L4_NOP2 = 2,
	/* No L4 Header */
	CFA_P70_ECV_L4_NOP3 = 3,
	/* Add L4 Header without entropy and with CS=0. */
	CFA_P70_ECV_L4_L4 = 4,
	/* Add L4 Header without entropy and with CS=calculated. */
	CFA_P70_ECV_L4_L4_CS = 5,
	/* Add L4 Header with entropy and with CS=0. */
	CFA_P70_ECV_L4_L4_ENT = 6,
	/* Add L4 Header with entropy and with CS=calculated. */
	CFA_P70_ECV_L4_L4_ENT_CS = 7,
};
#define CFA_P70_ACT_ENC_ECV_L4_NUM_BITS 3

/**
 * Enumeration definition for field 'ecv_l3'
 */
enum cfa_p70_ecv_l3 {
	/* No L3 Header */
	CFA_P70_ECV_L3_NOP = 0,
	/* No L3 Header */
	CFA_P70_ECV_L3_NOP1 = 1,
	/* No L3 Header */
	CFA_P70_ECV_L3_NOP2 = 2,
	/* No L3 Header */
	CFA_P70_ECV_L3_NOP3 = 3,
	/* Add IPV4 Header */
	CFA_P70_ECV_L3_IPV4 = 4,
	/* Add IPV4 Header */
	CFA_P70_ECV_L3_IPV6 = 5,
	/* Add MPLS (8847) Header */
	CFA_P70_ECV_L3_MPLS8847 = 6,
	/* Add MPLS (8848) Header */
	CFA_P70_ECV_L3_MPLS8848 = 7,
};
#define CFA_P70_ACT_ENC_ECV_L3_NUM_BITS 3
#define CFA_P70_ACT_ENC_ECV_L2_NUM_BITS 1

/**
 * Enumeration definition for field 'ecv_vtag'
 */
enum cfa_p70_ecv_vtag {
	/* No VLAN tag will be added. */
	CFA_P70_ECV_VTAG_NOP = 0,
	/* Add one VLAN tag using the PRI field from the encap record. */
	CFA_P70_ECV_VTAG_ADD1_USE_PRI = 1,
	/* Add one VLAN tag remap with inner VLAN Tag PRI field. */
	CFA_P70_ECV_VTAG_ADD1_REMAP_INNER_PRI = 2,
	/* Add one VLAN tag remap with diff serve field. */
	CFA_P70_ECV_VTAG_ADD1_REMAP_DIFF = 3,
	/* Add two VLAN tags using the PRI field from the encap record. */
	CFA_P70_ECV_VTAG_ADD2_USE_PRI = 4,
	/* Add two VLAN tag remap with diff serve field. */
	CFA_P70_ECV_VTAG_ADD2_REMAP_DIFF = 5,
	/* Add zero VLAN tags remap with inner VLAN Tag PRI Field. */
	CFA_P70_ECV_VTAG_ADD0_REMAP_INNER_PRI = 6,
	/* Add zero VLAN tags remap with diff serve field. */
	CFA_P70_ECV_VTAG_ADD0_REMAP_DIFF = 7,
	/* Add zero VLAG tags remap with immediate PRI=0. */
	CFA_P70_ECV_VTAG_ADD0_IMMED_PRI0 = 8,
	/* Add zero VLAG tags remap with immediate PRI=1. */
	CFA_P70_ECV_VTAG_ADD0_IMMED_PRI1 = 9,
	/* Add zero VLAG tags remap with immediate PRI=2. */
	CFA_P70_ECV_VTAG_ADD0_IMMED_PRI2 = 10,
	/* Add zero VLAG tags remap with immediate PRI=3. */
	CFA_P70_ECV_VTAG_ADD0_IMMED_PRI3 = 11,
	/* Add zero VLAG tags remap with immediate PRI=4. */
	CFA_P70_ECV_VTAG_ADD0_IMMED_PRI4 = 12,
	/* Add zero VLAG tags remap with immediate PRI=5. */
	CFA_P70_ECV_VTAG_ADD0_IMMED_PRI5 = 13,
	/* Add zero VLAG tags remap with immediate PRI=6. */
	CFA_P70_ECV_VTAG_ADD0_IMMED_PRI6 = 14,
	/* Add zero VLAG tags remap with immediate PRI=7. */
	CFA_P70_ECV_VTAG_ADD0_IMMED_PRI7 = 15,
};
#define CFA_P70_ACT_ENC_ECV_VTAG_NUM_BITS 4
#define CFA_P70_ACT_ENC_ECV_EC_NUM_BITS 1
#define CFA_P70_ACT_ENC_ECV_VALID_NUM_BITS 1
#define CFA_P70_ACT_ENC_EC_IP_TTL_IH_NUM_BITS 1
#define CFA_P70_ACT_ENC_EC_IP_TOS_IH_NUM_BITS 1
#define CFA_P70_ACT_ENC_EC_TUN_QOS_NUM_BITS 3
#define CFA_P70_ACT_ENC_EC_GRE_SET_K_NUM_BITS 1

/**
 * Enumeration definition for field 'enccfg_dmac_ovr'
 */
enum cfa_p70_enccfg_dmac_ovr {
	/* use encap record DMAC */
	CFA_P70_ENCCFG_DMAC_OVR_ENCAP = 0,
	/* re-use existing inner L2 header DMAC */
	CFA_P70_ENCCFG_DMAC_OVR_INNER_DMAC = 1,
	/* re-use existing tunnel L2 header DMAC */
	CFA_P70_ENCCFG_DMAC_OVR_TUNNEL_DMAC = 2,
	/* re-use existing outer-most L2 header DMAC */
	CFA_P70_ENCCFG_DMAC_OVR_OUTER_DMAC = 3,
};
#define CFA_P70_ACT_ENC_EC_DMAC_OVR_NUM_BITS 2

/**
 * Enumeration definition for field 'enccfg_vlan_ovr'
 */
enum cfa_p70_enccfg_vlan_ovr {
	/* use only encap record VLAN tags */
	CFA_P70_ENCCFG_VLAN_OVR_ENCAP = 0,
	/* use only existing inner L2 header VLAN tags */
	CFA_P70_ENCCFG_VLAN_OVR_INNER_L2 = 1,
	/* use only existing tunnel L2 header VLAN tags */
	CFA_P70_ENCCFG_VLAN_OVR_TUNNEL_L2 = 2,
	/* use only existing outer-most L2 header VLAN tags */
	CFA_P70_ENCCFG_VLAN_OVR_OUTER_L2 = 3,
	/* include inner VLAN Tag from existing inner L2 header (keeps 1 TAG) */
	CFA_P70_ENCCFG_VLAN_OVR_INNER_INNER = 4,
	/* include outer VLAN Tag from existing inner L2 header (keeps 1 TAG) */
	CFA_P70_ENCCFG_VLAN_OVR_INNER_OUTER = 5,
	/*
	 * include inner VLAN Tag from existing outer-most L2 header (keeps 1
	 * TAG)
	 */
	CFA_P70_ENCCFG_VLAN_OVR_OUTER_INNER = 6,
	/*
	 * include outer VLAN Tag from existing outer-most L2 header (keeps 1
	 * TAG)
	 */
	CFA_P70_ENCCFG_VLAN_OVR_OUTER_OUTER = 7,
};
#define CFA_P70_ACT_ENC_EC_VLAN_OVR_NUM_BITS 3

/**
 * Enumeration definition for field 'enccfg_smac_ovr'
 */
enum cfa_p70_enccfg_smac_ovr {
	/* use only source property record SMAC */
	CFA_P70_ENCCFG_SMAC_OVR_ENCAP = 0,
	/* re-use existing inner L2 header SMAC */
	CFA_P70_ENCCFG_SMAC_OVR_INNER_SMAC = 1,
	/* re-use existing tunnel L2 header SMAC */
	CFA_P70_ENCCFG_SMAC_OVR_TUNNEL_SMAC = 2,
	/* re-use existing outer-most L2 header SMAC */
	CFA_P70_ENCCFG_SMAC_OVR_OUTER_SMAC = 3,
	/* re-use existing inner L2 header DMAC */
	CFA_P70_ENCCFG_SMAC_OVR_INNER_DMAC = 5,
	/* re-use existing tunnel L2 header DMAC */
	CFA_P70_ENCCFG_SMAC_OVR_TUNNEL_DMAC = 6,
	/* re-use existing outer-most L2 header DMAC */
	CFA_P70_ENCCFG_SMAC_OVR_OUTER_DMAC = 7,
};
#define CFA_P70_ACT_ENC_EC_SMAC_OVR_NUM_BITS 3

/**
 * Enumeration definition for field 'enccfg_ipv4_id_ctrl'
 */
enum cfa_p70_enccfg_ipv4_id_ctrl {
	/* use encap record IPv4 ID field */
	CFA_P70_ENCCFG_IPV4_ID_CTRL_ENCAP = 0,
	/* inherit from next existing IPv4 header ID field */
	CFA_P70_ENCCFG_IPV4_ID_CTRL_INHERIT = 2,
	/* use CFA incrementing IPv4 ID counter */
	CFA_P70_ENCCFG_IPV4_ID_CTRL_INCREMENT = 3,
};
#define CFA_P70_ACT_ENC_EC_IPV4_ID_CTRL_NUM_BITS 2
#define CFA_P70_ACT_ENC_L2_DMAC_NUM_BITS 48
#define CFA_P70_ACT_ENC_VLAN1_TAG_VID_NUM_BITS 12
#define CFA_P70_ACT_ENC_VLAN1_TAG_DE_NUM_BITS 1
#define CFA_P70_ACT_ENC_VLAN1_TAG_PRI_NUM_BITS 3
#define CFA_P70_ACT_ENC_VLAN1_TAG_TPID_NUM_BITS 16
#define CFA_P70_ACT_ENC_VLAN2_IT_VID_NUM_BITS 12
#define CFA_P70_ACT_ENC_VLAN2_IT_DE_NUM_BITS 1
#define CFA_P70_ACT_ENC_VLAN2_IT_PRI_NUM_BITS 3
#define CFA_P70_ACT_ENC_VLAN2_IT_TPID_NUM_BITS 16
#define CFA_P70_ACT_ENC_VLAN2_OT_VID_NUM_BITS 12
#define CFA_P70_ACT_ENC_VLAN2_OT_DE_NUM_BITS 1
#define CFA_P70_ACT_ENC_VLAN2_OT_PRI_NUM_BITS 3
#define CFA_P70_ACT_ENC_VLAN2_OT_TPID_NUM_BITS 16
#define CFA_P70_ACT_ENC_IPV4_ID_NUM_BITS 16
#define CFA_P70_ACT_ENC_IPV4_TOS_NUM_BITS 8
#define CFA_P70_ACT_ENC_IPV4_HLEN_NUM_BITS 4
#define CFA_P70_ACT_ENC_IPV4_VER_NUM_BITS 4
#define CFA_P70_ACT_ENC_IPV4_PROT_NUM_BITS 8
#define CFA_P70_ACT_ENC_IPV4_TTL_NUM_BITS 8
#define CFA_P70_ACT_ENC_IPV4_FRAG_NUM_BITS 13
#define CFA_P70_ACT_ENC_IPV4_FLAGS_NUM_BITS 3
#define CFA_P70_ACT_ENC_IPV4_DEST_NUM_BITS 32
#define CFA_P70_ACT_ENC_IPV6_FLOW_LABEL_NUM_BITS 20
#define CFA_P70_ACT_ENC_IPV6_TRAFFIC_CLASS_NUM_BITS 8
#define CFA_P70_ACT_ENC_IPV6_VER_NUM_BITS 4
#define CFA_P70_ACT_ENC_IPV6_HOP_LIMIT_NUM_BITS 8
#define CFA_P70_ACT_ENC_IPV6_NEXT_HEADER_NUM_BITS 8
#define CFA_P70_ACT_ENC_IPV6_PAYLOAD_LENGTH_NUM_BITS 16
#define CFA_P70_ACT_ENC_IPV6_DEST_NUM_BITS 128
#define CFA_P70_ACT_ENC_MPLS_TAG1_NUM_BITS 32
#define CFA_P70_ACT_ENC_MPLS_TAG2_NUM_BITS 32
#define CFA_P70_ACT_ENC_MPLS_TAG3_NUM_BITS 32
#define CFA_P70_ACT_ENC_MPLS_TAG4_NUM_BITS 32
#define CFA_P70_ACT_ENC_MPLS_TAG5_NUM_BITS 32
#define CFA_P70_ACT_ENC_MPLS_TAG6_NUM_BITS 32
#define CFA_P70_ACT_ENC_MPLS_TAG7_NUM_BITS 32
#define CFA_P70_ACT_ENC_MPLS_TAG8_NUM_BITS 32
#define CFA_P70_ACT_ENC_L4_DEST_PORT_NUM_BITS 16
#define CFA_P70_ACT_ENC_L4_SRC_PORT_NUM_BITS 16
#define CFA_P70_ACT_ENC_TNL_VXLAN_NEXT_PROT_NUM_BITS 8
#define CFA_P70_ACT_ENC_TNL_VXLAN_RSVD_0_NUM_BITS 16
#define CFA_P70_ACT_ENC_TNL_VXLAN_FLAGS_NUM_BITS 8
#define CFA_P70_ACT_ENC_TNL_VXLAN_RSVD_1_NUM_BITS 8
#define CFA_P70_ACT_ENC_TNL_VXLAN_VNI_NUM_BITS 24
#define CFA_P70_ACT_ENC_TNL_NGE_PROT_TYPE_NUM_BITS 16
#define CFA_P70_ACT_ENC_TNL_NGE_RSVD_0_NUM_BITS 6
#define CFA_P70_ACT_ENC_TNL_NGE_FLAGS_C_NUM_BITS 1
#define CFA_P70_ACT_ENC_TNL_NGE_FLAGS_O_NUM_BITS 1
#define CFA_P70_ACT_ENC_TNL_NGE_FLAGS_OPT_LEN_NUM_BITS 6
#define CFA_P70_ACT_ENC_TNL_NGE_FLAGS_VER_NUM_BITS 2
#define CFA_P70_ACT_ENC_TNL_NGE_RSVD_1_NUM_BITS 8
#define CFA_P70_ACT_ENC_TNL_NGE_VNI_NUM_BITS 24
#define CFA_P70_ACT_ENC_TNL_NGE_OPTIONS_NUM_BITS 64
#define CFA_P70_ACT_ENC_TNL_NVGRE_FLOW_ID_NUM_BITS 8
#define CFA_P70_ACT_ENC_TNL_NVGRE_VSID_NUM_BITS 24
#define CFA_P70_ACT_ENC_TNL_GRE_KEY_NUM_BITS 32
#define CFA_P70_ACT_ENC_TNL_GENERIC_TID_NUM_BITS 8
#define CFA_P70_ACT_ENC_TNL_GENERIC_LENGTH_NUM_BITS 8
#define CFA_P70_ACT_ENC_TNL_GENERIC_HEADER_NUM_BITS 32
#define CFA_P70_ACT_SRC_MAC_NUM_BITS 48
#define CFA_P70_ACT_SRC_IPV4_ADDR_NUM_BITS 32
#define CFA_P70_ACT_SRC_IPV6_ADDR_NUM_BITS 128
#define CFA_P70_ACT_STAT0_B16_FPC_NUM_BITS 64
#define CFA_P70_ACT_STAT1_B16_FPC_NUM_BITS 64
#define CFA_P70_ACT_STAT0_B16_FBC_NUM_BITS 64
#define CFA_P70_ACT_STAT1_B16_FBC_NUM_BITS 64
#define CFA_P70_ACT_STAT0_B24_FPC_NUM_BITS 64
#define CFA_P70_ACT_STAT1_B24_FPC_NUM_BITS 64
#define CFA_P70_ACT_STAT0_B24_FBC_NUM_BITS 64
#define CFA_P70_ACT_STAT1_B24_FBC_NUM_BITS 64
#define CFA_P70_ACT_STAT0_B24_TIMESTAMP_NUM_BITS 32
#define CFA_P70_ACT_STAT1_B24_TIMESTAMP_NUM_BITS 32
#define CFA_P70_ACT_STAT0_B24_TCP_FLAGS_NUM_BITS 9
#define CFA_P70_ACT_STAT1_B24_TCP_FLAGS_NUM_BITS 9
#define CFA_P70_ACT_STAT0_B24_UNUSED_0_NUM_BITS 23
#define CFA_P70_ACT_STAT1_B24_UNUSED_0_NUM_BITS 23
#define CFA_P70_ACT_STAT0_B32A_FPC_NUM_BITS 64
#define CFA_P70_ACT_STAT1_B32A_FPC_NUM_BITS 64
#define CFA_P70_ACT_STAT0_B32A_FBC_NUM_BITS 64
#define CFA_P70_ACT_STAT1_B32A_FBC_NUM_BITS 64
#define CFA_P70_ACT_STAT0_B32A_MPC_NUM_BITS 64
#define CFA_P70_ACT_STAT1_B32A_MPC_NUM_BITS 64
#define CFA_P70_ACT_STAT0_B32A_MBC_NUM_BITS 64
#define CFA_P70_ACT_STAT1_B32A_MBC_NUM_BITS 64
#define CFA_P70_ACT_STAT0_B32B_FPC_NUM_BITS 64
#define CFA_P70_ACT_STAT1_B32B_FPC_NUM_BITS 64
#define CFA_P70_ACT_STAT0_B32B_FBC_NUM_BITS 64
#define CFA_P70_ACT_STAT1_B32B_FBC_NUM_BITS 64
#define CFA_P70_ACT_STAT0_B32B_TIMESTAMP_NUM_BITS 32
#define CFA_P70_ACT_STAT1_B32B_TIMESTAMP_NUM_BITS 32
#define CFA_P70_ACT_STAT0_B32B_TCP_FLAGS_NUM_BITS 9
#define CFA_P70_ACT_STAT1_B32B_TCP_FLAGS_NUM_BITS 9
#define CFA_P70_ACT_STAT0_B32B_UNUSED_0_NUM_BITS 7
#define CFA_P70_ACT_STAT1_B32B_UNUSED_0_NUM_BITS 7
#define CFA_P70_ACT_STAT0_B32B_MPC15_0_NUM_BITS 16
#define CFA_P70_ACT_STAT1_B32B_MPC15_0_NUM_BITS 16
#define CFA_P70_ACT_STAT0_B32B_MPC37_16_NUM_BITS 22
#define CFA_P70_ACT_STAT1_B32B_MPC37_16_NUM_BITS 22
#define CFA_P70_ACT_STAT0_B32B_MBC_NUM_BITS 42
#define CFA_P70_ACT_STAT1_B32B_MBC_NUM_BITS 42

#define CFA_P70_CACHE_LINE_BYTES 32
#define CFA_P70_CACHE_LINE_BITS \
	(CFA_P70_CACHE_LINE_BYTES * BITS_PER_BYTE)

/* clang-format on */

#endif /* _CFA_P70_HW_H_ */
