/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <string.h>

#include <rte_common.h>

#include "tf_rm.h"
#include "tf_core.h"
#include "tf_session.h"
#include "tf_resources.h"
#include "tf_msg.h"
#include "bnxt.h"

/**
 * Internal macro to perform HW resource allocation check between what
 * firmware reports vs what was statically requested.
 *
 * Parameters:
 *   struct tf_rm_hw_query    *hquery      - Pointer to the hw query result
 *   enum tf_dir               dir         - Direction to process
 *   enum tf_resource_type_hw  hcapi_type  - HCAPI type, the index element
 *                                           in the hw query structure
 *   define                    def_value   - Define value to check against
 *   uint32_t                 *eflag       - Result of the check
 */
#define TF_RM_CHECK_HW_ALLOC(hquery, dir, hcapi_type, def_value, eflag) do {  \
	if ((dir) == TF_DIR_RX) {					      \
		if ((hquery)->hw_query[(hcapi_type)].max != def_value ## _RX) \
			*(eflag) |= 1 << (hcapi_type);			      \
	} else {							      \
		if ((hquery)->hw_query[(hcapi_type)].max != def_value ## _TX) \
			*(eflag) |= 1 << (hcapi_type);			      \
	}								      \
} while (0)

/**
 * Internal macro to perform HW resource allocation check between what
 * firmware reports vs what was statically requested.
 *
 * Parameters:
 *   struct tf_rm_sram_query   *squery      - Pointer to the sram query result
 *   enum tf_dir                dir         - Direction to process
 *   enum tf_resource_type_sram hcapi_type  - HCAPI type, the index element
 *                                            in the hw query structure
 *   define                     def_value   - Define value to check against
 *   uint32_t                  *eflag       - Result of the check
 */
#define TF_RM_CHECK_SRAM_ALLOC(squery, dir, hcapi_type, def_value, eflag) do { \
	if ((dir) == TF_DIR_RX) {					       \
		if ((squery)->sram_query[(hcapi_type)].max != def_value ## _RX)\
			*(eflag) |= 1 << (hcapi_type);			       \
	} else {							       \
		if ((squery)->sram_query[(hcapi_type)].max != def_value ## _TX)\
			*(eflag) |= 1 << (hcapi_type);			       \
	}								       \
} while (0)

/**
 * Internal macro to convert a reserved resource define name to be
 * direction specific.
 *
 * Parameters:
 *   enum tf_dir    dir         - Direction to process
 *   string         type        - Type name to append RX or TX to
 *   string         dtype       - Direction specific type
 *
 *
 */
#define TF_RESC_RSVD(dir, type, dtype) do {	\
		if ((dir) == TF_DIR_RX)		\
			(dtype) = type ## _RX;	\
		else				\
			(dtype) = type ## _TX;	\
	} while (0)

const char
*tf_dir_2_str(enum tf_dir dir)
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

const char
*tf_ident_2_str(enum tf_identifier_type id_type)
{
	switch (id_type) {
	case TF_IDENT_TYPE_L2_CTXT:
		return "l2_ctxt_remap";
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

const char
*tf_tcam_tbl_2_str(enum tf_tcam_tbl_type tcam_type)
{
	switch (tcam_type) {
	case TF_TCAM_TBL_TYPE_L2_CTXT_TCAM:
		return "l2_ctxt_tcam";
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
	default:
		return "Invalid tcam table type";
	}
}

const char
*tf_hcapi_hw_2_str(enum tf_resource_type_hw hw_type)
{
	switch (hw_type) {
	case TF_RESC_TYPE_HW_L2_CTXT_TCAM:
		return "L2 ctxt tcam";
	case TF_RESC_TYPE_HW_PROF_FUNC:
		return "Profile Func";
	case TF_RESC_TYPE_HW_PROF_TCAM:
		return "Profile tcam";
	case TF_RESC_TYPE_HW_EM_PROF_ID:
		return "EM profile id";
	case TF_RESC_TYPE_HW_EM_REC:
		return "EM record";
	case TF_RESC_TYPE_HW_WC_TCAM_PROF_ID:
		return "WC tcam profile id";
	case TF_RESC_TYPE_HW_WC_TCAM:
		return "WC tcam";
	case TF_RESC_TYPE_HW_METER_PROF:
		return "Meter profile";
	case TF_RESC_TYPE_HW_METER_INST:
		return "Meter instance";
	case TF_RESC_TYPE_HW_MIRROR:
		return "Mirror";
	case TF_RESC_TYPE_HW_UPAR:
		return "UPAR";
	case TF_RESC_TYPE_HW_SP_TCAM:
		return "Source properties tcam";
	case TF_RESC_TYPE_HW_L2_FUNC:
		return "L2 Function";
	case TF_RESC_TYPE_HW_FKB:
		return "FKB";
	case TF_RESC_TYPE_HW_TBL_SCOPE:
		return "Table scope";
	case TF_RESC_TYPE_HW_EPOCH0:
		return "EPOCH0";
	case TF_RESC_TYPE_HW_EPOCH1:
		return "EPOCH1";
	case TF_RESC_TYPE_HW_METADATA:
		return "Metadata";
	case TF_RESC_TYPE_HW_CT_STATE:
		return "Connection tracking state";
	case TF_RESC_TYPE_HW_RANGE_PROF:
		return "Range profile";
	case TF_RESC_TYPE_HW_RANGE_ENTRY:
		return "Range entry";
	case TF_RESC_TYPE_HW_LAG_ENTRY:
		return "LAG";
	default:
		return "Invalid identifier";
	}
}

const char
*tf_hcapi_sram_2_str(enum tf_resource_type_sram sram_type)
{
	switch (sram_type) {
	case TF_RESC_TYPE_SRAM_FULL_ACTION:
		return "Full action";
	case TF_RESC_TYPE_SRAM_MCG:
		return "MCG";
	case TF_RESC_TYPE_SRAM_ENCAP_8B:
		return "Encap 8B";
	case TF_RESC_TYPE_SRAM_ENCAP_16B:
		return "Encap 16B";
	case TF_RESC_TYPE_SRAM_ENCAP_64B:
		return "Encap 64B";
	case TF_RESC_TYPE_SRAM_SP_SMAC:
		return "Source properties SMAC";
	case TF_RESC_TYPE_SRAM_SP_SMAC_IPV4:
		return "Source properties SMAC IPv4";
	case TF_RESC_TYPE_SRAM_SP_SMAC_IPV6:
		return "Source properties IPv6";
	case TF_RESC_TYPE_SRAM_COUNTER_64B:
		return "Counter 64B";
	case TF_RESC_TYPE_SRAM_NAT_SPORT:
		return "NAT source port";
	case TF_RESC_TYPE_SRAM_NAT_DPORT:
		return "NAT destination port";
	case TF_RESC_TYPE_SRAM_NAT_S_IPV4:
		return "NAT source IPv4";
	case TF_RESC_TYPE_SRAM_NAT_D_IPV4:
		return "NAT destination IPv4";
	default:
		return "Invalid identifier";
	}
}

/**
 * Helper function to perform a HW HCAPI resource type lookup against
 * the reserved value of the same static type.
 *
 * Returns:
 *   -EOPNOTSUPP - Reserved resource type not supported
 *   Value       - Integer value of the reserved value for the requested type
 */
static int
tf_rm_rsvd_hw_value(enum tf_dir dir, enum tf_resource_type_hw index)
{
	uint32_t value = -EOPNOTSUPP;

	switch (index) {
	case TF_RESC_TYPE_HW_L2_CTXT_TCAM:
		TF_RESC_RSVD(dir, TF_RSVD_L2_CTXT_TCAM, value);
		break;
	case TF_RESC_TYPE_HW_PROF_FUNC:
		TF_RESC_RSVD(dir, TF_RSVD_PROF_FUNC, value);
		break;
	case TF_RESC_TYPE_HW_PROF_TCAM:
		TF_RESC_RSVD(dir, TF_RSVD_PROF_TCAM, value);
		break;
	case TF_RESC_TYPE_HW_EM_PROF_ID:
		TF_RESC_RSVD(dir, TF_RSVD_EM_PROF_ID, value);
		break;
	case TF_RESC_TYPE_HW_EM_REC:
		TF_RESC_RSVD(dir, TF_RSVD_EM_REC, value);
		break;
	case TF_RESC_TYPE_HW_WC_TCAM_PROF_ID:
		TF_RESC_RSVD(dir, TF_RSVD_WC_TCAM_PROF_ID, value);
		break;
	case TF_RESC_TYPE_HW_WC_TCAM:
		TF_RESC_RSVD(dir, TF_RSVD_WC_TCAM, value);
		break;
	case TF_RESC_TYPE_HW_METER_PROF:
		TF_RESC_RSVD(dir, TF_RSVD_METER_PROF, value);
		break;
	case TF_RESC_TYPE_HW_METER_INST:
		TF_RESC_RSVD(dir, TF_RSVD_METER_INST, value);
		break;
	case TF_RESC_TYPE_HW_MIRROR:
		TF_RESC_RSVD(dir, TF_RSVD_MIRROR, value);
		break;
	case TF_RESC_TYPE_HW_UPAR:
		TF_RESC_RSVD(dir, TF_RSVD_UPAR, value);
		break;
	case TF_RESC_TYPE_HW_SP_TCAM:
		TF_RESC_RSVD(dir, TF_RSVD_SP_TCAM, value);
		break;
	case TF_RESC_TYPE_HW_L2_FUNC:
		TF_RESC_RSVD(dir, TF_RSVD_L2_FUNC, value);
		break;
	case TF_RESC_TYPE_HW_FKB:
		TF_RESC_RSVD(dir, TF_RSVD_FKB, value);
		break;
	case TF_RESC_TYPE_HW_TBL_SCOPE:
		TF_RESC_RSVD(dir, TF_RSVD_TBL_SCOPE, value);
		break;
	case TF_RESC_TYPE_HW_EPOCH0:
		TF_RESC_RSVD(dir, TF_RSVD_EPOCH0, value);
		break;
	case TF_RESC_TYPE_HW_EPOCH1:
		TF_RESC_RSVD(dir, TF_RSVD_EPOCH1, value);
		break;
	case TF_RESC_TYPE_HW_METADATA:
		TF_RESC_RSVD(dir, TF_RSVD_METADATA, value);
		break;
	case TF_RESC_TYPE_HW_CT_STATE:
		TF_RESC_RSVD(dir, TF_RSVD_CT_STATE, value);
		break;
	case TF_RESC_TYPE_HW_RANGE_PROF:
		TF_RESC_RSVD(dir, TF_RSVD_RANGE_PROF, value);
		break;
	case TF_RESC_TYPE_HW_RANGE_ENTRY:
		TF_RESC_RSVD(dir, TF_RSVD_RANGE_ENTRY, value);
		break;
	case TF_RESC_TYPE_HW_LAG_ENTRY:
		TF_RESC_RSVD(dir, TF_RSVD_LAG_ENTRY, value);
		break;
	default:
		break;
	}

	return value;
}

/**
 * Helper function to perform a SRAM HCAPI resource type lookup
 * against the reserved value of the same static type.
 *
 * Returns:
 *   -EOPNOTSUPP - Reserved resource type not supported
 *   Value       - Integer value of the reserved value for the requested type
 */
static int
tf_rm_rsvd_sram_value(enum tf_dir dir, enum tf_resource_type_sram index)
{
	uint32_t value = -EOPNOTSUPP;

	switch (index) {
	case TF_RESC_TYPE_SRAM_FULL_ACTION:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_FULL_ACTION, value);
		break;
	case TF_RESC_TYPE_SRAM_MCG:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_MCG, value);
		break;
	case TF_RESC_TYPE_SRAM_ENCAP_8B:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_ENCAP_8B, value);
		break;
	case TF_RESC_TYPE_SRAM_ENCAP_16B:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_ENCAP_16B, value);
		break;
	case TF_RESC_TYPE_SRAM_ENCAP_64B:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_ENCAP_64B, value);
		break;
	case TF_RESC_TYPE_SRAM_SP_SMAC:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_SP_SMAC, value);
		break;
	case TF_RESC_TYPE_SRAM_SP_SMAC_IPV4:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_SP_SMAC_IPV4, value);
		break;
	case TF_RESC_TYPE_SRAM_SP_SMAC_IPV6:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_SP_SMAC_IPV6, value);
		break;
	case TF_RESC_TYPE_SRAM_COUNTER_64B:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_COUNTER_64B, value);
		break;
	case TF_RESC_TYPE_SRAM_NAT_SPORT:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_NAT_SPORT, value);
		break;
	case TF_RESC_TYPE_SRAM_NAT_DPORT:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_NAT_DPORT, value);
		break;
	case TF_RESC_TYPE_SRAM_NAT_S_IPV4:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_NAT_S_IPV4, value);
		break;
	case TF_RESC_TYPE_SRAM_NAT_D_IPV4:
		TF_RESC_RSVD(dir, TF_RSVD_SRAM_NAT_D_IPV4, value);
		break;
	default:
		break;
	}

	return value;
}

/**
 * Helper function to print all the HW resource qcaps errors reported
 * in the error_flag.
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * [in] error_flag
 *   Pointer to the hw error flags created at time of the query check
 */
static void
tf_rm_print_hw_qcaps_error(enum tf_dir dir,
			   struct tf_rm_hw_query *hw_query,
			   uint32_t *error_flag)
{
	int i;

	PMD_DRV_LOG(ERR, "QCAPS errors HW\n");
	PMD_DRV_LOG(ERR, "  Direction: %s\n", tf_dir_2_str(dir));
	PMD_DRV_LOG(ERR, "  Elements:\n");

	for (i = 0; i < TF_RESC_TYPE_HW_MAX; i++) {
		if (*error_flag & 1 << i)
			PMD_DRV_LOG(ERR, "    %s, %d elem available, req:%d\n",
				    tf_hcapi_hw_2_str(i),
				    hw_query->hw_query[i].max,
				    tf_rm_rsvd_hw_value(dir, i));
	}
}

/**
 * Helper function to print all the SRAM resource qcaps errors
 * reported in the error_flag.
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * [in] error_flag
 *   Pointer to the sram error flags created at time of the query check
 */
static void
tf_rm_print_sram_qcaps_error(enum tf_dir dir,
			     struct tf_rm_sram_query *sram_query,
			     uint32_t *error_flag)
{
	int i;

	PMD_DRV_LOG(ERR, "QCAPS errors SRAM\n");
	PMD_DRV_LOG(ERR, "  Direction: %s\n", tf_dir_2_str(dir));
	PMD_DRV_LOG(ERR, "  Elements:\n");

	for (i = 0; i < TF_RESC_TYPE_SRAM_MAX; i++) {
		if (*error_flag & 1 << i)
			PMD_DRV_LOG(ERR, "    %s, %d elem available, req:%d\n",
				    tf_hcapi_sram_2_str(i),
				    sram_query->sram_query[i].max,
				    tf_rm_rsvd_sram_value(dir, i));
	}
}

/**
 * Performs a HW resource check between what firmware capability
 * reports and what the core expects is available.
 *
 * Firmware performs the resource carving at AFM init time and the
 * resource capability is reported in the TruFlow qcaps msg.
 *
 * [in] query
 *   Pointer to HW Query data structure. Query holds what the firmware
 *   offers of the HW resources.
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * [in/out] error_flag
 *   Pointer to a bit array indicating the error of a single HCAPI
 *   resource type. When a bit is set to 1, the HCAPI resource type
 *   failed static allocation.
 *
 * Returns:
 *  0       - Success
 *  -ENOMEM - Failure on one of the allocated resources. Check the
 *            error_flag for what types are flagged errored.
 */
static int
tf_rm_check_hw_qcaps_static(struct tf_rm_hw_query *query,
			    enum tf_dir dir,
			    uint32_t *error_flag)
{
	*error_flag = 0;

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_L2_CTXT_TCAM,
			     TF_RSVD_L2_CTXT_TCAM,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_PROF_FUNC,
			     TF_RSVD_PROF_FUNC,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_PROF_TCAM,
			     TF_RSVD_PROF_TCAM,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_EM_PROF_ID,
			     TF_RSVD_EM_PROF_ID,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_EM_REC,
			     TF_RSVD_EM_REC,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_WC_TCAM_PROF_ID,
			     TF_RSVD_WC_TCAM_PROF_ID,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_WC_TCAM,
			     TF_RSVD_WC_TCAM,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_METER_PROF,
			     TF_RSVD_METER_PROF,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_METER_INST,
			     TF_RSVD_METER_INST,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_MIRROR,
			     TF_RSVD_MIRROR,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_UPAR,
			     TF_RSVD_UPAR,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_SP_TCAM,
			     TF_RSVD_SP_TCAM,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_L2_FUNC,
			     TF_RSVD_L2_FUNC,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_FKB,
			     TF_RSVD_FKB,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_TBL_SCOPE,
			     TF_RSVD_TBL_SCOPE,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_EPOCH0,
			     TF_RSVD_EPOCH0,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_EPOCH1,
			     TF_RSVD_EPOCH1,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_METADATA,
			     TF_RSVD_METADATA,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_CT_STATE,
			     TF_RSVD_CT_STATE,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_RANGE_PROF,
			     TF_RSVD_RANGE_PROF,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_RANGE_ENTRY,
			     TF_RSVD_RANGE_ENTRY,
			     error_flag);

	TF_RM_CHECK_HW_ALLOC(query,
			     dir,
			     TF_RESC_TYPE_HW_LAG_ENTRY,
			     TF_RSVD_LAG_ENTRY,
			     error_flag);

	if (*error_flag != 0)
		return -ENOMEM;

	return 0;
}

/**
 * Performs a SRAM resource check between what firmware capability
 * reports and what the core expects is available.
 *
 * Firmware performs the resource carving at AFM init time and the
 * resource capability is reported in the TruFlow qcaps msg.
 *
 * [in] query
 *   Pointer to SRAM Query data structure. Query holds what the
 *   firmware offers of the SRAM resources.
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * [in/out] error_flag
 *   Pointer to a bit array indicating the error of a single HCAPI
 *   resource type. When a bit is set to 1, the HCAPI resource type
 *   failed static allocation.
 *
 * Returns:
 *  0       - Success
 *  -ENOMEM - Failure on one of the allocated resources. Check the
 *            error_flag for what types are flagged errored.
 */
static int
tf_rm_check_sram_qcaps_static(struct tf_rm_sram_query *query,
			      enum tf_dir dir,
			      uint32_t *error_flag)
{
	*error_flag = 0;

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_FULL_ACTION,
			       TF_RSVD_SRAM_FULL_ACTION,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_MCG,
			       TF_RSVD_SRAM_MCG,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_ENCAP_8B,
			       TF_RSVD_SRAM_ENCAP_8B,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_ENCAP_16B,
			       TF_RSVD_SRAM_ENCAP_16B,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_ENCAP_64B,
			       TF_RSVD_SRAM_ENCAP_64B,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_SP_SMAC,
			       TF_RSVD_SRAM_SP_SMAC,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_SP_SMAC_IPV4,
			       TF_RSVD_SRAM_SP_SMAC_IPV4,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_SP_SMAC_IPV6,
			       TF_RSVD_SRAM_SP_SMAC_IPV6,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_COUNTER_64B,
			       TF_RSVD_SRAM_COUNTER_64B,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_NAT_SPORT,
			       TF_RSVD_SRAM_NAT_SPORT,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_NAT_DPORT,
			       TF_RSVD_SRAM_NAT_DPORT,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_NAT_S_IPV4,
			       TF_RSVD_SRAM_NAT_S_IPV4,
			       error_flag);

	TF_RM_CHECK_SRAM_ALLOC(query,
			       dir,
			       TF_RESC_TYPE_SRAM_NAT_D_IPV4,
			       TF_RSVD_SRAM_NAT_D_IPV4,
			       error_flag);

	if (*error_flag != 0)
		return -ENOMEM;

	return 0;
}

/**
 * Internal function to mark pool entries used.
 */
static void
tf_rm_reserve_range(uint32_t count,
		    uint32_t rsv_begin,
		    uint32_t rsv_end,
		    uint32_t max,
		    struct bitalloc *pool)
{
	uint32_t i;

	/* If no resources has been requested we mark everything
	 * 'used'
	 */
	if (count == 0)	{
		for (i = 0; i < max; i++)
			ba_alloc_index(pool, i);
	} else {
		/* Support 2 main modes
		 * Reserved range starts from bottom up (with
		 * pre-reserved value or not)
		 * - begin = 0 to end xx
		 * - begin = 1 to end xx
		 *
		 * Reserved range starts from top down
		 * - begin = yy to end max
		 */

		/* Bottom up check, start from 0 */
		if (rsv_begin == 0) {
			for (i = rsv_end + 1; i < max; i++)
				ba_alloc_index(pool, i);
		}

		/* Bottom up check, start from 1 or higher OR
		 * Top Down
		 */
		if (rsv_begin >= 1) {
			/* Allocate from 0 until start */
			for (i = 0; i < rsv_begin; i++)
				ba_alloc_index(pool, i);

			/* Skip and then do the remaining */
			if (rsv_end < max - 1) {
				for (i = rsv_end; i < max; i++)
					ba_alloc_index(pool, i);
			}
		}
	}
}

/**
 * Internal function to mark all the l2 ctxt allocated that Truflow
 * does not own.
 */
static void
tf_rm_rsvd_l2_ctxt(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_L2_CTXT_TCAM;
	uint32_t end = 0;

	/* l2 ctxt rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_L2_CTXT_TCAM,
			    tfs->TF_L2_CTXT_TCAM_POOL_NAME_RX);

	/* l2 ctxt tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_L2_CTXT_TCAM,
			    tfs->TF_L2_CTXT_TCAM_POOL_NAME_TX);
}

/**
 * Internal function to mark all the profile tcam and profile func
 * resources that Truflow does not own.
 */
static void
tf_rm_rsvd_prof(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_PROF_FUNC;
	uint32_t end = 0;

	/* profile func rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_PROF_FUNC,
			    tfs->TF_PROF_FUNC_POOL_NAME_RX);

	/* profile func tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_PROF_FUNC,
			    tfs->TF_PROF_FUNC_POOL_NAME_TX);

	index = TF_RESC_TYPE_HW_PROF_TCAM;

	/* profile tcam rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_PROF_TCAM,
			    tfs->TF_PROF_TCAM_POOL_NAME_RX);

	/* profile tcam tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_PROF_TCAM,
			    tfs->TF_PROF_TCAM_POOL_NAME_TX);
}

/**
 * Internal function to mark all the em profile id allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_em_prof(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_EM_PROF_ID;
	uint32_t end = 0;

	/* em prof id rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_EM_PROF_ID,
			    tfs->TF_EM_PROF_ID_POOL_NAME_RX);

	/* em prof id tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_EM_PROF_ID,
			    tfs->TF_EM_PROF_ID_POOL_NAME_TX);
}

/**
 * Internal function to mark all the wildcard tcam and profile id
 * resources that Truflow does not own.
 */
static void
tf_rm_rsvd_wc(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_WC_TCAM_PROF_ID;
	uint32_t end = 0;

	/* wc profile id rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_WC_PROF_ID,
			    tfs->TF_WC_TCAM_PROF_ID_POOL_NAME_RX);

	/* wc profile id tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_WC_PROF_ID,
			    tfs->TF_WC_TCAM_PROF_ID_POOL_NAME_TX);

	index = TF_RESC_TYPE_HW_WC_TCAM;

	/* wc tcam rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_WC_TCAM_ROW,
			    tfs->TF_WC_TCAM_POOL_NAME_RX);

	/* wc tcam tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_WC_TCAM_ROW,
			    tfs->TF_WC_TCAM_POOL_NAME_TX);
}

/**
 * Internal function to mark all the meter resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_meter(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_METER_PROF;
	uint32_t end = 0;

	/* meter profiles rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_METER_PROF,
			    tfs->TF_METER_PROF_POOL_NAME_RX);

	/* meter profiles tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_METER_PROF,
			    tfs->TF_METER_PROF_POOL_NAME_TX);

	index = TF_RESC_TYPE_HW_METER_INST;

	/* meter rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_METER,
			    tfs->TF_METER_INST_POOL_NAME_RX);

	/* meter tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_METER,
			    tfs->TF_METER_INST_POOL_NAME_TX);
}

/**
 * Internal function to mark all the mirror resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_mirror(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_MIRROR;
	uint32_t end = 0;

	/* mirror rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_MIRROR,
			    tfs->TF_MIRROR_POOL_NAME_RX);

	/* mirror tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_MIRROR,
			    tfs->TF_MIRROR_POOL_NAME_TX);
}

/**
 * Internal function to mark all the upar resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_upar(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_UPAR;
	uint32_t end = 0;

	/* upar rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_UPAR,
			    tfs->TF_UPAR_POOL_NAME_RX);

	/* upar tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_UPAR,
			    tfs->TF_UPAR_POOL_NAME_TX);
}

/**
 * Internal function to mark all the sp tcam resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_sp_tcam(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_SP_TCAM;
	uint32_t end = 0;

	/* sp tcam rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_SP_TCAM,
			    tfs->TF_SP_TCAM_POOL_NAME_RX);

	/* sp tcam tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_SP_TCAM,
			    tfs->TF_SP_TCAM_POOL_NAME_TX);
}

/**
 * Internal function to mark all the l2 func resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_l2_func(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_L2_FUNC;
	uint32_t end = 0;

	/* l2 func rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_L2_FUNC,
			    tfs->TF_L2_FUNC_POOL_NAME_RX);

	/* l2 func tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_L2_FUNC,
			    tfs->TF_L2_FUNC_POOL_NAME_TX);
}

/**
 * Internal function to mark all the fkb resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_fkb(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_FKB;
	uint32_t end = 0;

	/* fkb rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_FKB,
			    tfs->TF_FKB_POOL_NAME_RX);

	/* fkb tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_FKB,
			    tfs->TF_FKB_POOL_NAME_TX);
}

/**
 * Internal function to mark all the tbld scope resources allocated
 * that Truflow does not own.
 */
static void
tf_rm_rsvd_tbl_scope(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_TBL_SCOPE;
	uint32_t end = 0;

	/* tbl scope rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_TBL_SCOPE,
			    tfs->TF_TBL_SCOPE_POOL_NAME_RX);

	/* tbl scope tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_TBL_SCOPE,
			    tfs->TF_TBL_SCOPE_POOL_NAME_TX);
}

/**
 * Internal function to mark all the l2 epoch resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_epoch(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_EPOCH0;
	uint32_t end = 0;

	/* epoch0 rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_EPOCH0,
			    tfs->TF_EPOCH0_POOL_NAME_RX);

	/* epoch0 tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_EPOCH0,
			    tfs->TF_EPOCH0_POOL_NAME_TX);

	index = TF_RESC_TYPE_HW_EPOCH1;

	/* epoch1 rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_EPOCH1,
			    tfs->TF_EPOCH1_POOL_NAME_RX);

	/* epoch1 tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_EPOCH1,
			    tfs->TF_EPOCH1_POOL_NAME_TX);
}

/**
 * Internal function to mark all the metadata resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_metadata(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_METADATA;
	uint32_t end = 0;

	/* metadata rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_METADATA,
			    tfs->TF_METADATA_POOL_NAME_RX);

	/* metadata tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_METADATA,
			    tfs->TF_METADATA_POOL_NAME_TX);
}

/**
 * Internal function to mark all the ct state resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_ct_state(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_CT_STATE;
	uint32_t end = 0;

	/* ct state rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_CT_STATE,
			    tfs->TF_CT_STATE_POOL_NAME_RX);

	/* ct state tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_CT_STATE,
			    tfs->TF_CT_STATE_POOL_NAME_TX);
}

/**
 * Internal function to mark all the range resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_range(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_RANGE_PROF;
	uint32_t end = 0;

	/* range profile rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_RANGE_PROF,
			    tfs->TF_RANGE_PROF_POOL_NAME_RX);

	/* range profile tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_RANGE_PROF,
			    tfs->TF_RANGE_PROF_POOL_NAME_TX);

	index = TF_RESC_TYPE_HW_RANGE_ENTRY;

	/* range entry rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_RANGE_ENTRY,
			    tfs->TF_RANGE_ENTRY_POOL_NAME_RX);

	/* range entry tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_RANGE_ENTRY,
			    tfs->TF_RANGE_ENTRY_POOL_NAME_TX);
}

/**
 * Internal function to mark all the lag resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_lag_entry(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_HW_LAG_ENTRY;
	uint32_t end = 0;

	/* lag entry rx direction */
	if (tfs->resc.rx.hw_entry[index].stride > 0)
		end = tfs->resc.rx.hw_entry[index].start +
			tfs->resc.rx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.hw_entry[index].stride,
			    tfs->resc.rx.hw_entry[index].start,
			    end,
			    TF_NUM_LAG_ENTRY,
			    tfs->TF_LAG_ENTRY_POOL_NAME_RX);

	/* lag entry tx direction */
	if (tfs->resc.tx.hw_entry[index].stride > 0)
		end = tfs->resc.tx.hw_entry[index].start +
			tfs->resc.tx.hw_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.hw_entry[index].stride,
			    tfs->resc.tx.hw_entry[index].start,
			    end,
			    TF_NUM_LAG_ENTRY,
			    tfs->TF_LAG_ENTRY_POOL_NAME_TX);
}

/**
 * Internal function to mark all the full action resources allocated
 * that Truflow does not own.
 */
static void
tf_rm_rsvd_sram_full_action(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_SRAM_FULL_ACTION;
	uint16_t end = 0;

	/* full action rx direction */
	if (tfs->resc.rx.sram_entry[index].stride > 0)
		end = tfs->resc.rx.sram_entry[index].start +
			tfs->resc.rx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.sram_entry[index].stride,
			    TF_RSVD_SRAM_FULL_ACTION_BEGIN_IDX_RX,
			    end,
			    TF_RSVD_SRAM_FULL_ACTION_RX,
			    tfs->TF_SRAM_FULL_ACTION_POOL_NAME_RX);

	/* full action tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_FULL_ACTION_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_FULL_ACTION_TX,
			    tfs->TF_SRAM_FULL_ACTION_POOL_NAME_TX);
}

/**
 * Internal function to mark all the multicast group resources
 * allocated that Truflow does not own.
 */
static void
tf_rm_rsvd_sram_mcg(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_SRAM_MCG;
	uint16_t end = 0;

	/* multicast group rx direction */
	if (tfs->resc.rx.sram_entry[index].stride > 0)
		end = tfs->resc.rx.sram_entry[index].start +
			tfs->resc.rx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.sram_entry[index].stride,
			    TF_RSVD_SRAM_MCG_BEGIN_IDX_RX,
			    end,
			    TF_RSVD_SRAM_MCG_RX,
			    tfs->TF_SRAM_MCG_POOL_NAME_RX);

	/* Multicast Group on TX is not supported */
}

/**
 * Internal function to mark all the encap resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_sram_encap(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_SRAM_ENCAP_8B;
	uint16_t end = 0;

	/* encap 8b rx direction */
	if (tfs->resc.rx.sram_entry[index].stride > 0)
		end = tfs->resc.rx.sram_entry[index].start +
			tfs->resc.rx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.sram_entry[index].stride,
			    TF_RSVD_SRAM_ENCAP_8B_BEGIN_IDX_RX,
			    end,
			    TF_RSVD_SRAM_ENCAP_8B_RX,
			    tfs->TF_SRAM_ENCAP_8B_POOL_NAME_RX);

	/* encap 8b tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_ENCAP_8B_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_ENCAP_8B_TX,
			    tfs->TF_SRAM_ENCAP_8B_POOL_NAME_TX);

	index = TF_RESC_TYPE_SRAM_ENCAP_16B;

	/* encap 16b rx direction */
	if (tfs->resc.rx.sram_entry[index].stride > 0)
		end = tfs->resc.rx.sram_entry[index].start +
			tfs->resc.rx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.sram_entry[index].stride,
			    TF_RSVD_SRAM_ENCAP_16B_BEGIN_IDX_RX,
			    end,
			    TF_RSVD_SRAM_ENCAP_16B_RX,
			    tfs->TF_SRAM_ENCAP_16B_POOL_NAME_RX);

	/* encap 16b tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_ENCAP_16B_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_ENCAP_16B_TX,
			    tfs->TF_SRAM_ENCAP_16B_POOL_NAME_TX);

	index = TF_RESC_TYPE_SRAM_ENCAP_64B;

	/* Encap 64B not supported on RX */

	/* Encap 64b tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_ENCAP_64B_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_ENCAP_64B_TX,
			    tfs->TF_SRAM_ENCAP_64B_POOL_NAME_TX);
}

/**
 * Internal function to mark all the sp resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_sram_sp(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_SRAM_SP_SMAC;
	uint16_t end = 0;

	/* sp smac rx direction */
	if (tfs->resc.rx.sram_entry[index].stride > 0)
		end = tfs->resc.rx.sram_entry[index].start +
			tfs->resc.rx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.sram_entry[index].stride,
			    TF_RSVD_SRAM_SP_SMAC_BEGIN_IDX_RX,
			    end,
			    TF_RSVD_SRAM_SP_SMAC_RX,
			    tfs->TF_SRAM_SP_SMAC_POOL_NAME_RX);

	/* sp smac tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_SP_SMAC_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_SP_SMAC_TX,
			    tfs->TF_SRAM_SP_SMAC_POOL_NAME_TX);

	index = TF_RESC_TYPE_SRAM_SP_SMAC_IPV4;

	/* SP SMAC IPv4 not supported on RX */

	/* sp smac ipv4 tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_SP_SMAC_IPV4_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_SP_SMAC_IPV4_TX,
			    tfs->TF_SRAM_SP_SMAC_IPV4_POOL_NAME_TX);

	index = TF_RESC_TYPE_SRAM_SP_SMAC_IPV6;

	/* SP SMAC IPv6 not supported on RX */

	/* sp smac ipv6 tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_SP_SMAC_IPV6_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_SP_SMAC_IPV6_TX,
			    tfs->TF_SRAM_SP_SMAC_IPV6_POOL_NAME_TX);
}

/**
 * Internal function to mark all the stat resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_sram_stats(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_SRAM_COUNTER_64B;
	uint16_t end = 0;

	/* counter 64b rx direction */
	if (tfs->resc.rx.sram_entry[index].stride > 0)
		end = tfs->resc.rx.sram_entry[index].start +
			tfs->resc.rx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.sram_entry[index].stride,
			    TF_RSVD_SRAM_COUNTER_64B_BEGIN_IDX_RX,
			    end,
			    TF_RSVD_SRAM_COUNTER_64B_RX,
			    tfs->TF_SRAM_STATS_64B_POOL_NAME_RX);

	/* counter 64b tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_COUNTER_64B_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_COUNTER_64B_TX,
			    tfs->TF_SRAM_STATS_64B_POOL_NAME_TX);
}

/**
 * Internal function to mark all the nat resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_rsvd_sram_nat(struct tf_session *tfs)
{
	uint32_t index = TF_RESC_TYPE_SRAM_NAT_SPORT;
	uint16_t end = 0;

	/* nat source port rx direction */
	if (tfs->resc.rx.sram_entry[index].stride > 0)
		end = tfs->resc.rx.sram_entry[index].start +
			tfs->resc.rx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.sram_entry[index].stride,
			    TF_RSVD_SRAM_NAT_SPORT_BEGIN_IDX_RX,
			    end,
			    TF_RSVD_SRAM_NAT_SPORT_RX,
			    tfs->TF_SRAM_NAT_SPORT_POOL_NAME_RX);

	/* nat source port tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_NAT_SPORT_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_NAT_SPORT_TX,
			    tfs->TF_SRAM_NAT_SPORT_POOL_NAME_TX);

	index = TF_RESC_TYPE_SRAM_NAT_DPORT;

	/* nat destination port rx direction */
	if (tfs->resc.rx.sram_entry[index].stride > 0)
		end = tfs->resc.rx.sram_entry[index].start +
			tfs->resc.rx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.sram_entry[index].stride,
			    TF_RSVD_SRAM_NAT_DPORT_BEGIN_IDX_RX,
			    end,
			    TF_RSVD_SRAM_NAT_DPORT_RX,
			    tfs->TF_SRAM_NAT_DPORT_POOL_NAME_RX);

	/* nat destination port tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_NAT_DPORT_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_NAT_DPORT_TX,
			    tfs->TF_SRAM_NAT_DPORT_POOL_NAME_TX);

	index = TF_RESC_TYPE_SRAM_NAT_S_IPV4;

	/* nat source port ipv4 rx direction */
	if (tfs->resc.rx.sram_entry[index].stride > 0)
		end = tfs->resc.rx.sram_entry[index].start +
			tfs->resc.rx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.sram_entry[index].stride,
			    TF_RSVD_SRAM_NAT_S_IPV4_BEGIN_IDX_RX,
			    end,
			    TF_RSVD_SRAM_NAT_S_IPV4_RX,
			    tfs->TF_SRAM_NAT_S_IPV4_POOL_NAME_RX);

	/* nat source ipv4 port tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_NAT_S_IPV4_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_NAT_S_IPV4_TX,
			    tfs->TF_SRAM_NAT_S_IPV4_POOL_NAME_TX);

	index = TF_RESC_TYPE_SRAM_NAT_D_IPV4;

	/* nat destination port ipv4 rx direction */
	if (tfs->resc.rx.sram_entry[index].stride > 0)
		end = tfs->resc.rx.sram_entry[index].start +
			tfs->resc.rx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.rx.sram_entry[index].stride,
			    TF_RSVD_SRAM_NAT_D_IPV4_BEGIN_IDX_RX,
			    end,
			    TF_RSVD_SRAM_NAT_D_IPV4_RX,
			    tfs->TF_SRAM_NAT_D_IPV4_POOL_NAME_RX);

	/* nat destination ipv4 port tx direction */
	if (tfs->resc.tx.sram_entry[index].stride > 0)
		end = tfs->resc.tx.sram_entry[index].start +
			tfs->resc.tx.sram_entry[index].stride - 1;

	tf_rm_reserve_range(tfs->resc.tx.sram_entry[index].stride,
			    TF_RSVD_SRAM_NAT_D_IPV4_BEGIN_IDX_TX,
			    end,
			    TF_RSVD_SRAM_NAT_D_IPV4_TX,
			    tfs->TF_SRAM_NAT_D_IPV4_POOL_NAME_TX);
}

/**
 * Internal function used to validate the HW allocated resources
 * against the requested values.
 */
static int
tf_rm_hw_alloc_validate(enum tf_dir dir,
			struct tf_rm_hw_alloc *hw_alloc,
			struct tf_rm_entry *hw_entry)
{
	int error = 0;
	int i;

	for (i = 0; i < TF_RESC_TYPE_HW_MAX; i++) {
		if (hw_entry[i].stride != hw_alloc->hw_num[i]) {
			PMD_DRV_LOG(ERR,
				"%s, Alloc failed id:%d expect:%d got:%d\n",
				tf_dir_2_str(dir),
				i,
				hw_alloc->hw_num[i],
				hw_entry[i].stride);
			error = -1;
		}
	}

	return error;
}

/**
 * Internal function used to validate the SRAM allocated resources
 * against the requested values.
 */
static int
tf_rm_sram_alloc_validate(enum tf_dir dir __rte_unused,
			  struct tf_rm_sram_alloc *sram_alloc,
			  struct tf_rm_entry *sram_entry)
{
	int error = 0;
	int i;

	for (i = 0; i < TF_RESC_TYPE_SRAM_MAX; i++) {
		if (sram_entry[i].stride != sram_alloc->sram_num[i]) {
			PMD_DRV_LOG(ERR,
				"%s, Alloc failed idx:%d expect:%d got:%d\n",
				tf_dir_2_str(dir),
				i,
				sram_alloc->sram_num[i],
				sram_entry[i].stride);
			error = -1;
		}
	}

	return error;
}

/**
 * Internal function used to mark all the HW resources allocated that
 * Truflow does not own.
 */
static void
tf_rm_reserve_hw(struct tf *tfp)
{
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	/* TBD
	 * There is no direct AFM resource allocation as it is carved
	 * statically at AFM boot time. Thus the bit allocators work
	 * on the full HW resource amount and we just mark everything
	 * used except the resources that Truflow took ownership off.
	 */
	tf_rm_rsvd_l2_ctxt(tfs);
	tf_rm_rsvd_prof(tfs);
	tf_rm_rsvd_em_prof(tfs);
	tf_rm_rsvd_wc(tfs);
	tf_rm_rsvd_mirror(tfs);
	tf_rm_rsvd_meter(tfs);
	tf_rm_rsvd_upar(tfs);
	tf_rm_rsvd_sp_tcam(tfs);
	tf_rm_rsvd_l2_func(tfs);
	tf_rm_rsvd_fkb(tfs);
	tf_rm_rsvd_tbl_scope(tfs);
	tf_rm_rsvd_epoch(tfs);
	tf_rm_rsvd_metadata(tfs);
	tf_rm_rsvd_ct_state(tfs);
	tf_rm_rsvd_range(tfs);
	tf_rm_rsvd_lag_entry(tfs);
}

/**
 * Internal function used to mark all the SRAM resources allocated
 * that Truflow does not own.
 */
static void
tf_rm_reserve_sram(struct tf *tfp)
{
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	/* TBD
	 * There is no direct AFM resource allocation as it is carved
	 * statically at AFM boot time. Thus the bit allocators work
	 * on the full HW resource amount and we just mark everything
	 * used except the resources that Truflow took ownership off.
	 */
	tf_rm_rsvd_sram_full_action(tfs);
	tf_rm_rsvd_sram_mcg(tfs);
	tf_rm_rsvd_sram_encap(tfs);
	tf_rm_rsvd_sram_sp(tfs);
	tf_rm_rsvd_sram_stats(tfs);
	tf_rm_rsvd_sram_nat(tfs);
}

/**
 * Internal function used to allocate and validate all HW resources.
 */
static int
tf_rm_allocate_validate_hw(struct tf *tfp,
			   enum tf_dir dir)
{
	int rc;
	int i;
	struct tf_rm_hw_query hw_query;
	struct tf_rm_hw_alloc hw_alloc;
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);
	struct tf_rm_entry *hw_entries;
	uint32_t error_flag;

	if (dir == TF_DIR_RX)
		hw_entries = tfs->resc.rx.hw_entry;
	else
		hw_entries = tfs->resc.tx.hw_entry;

	/* Query for Session HW Resources */
	rc = tf_msg_session_hw_resc_qcaps(tfp, dir, &hw_query);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			    "%s, HW qcaps message send failed\n",
			    tf_dir_2_str(dir));
		goto cleanup;
	}

	rc = tf_rm_check_hw_qcaps_static(&hw_query, dir, &error_flag);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			"%s, HW QCAPS validation failed, error_flag:0x%x\n",
			tf_dir_2_str(dir),
			error_flag);
		tf_rm_print_hw_qcaps_error(dir, &hw_query, &error_flag);
		goto cleanup;
	}

	/* Post process HW capability */
	for (i = 0; i < TF_RESC_TYPE_HW_MAX; i++)
		hw_alloc.hw_num[i] = hw_query.hw_query[i].max;

	/* Allocate Session HW Resources */
	rc = tf_msg_session_hw_resc_alloc(tfp, dir, &hw_alloc, hw_entries);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			    "%s, HW alloc message send failed\n",
			    tf_dir_2_str(dir));
		goto cleanup;
	}

	/* Perform HW allocation validation as its possible the
	 * resource availability changed between qcaps and alloc
	 */
	rc = tf_rm_hw_alloc_validate(dir, &hw_alloc, hw_entries);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			    "%s, HW Resource validation failed\n",
			    tf_dir_2_str(dir));
		goto cleanup;
	}

	return 0;

 cleanup:
	return -1;
}

/**
 * Internal function used to allocate and validate all SRAM resources.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * Returns:
 *   0  - Success
 *   -1 - Internal error
 */
static int
tf_rm_allocate_validate_sram(struct tf *tfp,
			     enum tf_dir dir)
{
	int rc;
	int i;
	struct tf_rm_sram_query sram_query;
	struct tf_rm_sram_alloc sram_alloc;
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);
	struct tf_rm_entry *sram_entries;
	uint32_t error_flag;

	if (dir == TF_DIR_RX)
		sram_entries = tfs->resc.rx.sram_entry;
	else
		sram_entries = tfs->resc.tx.sram_entry;

	/* Query for Session SRAM Resources */
	rc = tf_msg_session_sram_resc_qcaps(tfp, dir, &sram_query);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			    "%s, SRAM qcaps message send failed\n",
			    tf_dir_2_str(dir));
		goto cleanup;
	}

	rc = tf_rm_check_sram_qcaps_static(&sram_query, dir, &error_flag);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			"%s, SRAM QCAPS validation failed, error_flag:%x\n",
			tf_dir_2_str(dir),
			error_flag);
		tf_rm_print_sram_qcaps_error(dir, &sram_query, &error_flag);
		goto cleanup;
	}

	/* Post process SRAM capability */
	for (i = 0; i < TF_RESC_TYPE_SRAM_MAX; i++)
		sram_alloc.sram_num[i] = sram_query.sram_query[i].max;

	/* Allocate Session SRAM Resources */
	rc = tf_msg_session_sram_resc_alloc(tfp,
					    dir,
					    &sram_alloc,
					    sram_entries);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			    "%s, SRAM alloc message send failed\n",
			    tf_dir_2_str(dir));
		goto cleanup;
	}

	/* Perform SRAM allocation validation as its possible the
	 * resource availability changed between qcaps and alloc
	 */
	rc = tf_rm_sram_alloc_validate(dir, &sram_alloc, sram_entries);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			    "%s, SRAM Resource allocation validation failed\n",
			    tf_dir_2_str(dir));
		goto cleanup;
	}

	return 0;

 cleanup:
	return -1;
}

/**
 * Helper function used to prune a HW resource array to only hold
 * elements that needs to be flushed.
 *
 * [in] tfs
 *   Session handle
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * [in] hw_entries
 *   Master HW Resource database
 *
 * [in/out] flush_entries
 *   Pruned HW Resource database of entries to be flushed. This
 *   array should be passed in as a complete copy of the master HW
 *   Resource database. The outgoing result will be a pruned version
 *   based on the result of the requested checking
 *
 * Returns:
 *    0 - Success, no flush required
 *    1 - Success, flush required
 *   -1 - Internal error
 */
static int
tf_rm_hw_to_flush(struct tf_session *tfs,
		  enum tf_dir dir,
		  struct tf_rm_entry *hw_entries,
		  struct tf_rm_entry *flush_entries)
{
	int rc;
	int flush_rc = 0;
	int free_cnt;
	struct bitalloc *pool;

	/* Check all the hw resource pools and check for left over
	 * elements. Any found will result in the complete pool of a
	 * type to get invalidated.
	 */

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_L2_CTXT_TCAM_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_L2_CTXT_TCAM].stride) {
		flush_entries[TF_RESC_TYPE_HW_L2_CTXT_TCAM].start = 0;
		flush_entries[TF_RESC_TYPE_HW_L2_CTXT_TCAM].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_PROF_FUNC_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_PROF_FUNC].stride) {
		flush_entries[TF_RESC_TYPE_HW_PROF_FUNC].start = 0;
		flush_entries[TF_RESC_TYPE_HW_PROF_FUNC].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_PROF_TCAM_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_PROF_TCAM].stride) {
		flush_entries[TF_RESC_TYPE_HW_PROF_TCAM].start = 0;
		flush_entries[TF_RESC_TYPE_HW_PROF_TCAM].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_EM_PROF_ID_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_EM_PROF_ID].stride) {
		flush_entries[TF_RESC_TYPE_HW_EM_PROF_ID].start = 0;
		flush_entries[TF_RESC_TYPE_HW_EM_PROF_ID].stride = 0;
	} else {
		flush_rc = 1;
	}

	flush_entries[TF_RESC_TYPE_HW_EM_REC].start = 0;
	flush_entries[TF_RESC_TYPE_HW_EM_REC].stride = 0;

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_WC_TCAM_PROF_ID_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_WC_TCAM_PROF_ID].stride) {
		flush_entries[TF_RESC_TYPE_HW_WC_TCAM_PROF_ID].start = 0;
		flush_entries[TF_RESC_TYPE_HW_WC_TCAM_PROF_ID].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_WC_TCAM_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_WC_TCAM].stride) {
		flush_entries[TF_RESC_TYPE_HW_WC_TCAM].start = 0;
		flush_entries[TF_RESC_TYPE_HW_WC_TCAM].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_METER_PROF_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_METER_PROF].stride) {
		flush_entries[TF_RESC_TYPE_HW_METER_PROF].start = 0;
		flush_entries[TF_RESC_TYPE_HW_METER_PROF].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_METER_INST_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_METER_INST].stride) {
		flush_entries[TF_RESC_TYPE_HW_METER_INST].start = 0;
		flush_entries[TF_RESC_TYPE_HW_METER_INST].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_MIRROR_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_MIRROR].stride) {
		flush_entries[TF_RESC_TYPE_HW_MIRROR].start = 0;
		flush_entries[TF_RESC_TYPE_HW_MIRROR].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_UPAR_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_UPAR].stride) {
		flush_entries[TF_RESC_TYPE_HW_UPAR].start = 0;
		flush_entries[TF_RESC_TYPE_HW_UPAR].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_SP_TCAM_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_SP_TCAM].stride) {
		flush_entries[TF_RESC_TYPE_HW_SP_TCAM].start = 0;
		flush_entries[TF_RESC_TYPE_HW_SP_TCAM].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_L2_FUNC_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_L2_FUNC].stride) {
		flush_entries[TF_RESC_TYPE_HW_L2_FUNC].start = 0;
		flush_entries[TF_RESC_TYPE_HW_L2_FUNC].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_FKB_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_FKB].stride) {
		flush_entries[TF_RESC_TYPE_HW_FKB].start = 0;
		flush_entries[TF_RESC_TYPE_HW_FKB].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_TBL_SCOPE_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_TBL_SCOPE].stride) {
		flush_entries[TF_RESC_TYPE_HW_TBL_SCOPE].start = 0;
		flush_entries[TF_RESC_TYPE_HW_TBL_SCOPE].stride = 0;
	} else {
		PMD_DRV_LOG(ERR, "%s: TBL_SCOPE free_cnt:%d, entries:%d\n",
			    tf_dir_2_str(dir),
			    free_cnt,
			    hw_entries[TF_RESC_TYPE_HW_TBL_SCOPE].stride);
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_EPOCH0_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_EPOCH0].stride) {
		flush_entries[TF_RESC_TYPE_HW_EPOCH0].start = 0;
		flush_entries[TF_RESC_TYPE_HW_EPOCH0].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_EPOCH1_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_EPOCH1].stride) {
		flush_entries[TF_RESC_TYPE_HW_EPOCH1].start = 0;
		flush_entries[TF_RESC_TYPE_HW_EPOCH1].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_METADATA_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_METADATA].stride) {
		flush_entries[TF_RESC_TYPE_HW_METADATA].start = 0;
		flush_entries[TF_RESC_TYPE_HW_METADATA].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_CT_STATE_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_CT_STATE].stride) {
		flush_entries[TF_RESC_TYPE_HW_CT_STATE].start = 0;
		flush_entries[TF_RESC_TYPE_HW_CT_STATE].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_RANGE_PROF_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_RANGE_PROF].stride) {
		flush_entries[TF_RESC_TYPE_HW_RANGE_PROF].start = 0;
		flush_entries[TF_RESC_TYPE_HW_RANGE_PROF].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_RANGE_ENTRY_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_RANGE_ENTRY].stride) {
		flush_entries[TF_RESC_TYPE_HW_RANGE_ENTRY].start = 0;
		flush_entries[TF_RESC_TYPE_HW_RANGE_ENTRY].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_LAG_ENTRY_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == hw_entries[TF_RESC_TYPE_HW_LAG_ENTRY].stride) {
		flush_entries[TF_RESC_TYPE_HW_LAG_ENTRY].start = 0;
		flush_entries[TF_RESC_TYPE_HW_LAG_ENTRY].stride = 0;
	} else {
		flush_rc = 1;
	}

	return flush_rc;
}

/**
 * Helper function used to prune a SRAM resource array to only hold
 * elements that needs to be flushed.
 *
 * [in] tfs
 *   Session handle
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * [in] hw_entries
 *   Master SRAM Resource data base
 *
 * [in/out] flush_entries
 *   Pruned SRAM Resource database of entries to be flushed. This
 *   array should be passed in as a complete copy of the master SRAM
 *   Resource database. The outgoing result will be a pruned version
 *   based on the result of the requested checking
 *
 * Returns:
 *    0 - Success, no flush required
 *    1 - Success, flush required
 *   -1 - Internal error
 */
static int
tf_rm_sram_to_flush(struct tf_session *tfs,
		    enum tf_dir dir,
		    struct tf_rm_entry *sram_entries,
		    struct tf_rm_entry *flush_entries)
{
	int rc;
	int flush_rc = 0;
	int free_cnt;
	struct bitalloc *pool;

	/* Check all the sram resource pools and check for left over
	 * elements. Any found will result in the complete pool of a
	 * type to get invalidated.
	 */

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_SRAM_FULL_ACTION_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == sram_entries[TF_RESC_TYPE_SRAM_FULL_ACTION].stride) {
		flush_entries[TF_RESC_TYPE_SRAM_FULL_ACTION].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_FULL_ACTION].stride = 0;
	} else {
		flush_rc = 1;
	}

	/* Only pools for RX direction */
	if (dir == TF_DIR_RX) {
		TF_RM_GET_POOLS_RX(tfs, &pool,
				   TF_SRAM_MCG_POOL_NAME);
		if (rc)
			return rc;
		free_cnt = ba_free_count(pool);
		if (free_cnt == sram_entries[TF_RESC_TYPE_SRAM_MCG].stride) {
			flush_entries[TF_RESC_TYPE_SRAM_MCG].start = 0;
			flush_entries[TF_RESC_TYPE_SRAM_MCG].stride = 0;
		} else {
			flush_rc = 1;
		}
	} else {
		/* Always prune TX direction */
		flush_entries[TF_RESC_TYPE_SRAM_MCG].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_MCG].stride = 0;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_SRAM_ENCAP_8B_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == sram_entries[TF_RESC_TYPE_SRAM_ENCAP_8B].stride) {
		flush_entries[TF_RESC_TYPE_SRAM_ENCAP_8B].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_ENCAP_8B].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_SRAM_ENCAP_16B_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == sram_entries[TF_RESC_TYPE_SRAM_ENCAP_16B].stride) {
		flush_entries[TF_RESC_TYPE_SRAM_ENCAP_16B].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_ENCAP_16B].stride = 0;
	} else {
		flush_rc = 1;
	}

	/* Only pools for TX direction */
	if (dir == TF_DIR_TX) {
		TF_RM_GET_POOLS_TX(tfs, &pool,
				   TF_SRAM_ENCAP_64B_POOL_NAME);
		if (rc)
			return rc;
		free_cnt = ba_free_count(pool);
		if (free_cnt ==
		    sram_entries[TF_RESC_TYPE_SRAM_ENCAP_64B].stride) {
			flush_entries[TF_RESC_TYPE_SRAM_ENCAP_64B].start = 0;
			flush_entries[TF_RESC_TYPE_SRAM_ENCAP_64B].stride = 0;
		} else {
			flush_rc = 1;
		}
	} else {
		/* Always prune RX direction */
		flush_entries[TF_RESC_TYPE_SRAM_ENCAP_64B].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_ENCAP_64B].stride = 0;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_SRAM_SP_SMAC_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == sram_entries[TF_RESC_TYPE_SRAM_SP_SMAC].stride) {
		flush_entries[TF_RESC_TYPE_SRAM_SP_SMAC].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_SP_SMAC].stride = 0;
	} else {
		flush_rc = 1;
	}

	/* Only pools for TX direction */
	if (dir == TF_DIR_TX) {
		TF_RM_GET_POOLS_TX(tfs, &pool,
				   TF_SRAM_SP_SMAC_IPV4_POOL_NAME);
		if (rc)
			return rc;
		free_cnt = ba_free_count(pool);
		if (free_cnt ==
		    sram_entries[TF_RESC_TYPE_SRAM_SP_SMAC_IPV4].stride) {
			flush_entries[TF_RESC_TYPE_SRAM_SP_SMAC_IPV4].start = 0;
			flush_entries[TF_RESC_TYPE_SRAM_SP_SMAC_IPV4].stride =
				0;
		} else {
			flush_rc = 1;
		}
	} else {
		/* Always prune RX direction */
		flush_entries[TF_RESC_TYPE_SRAM_SP_SMAC_IPV4].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_SP_SMAC_IPV4].stride = 0;
	}

	/* Only pools for TX direction */
	if (dir == TF_DIR_TX) {
		TF_RM_GET_POOLS_TX(tfs, &pool,
				   TF_SRAM_SP_SMAC_IPV6_POOL_NAME);
		if (rc)
			return rc;
		free_cnt = ba_free_count(pool);
		if (free_cnt ==
		    sram_entries[TF_RESC_TYPE_SRAM_SP_SMAC_IPV6].stride) {
			flush_entries[TF_RESC_TYPE_SRAM_SP_SMAC_IPV6].start = 0;
			flush_entries[TF_RESC_TYPE_SRAM_SP_SMAC_IPV6].stride =
				0;
		} else {
			flush_rc = 1;
		}
	} else {
		/* Always prune RX direction */
		flush_entries[TF_RESC_TYPE_SRAM_SP_SMAC_IPV6].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_SP_SMAC_IPV6].stride = 0;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_SRAM_STATS_64B_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == sram_entries[TF_RESC_TYPE_SRAM_COUNTER_64B].stride) {
		flush_entries[TF_RESC_TYPE_SRAM_COUNTER_64B].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_COUNTER_64B].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_SRAM_NAT_SPORT_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == sram_entries[TF_RESC_TYPE_SRAM_NAT_SPORT].stride) {
		flush_entries[TF_RESC_TYPE_SRAM_NAT_SPORT].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_NAT_SPORT].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_SRAM_NAT_DPORT_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == sram_entries[TF_RESC_TYPE_SRAM_NAT_DPORT].stride) {
		flush_entries[TF_RESC_TYPE_SRAM_NAT_DPORT].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_NAT_DPORT].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_SRAM_NAT_S_IPV4_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == sram_entries[TF_RESC_TYPE_SRAM_NAT_S_IPV4].stride) {
		flush_entries[TF_RESC_TYPE_SRAM_NAT_S_IPV4].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_NAT_S_IPV4].stride = 0;
	} else {
		flush_rc = 1;
	}

	TF_RM_GET_POOLS(tfs, dir, &pool,
			TF_SRAM_NAT_D_IPV4_POOL_NAME,
			rc);
	if (rc)
		return rc;
	free_cnt = ba_free_count(pool);
	if (free_cnt == sram_entries[TF_RESC_TYPE_SRAM_NAT_D_IPV4].stride) {
		flush_entries[TF_RESC_TYPE_SRAM_NAT_D_IPV4].start = 0;
		flush_entries[TF_RESC_TYPE_SRAM_NAT_D_IPV4].stride = 0;
	} else {
		flush_rc = 1;
	}

	return flush_rc;
}

/**
 * Helper function used to generate an error log for the HW types that
 * needs to be flushed. The types should have been cleaned up ahead of
 * invoking tf_close_session.
 *
 * [in] hw_entries
 *   HW Resource database holding elements to be flushed
 */
static void
tf_rm_log_hw_flush(enum tf_dir dir,
		   struct tf_rm_entry *hw_entries)
{
	int i;

	/* Walk the hw flush array and log the types that wasn't
	 * cleaned up.
	 */
	for (i = 0; i < TF_RESC_TYPE_HW_MAX; i++) {
		if (hw_entries[i].stride != 0)
			PMD_DRV_LOG(ERR,
				    "%s: %s was not cleaned up\n",
				    tf_dir_2_str(dir),
				    tf_hcapi_hw_2_str(i));
	}
}

/**
 * Helper function used to generate an error log for the SRAM types
 * that needs to be flushed. The types should have been cleaned up
 * ahead of invoking tf_close_session.
 *
 * [in] sram_entries
 *   SRAM Resource database holding elements to be flushed
 */
static void
tf_rm_log_sram_flush(enum tf_dir dir,
		     struct tf_rm_entry *sram_entries)
{
	int i;

	/* Walk the sram flush array and log the types that wasn't
	 * cleaned up.
	 */
	for (i = 0; i < TF_RESC_TYPE_SRAM_MAX; i++) {
		if (sram_entries[i].stride != 0)
			PMD_DRV_LOG(ERR,
				    "%s: %s was not cleaned up\n",
				    tf_dir_2_str(dir),
				    tf_hcapi_sram_2_str(i));
	}
}

void
tf_rm_init(struct tf *tfp __rte_unused)
{
	struct tf_session *tfs =
		(struct tf_session *)(tfp->session->core_data);

	/* This version is host specific and should be checked against
	 * when attaching as there is no guarantee that a secondary
	 * would run from same image version.
	 */
	tfs->ver.major = TF_SESSION_VER_MAJOR;
	tfs->ver.minor = TF_SESSION_VER_MINOR;
	tfs->ver.update = TF_SESSION_VER_UPDATE;

	tfs->session_id.id = 0;
	tfs->ref_count = 0;

	/* Initialization of Table Scopes */
	/* ll_init(&tfs->tbl_scope_ll); */

	/* Initialization of HW and SRAM resource DB */
	memset(&tfs->resc, 0, sizeof(struct tf_rm_db));

	/* Initialization of HW Resource Pools */
	ba_init(tfs->TF_L2_CTXT_TCAM_POOL_NAME_RX, TF_NUM_L2_CTXT_TCAM);
	ba_init(tfs->TF_L2_CTXT_TCAM_POOL_NAME_TX, TF_NUM_L2_CTXT_TCAM);
	ba_init(tfs->TF_PROF_FUNC_POOL_NAME_RX, TF_NUM_PROF_FUNC);
	ba_init(tfs->TF_PROF_FUNC_POOL_NAME_TX, TF_NUM_PROF_FUNC);
	ba_init(tfs->TF_PROF_TCAM_POOL_NAME_RX, TF_NUM_PROF_TCAM);
	ba_init(tfs->TF_PROF_TCAM_POOL_NAME_TX, TF_NUM_PROF_TCAM);
	ba_init(tfs->TF_EM_PROF_ID_POOL_NAME_RX, TF_NUM_EM_PROF_ID);
	ba_init(tfs->TF_EM_PROF_ID_POOL_NAME_TX, TF_NUM_EM_PROF_ID);

	/* TBD, how do we want to handle EM records ?*/
	/* EM Records should not be controlled by way of a pool */

	ba_init(tfs->TF_WC_TCAM_PROF_ID_POOL_NAME_RX, TF_NUM_WC_PROF_ID);
	ba_init(tfs->TF_WC_TCAM_PROF_ID_POOL_NAME_TX, TF_NUM_WC_PROF_ID);
	ba_init(tfs->TF_WC_TCAM_POOL_NAME_RX, TF_NUM_WC_TCAM_ROW);
	ba_init(tfs->TF_WC_TCAM_POOL_NAME_TX, TF_NUM_WC_TCAM_ROW);
	ba_init(tfs->TF_METER_PROF_POOL_NAME_RX, TF_NUM_METER_PROF);
	ba_init(tfs->TF_METER_PROF_POOL_NAME_TX, TF_NUM_METER_PROF);
	ba_init(tfs->TF_METER_INST_POOL_NAME_RX, TF_NUM_METER);
	ba_init(tfs->TF_METER_INST_POOL_NAME_TX, TF_NUM_METER);
	ba_init(tfs->TF_MIRROR_POOL_NAME_RX, TF_NUM_MIRROR);
	ba_init(tfs->TF_MIRROR_POOL_NAME_TX, TF_NUM_MIRROR);
	ba_init(tfs->TF_UPAR_POOL_NAME_RX, TF_NUM_UPAR);
	ba_init(tfs->TF_UPAR_POOL_NAME_TX, TF_NUM_UPAR);

	ba_init(tfs->TF_SP_TCAM_POOL_NAME_RX, TF_NUM_SP_TCAM);
	ba_init(tfs->TF_SP_TCAM_POOL_NAME_TX, TF_NUM_SP_TCAM);

	ba_init(tfs->TF_FKB_POOL_NAME_RX, TF_NUM_FKB);
	ba_init(tfs->TF_FKB_POOL_NAME_TX, TF_NUM_FKB);

	ba_init(tfs->TF_TBL_SCOPE_POOL_NAME_RX, TF_NUM_TBL_SCOPE);
	ba_init(tfs->TF_TBL_SCOPE_POOL_NAME_TX, TF_NUM_TBL_SCOPE);
	ba_init(tfs->TF_L2_FUNC_POOL_NAME_RX, TF_NUM_L2_FUNC);
	ba_init(tfs->TF_L2_FUNC_POOL_NAME_TX, TF_NUM_L2_FUNC);
	ba_init(tfs->TF_EPOCH0_POOL_NAME_RX, TF_NUM_EPOCH0);
	ba_init(tfs->TF_EPOCH0_POOL_NAME_TX, TF_NUM_EPOCH0);
	ba_init(tfs->TF_EPOCH1_POOL_NAME_RX, TF_NUM_EPOCH1);
	ba_init(tfs->TF_EPOCH1_POOL_NAME_TX, TF_NUM_EPOCH1);
	ba_init(tfs->TF_METADATA_POOL_NAME_RX, TF_NUM_METADATA);
	ba_init(tfs->TF_METADATA_POOL_NAME_TX, TF_NUM_METADATA);
	ba_init(tfs->TF_CT_STATE_POOL_NAME_RX, TF_NUM_CT_STATE);
	ba_init(tfs->TF_CT_STATE_POOL_NAME_TX, TF_NUM_CT_STATE);
	ba_init(tfs->TF_RANGE_PROF_POOL_NAME_RX, TF_NUM_RANGE_PROF);
	ba_init(tfs->TF_RANGE_PROF_POOL_NAME_TX, TF_NUM_RANGE_PROF);
	ba_init(tfs->TF_RANGE_ENTRY_POOL_NAME_RX, TF_NUM_RANGE_ENTRY);
	ba_init(tfs->TF_RANGE_ENTRY_POOL_NAME_TX, TF_NUM_RANGE_ENTRY);
	ba_init(tfs->TF_LAG_ENTRY_POOL_NAME_RX, TF_NUM_LAG_ENTRY);
	ba_init(tfs->TF_LAG_ENTRY_POOL_NAME_TX, TF_NUM_LAG_ENTRY);

	/* Initialization of SRAM Resource Pools
	 * These pools are set to the TFLIB defined MAX sizes not
	 * AFM's HW max as to limit the memory consumption
	 */
	ba_init(tfs->TF_SRAM_FULL_ACTION_POOL_NAME_RX,
		TF_RSVD_SRAM_FULL_ACTION_RX);
	ba_init(tfs->TF_SRAM_FULL_ACTION_POOL_NAME_TX,
		TF_RSVD_SRAM_FULL_ACTION_TX);
	/* Only Multicast Group on RX is supported */
	ba_init(tfs->TF_SRAM_MCG_POOL_NAME_RX,
		TF_RSVD_SRAM_MCG_RX);
	ba_init(tfs->TF_SRAM_ENCAP_8B_POOL_NAME_RX,
		TF_RSVD_SRAM_ENCAP_8B_RX);
	ba_init(tfs->TF_SRAM_ENCAP_8B_POOL_NAME_TX,
		TF_RSVD_SRAM_ENCAP_8B_TX);
	ba_init(tfs->TF_SRAM_ENCAP_16B_POOL_NAME_RX,
		TF_RSVD_SRAM_ENCAP_16B_RX);
	ba_init(tfs->TF_SRAM_ENCAP_16B_POOL_NAME_TX,
		TF_RSVD_SRAM_ENCAP_16B_TX);
	/* Only Encap 64B on TX is supported */
	ba_init(tfs->TF_SRAM_ENCAP_64B_POOL_NAME_TX,
		TF_RSVD_SRAM_ENCAP_64B_TX);
	ba_init(tfs->TF_SRAM_SP_SMAC_POOL_NAME_RX,
		TF_RSVD_SRAM_SP_SMAC_RX);
	ba_init(tfs->TF_SRAM_SP_SMAC_POOL_NAME_TX,
		TF_RSVD_SRAM_SP_SMAC_TX);
	/* Only SP SMAC IPv4 on TX is supported */
	ba_init(tfs->TF_SRAM_SP_SMAC_IPV4_POOL_NAME_TX,
		TF_RSVD_SRAM_SP_SMAC_IPV4_TX);
	/* Only SP SMAC IPv6 on TX is supported */
	ba_init(tfs->TF_SRAM_SP_SMAC_IPV6_POOL_NAME_TX,
		TF_RSVD_SRAM_SP_SMAC_IPV6_TX);
	ba_init(tfs->TF_SRAM_STATS_64B_POOL_NAME_RX,
		TF_RSVD_SRAM_COUNTER_64B_RX);
	ba_init(tfs->TF_SRAM_STATS_64B_POOL_NAME_TX,
		TF_RSVD_SRAM_COUNTER_64B_TX);
	ba_init(tfs->TF_SRAM_NAT_SPORT_POOL_NAME_RX,
		TF_RSVD_SRAM_NAT_SPORT_RX);
	ba_init(tfs->TF_SRAM_NAT_SPORT_POOL_NAME_TX,
		TF_RSVD_SRAM_NAT_SPORT_TX);
	ba_init(tfs->TF_SRAM_NAT_DPORT_POOL_NAME_RX,
		TF_RSVD_SRAM_NAT_DPORT_RX);
	ba_init(tfs->TF_SRAM_NAT_DPORT_POOL_NAME_TX,
		TF_RSVD_SRAM_NAT_DPORT_TX);
	ba_init(tfs->TF_SRAM_NAT_S_IPV4_POOL_NAME_RX,
		TF_RSVD_SRAM_NAT_S_IPV4_RX);
	ba_init(tfs->TF_SRAM_NAT_S_IPV4_POOL_NAME_TX,
		TF_RSVD_SRAM_NAT_S_IPV4_TX);
	ba_init(tfs->TF_SRAM_NAT_D_IPV4_POOL_NAME_RX,
		TF_RSVD_SRAM_NAT_D_IPV4_RX);
	ba_init(tfs->TF_SRAM_NAT_D_IPV4_POOL_NAME_TX,
		TF_RSVD_SRAM_NAT_D_IPV4_TX);

	/* Initialization of pools local to TF Core */
	ba_init(tfs->TF_L2_CTXT_REMAP_POOL_NAME_RX, TF_NUM_L2_CTXT_TCAM);
	ba_init(tfs->TF_L2_CTXT_REMAP_POOL_NAME_TX, TF_NUM_L2_CTXT_TCAM);
}

int
tf_rm_allocate_validate(struct tf *tfp)
{
	int rc;
	int i;

	for (i = 0; i < TF_DIR_MAX; i++) {
		rc = tf_rm_allocate_validate_hw(tfp, i);
		if (rc)
			return rc;
		rc = tf_rm_allocate_validate_sram(tfp, i);
		if (rc)
			return rc;
	}

	/* With both HW and SRAM allocated and validated we can
	 * 'scrub' the reservation on the pools.
	 */
	tf_rm_reserve_hw(tfp);
	tf_rm_reserve_sram(tfp);

	return rc;
}

int
tf_rm_close(struct tf *tfp)
{
	int rc;
	int rc_close = 0;
	int i;
	struct tf_rm_entry *hw_entries;
	struct tf_rm_entry *hw_flush_entries;
	struct tf_rm_entry *sram_entries;
	struct tf_rm_entry *sram_flush_entries;
	struct tf_session *tfs __rte_unused =
		(struct tf_session *)(tfp->session->core_data);

	struct tf_rm_db flush_resc = tfs->resc;

	/* On close it is assumed that the session has already cleaned
	 * up all its resources, individually, while destroying its
	 * flows. No checking is performed thus the behavior is as
	 * follows.
	 *
	 * Session RM will signal FW to release session resources. FW
	 * will perform invalidation of all the allocated entries
	 * (assures any outstanding resources has been cleared, then
	 * free the FW RM instance.
	 *
	 * Session will then be freed by tf_close_session() thus there
	 * is no need to clean each resource pool as the whole session
	 * is going away.
	 */

	for (i = 0; i < TF_DIR_MAX; i++) {
		if (i == TF_DIR_RX) {
			hw_entries = tfs->resc.rx.hw_entry;
			hw_flush_entries = flush_resc.rx.hw_entry;
			sram_entries = tfs->resc.rx.sram_entry;
			sram_flush_entries = flush_resc.rx.sram_entry;
		} else {
			hw_entries = tfs->resc.tx.hw_entry;
			hw_flush_entries = flush_resc.tx.hw_entry;
			sram_entries = tfs->resc.tx.sram_entry;
			sram_flush_entries = flush_resc.tx.sram_entry;
		}

		/* Check for any not previously freed HW resources and
		 * flush if required.
		 */
		rc = tf_rm_hw_to_flush(tfs, i, hw_entries, hw_flush_entries);
		if (rc) {
			rc_close = -ENOTEMPTY;
			/* Log error */
			PMD_DRV_LOG(ERR,
				    "%s, lingering HW resources\n",
				    tf_dir_2_str(i));

			/* Log the entries to be flushed */
			tf_rm_log_hw_flush(i, hw_flush_entries);
			rc = tf_msg_session_hw_resc_flush(tfp,
							  i,
							  hw_flush_entries);
			if (rc) {
				rc_close = rc;
				/* Log error */
				PMD_DRV_LOG(ERR,
					    "%s, HW flush failed\n",
					    tf_dir_2_str(i));
			}
		}

		/* Check for any not previously freed SRAM resources
		 * and flush if required.
		 */
		rc = tf_rm_sram_to_flush(tfs,
					 i,
					 sram_entries,
					 sram_flush_entries);
		if (rc) {
			rc_close = -ENOTEMPTY;
			/* Log error */
			PMD_DRV_LOG(ERR,
				    "%s, lingering SRAM resources\n",
				    tf_dir_2_str(i));

			/* Log the entries to be flushed */
			tf_rm_log_sram_flush(i, sram_flush_entries);

			rc = tf_msg_session_sram_resc_flush(tfp,
							    i,
							    sram_flush_entries);
			if (rc) {
				rc_close = rc;
				/* Log error */
				PMD_DRV_LOG(ERR,
					    "%s, HW flush failed\n",
					    tf_dir_2_str(i));
			}
		}

		rc = tf_msg_session_hw_resc_free(tfp, i, hw_entries);
		if (rc) {
			rc_close = rc;
			/* Log error */
			PMD_DRV_LOG(ERR,
				    "%s, HW free failed\n",
				    tf_dir_2_str(i));
		}

		rc = tf_msg_session_sram_resc_free(tfp, i, sram_entries);
		if (rc) {
			rc_close = rc;
			/* Log error */
			PMD_DRV_LOG(ERR,
				    "%s, SRAM free failed\n",
				    tf_dir_2_str(i));
		}
	}

	return rc_close;
}

#if (TF_SHADOW == 1)
int
tf_rm_shadow_db_init(struct tf_session *tfs)
{
	rc = 1;

	return rc;
}
#endif /* TF_SHADOW */

int
tf_rm_lookup_tcam_type_pool(struct tf_session *tfs,
			    enum tf_dir dir,
			    enum tf_tcam_tbl_type type,
			    struct bitalloc **pool)
{
	int rc = -EOPNOTSUPP;

	*pool = NULL;

	switch (type) {
	case TF_TCAM_TBL_TYPE_L2_CTXT_TCAM:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_L2_CTXT_TCAM_POOL_NAME,
				rc);
		break;
	case TF_TCAM_TBL_TYPE_PROF_TCAM:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_PROF_TCAM_POOL_NAME,
				rc);
		break;
	case TF_TCAM_TBL_TYPE_WC_TCAM:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_WC_TCAM_POOL_NAME,
				rc);
		break;
	case TF_TCAM_TBL_TYPE_VEB_TCAM:
	case TF_TCAM_TBL_TYPE_SP_TCAM:
	case TF_TCAM_TBL_TYPE_CT_RULE_TCAM:
	default:
		break;
	}

	if (rc == -EOPNOTSUPP) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Tcam type not supported, type:%d\n",
			    dir,
			    type);
		return rc;
	} else if (rc == -1) {
		PMD_DRV_LOG(ERR,
			    "%s:, Tcam type lookup failed, type:%d\n",
			    tf_dir_2_str(dir),
			    type);
		return rc;
	}

	return 0;
}

int
tf_rm_lookup_tbl_type_pool(struct tf_session *tfs,
			   enum tf_dir dir,
			   enum tf_tbl_type type,
			   struct bitalloc **pool)
{
	int rc = -EOPNOTSUPP;

	*pool = NULL;

	switch (type) {
	case TF_TBL_TYPE_FULL_ACT_RECORD:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_SRAM_FULL_ACTION_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_MCAST_GROUPS:
		/* No pools for TX direction, so bail out */
		if (dir == TF_DIR_TX)
			break;
		TF_RM_GET_POOLS_RX(tfs, pool,
				   TF_SRAM_MCG_POOL_NAME);
		rc = 0;
		break;
	case TF_TBL_TYPE_ACT_ENCAP_8B:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_SRAM_ENCAP_8B_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_ACT_ENCAP_16B:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_SRAM_ENCAP_16B_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_ACT_ENCAP_64B:
		/* No pools for RX direction, so bail out */
		if (dir == TF_DIR_RX)
			break;
		TF_RM_GET_POOLS_TX(tfs, pool,
				   TF_SRAM_ENCAP_64B_POOL_NAME);
		rc = 0;
		break;
	case TF_TBL_TYPE_ACT_SP_SMAC:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_SRAM_SP_SMAC_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV4:
		/* No pools for TX direction, so bail out */
		if (dir == TF_DIR_RX)
			break;
		TF_RM_GET_POOLS_TX(tfs, pool,
				   TF_SRAM_SP_SMAC_IPV4_POOL_NAME);
		rc = 0;
		break;
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV6:
		/* No pools for TX direction, so bail out */
		if (dir == TF_DIR_RX)
			break;
		TF_RM_GET_POOLS_TX(tfs, pool,
				   TF_SRAM_SP_SMAC_IPV6_POOL_NAME);
		rc = 0;
		break;
	case TF_TBL_TYPE_ACT_STATS_64:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_SRAM_STATS_64B_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_ACT_MODIFY_SPORT:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_SRAM_NAT_SPORT_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_ACT_MODIFY_IPV4_SRC:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_SRAM_NAT_S_IPV4_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_ACT_MODIFY_IPV4_DEST:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_SRAM_NAT_D_IPV4_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_METER_PROF:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_METER_PROF_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_METER_INST:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_METER_INST_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_MIRROR_CONFIG:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_MIRROR_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_UPAR:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_UPAR_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_EPOCH0:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_EPOCH0_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_EPOCH1:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_EPOCH1_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_METADATA:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_METADATA_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_CT_STATE:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_CT_STATE_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_RANGE_PROF:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_RANGE_PROF_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_RANGE_ENTRY:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_RANGE_ENTRY_POOL_NAME,
				rc);
		break;
	case TF_TBL_TYPE_LAG:
		TF_RM_GET_POOLS(tfs, dir, pool,
				TF_LAG_ENTRY_POOL_NAME,
				rc);
		break;
	/* Not yet supported */
	case TF_TBL_TYPE_ACT_ENCAP_32B:
	case TF_TBL_TYPE_ACT_MODIFY_IPV6_DEST:
	case TF_TBL_TYPE_ACT_MODIFY_IPV6_SRC:
	case TF_TBL_TYPE_VNIC_SVIF:
		break;
	/* No bitalloc pools for these types */
	case TF_TBL_TYPE_EXT:
	case TF_TBL_TYPE_EXT_0:
	default:
		break;
	}

	if (rc == -EOPNOTSUPP) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Table type not supported, type:%d\n",
			    dir,
			    type);
		return rc;
	} else if (rc == -1) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Table type lookup failed, type:%d\n",
			    dir,
			    type);
		return rc;
	}

	return 0;
}

int
tf_rm_convert_tbl_type(enum tf_tbl_type type,
		       uint32_t *hcapi_type)
{
	int rc = 0;

	switch (type) {
	case TF_TBL_TYPE_FULL_ACT_RECORD:
		*hcapi_type = TF_RESC_TYPE_SRAM_FULL_ACTION;
		break;
	case TF_TBL_TYPE_MCAST_GROUPS:
		*hcapi_type = TF_RESC_TYPE_SRAM_MCG;
		break;
	case TF_TBL_TYPE_ACT_ENCAP_8B:
		*hcapi_type = TF_RESC_TYPE_SRAM_ENCAP_8B;
		break;
	case TF_TBL_TYPE_ACT_ENCAP_16B:
		*hcapi_type = TF_RESC_TYPE_SRAM_ENCAP_16B;
		break;
	case TF_TBL_TYPE_ACT_ENCAP_64B:
		*hcapi_type = TF_RESC_TYPE_SRAM_ENCAP_64B;
		break;
	case TF_TBL_TYPE_ACT_SP_SMAC:
		*hcapi_type = TF_RESC_TYPE_SRAM_SP_SMAC;
		break;
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV4:
		*hcapi_type = TF_RESC_TYPE_SRAM_SP_SMAC_IPV4;
		break;
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV6:
		*hcapi_type = TF_RESC_TYPE_SRAM_SP_SMAC_IPV6;
		break;
	case TF_TBL_TYPE_ACT_STATS_64:
		*hcapi_type = TF_RESC_TYPE_SRAM_COUNTER_64B;
		break;
	case TF_TBL_TYPE_ACT_MODIFY_SPORT:
		*hcapi_type = TF_RESC_TYPE_SRAM_NAT_SPORT;
		break;
	case TF_TBL_TYPE_ACT_MODIFY_DPORT:
		*hcapi_type = TF_RESC_TYPE_SRAM_NAT_DPORT;
		break;
	case TF_TBL_TYPE_ACT_MODIFY_IPV4_SRC:
		*hcapi_type = TF_RESC_TYPE_SRAM_NAT_S_IPV4;
		break;
	case TF_TBL_TYPE_ACT_MODIFY_IPV4_DEST:
		*hcapi_type = TF_RESC_TYPE_SRAM_NAT_D_IPV4;
		break;
	case TF_TBL_TYPE_METER_PROF:
		*hcapi_type = TF_RESC_TYPE_HW_METER_PROF;
		break;
	case TF_TBL_TYPE_METER_INST:
		*hcapi_type = TF_RESC_TYPE_HW_METER_INST;
		break;
	case TF_TBL_TYPE_MIRROR_CONFIG:
		*hcapi_type = TF_RESC_TYPE_HW_MIRROR;
		break;
	case TF_TBL_TYPE_UPAR:
		*hcapi_type = TF_RESC_TYPE_HW_UPAR;
		break;
	case TF_TBL_TYPE_EPOCH0:
		*hcapi_type = TF_RESC_TYPE_HW_EPOCH0;
		break;
	case TF_TBL_TYPE_EPOCH1:
		*hcapi_type = TF_RESC_TYPE_HW_EPOCH1;
		break;
	case TF_TBL_TYPE_METADATA:
		*hcapi_type = TF_RESC_TYPE_HW_METADATA;
		break;
	case TF_TBL_TYPE_CT_STATE:
		*hcapi_type = TF_RESC_TYPE_HW_CT_STATE;
		break;
	case TF_TBL_TYPE_RANGE_PROF:
		*hcapi_type = TF_RESC_TYPE_HW_RANGE_PROF;
		break;
	case TF_TBL_TYPE_RANGE_ENTRY:
		*hcapi_type = TF_RESC_TYPE_HW_RANGE_ENTRY;
		break;
	case TF_TBL_TYPE_LAG:
		*hcapi_type = TF_RESC_TYPE_HW_LAG_ENTRY;
		break;
	/* Not yet supported */
	case TF_TBL_TYPE_ACT_ENCAP_32B:
	case TF_TBL_TYPE_ACT_MODIFY_IPV6_DEST:
	case TF_TBL_TYPE_ACT_MODIFY_IPV6_SRC:
	case TF_TBL_TYPE_VNIC_SVIF:
	case TF_TBL_TYPE_EXT:   /* No pools for this type */
	case TF_TBL_TYPE_EXT_0: /* No pools for this type */
	default:
		*hcapi_type = -1;
		rc = -EOPNOTSUPP;
	}

	return rc;
}

int
tf_rm_convert_index(struct tf_session *tfs,
		    enum tf_dir dir,
		    enum tf_tbl_type type,
		    enum tf_rm_convert_type c_type,
		    uint32_t index,
		    uint32_t *convert_index)
{
	int rc;
	struct tf_rm_resc *resc;
	uint32_t hcapi_type;
	uint32_t base_index;

	if (dir == TF_DIR_RX)
		resc = &tfs->resc.rx;
	else if (dir == TF_DIR_TX)
		resc = &tfs->resc.tx;
	else
		return -EOPNOTSUPP;

	rc = tf_rm_convert_tbl_type(type, &hcapi_type);
	if (rc)
		return -1;

	switch (type) {
	case TF_TBL_TYPE_FULL_ACT_RECORD:
	case TF_TBL_TYPE_MCAST_GROUPS:
	case TF_TBL_TYPE_ACT_ENCAP_8B:
	case TF_TBL_TYPE_ACT_ENCAP_16B:
	case TF_TBL_TYPE_ACT_ENCAP_32B:
	case TF_TBL_TYPE_ACT_ENCAP_64B:
	case TF_TBL_TYPE_ACT_SP_SMAC:
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV4:
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV6:
	case TF_TBL_TYPE_ACT_STATS_64:
	case TF_TBL_TYPE_ACT_MODIFY_SPORT:
	case TF_TBL_TYPE_ACT_MODIFY_DPORT:
	case TF_TBL_TYPE_ACT_MODIFY_IPV4_SRC:
	case TF_TBL_TYPE_ACT_MODIFY_IPV4_DEST:
		base_index = resc->sram_entry[hcapi_type].start;
		break;
	case TF_TBL_TYPE_MIRROR_CONFIG:
	case TF_TBL_TYPE_METER_PROF:
	case TF_TBL_TYPE_METER_INST:
	case TF_TBL_TYPE_UPAR:
	case TF_TBL_TYPE_EPOCH0:
	case TF_TBL_TYPE_EPOCH1:
	case TF_TBL_TYPE_METADATA:
	case TF_TBL_TYPE_CT_STATE:
	case TF_TBL_TYPE_RANGE_PROF:
	case TF_TBL_TYPE_RANGE_ENTRY:
	case TF_TBL_TYPE_LAG:
		base_index = resc->hw_entry[hcapi_type].start;
		break;
	/* Not yet supported */
	case TF_TBL_TYPE_VNIC_SVIF:
	case TF_TBL_TYPE_EXT:   /* No pools for this type */
	case TF_TBL_TYPE_EXT_0: /* No pools for this type */
	default:
		return -EOPNOTSUPP;
	}

	switch (c_type) {
	case TF_RM_CONVERT_RM_BASE:
		*convert_index = index - base_index;
		break;
	case TF_RM_CONVERT_ADD_BASE:
		*convert_index = index + base_index;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}
