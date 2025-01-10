/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2024 Broadcom
 * All rights reserved.
 */

#ifndef _TF_RESOURCES_H_
#define _TF_RESOURCES_H_
#include <rte_common.h>
#include <tf_rm.h>
#include "bnxt.h"

#define TF_NUM_TBL_SCOPE           16      /* < Number of TBL scopes */

#ifdef TF_FLOW_SCALE_QUERY
/* Feature of flow scale query */
enum tf_resc_opt {
	TF_RESC_FREE,
	TF_RESC_ALLOC
};

/**
 *  WC TCAM includes a set of rows, and each row have 4-slices;
 *  each slice has 160bit
 */
typedef struct __rte_packed_begin tf_resc_wc_tcam_usage {
	uint16_t max_row_number;      /* Max number of rows (excluding AFM), 160bit row */
	uint16_t slice_row_1_p_used;  /* 1-slice rows partially used */
	uint16_t slice_row_1_f_used;  /* 1-slice rows fully used */
	uint16_t slice_row_2_p_used;  /* 2-slice rows partially used */
	uint16_t slice_row_2_f_used;  /* 2-slice rows fully used */
	uint16_t slice_row_4_used;    /* 4-slice rows fully used */
	uint16_t unused_row_number;   /* number of unused rows */
	uint8_t  reserved[2];
} __rte_packed_end tf_resc_wc_tcam_usage_t;

/* Resource Internal EM memory pool; vary size records */
typedef struct __rte_packed_begin tf_resc_em_usage {
	uint16_t max_entries;   /* Max 16-Bytes entries */
	uint16_t used_entries;  /* each record takes up to 7 entries by design */
} __rte_packed_end tf_resc_em_usage_t;

/* Resource Meter */
typedef struct __rte_packed_begin tf_resc_meter_usage {
	uint16_t max_meter_instance;    /* 1023 for Thor, app can reserve some entries */
	uint16_t max_meter_profile;     /* 256 for Thor, app can reserve some profiles  */
	uint16_t used_meter_instance;   /* meter instance: fixed size record */
	uint16_t used_meter_profile;    /* meter profile: fixed size record */
} __rte_packed_end tf_resc_meter_usage_t;

/* Resource Counter */
typedef struct __rte_packed_begin tf_resc_cnt_usage {
	uint16_t max_entries;           /* each counter take 64-Bytes */
	uint16_t used_entries;          /* each record uses one entry */
} __rte_packed_end tf_resc_cnt_usage_t;

/* Resource Action */
typedef struct __rte_packed_begin tf_resc_act_usage {
	uint16_t max_entries;              /* Max 8-Bytes entries */
	uint16_t num_compact_act_records;  /* 8-Bytes records */
	uint16_t num_full_act_records;     /* 16-Bytes records */
	uint16_t free_entries;             /* unused entries */
} __rte_packed_end tf_resc_act_usage_t;

/* Resource SP SMAC  */
typedef struct __rte_packed_begin tf_resc_act_sp_smac_usage {
	uint16_t max_entries;              /* Max 8-Bytes entries */
	uint16_t num_sp_smac_records;      /* 8-Bytes records */
	uint16_t num_sp_smac_ipv4_records; /* 8-Bytes records */
	uint16_t num_sp_smac_ipv6_records; /* 16-Bytes records */
	uint16_t free_entries;             /* unused entries */
} __rte_packed_end tf_resc_act_sp_smac_usage_t;

/* Resource ACT MODIFY and ACT ENCAP */
typedef struct __rte_packed_begin tf_resc_act_mod_enc_usage {
	uint16_t max_entries;	            /* Max 8-Bytes entries */
	struct {
		uint16_t num_8b_records;    /* 8-bytes records */
		uint16_t num_16b_records;   /* 16-bytes records  */
		uint16_t num_32b_records;   /* 32-bytes records  */
		uint16_t num_64b_records;   /* 64-bytes records  */
		uint16_t num_128b_records;  /* 128-bytes records  */
	} data;
	int16_t free_entries; /* unused entries */
} __rte_packed_end tf_resc_act_mod_enc_usage_t;

/* All types of resource usage on both direction */
typedef struct __rte_packed_begin cfa_tf_resc_usage {
	tf_resc_em_usage_t           em_int_usage;
	tf_resc_wc_tcam_usage_t      wc_tcam_usage;
	tf_resc_cnt_usage_t          cnt_usage;
	tf_resc_act_usage_t          act_usage;
	tf_resc_meter_usage_t        meter_usage;
	tf_resc_act_mod_enc_usage_t  mod_encap_usage;
	tf_resc_act_sp_smac_usage_t  sp_smac_usage;
} __rte_packed_end cfa_tf_resc_usage_t;

/* global data stored in firmware memory and TruFlow driver */
extern cfa_tf_resc_usage_t tf_resc_usage[TF_DIR_MAX];

void tf_resc_usage_reset(struct tf *tfp, enum tf_device_type type);

void tf_tcam_usage_init(struct tf *tfp);

int tf_tcam_usage_update(struct tf *tfp,
			 enum tf_dir dir,
			 int tcam_tbl_type,
			 void *key_row,
			 enum tf_resc_opt resc_opt);

void tf_em_usage_init(struct tf *tfp, enum tf_dir dir, uint16_t max_entries);

int tf_em_usage_update(struct tf *tfp,
		       enum tf_dir dir,
		       uint16_t size,
		       enum tf_resc_opt resc_opt);

void tf_tbl_usage_init(struct tf *tfp,
		       enum tf_dir dir,
		       uint32_t tbl_type,
		       uint16_t max_entries);

int tf_tbl_usage_update(struct tf *tfp,
			enum tf_dir dir,
			uint32_t tbl_type,
			enum tf_resc_opt resc_opt);

void dump_tf_resc_usage(enum tf_dir dir, void *data, uint32_t size);

extern struct tf_rm_element_cfg tf_tbl_p58[TF_DIR_MAX][TF_TBL_TYPE_MAX];

void tf_resc_pause_usage_update(void);

void tf_resc_resume_usage_update(void);

void tf_resc_usage_update_all(struct bnxt *bp);

#endif /* TF_FLOW_SCALE_QUERY */

#endif /* _TF_RESOURCES_H_ */
