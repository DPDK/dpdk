/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */
#ifndef _HWRM_TF_H_
#define _HWRM_TF_H_

#include "tf_core.h"

typedef enum tf_type {
	TF_TYPE_TRUFLOW,
	TF_TYPE_LAST = TF_TYPE_TRUFLOW,
} tf_type_t;

typedef enum tf_subtype {
	HWRM_TFT_SESSION_ATTACH = 712,
	HWRM_TFT_SESSION_HW_RESC_QCAPS = 721,
	HWRM_TFT_SESSION_HW_RESC_ALLOC = 722,
	HWRM_TFT_SESSION_HW_RESC_FREE = 723,
	HWRM_TFT_SESSION_HW_RESC_FLUSH = 724,
	HWRM_TFT_SESSION_SRAM_RESC_QCAPS = 725,
	HWRM_TFT_SESSION_SRAM_RESC_ALLOC = 726,
	HWRM_TFT_SESSION_SRAM_RESC_FREE = 727,
	HWRM_TFT_SESSION_SRAM_RESC_FLUSH = 728,
	HWRM_TFT_TBL_SCOPE_CFG = 731,
	HWRM_TFT_EM_RULE_INSERT = 739,
	HWRM_TFT_EM_RULE_DELETE = 740,
	HWRM_TFT_REG_GET = 821,
	HWRM_TFT_REG_SET = 822,
	HWRM_TFT_TBL_TYPE_SET = 823,
	HWRM_TFT_TBL_TYPE_GET = 824,
	TF_SUBTYPE_LAST = HWRM_TFT_TBL_TYPE_GET,
} tf_subtype_t;

/* Request and Response compile time checking */
/* u32_t	tlv_req_value[26]; */
#define TF_MAX_REQ_SIZE 104
/* u32_t	tlv_resp_value[170]; */
#define TF_MAX_RESP_SIZE 680

/* Use this to allocate/free any kind of
 * indexes over HWRM and fill the parms pointer
 */
#define TF_BULK_RECV	 128
#define TF_BULK_SEND	  16

/* EM Key value */
#define TF_DEV_DATA_TYPE_TF_EM_RULE_INSERT_KEY_DATA 0x2e30UL
/* EM Key value */
#define TF_DEV_DATA_TYPE_TF_EM_RULE_DELETE_KEY_DATA 0x2e40UL
/* L2 Context DMA Address Type */
#define TF_DEV_DATA_TYPE_TF_L2_CTX_DMA_ADDR		0x2fe0UL
/* L2 Context Entry */
#define TF_DEV_DATA_TYPE_TF_L2_CTX_ENTRY		0x2fe1UL
/* Prof tcam DMA Address Type */
#define TF_DEV_DATA_TYPE_TF_PROF_TCAM_DMA_ADDR		0x3030UL
/* Prof tcam Entry */
#define TF_DEV_DATA_TYPE_TF_PROF_TCAM_ENTRY		0x3031UL
/* WC DMA Address Type */
#define TF_DEV_DATA_TYPE_TF_WC_DMA_ADDR			0x30d0UL
/* WC Entry */
#define TF_DEV_DATA_TYPE_TF_WC_ENTRY			0x30d1UL
/* Action Data */
#define TF_DEV_DATA_TYPE_TF_ACTION_DATA			0x3170UL
#define TF_DEV_DATA_TYPE_LAST   TF_DEV_DATA_TYPE_TF_ACTION_DATA

#define TF_BITS2BYTES(x) (((x) + 7) >> 3)
#define TF_BITS2BYTES_WORD_ALIGN(x) ((((x) + 31) >> 5) * 4)

struct tf_session_attach_input;
struct tf_session_hw_resc_qcaps_input;
struct tf_session_hw_resc_qcaps_output;
struct tf_session_hw_resc_alloc_input;
struct tf_session_hw_resc_alloc_output;
struct tf_session_hw_resc_free_input;
struct tf_session_hw_resc_flush_input;
struct tf_session_sram_resc_qcaps_input;
struct tf_session_sram_resc_qcaps_output;
struct tf_session_sram_resc_alloc_input;
struct tf_session_sram_resc_alloc_output;
struct tf_session_sram_resc_free_input;
struct tf_session_sram_resc_flush_input;
struct tf_tbl_type_set_input;
struct tf_tbl_type_get_input;
struct tf_tbl_type_get_output;
struct tf_em_internal_insert_input;
struct tf_em_internal_insert_output;
struct tf_em_internal_delete_input;
/* Input params for session attach */
typedef struct tf_session_attach_input {
	/* Firmware session id returned when HWRM_TF_SESSION_OPEN is sent */
	uint32_t			 fw_session_id;
	/* Session Name */
	char				 session_name[TF_SESSION_NAME_MAX];
} tf_session_attach_input_t, *ptf_session_attach_input_t;

/* Input params for session resource HW qcaps */
typedef struct tf_session_hw_resc_qcaps_input {
	/* Firmware session id returned when HWRM_TF_SESSION_OPEN is sent */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the query apply to RX */
#define TF_SESSION_HW_RESC_QCAPS_INPUT_FLAGS_DIR_RX	  (0x0)
	/* When set to 1, indicates the query apply to TX */
#define TF_SESSION_HW_RESC_QCAPS_INPUT_FLAGS_DIR_TX	  (0x1)
} tf_session_hw_resc_qcaps_input_t, *ptf_session_hw_resc_qcaps_input_t;

/* Output params for session resource HW qcaps */
typedef struct tf_session_hw_resc_qcaps_output {
	/* Control Flags */
	uint32_t			 flags;
	/* When set to 0, indicates Static partitioning */
#define TF_SESSION_HW_RESC_QCAPS_OUTPUT_FLAGS_SESS_RES_STRATEGY_STATIC	  (0x0)
	/* When set to 1, indicates Strategy 1 */
#define TF_SESSION_HW_RESC_QCAPS_OUTPUT_FLAGS_SESS_RES_STRATEGY_1	  (0x1)
	/* When set to 1, indicates Strategy 2 */
#define TF_SESSION_HW_RESC_QCAPS_OUTPUT_FLAGS_SESS_RES_STRATEGY_2	  (0x2)
	/* When set to 1, indicates Strategy 3 */
#define TF_SESSION_HW_RESC_QCAPS_OUTPUT_FLAGS_SESS_RES_STRATEGY_3	  (0x3)
	/* Unused */
	uint8_t			  unused[4];
	/* Minimum guaranteed number of L2 Ctx */
	uint16_t			 l2_ctx_tcam_entries_min;
	/* Maximum non-guaranteed number of L2 Ctx */
	uint16_t			 l2_ctx_tcam_entries_max;
	/* Minimum guaranteed number of profile functions */
	uint16_t			 prof_func_min;
	/* Maximum non-guaranteed number of profile functions */
	uint16_t			 prof_func_max;
	/* Minimum guaranteed number of profile TCAM entries */
	uint16_t			 prof_tcam_entries_min;
	/* Maximum non-guaranteed number of profile TCAM entries */
	uint16_t			 prof_tcam_entries_max;
	/* Minimum guaranteed number of EM profile ID */
	uint16_t			 em_prof_id_min;
	/* Maximum non-guaranteed number of EM profile ID */
	uint16_t			 em_prof_id_max;
	/* Minimum guaranteed number of EM records entries */
	uint16_t			 em_record_entries_min;
	/* Maximum non-guaranteed number of EM record entries */
	uint16_t			 em_record_entries_max;
	/* Minimum guaranteed number of WC TCAM profile ID */
	uint16_t			 wc_tcam_prof_id_min;
	/* Maximum non-guaranteed number of WC TCAM profile ID */
	uint16_t			 wc_tcam_prof_id_max;
	/* Minimum guaranteed number of WC TCAM entries */
	uint16_t			 wc_tcam_entries_min;
	/* Maximum non-guaranteed number of WC TCAM entries */
	uint16_t			 wc_tcam_entries_max;
	/* Minimum guaranteed number of meter profiles */
	uint16_t			 meter_profiles_min;
	/* Maximum non-guaranteed number of meter profiles */
	uint16_t			 meter_profiles_max;
	/* Minimum guaranteed number of meter instances */
	uint16_t			 meter_inst_min;
	/* Maximum non-guaranteed number of meter instances */
	uint16_t			 meter_inst_max;
	/* Minimum guaranteed number of mirrors */
	uint16_t			 mirrors_min;
	/* Maximum non-guaranteed number of mirrors */
	uint16_t			 mirrors_max;
	/* Minimum guaranteed number of UPAR */
	uint16_t			 upar_min;
	/* Maximum non-guaranteed number of UPAR */
	uint16_t			 upar_max;
	/* Minimum guaranteed number of SP TCAM entries */
	uint16_t			 sp_tcam_entries_min;
	/* Maximum non-guaranteed number of SP TCAM entries */
	uint16_t			 sp_tcam_entries_max;
	/* Minimum guaranteed number of L2 Functions */
	uint16_t			 l2_func_min;
	/* Maximum non-guaranteed number of L2 Functions */
	uint16_t			 l2_func_max;
	/* Minimum guaranteed number of flexible key templates */
	uint16_t			 flex_key_templ_min;
	/* Maximum non-guaranteed number of flexible key templates */
	uint16_t			 flex_key_templ_max;
	/* Minimum guaranteed number of table Scopes */
	uint16_t			 tbl_scope_min;
	/* Maximum non-guaranteed number of table Scopes */
	uint16_t			 tbl_scope_max;
	/* Minimum guaranteed number of epoch0 entries */
	uint16_t			 epoch0_entries_min;
	/* Maximum non-guaranteed number of epoch0 entries */
	uint16_t			 epoch0_entries_max;
	/* Minimum guaranteed number of epoch1 entries */
	uint16_t			 epoch1_entries_min;
	/* Maximum non-guaranteed number of epoch1 entries */
	uint16_t			 epoch1_entries_max;
	/* Minimum guaranteed number of metadata */
	uint16_t			 metadata_min;
	/* Maximum non-guaranteed number of metadata */
	uint16_t			 metadata_max;
	/* Minimum guaranteed number of CT states */
	uint16_t			 ct_state_min;
	/* Maximum non-guaranteed number of CT states */
	uint16_t			 ct_state_max;
	/* Minimum guaranteed number of range profiles */
	uint16_t			 range_prof_min;
	/* Maximum non-guaranteed number range profiles */
	uint16_t			 range_prof_max;
	/* Minimum guaranteed number of range entries */
	uint16_t			 range_entries_min;
	/* Maximum non-guaranteed number of range entries */
	uint16_t			 range_entries_max;
	/* Minimum guaranteed number of LAG table entries */
	uint16_t			 lag_tbl_entries_min;
	/* Maximum non-guaranteed number of LAG table entries */
	uint16_t			 lag_tbl_entries_max;
} tf_session_hw_resc_qcaps_output_t, *ptf_session_hw_resc_qcaps_output_t;

/* Input params for session resource HW alloc */
typedef struct tf_session_hw_resc_alloc_input {
	/* Firmware session id returned when HWRM_TF_SESSION_OPEN is sent */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the query apply to RX */
#define TF_SESSION_HW_RESC_ALLOC_INPUT_FLAGS_DIR_RX	  (0x0)
	/* When set to 1, indicates the query apply to TX */
#define TF_SESSION_HW_RESC_ALLOC_INPUT_FLAGS_DIR_TX	  (0x1)
	/* Unused */
	uint8_t			  unused[2];
	/* Number of L2 CTX TCAM entries to be allocated */
	uint16_t			 num_l2_ctx_tcam_entries;
	/* Number of profile functions to be allocated */
	uint16_t			 num_prof_func_entries;
	/* Number of profile TCAM entries to be allocated */
	uint16_t			 num_prof_tcam_entries;
	/* Number of EM profile ids to be allocated */
	uint16_t			 num_em_prof_id;
	/* Number of EM records entries to be allocated */
	uint16_t			 num_em_record_entries;
	/* Number of WC profiles ids to be allocated */
	uint16_t			 num_wc_tcam_prof_id;
	/* Number of WC TCAM entries to be allocated */
	uint16_t			 num_wc_tcam_entries;
	/* Number of meter profiles to be allocated */
	uint16_t			 num_meter_profiles;
	/* Number of meter instances to be allocated */
	uint16_t			 num_meter_inst;
	/* Number of mirrors to be allocated */
	uint16_t			 num_mirrors;
	/* Number of UPAR to be allocated */
	uint16_t			 num_upar;
	/* Number of SP TCAM entries to be allocated */
	uint16_t			 num_sp_tcam_entries;
	/* Number of L2 functions to be allocated */
	uint16_t			 num_l2_func;
	/* Number of flexible key templates to be allocated */
	uint16_t			 num_flex_key_templ;
	/* Number of table scopes to be allocated */
	uint16_t			 num_tbl_scope;
	/* Number of epoch0 entries to be allocated */
	uint16_t			 num_epoch0_entries;
	/* Number of epoch1 entries to be allocated */
	uint16_t			 num_epoch1_entries;
	/* Number of metadata to be allocated */
	uint16_t			 num_metadata;
	/* Number of CT states to be allocated */
	uint16_t			 num_ct_state;
	/* Number of range profiles to be allocated */
	uint16_t			 num_range_prof;
	/* Number of range Entries to be allocated */
	uint16_t			 num_range_entries;
	/* Number of LAG table entries to be allocated */
	uint16_t			 num_lag_tbl_entries;
} tf_session_hw_resc_alloc_input_t, *ptf_session_hw_resc_alloc_input_t;

/* Output params for session resource HW alloc */
typedef struct tf_session_hw_resc_alloc_output {
	/* Starting index of L2 CTX TCAM entries allocated to the session */
	uint16_t			 l2_ctx_tcam_entries_start;
	/* Number of L2 CTX TCAM entries allocated */
	uint16_t			 l2_ctx_tcam_entries_stride;
	/* Starting index of profile functions allocated to the session */
	uint16_t			 prof_func_start;
	/* Number of profile functions allocated */
	uint16_t			 prof_func_stride;
	/* Starting index of profile TCAM entries allocated to the session */
	uint16_t			 prof_tcam_entries_start;
	/* Number of profile TCAM entries allocated */
	uint16_t			 prof_tcam_entries_stride;
	/* Starting index of EM profile ids allocated to the session */
	uint16_t			 em_prof_id_start;
	/* Number of EM profile ids allocated */
	uint16_t			 em_prof_id_stride;
	/* Starting index of EM record entries allocated to the session */
	uint16_t			 em_record_entries_start;
	/* Number of EM record entries allocated */
	uint16_t			 em_record_entries_stride;
	/* Starting index of WC TCAM profiles ids allocated to the session */
	uint16_t			 wc_tcam_prof_id_start;
	/* Number of WC TCAM profile ids allocated */
	uint16_t			 wc_tcam_prof_id_stride;
	/* Starting index of WC TCAM entries allocated to the session */
	uint16_t			 wc_tcam_entries_start;
	/* Number of WC TCAM allocated */
	uint16_t			 wc_tcam_entries_stride;
	/* Starting index of meter profiles allocated to the session */
	uint16_t			 meter_profiles_start;
	/* Number of meter profiles allocated */
	uint16_t			 meter_profiles_stride;
	/* Starting index of meter instance allocated to the session */
	uint16_t			 meter_inst_start;
	/* Number of meter instance allocated */
	uint16_t			 meter_inst_stride;
	/* Starting index of mirrors allocated to the session */
	uint16_t			 mirrors_start;
	/* Number of mirrors allocated */
	uint16_t			 mirrors_stride;
	/* Starting index of UPAR allocated to the session */
	uint16_t			 upar_start;
	/* Number of UPAR allocated */
	uint16_t			 upar_stride;
	/* Starting index of SP TCAM entries allocated to the session */
	uint16_t			 sp_tcam_entries_start;
	/* Number of SP TCAM entries allocated */
	uint16_t			 sp_tcam_entries_stride;
	/* Starting index of L2 functions allocated to the session */
	uint16_t			 l2_func_start;
	/* Number of L2 functions allocated */
	uint16_t			 l2_func_stride;
	/* Starting index of flexible key templates allocated to the session */
	uint16_t			 flex_key_templ_start;
	/* Number of flexible key templates allocated */
	uint16_t			 flex_key_templ_stride;
	/* Starting index of table scopes allocated to the session */
	uint16_t			 tbl_scope_start;
	/* Number of table scopes allocated */
	uint16_t			 tbl_scope_stride;
	/* Starting index of epoch0 entries allocated to the session */
	uint16_t			 epoch0_entries_start;
	/* Number of epoch0 entries allocated */
	uint16_t			 epoch0_entries_stride;
	/* Starting index of epoch1 entries allocated to the session */
	uint16_t			 epoch1_entries_start;
	/* Number of epoch1 entries allocated */
	uint16_t			 epoch1_entries_stride;
	/* Starting index of metadata allocated to the session */
	uint16_t			 metadata_start;
	/* Number of metadata allocated */
	uint16_t			 metadata_stride;
	/* Starting index of CT states allocated to the session */
	uint16_t			 ct_state_start;
	/* Number of CT states allocated */
	uint16_t			 ct_state_stride;
	/* Starting index of range profiles allocated to the session */
	uint16_t			 range_prof_start;
	/* Number range profiles allocated */
	uint16_t			 range_prof_stride;
	/* Starting index of range entries allocated to the session */
	uint16_t			 range_entries_start;
	/* Number of range entries allocated */
	uint16_t			 range_entries_stride;
	/* Starting index of LAG table entries allocated to the session */
	uint16_t			 lag_tbl_entries_start;
	/* Number of LAG table entries allocated */
	uint16_t			 lag_tbl_entries_stride;
} tf_session_hw_resc_alloc_output_t, *ptf_session_hw_resc_alloc_output_t;

/* Input params for session resource HW free */
typedef struct tf_session_hw_resc_free_input {
	/* Firmware session id returned when HWRM_TF_SESSION_OPEN is sent */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the query apply to RX */
#define TF_SESSION_HW_RESC_FREE_INPUT_FLAGS_DIR_RX	  (0x0)
	/* When set to 1, indicates the query apply to TX */
#define TF_SESSION_HW_RESC_FREE_INPUT_FLAGS_DIR_TX	  (0x1)
	/* Unused */
	uint8_t			  unused[2];
	/* Starting index of L2 CTX TCAM entries allocated to the session */
	uint16_t			 l2_ctx_tcam_entries_start;
	/* Number of L2 CTX TCAM entries allocated */
	uint16_t			 l2_ctx_tcam_entries_stride;
	/* Starting index of profile functions allocated to the session */
	uint16_t			 prof_func_start;
	/* Number of profile functions allocated */
	uint16_t			 prof_func_stride;
	/* Starting index of profile TCAM entries allocated to the session */
	uint16_t			 prof_tcam_entries_start;
	/* Number of profile TCAM entries allocated */
	uint16_t			 prof_tcam_entries_stride;
	/* Starting index of EM profile ids allocated to the session */
	uint16_t			 em_prof_id_start;
	/* Number of EM profile ids allocated */
	uint16_t			 em_prof_id_stride;
	/* Starting index of EM record entries allocated to the session */
	uint16_t			 em_record_entries_start;
	/* Number of EM record entries allocated */
	uint16_t			 em_record_entries_stride;
	/* Starting index of WC TCAM profiles ids allocated to the session */
	uint16_t			 wc_tcam_prof_id_start;
	/* Number of WC TCAM profile ids allocated */
	uint16_t			 wc_tcam_prof_id_stride;
	/* Starting index of WC TCAM entries allocated to the session */
	uint16_t			 wc_tcam_entries_start;
	/* Number of WC TCAM allocated */
	uint16_t			 wc_tcam_entries_stride;
	/* Starting index of meter profiles allocated to the session */
	uint16_t			 meter_profiles_start;
	/* Number of meter profiles allocated */
	uint16_t			 meter_profiles_stride;
	/* Starting index of meter instance allocated to the session */
	uint16_t			 meter_inst_start;
	/* Number of meter instance allocated */
	uint16_t			 meter_inst_stride;
	/* Starting index of mirrors allocated to the session */
	uint16_t			 mirrors_start;
	/* Number of mirrors allocated */
	uint16_t			 mirrors_stride;
	/* Starting index of UPAR allocated to the session */
	uint16_t			 upar_start;
	/* Number of UPAR allocated */
	uint16_t			 upar_stride;
	/* Starting index of SP TCAM entries allocated to the session */
	uint16_t			 sp_tcam_entries_start;
	/* Number of SP TCAM entries allocated */
	uint16_t			 sp_tcam_entries_stride;
	/* Starting index of L2 functions allocated to the session */
	uint16_t			 l2_func_start;
	/* Number of L2 functions allocated */
	uint16_t			 l2_func_stride;
	/* Starting index of flexible key templates allocated to the session */
	uint16_t			 flex_key_templ_start;
	/* Number of flexible key templates allocated */
	uint16_t			 flex_key_templ_stride;
	/* Starting index of table scopes allocated to the session */
	uint16_t			 tbl_scope_start;
	/* Number of table scopes allocated */
	uint16_t			 tbl_scope_stride;
	/* Starting index of epoch0 entries allocated to the session */
	uint16_t			 epoch0_entries_start;
	/* Number of epoch0 entries allocated */
	uint16_t			 epoch0_entries_stride;
	/* Starting index of epoch1 entries allocated to the session */
	uint16_t			 epoch1_entries_start;
	/* Number of epoch1 entries allocated */
	uint16_t			 epoch1_entries_stride;
	/* Starting index of metadata allocated to the session */
	uint16_t			 metadata_start;
	/* Number of metadata allocated */
	uint16_t			 metadata_stride;
	/* Starting index of CT states allocated to the session */
	uint16_t			 ct_state_start;
	/* Number of CT states allocated */
	uint16_t			 ct_state_stride;
	/* Starting index of range profiles allocated to the session */
	uint16_t			 range_prof_start;
	/* Number range profiles allocated */
	uint16_t			 range_prof_stride;
	/* Starting index of range entries allocated to the session */
	uint16_t			 range_entries_start;
	/* Number of range entries allocated */
	uint16_t			 range_entries_stride;
	/* Starting index of LAG table entries allocated to the session */
	uint16_t			 lag_tbl_entries_start;
	/* Number of LAG table entries allocated */
	uint16_t			 lag_tbl_entries_stride;
} tf_session_hw_resc_free_input_t, *ptf_session_hw_resc_free_input_t;

/* Input params for session resource HW flush */
typedef struct tf_session_hw_resc_flush_input {
	/* Firmware session id returned when HWRM_TF_SESSION_OPEN is sent */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the flush apply to RX */
#define TF_SESSION_HW_RESC_FLUSH_INPUT_FLAGS_DIR_RX	  (0x0)
	/* When set to 1, indicates the flush apply to TX */
#define TF_SESSION_HW_RESC_FLUSH_INPUT_FLAGS_DIR_TX	  (0x1)
	/* Unused */
	uint8_t			  unused[2];
	/* Starting index of L2 CTX TCAM entries allocated to the session */
	uint16_t			 l2_ctx_tcam_entries_start;
	/* Number of L2 CTX TCAM entries allocated */
	uint16_t			 l2_ctx_tcam_entries_stride;
	/* Starting index of profile functions allocated to the session */
	uint16_t			 prof_func_start;
	/* Number of profile functions allocated */
	uint16_t			 prof_func_stride;
	/* Starting index of profile TCAM entries allocated to the session */
	uint16_t			 prof_tcam_entries_start;
	/* Number of profile TCAM entries allocated */
	uint16_t			 prof_tcam_entries_stride;
	/* Starting index of EM profile ids allocated to the session */
	uint16_t			 em_prof_id_start;
	/* Number of EM profile ids allocated */
	uint16_t			 em_prof_id_stride;
	/* Starting index of EM record entries allocated to the session */
	uint16_t			 em_record_entries_start;
	/* Number of EM record entries allocated */
	uint16_t			 em_record_entries_stride;
	/* Starting index of WC TCAM profiles ids allocated to the session */
	uint16_t			 wc_tcam_prof_id_start;
	/* Number of WC TCAM profile ids allocated */
	uint16_t			 wc_tcam_prof_id_stride;
	/* Starting index of WC TCAM entries allocated to the session */
	uint16_t			 wc_tcam_entries_start;
	/* Number of WC TCAM allocated */
	uint16_t			 wc_tcam_entries_stride;
	/* Starting index of meter profiles allocated to the session */
	uint16_t			 meter_profiles_start;
	/* Number of meter profiles allocated */
	uint16_t			 meter_profiles_stride;
	/* Starting index of meter instance allocated to the session */
	uint16_t			 meter_inst_start;
	/* Number of meter instance allocated */
	uint16_t			 meter_inst_stride;
	/* Starting index of mirrors allocated to the session */
	uint16_t			 mirrors_start;
	/* Number of mirrors allocated */
	uint16_t			 mirrors_stride;
	/* Starting index of UPAR allocated to the session */
	uint16_t			 upar_start;
	/* Number of UPAR allocated */
	uint16_t			 upar_stride;
	/* Starting index of SP TCAM entries allocated to the session */
	uint16_t			 sp_tcam_entries_start;
	/* Number of SP TCAM entries allocated */
	uint16_t			 sp_tcam_entries_stride;
	/* Starting index of L2 functions allocated to the session */
	uint16_t			 l2_func_start;
	/* Number of L2 functions allocated */
	uint16_t			 l2_func_stride;
	/* Starting index of flexible key templates allocated to the session */
	uint16_t			 flex_key_templ_start;
	/* Number of flexible key templates allocated */
	uint16_t			 flex_key_templ_stride;
	/* Starting index of table scopes allocated to the session */
	uint16_t			 tbl_scope_start;
	/* Number of table scopes allocated */
	uint16_t			 tbl_scope_stride;
	/* Starting index of epoch0 entries allocated to the session */
	uint16_t			 epoch0_entries_start;
	/* Number of epoch0 entries allocated */
	uint16_t			 epoch0_entries_stride;
	/* Starting index of epoch1 entries allocated to the session */
	uint16_t			 epoch1_entries_start;
	/* Number of epoch1 entries allocated */
	uint16_t			 epoch1_entries_stride;
	/* Starting index of metadata allocated to the session */
	uint16_t			 metadata_start;
	/* Number of metadata allocated */
	uint16_t			 metadata_stride;
	/* Starting index of CT states allocated to the session */
	uint16_t			 ct_state_start;
	/* Number of CT states allocated */
	uint16_t			 ct_state_stride;
	/* Starting index of range profiles allocated to the session */
	uint16_t			 range_prof_start;
	/* Number range profiles allocated */
	uint16_t			 range_prof_stride;
	/* Starting index of range entries allocated to the session */
	uint16_t			 range_entries_start;
	/* Number of range entries allocated */
	uint16_t			 range_entries_stride;
	/* Starting index of LAG table entries allocated to the session */
	uint16_t			 lag_tbl_entries_start;
	/* Number of LAG table entries allocated */
	uint16_t			 lag_tbl_entries_stride;
} tf_session_hw_resc_flush_input_t, *ptf_session_hw_resc_flush_input_t;

/* Input params for session resource SRAM qcaps */
typedef struct tf_session_sram_resc_qcaps_input {
	/* Firmware session id returned when HWRM_TF_SESSION_OPEN is sent */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the query apply to RX */
#define TF_SESSION_SRAM_RESC_QCAPS_INPUT_FLAGS_DIR_RX	  (0x0)
	/* When set to 1, indicates the query apply to TX */
#define TF_SESSION_SRAM_RESC_QCAPS_INPUT_FLAGS_DIR_TX	  (0x1)
} tf_session_sram_resc_qcaps_input_t, *ptf_session_sram_resc_qcaps_input_t;

/* Output params for session resource SRAM qcaps */
typedef struct tf_session_sram_resc_qcaps_output {
	/* Flags */
	uint32_t			 flags;
	/* When set to 0, indicates Static partitioning */
#define TF_SESSION_SRAM_RESC_QCAPS_OUTPUT_FLAGS_SESS_RES_STRATEGY_STATIC	  (0x0)
	/* When set to 1, indicates Strategy 1 */
#define TF_SESSION_SRAM_RESC_QCAPS_OUTPUT_FLAGS_SESS_RES_STRATEGY_1	  (0x1)
	/* When set to 1, indicates Strategy 2 */
#define TF_SESSION_SRAM_RESC_QCAPS_OUTPUT_FLAGS_SESS_RES_STRATEGY_2	  (0x2)
	/* When set to 1, indicates Strategy 3 */
#define TF_SESSION_SRAM_RESC_QCAPS_OUTPUT_FLAGS_SESS_RES_STRATEGY_3	  (0x3)
	/* Minimum guaranteed number of Full Action */
	uint16_t			 full_action_min;
	/* Maximum non-guaranteed number of Full Action */
	uint16_t			 full_action_max;
	/* Minimum guaranteed number of MCG */
	uint16_t			 mcg_min;
	/* Maximum non-guaranteed number of MCG */
	uint16_t			 mcg_max;
	/* Minimum guaranteed number of Encap 8B */
	uint16_t			 encap_8b_min;
	/* Maximum non-guaranteed number of Encap 8B */
	uint16_t			 encap_8b_max;
	/* Minimum guaranteed number of Encap 16B */
	uint16_t			 encap_16b_min;
	/* Maximum non-guaranteed number of Encap 16B */
	uint16_t			 encap_16b_max;
	/* Minimum guaranteed number of Encap 64B */
	uint16_t			 encap_64b_min;
	/* Maximum non-guaranteed number of Encap 64B */
	uint16_t			 encap_64b_max;
	/* Minimum guaranteed number of SP SMAC */
	uint16_t			 sp_smac_min;
	/* Maximum non-guaranteed number of SP SMAC */
	uint16_t			 sp_smac_max;
	/* Minimum guaranteed number of SP SMAC IPv4 */
	uint16_t			 sp_smac_ipv4_min;
	/* Maximum non-guaranteed number of SP SMAC IPv4 */
	uint16_t			 sp_smac_ipv4_max;
	/* Minimum guaranteed number of SP SMAC IPv6 */
	uint16_t			 sp_smac_ipv6_min;
	/* Maximum non-guaranteed number of SP SMAC IPv6 */
	uint16_t			 sp_smac_ipv6_max;
	/* Minimum guaranteed number of Counter 64B */
	uint16_t			 counter_64b_min;
	/* Maximum non-guaranteed number of Counter 64B */
	uint16_t			 counter_64b_max;
	/* Minimum guaranteed number of NAT SPORT */
	uint16_t			 nat_sport_min;
	/* Maximum non-guaranteed number of NAT SPORT */
	uint16_t			 nat_sport_max;
	/* Minimum guaranteed number of NAT DPORT */
	uint16_t			 nat_dport_min;
	/* Maximum non-guaranteed number of NAT DPORT */
	uint16_t			 nat_dport_max;
	/* Minimum guaranteed number of NAT S_IPV4 */
	uint16_t			 nat_s_ipv4_min;
	/* Maximum non-guaranteed number of NAT S_IPV4 */
	uint16_t			 nat_s_ipv4_max;
	/* Minimum guaranteed number of NAT D_IPV4 */
	uint16_t			 nat_d_ipv4_min;
	/* Maximum non-guaranteed number of NAT D_IPV4 */
	uint16_t			 nat_d_ipv4_max;
} tf_session_sram_resc_qcaps_output_t, *ptf_session_sram_resc_qcaps_output_t;

/* Input params for session resource SRAM alloc */
typedef struct tf_session_sram_resc_alloc_input {
	/* FW Session Id */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the query apply to RX */
#define TF_SESSION_SRAM_RESC_ALLOC_INPUT_FLAGS_DIR_RX	  (0x0)
	/* When set to 1, indicates the query apply to TX */
#define TF_SESSION_SRAM_RESC_ALLOC_INPUT_FLAGS_DIR_TX	  (0x1)
	/* Unused */
	uint8_t			  unused[2];
	/* Number of full action SRAM entries to be allocated */
	uint16_t			 num_full_action;
	/* Number of multicast groups to be allocated */
	uint16_t			 num_mcg;
	/* Number of Encap 8B entries to be allocated */
	uint16_t			 num_encap_8b;
	/* Number of Encap 16B entries to be allocated */
	uint16_t			 num_encap_16b;
	/* Number of Encap 64B entries to be allocated */
	uint16_t			 num_encap_64b;
	/* Number of SP SMAC entries to be allocated */
	uint16_t			 num_sp_smac;
	/* Number of SP SMAC IPv4 entries to be allocated */
	uint16_t			 num_sp_smac_ipv4;
	/* Number of SP SMAC IPv6 entries to be allocated */
	uint16_t			 num_sp_smac_ipv6;
	/* Number of Counter 64B entries to be allocated */
	uint16_t			 num_counter_64b;
	/* Number of NAT source ports to be allocated */
	uint16_t			 num_nat_sport;
	/* Number of NAT destination ports to be allocated */
	uint16_t			 num_nat_dport;
	/* Number of NAT source iPV4 addresses to be allocated */
	uint16_t			 num_nat_s_ipv4;
	/* Number of NAT destination IPV4 addresses to be allocated */
	uint16_t			 num_nat_d_ipv4;
} tf_session_sram_resc_alloc_input_t, *ptf_session_sram_resc_alloc_input_t;

/* Output params for session resource SRAM alloc */
typedef struct tf_session_sram_resc_alloc_output {
	/* Unused */
	uint8_t			  unused[2];
	/* Starting index of full action SRAM entries allocated to the session */
	uint16_t			 full_action_start;
	/* Number of full action SRAM entries allocated */
	uint16_t			 full_action_stride;
	/* Starting index of multicast groups allocated to this session */
	uint16_t			 mcg_start;
	/* Number of multicast groups allocated */
	uint16_t			 mcg_stride;
	/* Starting index of encap 8B entries allocated to the session */
	uint16_t			 encap_8b_start;
	/* Number of encap 8B entries allocated */
	uint16_t			 encap_8b_stride;
	/* Starting index of encap 16B entries allocated to the session */
	uint16_t			 encap_16b_start;
	/* Number of encap 16B entries allocated */
	uint16_t			 encap_16b_stride;
	/* Starting index of encap 64B entries allocated to the session */
	uint16_t			 encap_64b_start;
	/* Number of encap 64B entries allocated */
	uint16_t			 encap_64b_stride;
	/* Starting index of SP SMAC entries allocated to the session */
	uint16_t			 sp_smac_start;
	/* Number of SP SMAC entries allocated */
	uint16_t			 sp_smac_stride;
	/* Starting index of SP SMAC IPv4 entries allocated to the session */
	uint16_t			 sp_smac_ipv4_start;
	/* Number of SP SMAC IPv4 entries allocated */
	uint16_t			 sp_smac_ipv4_stride;
	/* Starting index of SP SMAC IPv6 entries allocated to the session */
	uint16_t			 sp_smac_ipv6_start;
	/* Number of SP SMAC IPv6 entries allocated */
	uint16_t			 sp_smac_ipv6_stride;
	/* Starting index of Counter 64B entries allocated to the session */
	uint16_t			 counter_64b_start;
	/* Number of Counter 64B entries allocated */
	uint16_t			 counter_64b_stride;
	/* Starting index of NAT source ports allocated to the session */
	uint16_t			 nat_sport_start;
	/* Number of NAT source ports allocated */
	uint16_t			 nat_sport_stride;
	/* Starting index of NAT destination ports allocated to the session */
	uint16_t			 nat_dport_start;
	/* Number of NAT destination ports allocated */
	uint16_t			 nat_dport_stride;
	/* Starting index of NAT source IPV4 addresses allocated to the session */
	uint16_t			 nat_s_ipv4_start;
	/* Number of NAT source IPV4 addresses allocated */
	uint16_t			 nat_s_ipv4_stride;
	/*
	 * Starting index of NAT destination IPV4 addresses allocated to the
	 * session
	 */
	uint16_t			 nat_d_ipv4_start;
	/* Number of NAT destination IPV4 addresses allocated */
	uint16_t			 nat_d_ipv4_stride;
} tf_session_sram_resc_alloc_output_t, *ptf_session_sram_resc_alloc_output_t;

/* Input params for session resource SRAM free */
typedef struct tf_session_sram_resc_free_input {
	/* Firmware session id returned when HWRM_TF_SESSION_OPEN is sent */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the query apply to RX */
#define TF_SESSION_SRAM_RESC_FREE_INPUT_FLAGS_DIR_RX	  (0x0)
	/* When set to 1, indicates the query apply to TX */
#define TF_SESSION_SRAM_RESC_FREE_INPUT_FLAGS_DIR_TX	  (0x1)
	/* Starting index of full action SRAM entries allocated to the session */
	uint16_t			 full_action_start;
	/* Number of full action SRAM entries allocated */
	uint16_t			 full_action_stride;
	/* Starting index of multicast groups allocated to this session */
	uint16_t			 mcg_start;
	/* Number of multicast groups allocated */
	uint16_t			 mcg_stride;
	/* Starting index of encap 8B entries allocated to the session */
	uint16_t			 encap_8b_start;
	/* Number of encap 8B entries allocated */
	uint16_t			 encap_8b_stride;
	/* Starting index of encap 16B entries allocated to the session */
	uint16_t			 encap_16b_start;
	/* Number of encap 16B entries allocated */
	uint16_t			 encap_16b_stride;
	/* Starting index of encap 64B entries allocated to the session */
	uint16_t			 encap_64b_start;
	/* Number of encap 64B entries allocated */
	uint16_t			 encap_64b_stride;
	/* Starting index of SP SMAC entries allocated to the session */
	uint16_t			 sp_smac_start;
	/* Number of SP SMAC entries allocated */
	uint16_t			 sp_smac_stride;
	/* Starting index of SP SMAC IPv4 entries allocated to the session */
	uint16_t			 sp_smac_ipv4_start;
	/* Number of SP SMAC IPv4 entries allocated */
	uint16_t			 sp_smac_ipv4_stride;
	/* Starting index of SP SMAC IPv6 entries allocated to the session */
	uint16_t			 sp_smac_ipv6_start;
	/* Number of SP SMAC IPv6 entries allocated */
	uint16_t			 sp_smac_ipv6_stride;
	/* Starting index of Counter 64B entries allocated to the session */
	uint16_t			 counter_64b_start;
	/* Number of Counter 64B entries allocated */
	uint16_t			 counter_64b_stride;
	/* Starting index of NAT source ports allocated to the session */
	uint16_t			 nat_sport_start;
	/* Number of NAT source ports allocated */
	uint16_t			 nat_sport_stride;
	/* Starting index of NAT destination ports allocated to the session */
	uint16_t			 nat_dport_start;
	/* Number of NAT destination ports allocated */
	uint16_t			 nat_dport_stride;
	/* Starting index of NAT source IPV4 addresses allocated to the session */
	uint16_t			 nat_s_ipv4_start;
	/* Number of NAT source IPV4 addresses allocated */
	uint16_t			 nat_s_ipv4_stride;
	/*
	 * Starting index of NAT destination IPV4 addresses allocated to the
	 * session
	 */
	uint16_t			 nat_d_ipv4_start;
	/* Number of NAT destination IPV4 addresses allocated */
	uint16_t			 nat_d_ipv4_stride;
} tf_session_sram_resc_free_input_t, *ptf_session_sram_resc_free_input_t;

/* Input params for session resource SRAM flush */
typedef struct tf_session_sram_resc_flush_input {
	/* Firmware session id returned when HWRM_TF_SESSION_OPEN is sent */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the flush apply to RX */
#define TF_SESSION_SRAM_RESC_FLUSH_INPUT_FLAGS_DIR_RX	  (0x0)
	/* When set to 1, indicates the flush apply to TX */
#define TF_SESSION_SRAM_RESC_FLUSH_INPUT_FLAGS_DIR_TX	  (0x1)
	/* Starting index of full action SRAM entries allocated to the session */
	uint16_t			 full_action_start;
	/* Number of full action SRAM entries allocated */
	uint16_t			 full_action_stride;
	/* Starting index of multicast groups allocated to this session */
	uint16_t			 mcg_start;
	/* Number of multicast groups allocated */
	uint16_t			 mcg_stride;
	/* Starting index of encap 8B entries allocated to the session */
	uint16_t			 encap_8b_start;
	/* Number of encap 8B entries allocated */
	uint16_t			 encap_8b_stride;
	/* Starting index of encap 16B entries allocated to the session */
	uint16_t			 encap_16b_start;
	/* Number of encap 16B entries allocated */
	uint16_t			 encap_16b_stride;
	/* Starting index of encap 64B entries allocated to the session */
	uint16_t			 encap_64b_start;
	/* Number of encap 64B entries allocated */
	uint16_t			 encap_64b_stride;
	/* Starting index of SP SMAC entries allocated to the session */
	uint16_t			 sp_smac_start;
	/* Number of SP SMAC entries allocated */
	uint16_t			 sp_smac_stride;
	/* Starting index of SP SMAC IPv4 entries allocated to the session */
	uint16_t			 sp_smac_ipv4_start;
	/* Number of SP SMAC IPv4 entries allocated */
	uint16_t			 sp_smac_ipv4_stride;
	/* Starting index of SP SMAC IPv6 entries allocated to the session */
	uint16_t			 sp_smac_ipv6_start;
	/* Number of SP SMAC IPv6 entries allocated */
	uint16_t			 sp_smac_ipv6_stride;
	/* Starting index of Counter 64B entries allocated to the session */
	uint16_t			 counter_64b_start;
	/* Number of Counter 64B entries allocated */
	uint16_t			 counter_64b_stride;
	/* Starting index of NAT source ports allocated to the session */
	uint16_t			 nat_sport_start;
	/* Number of NAT source ports allocated */
	uint16_t			 nat_sport_stride;
	/* Starting index of NAT destination ports allocated to the session */
	uint16_t			 nat_dport_start;
	/* Number of NAT destination ports allocated */
	uint16_t			 nat_dport_stride;
	/* Starting index of NAT source IPV4 addresses allocated to the session */
	uint16_t			 nat_s_ipv4_start;
	/* Number of NAT source IPV4 addresses allocated */
	uint16_t			 nat_s_ipv4_stride;
	/*
	 * Starting index of NAT destination IPV4 addresses allocated to the
	 * session
	 */
	uint16_t			 nat_d_ipv4_start;
	/* Number of NAT destination IPV4 addresses allocated */
	uint16_t			 nat_d_ipv4_stride;
} tf_session_sram_resc_flush_input_t, *ptf_session_sram_resc_flush_input_t;

/* Input params for table type set */
typedef struct tf_tbl_type_set_input {
	/* Session Id */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the get apply to RX */
#define TF_TBL_TYPE_SET_INPUT_FLAGS_DIR_RX			(0x0)
	/* When set to 1, indicates the get apply to TX */
#define TF_TBL_TYPE_SET_INPUT_FLAGS_DIR_TX			(0x1)
	/* Type of the object to set */
	uint32_t			 type;
	/* Size of the data to set in bytes */
	uint16_t			 size;
	/* Data to set */
	uint8_t			  data[TF_BULK_SEND];
	/* Index to set */
	uint32_t			 index;
} tf_tbl_type_set_input_t, *ptf_tbl_type_set_input_t;

/* Input params for table type get */
typedef struct tf_tbl_type_get_input {
	/* Session Id */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the get apply to RX */
#define TF_TBL_TYPE_GET_INPUT_FLAGS_DIR_RX			(0x0)
	/* When set to 1, indicates the get apply to TX */
#define TF_TBL_TYPE_GET_INPUT_FLAGS_DIR_TX			(0x1)
	/* Type of the object to set */
	uint32_t			 type;
	/* Index to get */
	uint32_t			 index;
} tf_tbl_type_get_input_t, *ptf_tbl_type_get_input_t;

/* Output params for table type get */
typedef struct tf_tbl_type_get_output {
	/* Size of the data read in bytes */
	uint16_t			 size;
	/* Data read */
	uint8_t			  data[TF_BULK_RECV];
} tf_tbl_type_get_output_t, *ptf_tbl_type_get_output_t;

/* Input params for EM internal rule insert */
typedef struct tf_em_internal_insert_input {
	/* Firmware Session Id */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the get apply to RX */
#define TF_EM_INTERNAL_INSERT_INPUT_FLAGS_DIR_RX	  (0x0)
	/* When set to 1, indicates the get apply to TX */
#define TF_EM_INTERNAL_INSERT_INPUT_FLAGS_DIR_TX	  (0x1)
	/* strength */
	uint16_t			 strength;
	/* index to action */
	uint32_t			 action_ptr;
	/* index of em record */
	uint32_t			 em_record_idx;
	/* EM Key value */
	uint64_t			 em_key[8];
	/* number of bits in em_key */
	uint16_t			 em_key_bitlen;
} tf_em_internal_insert_input_t, *ptf_em_internal_insert_input_t;

/* Output params for EM internal rule insert */
typedef struct tf_em_internal_insert_output {
	/* EM record pointer index */
	uint16_t			 rptr_index;
	/* EM record offset 0~3 */
	uint8_t			  rptr_entry;
} tf_em_internal_insert_output_t, *ptf_em_internal_insert_output_t;

/* Input params for EM INTERNAL rule delete */
typedef struct tf_em_internal_delete_input {
	/* Session Id */
	uint32_t			 tf_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the get apply to RX */
#define TF_EM_INTERNAL_DELETE_INPUT_FLAGS_DIR_RX	  (0x0)
	/* When set to 1, indicates the get apply to TX */
#define TF_EM_INTERNAL_DELETE_INPUT_FLAGS_DIR_TX	  (0x1)
	/* EM internal flow hanndle */
	uint64_t			 flow_handle;
	/* EM Key value */
	uint64_t			 em_key[8];
	/* number of bits in em_key */
	uint16_t			 em_key_bitlen;
} tf_em_internal_delete_input_t, *ptf_em_internal_delete_input_t;

#endif /* _HWRM_TF_H_ */
