/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Broadcom
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
	HWRM_TFT_GET_GLOBAL_CFG = 821,
	HWRM_TFT_SET_GLOBAL_CFG = 822,
	HWRM_TFT_TBL_TYPE_BULK_GET = 825,
	HWRM_TFT_IF_TBL_SET = 827,
	HWRM_TFT_IF_TBL_GET = 828,
	TF_SUBTYPE_LAST = HWRM_TFT_IF_TBL_GET,
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
#define TF_DEV_DATA_TYPE_TF_WC_ENTRY				0x30d1UL
/* SPIF DFLT L2 CTXT Entry */
#define TF_DEV_DATA_TYPE_SPIF_DFLT_L2_CTXT		  0x3131UL
/* PARIF DFLT ACT REC PTR Entry */
#define TF_DEV_DATA_TYPE_PARIF_DFLT_ACT_REC		0x3132UL
/* PARIF ERR DFLT ACT REC PTR Entry */
#define TF_DEV_DATA_TYPE_PARIF_ERR_DFLT_ACT_REC	 0x3133UL
/* ILT Entry */
#define TF_DEV_DATA_TYPE_ILT				0x3134UL
/* VNIC SVIF entry */
#define TF_DEV_DATA_TYPE_VNIC_SVIF			0x3135UL
/* Action Data */
#define TF_DEV_DATA_TYPE_TF_ACTION_DATA			0x3170UL
#define TF_DEV_DATA_TYPE_LAST   TF_DEV_DATA_TYPE_TF_ACTION_DATA

#define TF_BITS2BYTES(x) (((x) + 7) >> 3)
#define TF_BITS2BYTES_WORD_ALIGN(x) ((((x) + 31) >> 5) * 4)

struct tf_set_global_cfg_input;
struct tf_get_global_cfg_input;
struct tf_get_global_cfg_output;
struct tf_tbl_type_bulk_get_input;
struct tf_tbl_type_bulk_get_output;
struct tf_if_tbl_set_input;
struct tf_if_tbl_get_input;
struct tf_if_tbl_get_output;
/* Input params for global config set */
typedef struct tf_set_global_cfg_input {
	/* Session Id */
	uint32_t			 fw_session_id;
	/* flags */
	uint32_t			 flags;
	/* When set to 0, indicates the query apply to RX */
#define TF_SET_GLOBAL_CFG_INPUT_FLAGS_DIR_RX		  (0x0)
	/* When set to 1, indicates the query apply to TX */
#define TF_SET_GLOBAL_CFG_INPUT_FLAGS_DIR_TX		  (0x1)
	/* Config type */
	uint32_t			 type;
	/* Offset of the type */
	uint32_t			 offset;
	/* Size of the data to set in bytes */
	uint16_t			 size;
	/* Data to set */
	uint8_t			  data[TF_BULK_SEND];
} tf_set_global_cfg_input_t, *ptf_set_global_cfg_input_t;

/* Input params for global config to get */
typedef struct tf_get_global_cfg_input {
	/* Session Id */
	uint32_t			 fw_session_id;
	/* flags */
	uint32_t			 flags;
	/* When set to 0, indicates the query apply to RX */
#define TF_GET_GLOBAL_CFG_INPUT_FLAGS_DIR_RX		  (0x0)
	/* When set to 1, indicates the query apply to TX */
#define TF_GET_GLOBAL_CFG_INPUT_FLAGS_DIR_TX		  (0x1)
	/* Config to retrieve */
	uint32_t			 type;
	/* Offset to retrieve */
	uint32_t			 offset;
	/* Size of the data to set in bytes */
	uint16_t			 size;
} tf_get_global_cfg_input_t, *ptf_get_global_cfg_input_t;

/* Output params for global config */
typedef struct tf_get_global_cfg_output {
	/* Size of the total data read in bytes */
	uint16_t			 size;
	/* Data to get */
	uint8_t			  data[TF_BULK_SEND];
} tf_get_global_cfg_output_t, *ptf_get_global_cfg_output_t;

/* Input params for table type get */
typedef struct tf_tbl_type_bulk_get_input {
	/* Session Id */
	uint32_t			 fw_session_id;
	/* flags */
	uint32_t			 flags;
	/* When set to 0, indicates the get apply to RX */
#define TF_TBL_TYPE_BULK_GET_INPUT_FLAGS_DIR_RX	   (0x0)
	/* When set to 1, indicates the get apply to TX */
#define TF_TBL_TYPE_BULK_GET_INPUT_FLAGS_DIR_TX	   (0x1)
	/* When set to 1, indicates the clear entry on read */
#define TF_TBL_TYPE_BULK_GET_INPUT_FLAGS_CLEAR_ON_READ	  (0x2)
	/* Type of the object to set */
	uint32_t			 type;
	/* Starting index to get from */
	uint32_t			 start_index;
	/* Number of entries to get */
	uint32_t			 num_entries;
	/* Host memory where data will be stored */
	uint64_t			 host_addr;
} tf_tbl_type_bulk_get_input_t, *ptf_tbl_type_bulk_get_input_t;

/* Output params for table type get */
typedef struct tf_tbl_type_bulk_get_output {
	/* Size of the total data read in bytes */
	uint16_t			 size;
} tf_tbl_type_bulk_get_output_t, *ptf_tbl_type_bulk_get_output_t;

/* Input params for if tbl set */
typedef struct tf_if_tbl_set_input {
	/* Session Id */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the query apply to RX */
#define TF_IF_TBL_SET_INPUT_FLAGS_DIR_RX			  (0x0)
	/* When set to 1, indicates the query apply to TX */
#define TF_IF_TBL_SET_INPUT_FLAGS_DIR_TX			  (0x1)
	/* if table type */
	uint16_t			 tf_if_tbl_type;
	/* index of table entry */
	uint16_t			 idx;
	/* size of the data write to table entry */
	uint32_t			 data_sz_in_bytes;
	/* data to write into table entry */
	uint32_t			 data[2];
} tf_if_tbl_set_input_t, *ptf_if_tbl_set_input_t;

/* Input params for if tbl get */
typedef struct tf_if_tbl_get_input {
	/* Session Id */
	uint32_t			 fw_session_id;
	/* flags */
	uint16_t			 flags;
	/* When set to 0, indicates the query apply to RX */
#define TF_IF_TBL_GET_INPUT_FLAGS_DIR_RX			  (0x0)
	/* When set to 1, indicates the query apply to TX */
#define TF_IF_TBL_GET_INPUT_FLAGS_DIR_TX			  (0x1)
	/* if table type */
	uint16_t			 tf_if_tbl_type;
	/* size of the data get from table entry */
	uint32_t			 data_sz_in_bytes;
	/* index of table entry */
	uint16_t			 idx;
} tf_if_tbl_get_input_t, *ptf_if_tbl_get_input_t;

/* output params for if tbl get */
typedef struct tf_if_tbl_get_output {
	/* Value read from table entry */
	uint32_t			 data[2];
} tf_if_tbl_get_output_t, *ptf_if_tbl_get_output_t;

#endif /* _HWRM_TF_H_ */
