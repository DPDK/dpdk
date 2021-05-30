/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Mon Sep 21 14:21:33 2020 */

#ifndef ULP_TEMPLATE_DB_TBL_H_
#define ULP_TEMPLATE_DB_TBL_H_

#include "ulp_template_struct.h"

/* WH_PLUS template table declarations */
extern struct bnxt_ulp_mapper_tmpl_info ulp_wh_plus_class_tmpl_list[];

extern struct bnxt_ulp_mapper_tbl_info ulp_wh_plus_class_tbl_list[];

extern struct
bnxt_ulp_mapper_key_field_info ulp_wh_plus_class_key_field_list[];

extern struct
bnxt_ulp_mapper_result_field_info ulp_wh_plus_class_result_field_list[];

extern struct bnxt_ulp_mapper_ident_info ulp_wh_plus_class_ident_list[];

extern struct bnxt_ulp_mapper_tmpl_info ulp_wh_plus_act_tmpl_list[];

extern struct bnxt_ulp_mapper_tbl_info ulp_wh_plus_act_tbl_list[];

extern struct
bnxt_ulp_mapper_result_field_info ulp_wh_plus_act_result_field_list[];

/* STINGRAY template table declarations */
extern struct bnxt_ulp_mapper_tmpl_info ulp_stingray_class_tmpl_list[];

extern struct bnxt_ulp_mapper_tbl_info ulp_stingray_class_tbl_list[];

extern struct
bnxt_ulp_mapper_key_field_info ulp_stingray_class_key_field_list[];

extern struct
bnxt_ulp_mapper_result_field_info ulp_stingray_class_result_field_list[];

extern struct bnxt_ulp_mapper_ident_info ulp_stingray_class_ident_list[];

extern struct bnxt_ulp_mapper_tmpl_info ulp_stingray_act_tmpl_list[];

extern struct bnxt_ulp_mapper_tbl_info ulp_stingray_act_tbl_list[];

extern struct
bnxt_ulp_mapper_result_field_info ulp_stingray_act_result_field_list[];

extern uint8_t ulp_glb_field_tbl[];
#endif
