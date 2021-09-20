/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <string.h>

#include "tf_util.h"

const char *
tf_dir_2_str(enum tf_dir dir)
{
	switch (dir) {
	case TF_DIR_RX:
		return "RX";
	case TF_DIR_TX:
		return "TX";
	default:
		return "Invalid direction";
	}
}

const char *
tf_ident_2_str(enum tf_identifier_type id_type)
{
	switch (id_type) {
	case TF_IDENT_TYPE_L2_CTXT_HIGH:
		return "l2_ctxt_remap_high";
	case TF_IDENT_TYPE_L2_CTXT_LOW:
		return "l2_ctxt_remap_low";
	case TF_IDENT_TYPE_PROF_FUNC:
		return "prof_func";
	case TF_IDENT_TYPE_WC_PROF:
		return "wc_prof";
	case TF_IDENT_TYPE_EM_PROF:
		return "em_prof";
	case TF_IDENT_TYPE_L2_FUNC:
		return "l2_func";
	default:
		return "Invalid identifier";
	}
}

const char *
tf_tcam_tbl_2_str(enum tf_tcam_tbl_type tcam_type)
{
	switch (tcam_type) {
	case TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH:
		return "l2_ctxt_tcam_high";
	case TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW:
		return "l2_ctxt_tcam_low";
	case TF_TCAM_TBL_TYPE_PROF_TCAM:
		return "prof_tcam";
	case TF_TCAM_TBL_TYPE_WC_TCAM:
		return "wc_tcam";
	case TF_TCAM_TBL_TYPE_VEB_TCAM:
		return "veb_tcam";
	case TF_TCAM_TBL_TYPE_SP_TCAM:
		return "sp_tcam";
	case TF_TCAM_TBL_TYPE_CT_RULE_TCAM:
		return "ct_rule_tcam";
#ifdef TF_TCAM_SHARED
	case TF_TCAM_TBL_TYPE_WC_TCAM_HIGH:
		return "wc_tcam_hi";
	case TF_TCAM_TBL_TYPE_WC_TCAM_LOW:
		return "wc_tcam_lo";
#endif
	default:
		return "Invalid tcam table type";
	}
}

const char *
tf_tbl_type_2_str(enum tf_tbl_type tbl_type)
{
	switch (tbl_type) {
	case TF_TBL_TYPE_FULL_ACT_RECORD:
		return "Full Action record";
	case TF_TBL_TYPE_COMPACT_ACT_RECORD:
		return "Compact Action record";
	case TF_TBL_TYPE_MCAST_GROUPS:
		return "Multicast Groups";
	case TF_TBL_TYPE_ACT_ENCAP_8B:
		return "Encap 8B";
	case TF_TBL_TYPE_ACT_ENCAP_16B:
		return "Encap 16B";
	case TF_TBL_TYPE_ACT_ENCAP_32B:
		return "Encap 32B";
	case TF_TBL_TYPE_ACT_ENCAP_64B:
		return "Encap 64B";
	case TF_TBL_TYPE_ACT_SP_SMAC:
		return "Source Properties SMAC";
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV4:
		return "Source Properties SMAC IPv4";
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV6:
		return "Source Properties SMAC IPv6";
	case TF_TBL_TYPE_ACT_STATS_64:
		return "Stats 64B";
	case TF_TBL_TYPE_ACT_MODIFY_IPV4:
		return "Modify IPv4";
	case TF_TBL_TYPE_ACT_MODIFY_8B:
		return "Modify 8B";
	case TF_TBL_TYPE_ACT_MODIFY_16B:
		return "Modify 16B";
	case TF_TBL_TYPE_ACT_MODIFY_32B:
		return "Modify 32B";
	case TF_TBL_TYPE_ACT_MODIFY_64B:
		return "Modify 64B";
	case TF_TBL_TYPE_METER_PROF:
		return "Meter Profile";
	case TF_TBL_TYPE_METER_INST:
		return "Meter";
	case TF_TBL_TYPE_MIRROR_CONFIG:
		return "Mirror";
	case TF_TBL_TYPE_UPAR:
		return "UPAR";
	case TF_TBL_TYPE_METADATA:
		return "Metadata";
	case TF_TBL_TYPE_EM_FKB:
		return "EM Flexible Key Builder";
	case TF_TBL_TYPE_WC_FKB:
		return "WC Flexible Key Builder";
	case TF_TBL_TYPE_EXT:
		return "External";
	case TF_TBL_TYPE_METER_DROP_CNT:
		return "Meter drop counter";
	default:
		return "Invalid tbl type";
	}
}

const char *
tf_em_tbl_type_2_str(enum tf_em_tbl_type em_type)
{
	switch (em_type) {
	case TF_EM_TBL_TYPE_EM_RECORD:
		return "EM Record";
	case TF_EM_TBL_TYPE_TBL_SCOPE:
		return "Table Scope";
	default:
		return "Invalid EM type";
	}
}

const char *
tf_module_subtype_2_str(enum tf_module_type module,
			uint16_t subtype)
{
	switch (module) {
	case TF_MODULE_TYPE_IDENTIFIER:
		return tf_ident_2_str(subtype);
	case TF_MODULE_TYPE_TABLE:
		return tf_tbl_type_2_str(subtype);
	case TF_MODULE_TYPE_TCAM:
		return tf_tcam_tbl_2_str(subtype);
	case TF_MODULE_TYPE_EM:
		return tf_em_tbl_type_2_str(subtype);
	default:
		return "Invalid Module type";
	}
}

const char *
tf_module_2_str(enum tf_module_type module)
{
	switch (module) {
	case TF_MODULE_TYPE_IDENTIFIER:
		return "Identifier";
	case TF_MODULE_TYPE_TABLE:
		return "Table";
	case TF_MODULE_TYPE_TCAM:
		return "TCAM";
	case TF_MODULE_TYPE_EM:
		return "EM";
	default:
		return "Invalid Device Module type";
	}
}
