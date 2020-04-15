/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_RESOURCES_H_
#define _TF_RESOURCES_H_

/*
 * Hardware specific MAX values
 * NOTE: Should really come from the chip_cfg.h in some MAX form or HCAPI
 */

/* Common HW resources for all chip variants */
#define TF_NUM_L2_CTXT_TCAM      1024      /* < Number of L2 context TCAM
					    * entries
					    */
#define TF_NUM_PROF_FUNC          128      /* < Number prof_func ID */
#define TF_NUM_PROF_TCAM         1024      /* < Number entries in profile
					    * TCAM
					    */
#define TF_NUM_EM_PROF_ID          64      /* < Number software EM Profile
					    * IDs
					    */
#define TF_NUM_WC_PROF_ID         256      /* < Number WC profile IDs */
#define TF_NUM_WC_TCAM_ROW        256      /*  Number slices per row in WC
					    * TCAM. A slices is a WC TCAM entry.
					    */
#define TF_NUM_METER_PROF         256      /* < Number of meter profiles */
#define TF_NUM_METER             1024      /* < Number of meter instances */
#define TF_NUM_MIRROR               2      /* < Number of mirror instances */
#define TF_NUM_UPAR                 2      /* < Number of UPAR instances */

/* Wh+/Brd2 specific HW resources */
#define TF_NUM_SP_TCAM            512      /* < Number of Source Property TCAM
					    * entries
					    */

/* Brd2/Brd4 specific HW resources */
#define TF_NUM_L2_FUNC            256      /* < Number of L2 Func */


/* Brd3, Brd4 common HW resources */
#define TF_NUM_FKB                  1      /* < Number of Flexible Key Builder
					    * templates
					    */

/* Brd4 specific HW resources */
#define TF_NUM_TBL_SCOPE           16      /* < Number of TBL scopes */
#define TF_NUM_EPOCH0               1      /* < Number of Epoch0 */
#define TF_NUM_EPOCH1               1      /* < Number of Epoch1 */
#define TF_NUM_METADATA             8      /* < Number of MetaData Profiles */
#define TF_NUM_CT_STATE            32      /* < Number of Connection Tracking
					    * States
					    */
#define TF_NUM_RANGE_PROF          16      /* < Number of Range Profiles */
#define TF_NUM_RANGE_ENTRY (64 * 1024)     /* < Number of Range Entries */
#define TF_NUM_LAG_ENTRY          256      /* < Number of LAG Entries */

/*
 * Common for the Reserved Resource defines below:
 *
 * - HW Resources
 *   For resources where a priority level plays a role, i.e. l2 ctx
 *   tcam entries, both a number of resources and a begin/end pair is
 *   required. The begin/end is used to assure TFLIB gets the correct
 *   priority setting for that resource.
 *
 *   For EM records there is no priority required thus a number of
 *   resources is sufficient.
 *
 *   Example, TCAM:
 *     64 L2 CTXT TCAM entries would in a max 1024 pool be entry
 *     0-63 as HW presents 0 as the highest priority entry.
 *
 * - SRAM Resources
 *   Handled as regular resources as there is no priority required.
 *
 * Common for these resources is that they are handled per direction,
 * rx/tx.
 */

/* HW Resources */

/* L2 CTX */
#define TF_RSVD_L2_CTXT_TCAM_RX                   64
#define TF_RSVD_L2_CTXT_TCAM_BEGIN_IDX_RX         0
#define TF_RSVD_L2_CTXT_TCAM_END_IDX_RX           (TF_RSVD_L2_CTXT_RX - 1)
#define TF_RSVD_L2_CTXT_TCAM_TX                   960
#define TF_RSVD_L2_CTXT_TCAM_BEGIN_IDX_TX         0
#define TF_RSVD_L2_CTXT_TCAM_END_IDX_TX           (TF_RSVD_L2_CTXT_TX - 1)

/* Profiler */
#define TF_RSVD_PROF_FUNC_RX                      64
#define TF_RSVD_PROF_FUNC_BEGIN_IDX_RX            64
#define TF_RSVD_PROF_FUNC_END_IDX_RX              127
#define TF_RSVD_PROF_FUNC_TX                      64
#define TF_RSVD_PROF_FUNC_BEGIN_IDX_TX            64
#define TF_RSVD_PROF_FUNC_END_IDX_TX              127

#define TF_RSVD_PROF_TCAM_RX                      64
#define TF_RSVD_PROF_TCAM_BEGIN_IDX_RX            960
#define TF_RSVD_PROF_TCAM_END_IDX_RX              1023
#define TF_RSVD_PROF_TCAM_TX                      64
#define TF_RSVD_PROF_TCAM_BEGIN_IDX_TX            960
#define TF_RSVD_PROF_TCAM_END_IDX_TX              1023

/* EM Profiles IDs */
#define TF_RSVD_EM_PROF_ID_RX                     64
#define TF_RSVD_EM_PROF_ID_BEGIN_IDX_RX           0
#define TF_RSVD_EM_PROF_ID_END_IDX_RX             63  /* Less on CU+ then SR */
#define TF_RSVD_EM_PROF_ID_TX                     64
#define TF_RSVD_EM_PROF_ID_BEGIN_IDX_TX           0
#define TF_RSVD_EM_PROF_ID_END_IDX_TX             63  /* Less on CU+ then SR */

/* EM Records */
#define TF_RSVD_EM_REC_RX                         16000
#define TF_RSVD_EM_REC_BEGIN_IDX_RX               0
#define TF_RSVD_EM_REC_TX                         16000
#define TF_RSVD_EM_REC_BEGIN_IDX_TX               0

/* Wildcard */
#define TF_RSVD_WC_TCAM_PROF_ID_RX                128
#define TF_RSVD_WC_TCAM_PROF_ID_BEGIN_IDX_RX      128
#define TF_RSVD_WC_TCAM_PROF_ID_END_IDX_RX        255
#define TF_RSVD_WC_TCAM_PROF_ID_TX                128
#define TF_RSVD_WC_TCAM_PROF_ID_BEGIN_IDX_TX      128
#define TF_RSVD_WC_TCAM_PROF_ID_END_IDX_TX        255

#define TF_RSVD_WC_TCAM_RX                        64
#define TF_RSVD_WC_TCAM_BEGIN_IDX_RX              0
#define TF_RSVD_WC_TCAM_END_IDX_RX                63
#define TF_RSVD_WC_TCAM_TX                        64
#define TF_RSVD_WC_TCAM_BEGIN_IDX_TX              0
#define TF_RSVD_WC_TCAM_END_IDX_TX                63

#define TF_RSVD_METER_PROF_RX                     0
#define TF_RSVD_METER_PROF_BEGIN_IDX_RX           0
#define TF_RSVD_METER_PROF_END_IDX_RX             0
#define TF_RSVD_METER_PROF_TX                     0
#define TF_RSVD_METER_PROF_BEGIN_IDX_TX           0
#define TF_RSVD_METER_PROF_END_IDX_TX             0

#define TF_RSVD_METER_INST_RX                     0
#define TF_RSVD_METER_INST_BEGIN_IDX_RX           0
#define TF_RSVD_METER_INST_END_IDX_RX             0
#define TF_RSVD_METER_INST_TX                     0
#define TF_RSVD_METER_INST_BEGIN_IDX_TX           0
#define TF_RSVD_METER_INST_END_IDX_TX             0

/* Mirror */
/* Not yet supported fully in the infra */
#define TF_RSVD_MIRROR_RX                         0
#define TF_RSVD_MIRROR_BEGIN_IDX_RX               0
#define TF_RSVD_MIRROR_END_IDX_RX                 0
#define TF_RSVD_MIRROR_TX                         0
#define TF_RSVD_MIRROR_BEGIN_IDX_TX               0
#define TF_RSVD_MIRROR_END_IDX_TX                 0

/* UPAR */
/* Not yet supported fully in the infra */
#define TF_RSVD_UPAR_RX                           0
#define TF_RSVD_UPAR_BEGIN_IDX_RX                 0
#define TF_RSVD_UPAR_END_IDX_RX                   0
#define TF_RSVD_UPAR_TX                           0
#define TF_RSVD_UPAR_BEGIN_IDX_TX                 0
#define TF_RSVD_UPAR_END_IDX_TX                   0

/* Source Properties */
/* Not yet supported fully in the infra */
#define TF_RSVD_SP_TCAM_RX                        0
#define TF_RSVD_SP_TCAM_BEGIN_IDX_RX              0
#define TF_RSVD_SP_TCAM_END_IDX_RX                0
#define TF_RSVD_SP_TCAM_TX                        0
#define TF_RSVD_SP_TCAM_BEGIN_IDX_TX              0
#define TF_RSVD_SP_TCAM_END_IDX_TX                0

/* L2 Func */
#define TF_RSVD_L2_FUNC_RX                        0
#define TF_RSVD_L2_FUNC_BEGIN_IDX_RX              0
#define TF_RSVD_L2_FUNC_END_IDX_RX                0
#define TF_RSVD_L2_FUNC_TX                        0
#define TF_RSVD_L2_FUNC_BEGIN_IDX_TX              0
#define TF_RSVD_L2_FUNC_END_IDX_TX                0

/* FKB */
#define TF_RSVD_FKB_RX                            0
#define TF_RSVD_FKB_BEGIN_IDX_RX                  0
#define TF_RSVD_FKB_END_IDX_RX                    0
#define TF_RSVD_FKB_TX                            0
#define TF_RSVD_FKB_BEGIN_IDX_TX                  0
#define TF_RSVD_FKB_END_IDX_TX                    0

/* TBL Scope */
#define TF_RSVD_TBL_SCOPE_RX                      1
#define TF_RSVD_TBL_SCOPE_BEGIN_IDX_RX            0
#define TF_RSVD_TBL_SCOPE_END_IDX_RX              1
#define TF_RSVD_TBL_SCOPE_TX                      1
#define TF_RSVD_TBL_SCOPE_BEGIN_IDX_TX            0
#define TF_RSVD_TBL_SCOPE_END_IDX_TX              1

/* EPOCH0 */
/* Not yet supported fully in the infra */
#define TF_RSVD_EPOCH0_RX                         0
#define TF_RSVD_EPOCH0_BEGIN_IDX_RX               0
#define TF_RSVD_EPOCH0_END_IDX_RX                 0
#define TF_RSVD_EPOCH0_TX                         0
#define TF_RSVD_EPOCH0_BEGIN_IDX_TX               0
#define TF_RSVD_EPOCH0_END_IDX_TX                 0

/* EPOCH1 */
/* Not yet supported fully in the infra */
#define TF_RSVD_EPOCH1_RX                         0
#define TF_RSVD_EPOCH1_BEGIN_IDX_RX               0
#define TF_RSVD_EPOCH1_END_IDX_RX                 0
#define TF_RSVD_EPOCH1_TX                         0
#define TF_RSVD_EPOCH1_BEGIN_IDX_TX               0
#define TF_RSVD_EPOCH1_END_IDX_TX                 0

/* METADATA */
/* Not yet supported fully in the infra */
#define TF_RSVD_METADATA_RX                       0
#define TF_RSVD_METADATA_BEGIN_IDX_RX             0
#define TF_RSVD_METADATA_END_IDX_RX               0
#define TF_RSVD_METADATA_TX                       0
#define TF_RSVD_METADATA_BEGIN_IDX_TX             0
#define TF_RSVD_METADATA_END_IDX_TX               0

/* CT_STATE */
/* Not yet supported fully in the infra */
#define TF_RSVD_CT_STATE_RX                       0
#define TF_RSVD_CT_STATE_BEGIN_IDX_RX             0
#define TF_RSVD_CT_STATE_END_IDX_RX               0
#define TF_RSVD_CT_STATE_TX                       0
#define TF_RSVD_CT_STATE_BEGIN_IDX_TX             0
#define TF_RSVD_CT_STATE_END_IDX_TX               0

/* RANGE_PROF */
/* Not yet supported fully in the infra */
#define TF_RSVD_RANGE_PROF_RX                     0
#define TF_RSVD_RANGE_PROF_BEGIN_IDX_RX           0
#define TF_RSVD_RANGE_PROF_END_IDX_RX             0
#define TF_RSVD_RANGE_PROF_TX                     0
#define TF_RSVD_RANGE_PROF_BEGIN_IDX_TX           0
#define TF_RSVD_RANGE_PROF_END_IDX_TX             0

/* RANGE_ENTRY */
/* Not yet supported fully in the infra */
#define TF_RSVD_RANGE_ENTRY_RX                    0
#define TF_RSVD_RANGE_ENTRY_BEGIN_IDX_RX          0
#define TF_RSVD_RANGE_ENTRY_END_IDX_RX            0
#define TF_RSVD_RANGE_ENTRY_TX                    0
#define TF_RSVD_RANGE_ENTRY_BEGIN_IDX_TX          0
#define TF_RSVD_RANGE_ENTRY_END_IDX_TX            0

/* LAG_ENTRY */
/* Not yet supported fully in the infra */
#define TF_RSVD_LAG_ENTRY_RX                      0
#define TF_RSVD_LAG_ENTRY_BEGIN_IDX_RX            0
#define TF_RSVD_LAG_ENTRY_END_IDX_RX              0
#define TF_RSVD_LAG_ENTRY_TX                      0
#define TF_RSVD_LAG_ENTRY_BEGIN_IDX_TX            0
#define TF_RSVD_LAG_ENTRY_END_IDX_TX              0


/* SRAM - Resources
 * Limited to the types that CFA provides.
 */
#define TF_RSVD_SRAM_FULL_ACTION_RX               8001
#define TF_RSVD_SRAM_FULL_ACTION_BEGIN_IDX_RX     0
#define TF_RSVD_SRAM_FULL_ACTION_TX               8001
#define TF_RSVD_SRAM_FULL_ACTION_BEGIN_IDX_TX     0

/* Not yet supported fully in the infra */
#define TF_RSVD_SRAM_MCG_RX                       0
#define TF_RSVD_SRAM_MCG_BEGIN_IDX_RX             0
/* Multicast Group on TX is not supported */
#define TF_RSVD_SRAM_MCG_TX                       0
#define TF_RSVD_SRAM_MCG_BEGIN_IDX_TX             0

/* First encap of 8B RX is reserved by CFA */
#define TF_RSVD_SRAM_ENCAP_8B_RX                  32
#define TF_RSVD_SRAM_ENCAP_8B_BEGIN_IDX_RX        0
/* First encap of 8B TX is reserved by CFA */
#define TF_RSVD_SRAM_ENCAP_8B_TX                  0
#define TF_RSVD_SRAM_ENCAP_8B_BEGIN_IDX_TX        0

#define TF_RSVD_SRAM_ENCAP_16B_RX                 16
#define TF_RSVD_SRAM_ENCAP_16B_BEGIN_IDX_RX       0
/* First encap of 16B TX is reserved by CFA */
#define TF_RSVD_SRAM_ENCAP_16B_TX                 20
#define TF_RSVD_SRAM_ENCAP_16B_BEGIN_IDX_TX       0

/* Encap of 64B on RX is not supported */
#define TF_RSVD_SRAM_ENCAP_64B_RX                 0
#define TF_RSVD_SRAM_ENCAP_64B_BEGIN_IDX_RX       0
/* First encap of 64B TX is reserved by CFA */
#define TF_RSVD_SRAM_ENCAP_64B_TX                 1007
#define TF_RSVD_SRAM_ENCAP_64B_BEGIN_IDX_TX       0

#define TF_RSVD_SRAM_SP_SMAC_RX                   0
#define TF_RSVD_SRAM_SP_SMAC_BEGIN_IDX_RX         0
#define TF_RSVD_SRAM_SP_SMAC_TX                   0
#define TF_RSVD_SRAM_SP_SMAC_BEGIN_IDX_TX         0

/* SRAM SP IPV4 on RX is not supported */
#define TF_RSVD_SRAM_SP_SMAC_IPV4_RX              0
#define TF_RSVD_SRAM_SP_SMAC_IPV4_BEGIN_IDX_RX    0
#define TF_RSVD_SRAM_SP_SMAC_IPV4_TX              511
#define TF_RSVD_SRAM_SP_SMAC_IPV4_BEGIN_IDX_TX    0

/* SRAM SP IPV6 on RX is not supported */
#define TF_RSVD_SRAM_SP_SMAC_IPV6_RX              0
#define TF_RSVD_SRAM_SP_SMAC_IPV6_BEGIN_IDX_RX    0
/* Not yet supported fully in infra */
#define TF_RSVD_SRAM_SP_SMAC_IPV6_TX              0
#define TF_RSVD_SRAM_SP_SMAC_IPV6_BEGIN_IDX_TX    0

#define TF_RSVD_SRAM_COUNTER_64B_RX               160
#define TF_RSVD_SRAM_COUNTER_64B_BEGIN_IDX_RX     0
#define TF_RSVD_SRAM_COUNTER_64B_TX               160
#define TF_RSVD_SRAM_COUNTER_64B_BEGIN_IDX_TX     0

#define TF_RSVD_SRAM_NAT_SPORT_RX                 0
#define TF_RSVD_SRAM_NAT_SPORT_BEGIN_IDX_RX       0
#define TF_RSVD_SRAM_NAT_SPORT_TX                 0
#define TF_RSVD_SRAM_NAT_SPORT_BEGIN_IDX_TX       0

#define TF_RSVD_SRAM_NAT_DPORT_RX                 0
#define TF_RSVD_SRAM_NAT_DPORT_BEGIN_IDX_RX       0
#define TF_RSVD_SRAM_NAT_DPORT_TX                 0
#define TF_RSVD_SRAM_NAT_DPORT_BEGIN_IDX_TX       0

#define TF_RSVD_SRAM_NAT_S_IPV4_RX                0
#define TF_RSVD_SRAM_NAT_S_IPV4_BEGIN_IDX_RX      0
#define TF_RSVD_SRAM_NAT_S_IPV4_TX                0
#define TF_RSVD_SRAM_NAT_S_IPV4_BEGIN_IDX_TX      0

#define TF_RSVD_SRAM_NAT_D_IPV4_RX                0
#define TF_RSVD_SRAM_NAT_D_IPV4_BEGIN_IDX_RX      0
#define TF_RSVD_SRAM_NAT_D_IPV4_TX                0
#define TF_RSVD_SRAM_NAT_D_IPV4_BEGIN_IDX_TX      0

/* HW Resource Pool names */

#define TF_L2_CTXT_TCAM_POOL_NAME         l2_ctxt_tcam_pool
#define TF_L2_CTXT_TCAM_POOL_NAME_RX      l2_ctxt_tcam_pool_rx
#define TF_L2_CTXT_TCAM_POOL_NAME_TX      l2_ctxt_tcam_pool_tx

#define TF_PROF_FUNC_POOL_NAME            prof_func_pool
#define TF_PROF_FUNC_POOL_NAME_RX         prof_func_pool_rx
#define TF_PROF_FUNC_POOL_NAME_TX         prof_func_pool_tx

#define TF_PROF_TCAM_POOL_NAME            prof_tcam_pool
#define TF_PROF_TCAM_POOL_NAME_RX         prof_tcam_pool_rx
#define TF_PROF_TCAM_POOL_NAME_TX         prof_tcam_pool_tx

#define TF_EM_PROF_ID_POOL_NAME           em_prof_id_pool
#define TF_EM_PROF_ID_POOL_NAME_RX        em_prof_id_pool_rx
#define TF_EM_PROF_ID_POOL_NAME_TX        em_prof_id_pool_tx

#define TF_WC_TCAM_PROF_ID_POOL_NAME      wc_tcam_prof_id_pool
#define TF_WC_TCAM_PROF_ID_POOL_NAME_RX   wc_tcam_prof_id_pool_rx
#define TF_WC_TCAM_PROF_ID_POOL_NAME_TX   wc_tcam_prof_id_pool_tx

#define TF_WC_TCAM_POOL_NAME              wc_tcam_pool
#define TF_WC_TCAM_POOL_NAME_RX           wc_tcam_pool_rx
#define TF_WC_TCAM_POOL_NAME_TX           wc_tcam_pool_tx

#define TF_METER_PROF_POOL_NAME           meter_prof_pool
#define TF_METER_PROF_POOL_NAME_RX        meter_prof_pool_rx
#define TF_METER_PROF_POOL_NAME_TX        meter_prof_pool_tx

#define TF_METER_INST_POOL_NAME           meter_inst_pool
#define TF_METER_INST_POOL_NAME_RX        meter_inst_pool_rx
#define TF_METER_INST_POOL_NAME_TX        meter_inst_pool_tx

#define TF_MIRROR_POOL_NAME               mirror_pool
#define TF_MIRROR_POOL_NAME_RX            mirror_pool_rx
#define TF_MIRROR_POOL_NAME_TX            mirror_pool_tx

#define TF_UPAR_POOL_NAME                 upar_pool
#define TF_UPAR_POOL_NAME_RX              upar_pool_rx
#define TF_UPAR_POOL_NAME_TX              upar_pool_tx

#define TF_SP_TCAM_POOL_NAME              sp_tcam_pool
#define TF_SP_TCAM_POOL_NAME_RX           sp_tcam_pool_rx
#define TF_SP_TCAM_POOL_NAME_TX           sp_tcam_pool_tx

#define TF_FKB_POOL_NAME                  fkb_pool
#define TF_FKB_POOL_NAME_RX               fkb_pool_rx
#define TF_FKB_POOL_NAME_TX               fkb_pool_tx

#define TF_TBL_SCOPE_POOL_NAME            tbl_scope_pool
#define TF_TBL_SCOPE_POOL_NAME_RX         tbl_scope_pool_rx
#define TF_TBL_SCOPE_POOL_NAME_TX         tbl_scope_pool_tx

#define TF_L2_FUNC_POOL_NAME              l2_func_pool
#define TF_L2_FUNC_POOL_NAME_RX           l2_func_pool_rx
#define TF_L2_FUNC_POOL_NAME_TX           l2_func_pool_tx

#define TF_EPOCH0_POOL_NAME               epoch0_pool
#define TF_EPOCH0_POOL_NAME_RX            epoch0_pool_rx
#define TF_EPOCH0_POOL_NAME_TX            epoch0_pool_tx

#define TF_EPOCH1_POOL_NAME               epoch1_pool
#define TF_EPOCH1_POOL_NAME_RX            epoch1_pool_rx
#define TF_EPOCH1_POOL_NAME_TX            epoch1_pool_tx

#define TF_METADATA_POOL_NAME             metadata_pool
#define TF_METADATA_POOL_NAME_RX          metadata_pool_rx
#define TF_METADATA_POOL_NAME_TX          metadata_pool_tx

#define TF_CT_STATE_POOL_NAME             ct_state_pool
#define TF_CT_STATE_POOL_NAME_RX          ct_state_pool_rx
#define TF_CT_STATE_POOL_NAME_TX          ct_state_pool_tx

#define TF_RANGE_PROF_POOL_NAME           range_prof_pool
#define TF_RANGE_PROF_POOL_NAME_RX        range_prof_pool_rx
#define TF_RANGE_PROF_POOL_NAME_TX        range_prof_pool_tx

#define TF_RANGE_ENTRY_POOL_NAME          range_entry_pool
#define TF_RANGE_ENTRY_POOL_NAME_RX       range_entry_pool_rx
#define TF_RANGE_ENTRY_POOL_NAME_TX       range_entry_pool_tx

#define TF_LAG_ENTRY_POOL_NAME            lag_entry_pool
#define TF_LAG_ENTRY_POOL_NAME_RX         lag_entry_pool_rx
#define TF_LAG_ENTRY_POOL_NAME_TX         lag_entry_pool_tx

/* SRAM Resource Pool names */
#define TF_SRAM_FULL_ACTION_POOL_NAME     sram_full_action_pool
#define TF_SRAM_FULL_ACTION_POOL_NAME_RX  sram_full_action_pool_rx
#define TF_SRAM_FULL_ACTION_POOL_NAME_TX  sram_full_action_pool_tx

#define TF_SRAM_MCG_POOL_NAME             sram_mcg_pool
#define TF_SRAM_MCG_POOL_NAME_RX          sram_mcg_pool_rx
#define TF_SRAM_MCG_POOL_NAME_TX          sram_mcg_pool_tx

#define TF_SRAM_ENCAP_8B_POOL_NAME        sram_encap_8b_pool
#define TF_SRAM_ENCAP_8B_POOL_NAME_RX     sram_encap_8b_pool_rx
#define TF_SRAM_ENCAP_8B_POOL_NAME_TX     sram_encap_8b_pool_tx

#define TF_SRAM_ENCAP_16B_POOL_NAME       sram_encap_16b_pool
#define TF_SRAM_ENCAP_16B_POOL_NAME_RX    sram_encap_16b_pool_rx
#define TF_SRAM_ENCAP_16B_POOL_NAME_TX    sram_encap_16b_pool_tx

#define TF_SRAM_ENCAP_64B_POOL_NAME       sram_encap_64b_pool
#define TF_SRAM_ENCAP_64B_POOL_NAME_RX    sram_encap_64b_pool_rx
#define TF_SRAM_ENCAP_64B_POOL_NAME_TX    sram_encap_64b_pool_tx

#define TF_SRAM_SP_SMAC_POOL_NAME         sram_sp_smac_pool
#define TF_SRAM_SP_SMAC_POOL_NAME_RX      sram_sp_smac_pool_rx
#define TF_SRAM_SP_SMAC_POOL_NAME_TX      sram_sp_smac_pool_tx

#define TF_SRAM_SP_SMAC_IPV4_POOL_NAME    sram_sp_smac_ipv4_pool
#define TF_SRAM_SP_SMAC_IPV4_POOL_NAME_RX sram_sp_smac_ipv4_pool_rx
#define TF_SRAM_SP_SMAC_IPV4_POOL_NAME_TX sram_sp_smac_ipv4_pool_tx

#define TF_SRAM_SP_SMAC_IPV6_POOL_NAME    sram_sp_smac_ipv6_pool
#define TF_SRAM_SP_SMAC_IPV6_POOL_NAME_RX sram_sp_smac_ipv6_pool_rx
#define TF_SRAM_SP_SMAC_IPV6_POOL_NAME_TX sram_sp_smac_ipv6_pool_tx

#define TF_SRAM_STATS_64B_POOL_NAME       sram_stats_64b_pool
#define TF_SRAM_STATS_64B_POOL_NAME_RX    sram_stats_64b_pool_rx
#define TF_SRAM_STATS_64B_POOL_NAME_TX    sram_stats_64b_pool_tx

#define TF_SRAM_NAT_SPORT_POOL_NAME       sram_nat_sport_pool
#define TF_SRAM_NAT_SPORT_POOL_NAME_RX    sram_nat_sport_pool_rx
#define TF_SRAM_NAT_SPORT_POOL_NAME_TX    sram_nat_sport_pool_tx

#define TF_SRAM_NAT_DPORT_POOL_NAME       sram_nat_dport_pool
#define TF_SRAM_NAT_DPORT_POOL_NAME_RX    sram_nat_dport_pool_rx
#define TF_SRAM_NAT_DPORT_POOL_NAME_TX    sram_nat_dport_pool_tx

#define TF_SRAM_NAT_S_IPV4_POOL_NAME      sram_nat_s_ipv4_pool
#define TF_SRAM_NAT_S_IPV4_POOL_NAME_RX   sram_nat_s_ipv4_pool_rx
#define TF_SRAM_NAT_S_IPV4_POOL_NAME_TX   sram_nat_s_ipv4_pool_tx

#define TF_SRAM_NAT_D_IPV4_POOL_NAME      sram_nat_d_ipv4_pool
#define TF_SRAM_NAT_D_IPV4_POOL_NAME_RX   sram_nat_d_ipv4_pool_rx
#define TF_SRAM_NAT_D_IPV4_POOL_NAME_TX   sram_nat_d_ipv4_pool_tx

/* Sw Resource Pool Names */

#define TF_L2_CTXT_REMAP_POOL_NAME         l2_ctxt_remap_pool
#define TF_L2_CTXT_REMAP_POOL_NAME_RX      l2_ctxt_remap_pool_rx
#define TF_L2_CTXT_REMAP_POOL_NAME_TX      l2_ctxt_remap_pool_tx


/** HW Resource types
 */
enum tf_resource_type_hw {
	/* Common HW resources for all chip variants */
	TF_RESC_TYPE_HW_L2_CTXT_TCAM,
	TF_RESC_TYPE_HW_PROF_FUNC,
	TF_RESC_TYPE_HW_PROF_TCAM,
	TF_RESC_TYPE_HW_EM_PROF_ID,
	TF_RESC_TYPE_HW_EM_REC,
	TF_RESC_TYPE_HW_WC_TCAM_PROF_ID,
	TF_RESC_TYPE_HW_WC_TCAM,
	TF_RESC_TYPE_HW_METER_PROF,
	TF_RESC_TYPE_HW_METER_INST,
	TF_RESC_TYPE_HW_MIRROR,
	TF_RESC_TYPE_HW_UPAR,
	/* Wh+/Brd2 specific HW resources */
	TF_RESC_TYPE_HW_SP_TCAM,
	/* Brd2/Brd4 specific HW resources */
	TF_RESC_TYPE_HW_L2_FUNC,
	/* Brd3, Brd4 common HW resources */
	TF_RESC_TYPE_HW_FKB,
	/* Brd4 specific HW resources */
	TF_RESC_TYPE_HW_TBL_SCOPE,
	TF_RESC_TYPE_HW_EPOCH0,
	TF_RESC_TYPE_HW_EPOCH1,
	TF_RESC_TYPE_HW_METADATA,
	TF_RESC_TYPE_HW_CT_STATE,
	TF_RESC_TYPE_HW_RANGE_PROF,
	TF_RESC_TYPE_HW_RANGE_ENTRY,
	TF_RESC_TYPE_HW_LAG_ENTRY,
	TF_RESC_TYPE_HW_MAX
};

/** HW Resource types
 */
enum tf_resource_type_sram {
	TF_RESC_TYPE_SRAM_FULL_ACTION,
	TF_RESC_TYPE_SRAM_MCG,
	TF_RESC_TYPE_SRAM_ENCAP_8B,
	TF_RESC_TYPE_SRAM_ENCAP_16B,
	TF_RESC_TYPE_SRAM_ENCAP_64B,
	TF_RESC_TYPE_SRAM_SP_SMAC,
	TF_RESC_TYPE_SRAM_SP_SMAC_IPV4,
	TF_RESC_TYPE_SRAM_SP_SMAC_IPV6,
	TF_RESC_TYPE_SRAM_COUNTER_64B,
	TF_RESC_TYPE_SRAM_NAT_SPORT,
	TF_RESC_TYPE_SRAM_NAT_DPORT,
	TF_RESC_TYPE_SRAM_NAT_S_IPV4,
	TF_RESC_TYPE_SRAM_NAT_D_IPV4,
	TF_RESC_TYPE_SRAM_MAX
};

#endif /* _TF_RESOURCES_H_ */
