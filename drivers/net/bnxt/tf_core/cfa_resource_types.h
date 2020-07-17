/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_RESOURCE_TYPES_H_
#define _CFA_RESOURCE_TYPES_H_
/*
 * This is the constant used to define invalid CFA
 * resource types across all devices.
 */
#define CFA_RESOURCE_TYPE_INVALID 65535

/* L2 Context TCAM High priority entries */
#define CFA_RESOURCE_TYPE_P59_L2_CTXT_TCAM_HIGH  0x0UL
/* L2 Context TCAM Low priority entries */
#define CFA_RESOURCE_TYPE_P59_L2_CTXT_TCAM_LOW   0x1UL
/* L2 Context REMAP high priority entries */
#define CFA_RESOURCE_TYPE_P59_L2_CTXT_REMAP_HIGH 0x2UL
/* L2 Context REMAP Low priority entries */
#define CFA_RESOURCE_TYPE_P59_L2_CTXT_REMAP_LOW  0x3UL
/* Profile Func */
#define CFA_RESOURCE_TYPE_P59_PROF_FUNC          0x4UL
/* Profile TCAM */
#define CFA_RESOURCE_TYPE_P59_PROF_TCAM          0x5UL
/* Exact Match Profile Id */
#define CFA_RESOURCE_TYPE_P59_EM_PROF_ID         0x6UL
/* Wildcard TCAM Profile Id */
#define CFA_RESOURCE_TYPE_P59_WC_TCAM_PROF_ID    0x7UL
/* Wildcard TCAM */
#define CFA_RESOURCE_TYPE_P59_WC_TCAM            0x8UL
/* Meter Profile */
#define CFA_RESOURCE_TYPE_P59_METER_PROF         0x9UL
/* Meter */
#define CFA_RESOURCE_TYPE_P59_METER              0xaUL
/* Meter */
#define CFA_RESOURCE_TYPE_P59_MIRROR             0xbUL
/* Source Properties TCAM */
#define CFA_RESOURCE_TYPE_P59_SP_TCAM            0xcUL
/* Exact Match Flexible Key Builder */
#define CFA_RESOURCE_TYPE_P59_EM_FKB             0xdUL
/* Wildcard Flexible Key Builder */
#define CFA_RESOURCE_TYPE_P59_WC_FKB             0xeUL
/* Table Scope */
#define CFA_RESOURCE_TYPE_P59_TBL_SCOPE          0xfUL
/* L2 Func */
#define CFA_RESOURCE_TYPE_P59_L2_FUNC            0x10UL
/* EPOCH 0 */
#define CFA_RESOURCE_TYPE_P59_EPOCH0             0x11UL
/* EPOCH 1 */
#define CFA_RESOURCE_TYPE_P59_EPOCH1             0x12UL
/* Metadata */
#define CFA_RESOURCE_TYPE_P59_METADATA           0x13UL
/* Connection Tracking Rule TCAM */
#define CFA_RESOURCE_TYPE_P59_CT_RULE_TCAM       0x14UL
/* Range Profile */
#define CFA_RESOURCE_TYPE_P59_RANGE_PROF         0x15UL
/* Range */
#define CFA_RESOURCE_TYPE_P59_RANGE              0x16UL
/* Link Aggrigation */
#define CFA_RESOURCE_TYPE_P59_LAG                0x17UL
/* VEB TCAM */
#define CFA_RESOURCE_TYPE_P59_VEB_TCAM           0x18UL
#define CFA_RESOURCE_TYPE_P59_LAST              CFA_RESOURCE_TYPE_P59_VEB_TCAM


/* Multicast Group */
#define CFA_RESOURCE_TYPE_P58_MCG                 0x0UL
/* Encap 8 byte record */
#define CFA_RESOURCE_TYPE_P58_ENCAP_8B            0x1UL
/* Encap 16 byte record */
#define CFA_RESOURCE_TYPE_P58_ENCAP_16B           0x2UL
/* Encap 64 byte record */
#define CFA_RESOURCE_TYPE_P58_ENCAP_64B           0x3UL
/* Source Property MAC */
#define CFA_RESOURCE_TYPE_P58_SP_MAC              0x4UL
/* Source Property MAC and IPv4 */
#define CFA_RESOURCE_TYPE_P58_SP_MAC_IPV4         0x5UL
/* Source Property MAC and IPv6 */
#define CFA_RESOURCE_TYPE_P58_SP_MAC_IPV6         0x6UL
/* Network Address Translation Port */
#define CFA_RESOURCE_TYPE_P58_NAT_PORT            0x7UL
/* Network Address Translation IPv4 address */
#define CFA_RESOURCE_TYPE_P58_NAT_IPV4            0x8UL
/* Meter */
#define CFA_RESOURCE_TYPE_P58_METER               0x9UL
/* Flow State */
#define CFA_RESOURCE_TYPE_P58_FLOW_STATE          0xaUL
/* Full Action Records */
#define CFA_RESOURCE_TYPE_P58_FULL_ACTION         0xbUL
/* Action Record Format 0 */
#define CFA_RESOURCE_TYPE_P58_FORMAT_0_ACTION     0xcUL
/* Action Record Ext Format 0 */
#define CFA_RESOURCE_TYPE_P58_EXT_FORMAT_0_ACTION 0xdUL
/* Action Record Format 1 */
#define CFA_RESOURCE_TYPE_P58_FORMAT_1_ACTION     0xeUL
/* Action Record Format 2 */
#define CFA_RESOURCE_TYPE_P58_FORMAT_2_ACTION     0xfUL
/* Action Record Format 3 */
#define CFA_RESOURCE_TYPE_P58_FORMAT_3_ACTION     0x10UL
/* Action Record Format 4 */
#define CFA_RESOURCE_TYPE_P58_FORMAT_4_ACTION     0x11UL
/* Action Record Format 5 */
#define CFA_RESOURCE_TYPE_P58_FORMAT_5_ACTION     0x12UL
/* Action Record Format 6 */
#define CFA_RESOURCE_TYPE_P58_FORMAT_6_ACTION     0x13UL
/* L2 Context TCAM High priority entries */
#define CFA_RESOURCE_TYPE_P58_L2_CTXT_TCAM_HIGH   0x14UL
/* L2 Context TCAM Low priority entries */
#define CFA_RESOURCE_TYPE_P58_L2_CTXT_TCAM_LOW    0x15UL
/* L2 Context REMAP high priority entries */
#define CFA_RESOURCE_TYPE_P58_L2_CTXT_REMAP_HIGH  0x16UL
/* L2 Context REMAP Low priority entries */
#define CFA_RESOURCE_TYPE_P58_L2_CTXT_REMAP_LOW   0x17UL
/* Profile Func */
#define CFA_RESOURCE_TYPE_P58_PROF_FUNC           0x18UL
/* Profile TCAM */
#define CFA_RESOURCE_TYPE_P58_PROF_TCAM           0x19UL
/* Exact Match Profile Id */
#define CFA_RESOURCE_TYPE_P58_EM_PROF_ID          0x1aUL
/* Wildcard Profile Id */
#define CFA_RESOURCE_TYPE_P58_WC_TCAM_PROF_ID     0x1bUL
/* Exact Match Record */
#define CFA_RESOURCE_TYPE_P58_EM_REC              0x1cUL
/* Wildcard TCAM */
#define CFA_RESOURCE_TYPE_P58_WC_TCAM             0x1dUL
/* Meter profile */
#define CFA_RESOURCE_TYPE_P58_METER_PROF          0x1eUL
/* Meter */
#define CFA_RESOURCE_TYPE_P58_MIRROR              0x1fUL
/* Source Property TCAM */
#define CFA_RESOURCE_TYPE_P58_SP_TCAM             0x20UL
/* Exact Match Flexible Key Builder */
#define CFA_RESOURCE_TYPE_P58_EM_FKB              0x21UL
/* Wildcard Flexible Key Builder */
#define CFA_RESOURCE_TYPE_P58_WC_FKB              0x22UL
/* VEB TCAM */
#define CFA_RESOURCE_TYPE_P58_VEB_TCAM            0x23UL
#define CFA_RESOURCE_TYPE_P58_LAST               CFA_RESOURCE_TYPE_P58_VEB_TCAM


/* Multicast Group */
#define CFA_RESOURCE_TYPE_P45_MCG                 0x0UL
/* Encap 8 byte record */
#define CFA_RESOURCE_TYPE_P45_ENCAP_8B            0x1UL
/* Encap 16 byte record */
#define CFA_RESOURCE_TYPE_P45_ENCAP_16B           0x2UL
/* Encap 64 byte record */
#define CFA_RESOURCE_TYPE_P45_ENCAP_64B           0x3UL
/* Source Property MAC */
#define CFA_RESOURCE_TYPE_P45_SP_MAC              0x4UL
/* Source Property MAC and IPv4 */
#define CFA_RESOURCE_TYPE_P45_SP_MAC_IPV4         0x5UL
/* Source Property MAC and IPv6 */
#define CFA_RESOURCE_TYPE_P45_SP_MAC_IPV6         0x6UL
/* 64B Counters */
#define CFA_RESOURCE_TYPE_P45_COUNTER_64B         0x7UL
/* Network Address Translation Port */
#define CFA_RESOURCE_TYPE_P45_NAT_PORT            0x8UL
/* Network Address Translation IPv4 address */
#define CFA_RESOURCE_TYPE_P45_NAT_IPV4            0x9UL
/* Meter */
#define CFA_RESOURCE_TYPE_P45_METER               0xaUL
/* Flow State */
#define CFA_RESOURCE_TYPE_P45_FLOW_STATE          0xbUL
/* Full Action Records */
#define CFA_RESOURCE_TYPE_P45_FULL_ACTION         0xcUL
/* Action Record Format 0 */
#define CFA_RESOURCE_TYPE_P45_FORMAT_0_ACTION     0xdUL
/* Action Record Ext Format 0 */
#define CFA_RESOURCE_TYPE_P45_EXT_FORMAT_0_ACTION 0xeUL
/* Action Record Format 1 */
#define CFA_RESOURCE_TYPE_P45_FORMAT_1_ACTION     0xfUL
/* Action Record Format 2 */
#define CFA_RESOURCE_TYPE_P45_FORMAT_2_ACTION     0x10UL
/* Action Record Format 3 */
#define CFA_RESOURCE_TYPE_P45_FORMAT_3_ACTION     0x11UL
/* Action Record Format 4 */
#define CFA_RESOURCE_TYPE_P45_FORMAT_4_ACTION     0x12UL
/* Action Record Format 5 */
#define CFA_RESOURCE_TYPE_P45_FORMAT_5_ACTION     0x13UL
/* Action Record Format 6 */
#define CFA_RESOURCE_TYPE_P45_FORMAT_6_ACTION     0x14UL
/* L2 Context TCAM High priority entries */
#define CFA_RESOURCE_TYPE_P45_L2_CTXT_TCAM_HIGH   0x15UL
/* L2 Context TCAM Low priority entries */
#define CFA_RESOURCE_TYPE_P45_L2_CTXT_TCAM_LOW    0x16UL
/* L2 Context REMAP high priority entries */
#define CFA_RESOURCE_TYPE_P45_L2_CTXT_REMAP_HIGH  0x17UL
/* L2 Context REMAP Low priority entries */
#define CFA_RESOURCE_TYPE_P45_L2_CTXT_REMAP_LOW   0x18UL
/* Profile Func */
#define CFA_RESOURCE_TYPE_P45_PROF_FUNC           0x19UL
/* Profile TCAM */
#define CFA_RESOURCE_TYPE_P45_PROF_TCAM           0x1aUL
/* Exact Match Profile Id */
#define CFA_RESOURCE_TYPE_P45_EM_PROF_ID          0x1bUL
/* Exact Match Record */
#define CFA_RESOURCE_TYPE_P45_EM_REC              0x1cUL
/* Wildcard Profile Id */
#define CFA_RESOURCE_TYPE_P45_WC_TCAM_PROF_ID     0x1dUL
/* Wildcard TCAM */
#define CFA_RESOURCE_TYPE_P45_WC_TCAM             0x1eUL
/* Meter profile */
#define CFA_RESOURCE_TYPE_P45_METER_PROF          0x1fUL
/* Meter */
#define CFA_RESOURCE_TYPE_P45_MIRROR              0x20UL
/* Source Property TCAM */
#define CFA_RESOURCE_TYPE_P45_SP_TCAM             0x21UL
/* VEB TCAM */
#define CFA_RESOURCE_TYPE_P45_VEB_TCAM            0x22UL
/* Table Scope */
#define CFA_RESOURCE_TYPE_P45_TBL_SCOPE           0x23UL
#define CFA_RESOURCE_TYPE_P45_LAST               CFA_RESOURCE_TYPE_P45_TBL_SCOPE


/* Multicast Group */
#define CFA_RESOURCE_TYPE_P4_MCG                 0x0UL
/* Encap 8 byte record */
#define CFA_RESOURCE_TYPE_P4_ENCAP_8B            0x1UL
/* Encap 16 byte record */
#define CFA_RESOURCE_TYPE_P4_ENCAP_16B           0x2UL
/* Encap 64 byte record */
#define CFA_RESOURCE_TYPE_P4_ENCAP_64B           0x3UL
/* Source Property MAC */
#define CFA_RESOURCE_TYPE_P4_SP_MAC              0x4UL
/* Source Property MAC and IPv4 */
#define CFA_RESOURCE_TYPE_P4_SP_MAC_IPV4         0x5UL
/* Source Property MAC and IPv6 */
#define CFA_RESOURCE_TYPE_P4_SP_MAC_IPV6         0x6UL
/* 64B Counters */
#define CFA_RESOURCE_TYPE_P4_COUNTER_64B         0x7UL
/* Network Address Translation Port */
#define CFA_RESOURCE_TYPE_P4_NAT_PORT            0x8UL
/* Network Address Translation IPv4 address */
#define CFA_RESOURCE_TYPE_P4_NAT_IPV4            0x9UL
/* Meter */
#define CFA_RESOURCE_TYPE_P4_METER               0xaUL
/* Flow State */
#define CFA_RESOURCE_TYPE_P4_FLOW_STATE          0xbUL
/* Full Action Records */
#define CFA_RESOURCE_TYPE_P4_FULL_ACTION         0xcUL
/* Action Record Format 0 */
#define CFA_RESOURCE_TYPE_P4_FORMAT_0_ACTION     0xdUL
/* Action Record Ext Format 0 */
#define CFA_RESOURCE_TYPE_P4_EXT_FORMAT_0_ACTION 0xeUL
/* Action Record Format 1 */
#define CFA_RESOURCE_TYPE_P4_FORMAT_1_ACTION     0xfUL
/* Action Record Format 2 */
#define CFA_RESOURCE_TYPE_P4_FORMAT_2_ACTION     0x10UL
/* Action Record Format 3 */
#define CFA_RESOURCE_TYPE_P4_FORMAT_3_ACTION     0x11UL
/* Action Record Format 4 */
#define CFA_RESOURCE_TYPE_P4_FORMAT_4_ACTION     0x12UL
/* Action Record Format 5 */
#define CFA_RESOURCE_TYPE_P4_FORMAT_5_ACTION     0x13UL
/* Action Record Format 6 */
#define CFA_RESOURCE_TYPE_P4_FORMAT_6_ACTION     0x14UL
/* L2 Context TCAM High priority entries */
#define CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_HIGH   0x15UL
/* L2 Context TCAM Low priority entries */
#define CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_LOW    0x16UL
/* L2 Context REMAP high priority entries */
#define CFA_RESOURCE_TYPE_P4_L2_CTXT_REMAP_HIGH  0x17UL
/* L2 Context REMAP Low priority entries */
#define CFA_RESOURCE_TYPE_P4_L2_CTXT_REMAP_LOW   0x18UL
/* Profile Func */
#define CFA_RESOURCE_TYPE_P4_PROF_FUNC           0x19UL
/* Profile TCAM */
#define CFA_RESOURCE_TYPE_P4_PROF_TCAM           0x1aUL
/* Exact Match Profile Id */
#define CFA_RESOURCE_TYPE_P4_EM_PROF_ID          0x1bUL
/* Exact Match Record */
#define CFA_RESOURCE_TYPE_P4_EM_REC              0x1cUL
/* Wildcard Profile Id */
#define CFA_RESOURCE_TYPE_P4_WC_TCAM_PROF_ID     0x1dUL
/* Wildcard TCAM */
#define CFA_RESOURCE_TYPE_P4_WC_TCAM             0x1eUL
/* Meter profile */
#define CFA_RESOURCE_TYPE_P4_METER_PROF          0x1fUL
/* Meter */
#define CFA_RESOURCE_TYPE_P4_MIRROR              0x20UL
/* Source Property TCAM */
#define CFA_RESOURCE_TYPE_P4_SP_TCAM             0x21UL
/* Table Scope */
#define CFA_RESOURCE_TYPE_P4_TBL_SCOPE           0x22UL
#define CFA_RESOURCE_TYPE_P4_LAST               CFA_RESOURCE_TYPE_P4_TBL_SCOPE


#endif /* _CFA_RESOURCE_TYPES_H_ */
