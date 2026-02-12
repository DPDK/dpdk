/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_BLD_DEVOPS_H_
#define _CFA_BLD_DEVOPS_H_

#include <stdio.h>

#include "cfa_bld.h"
#include "cfa_bld_defs.h"

struct cfa_bld_devops;

/**
 *  @addtogroup CFA_BLD CFA Builder Library
 *  \ingroup CFA_V3
 *  @{
 */

/**
 * CFA device information
 */
struct cfa_bld_devinfo {
	/** [out] CFA Builder operations function pointer table */
	const struct cfa_bld_devops *devops;
};

/**
 *  @name CFA_BLD CFA Builder Host Device OPS API
 *  CFA builder host specific API used by host CFA application to bind
 *  to different CFA devices and access device by using device OPS.
 */

/**@{*/
/** CFA bind builder API
 *
 * This API retrieves the CFA global device configuration. This API should be
 * called first before doing any operations to CFA through API. The returned
 * global device information should be referenced throughout the lifetime of
 * the CFA application.
 *
 * @param[in] hw_ver
 *   hardware version of the CFA
 *
 * @param[out] dev_info
 *   CFA global device information
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_bld_bind(enum cfa_ver hw_ver, struct cfa_bld_devinfo *dev_info);

/** CFA device specific function hooks structure
 *
 * The following device hooks can be defined; unless noted otherwise, they are
 * optional and can be filled with a null pointer. The purpose of these hooks
 * to support CFA device operations for different device variants.
 */
struct cfa_bld_devops {
	/** Get CFA layout for hw fix format tables
	 *
	 * This API takes returns the CFA layout for a given resource type
	 * resource subtype and CFA direction.
	 *
	 * @param[in] rtype
	 *   CFA HW resource type. Valid values are CFA_RTYPE_XXX
	 *
	 * @param[in] rsubtype
	 *   CFA HW resource sub type for the given resource type 'rtype'
	 *   Valid values are CFA_RSUBTYPE_XXX_YYY, where XXX is the resource
	 *   type
	 *
	 * @param[in] dir
	 *   CFA direction. RX/TX. Note that the returned layout is different
	 *   for RX and TX, only for VEB and VSPT tables. For all tables, the
	 *   layout is the same for both directions.
	 *
	 * @param[out] layout
	 *   Pointer to the table layout to be returned
	 *
	 * @return
	 *   0 for SUCCESS, negative errno for FAILURE
	 *
	 * @note example usage: To get L2 context TCAM table, use
	 *  struct cfa_layout *l2ctxt_tcam_layout;
	 *  devops->cfa_bld_get_table_layout(CFA_RTYPE_TCAM,
	 *                                   CFA_RSUBTYPE_TCAM_L2CTX,
	 *                                   CFA_TX,
	 *                                   &l2ctxt_tcam_layout);
	 */
	int (*cfa_bld_get_table_layout)(enum cfa_resource_type rtype,
					uint8_t rsubtype, enum cfa_dir dir,
					struct cfa_layout **layout);

	/** Get CFA layout for HW remap tables
	 *
	 * This API takes returns the CFA remap layout for a given tcam
	 * resource sub type, remap type and CFA direction.
	 *
	 * @param[in] st
	 *   CFA TCAM table sub types. Valid values are CFA_RSUBTYPE_TCAM_XXX
	 *
	 * @param[in] rmp_tt
	 *   CFA Remap table type. See enum cfa_remap_tbl_type
	 *
	 * @param[in] dir
	 *   CFA direction. RX/TX.
	 *
	 * @param[out] layout
	 *   Pointer to the remap table layout to be returned
	 *
	 * @return
	 *   0 for SUCCESS, negative errno for FAILURE
	 *
	 * @note example usage: To get Profiler TCAM Remap bypass table, use
	 *  struct cfa_layout *prof_tcam_rmp_byp_layout;
	 *  devops->cfa_bld_get_remap_table_layout(CFA_RSUBTYPE_TCAM_PROF_TCAM,
	 *                                         CFA_REMAP_TBL_TYPE_BYPASS,
	 *                                         CFA_TX,
	 *                                         &prof_tcam_rmp_byp_layout);
	 */
	int (*cfa_bld_get_remap_table_layout)(enum cfa_resource_subtype_tcam st,
					      enum cfa_remap_tbl_type rmp_tt,
					      enum cfa_dir dir,
					      struct cfa_layout **layout);

	/** build key layout
	 *
	 * This API takes the user provided key template as input and
	 * compiles it into a key layout supported by the hardware.
	 * It is intended that an application will only compile a
	 * key layout once for the provided key template and then
	 * reference the key layout throughout the lifetime of that
	 * key template.
	 *
	 * @param[in] key_template
	 *   A pointer to the key template
	 *
	 * @param[in,out] layout
	 *   A pointer of the key layout
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*cfa_bld_key_compile_layout)(struct cfa_key_template *t,
					  struct cfa_key_layout *l);

	/** Print formatted key object
	 *
	 * This API prints in human readable form the data in a key
	 * object based upon the key layout provided.  It also provides the
	 * option to provide a raw byte output.
	 *
	 * @param[in] stream
	 *   Generally set to stdout (stderr possible)
	 *
	 * @param[in] key_obj
	 *   A pointer to the key_obj to be displayed
	 *
	 * @param[in] key_layout
	 *   A pointer to the key_layout indicating the key format
	 *
	 * @param[in] decode
	 *   If set, decode the fields, if clear provide raw byte output.
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*cfa_bld_key_print_obj)(FILE *stream, struct cfa_key_obj *key_obj,
				     struct cfa_key_layout *key_layout,
				     bool decode);

	/** Transform key data with device specific control information
	 *
	 *  This API inserts or strips device specific control information
	 *  to/from a key object.
	 *
	 *  @param[in] op
	 *    specify key transform operations.
	 *
	 *  @param[in] key_obj
	 *    A pointer of the key object to be transformed
	 *
	 *  @param[out] key_obj_out
	 *    A pointer of the transformed key data object
	 *    The updated bitlen for the transformed key is returned
	 *    in the data_len_bits field of this object.
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*cfa_bld_key_transform)(enum cfa_key_ctrlops op,
				     struct cfa_key_obj *key_obj,
				     struct cfa_key_obj *key_obj_out);

	/** build action layout
	 *
	 * This API takes the user provided action template as input and
	 * compiles it into an action layout supported by the hardware.
	 * It is intended that an application will only compile an
	 * action layout once for the provided action template and then
	 * reference the action layout throughout the lifetime of that
	 * action template.
	 *
	 * @param[in] act_template
	 *   A pointer to the action template
	 *
	 * @param[in,out] layout
	 *   A pointer of the action layout
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*cfa_bld_act_compile_layout)(struct cfa_action_template *t,
					  struct cfa_action_layout *l);

	/** initialize action private fields
	 *
	 * This API provides the functionality to zero out the action
	 * object data fields and set pre-initialized private fields
	 * based on the layout. Any action object must be initialized
	 * using this API before any put and get APIs can be executed
	 * for an action object.
	 *
	 * @param[in,out] act_obj
	 *   A pointer of the action object to be initialized
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*cfa_bld_action_init_obj)(struct cfa_action_obj *act_obj);

	/** compute inline action object pointers/offsets
	 *
	 * This API provides the functionality to compute and set
	 * pointers/offset to the inlined actions in an action record.
	 * This API is applicable only to the action object type that
	 * support inline actions.
	 *
	 * @param[in,out] act_obj
	 *   A pointer of the action object to be initialized
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*cfa_bld_action_compute_ptr)(struct cfa_action_obj *obj);

	/** Print action object
	 *
	 * This API presents the action object in human readable
	 * format.
	 *
	 *
	 * @param[in] stream
	 *   Generally set to stdout (stderr possible)
	 *
	 * @param[in,out] act_obj
	 *   A pointer of the action object to be displayed
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*cfa_bld_action_print_obj)(FILE *stream,
					struct cfa_action_obj *obj,
					bool decode);

	/** Print field object
	 *
	 * This API prints out the raw field output
	 *
	 * @param[in] fld_obj
	 *   A pointer fld_obj to be displayed
	 *
	 * @param[in] fld_layout
	 *   A pointer to the cfa_layout indicating the field format
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*cfa_bld_fld_print_obj)(uint64_t *fld_obj,
				     struct cfa_layout *layout);
};

/**@}*/

/**@}*/
#endif /* _CFA_BLD_DEVOPS_H_ */
