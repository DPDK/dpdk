/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#define COMP_ID BLD

#include <errno.h>
#include <string.h>
#include "sys_util.h"
#include "cfa_trace.h"

#include "cfa_types.h"
#include "cfa_bld_mpcops.h"

#include "host/cfa_bld_mpc_field_ids.h"
#include "host/cfa_p70_mpc_field_ids.h"
#include "p70/cfa_p70_mpc_structs.h"
#include "p70/cfa_bld_p70_mpc.h"
#include "cfa_bld_p70_host_mpc_wrapper.h"
#include "host/cfa_p70_mpc_field_mapping.h"

#ifdef NXT_ENV_DEBUG
#define LOG_RC(ERRNO) PMD_DRV_LOG_LINE(ERR, "Returning error: %d", (ERRNO))
#else
#define LOG_RC(ERRNO)
#endif

/*
 * Helper macro to set an input parm field from fields array
 */
#define SET_PARM_VALUE(NAME, TYPE, INDEX, FIELDS)                              \
	do {                                                                   \
		if (FIELDS[INDEX].field_id != INVALID_U16)                     \
			parms.NAME = (TYPE)fields[INDEX].val;                  \
	} while (0)

/*
 * Helper macro to set an input parm field from fields array through a mapping
 * function
 */
#define SET_PARM_MAPPED_VALUE(NAME, TYPE, INDEX, FIELDS, MAP_FUNC)             \
	({                                                                     \
		int retcode = 0;                                               \
		if (FIELDS[INDEX].field_id != INVALID_U16) {                   \
			int retcode;                                           \
			uint64_t mapped_val;                                   \
			retcode = MAP_FUNC(fields[INDEX].val, &mapped_val);    \
			if (retcode)                                           \
				LOG_RC(retcode);                        \
			else                                                   \
				parms.NAME = (TYPE)mapped_val;                 \
		}                                                              \
		retcode;                                                       \
	})

/*
 * Helper macro to set a result field into fields array
 */
#define GET_RESP_VALUE(NAME, INDEX, FIELDS)                                    \
	do {                                                                   \
		if (FIELDS[INDEX].field_id != INVALID_U16)                     \
			FIELDS[INDEX].val = (uint64_t)result.NAME;             \
	} while (0)

/*
 * Helper macro to set a result field into fields array thorugh a mapping
 * function
 */
#define GET_RESP_MAPPED_VALUE(NAME, INDEX, FIELDS, MAP_FUNC)                   \
	({                                                                     \
		int retcode = 0;                                               \
		if (FIELDS[INDEX].field_id != INVALID_U16) {                   \
			int retcode;                                           \
			uint64_t mapped_val;                                   \
			retcode = MAP_FUNC(result.NAME, &mapped_val);          \
			if (retcode)                                           \
				LOG_RC(retcode);                        \
			else                                                   \
				fields[INDEX].val = mapped_val;                \
		}                                                              \
		retcode;                                                       \
	})

/*
 * MPC fields validate routine.
 */
static bool fields_valid(struct cfa_mpc_data_obj *fields, uint16_t len,
			 struct field_mapping *fld_map)
{
	int i;

	for (i = 0; i < len; i++) {
		/* Field not requested to be set by caller, skip it */
		if (fields[i].field_id == INVALID_U16)
			continue;

		/*
		 * Field id should be index value unless
		 * it is set to UINT16_MAx
		 */
		if (fields[i].field_id != i)
			return false;

		/* Field is valid */
		if (!fld_map[i].valid)
			return false;
	}

	return true;
}

/* Map global table type definition to p70 specific value */
static int table_type_map(uint64_t val, uint64_t *mapped_val)
{
	switch (val) {
	case CFA_BLD_MPC_HW_TABLE_TYPE_ACTION:
		*mapped_val = CFA_HW_TABLE_ACTION;
		break;
	case CFA_BLD_MPC_HW_TABLE_TYPE_LOOKUP:
		*mapped_val = CFA_HW_TABLE_LOOKUP;
		break;
	default:
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	return 0;
}

/* Map global read mode value to p70 specific value */
static int read_mode_map(uint64_t val, uint64_t *mapped_val)
{
	switch (val) {
	case CFA_BLD_MPC_RD_NORMAL:
		*mapped_val = CFA_MPC_RD_NORMAL;
		break;
	case CFA_BLD_MPC_RD_EVICT:
		*mapped_val = CFA_MPC_RD_EVICT;
		break;
	case CFA_BLD_MPC_RD_DEBUG_LINE:
		*mapped_val = CFA_MPC_RD_DEBUG_LINE;
		break;
	case CFA_BLD_MPC_RD_DEBUG_TAG:
		*mapped_val = CFA_MPC_RD_DEBUG_TAG;
		break;
	default:
		LOG_RC(-EINVAL);
		return -EINVAL;
	}
	return 0;
}

/* Map global write mode value to p70 specific value */
static int write_mode_map(uint64_t val, uint64_t *mapped_val)
{
	switch (val) {
	case CFA_BLD_MPC_WR_WRITE_THRU:
		*mapped_val = CFA_MPC_WR_WRITE_THRU;
		break;
	case CFA_BLD_MPC_WR_WRITE_BACK:
		*mapped_val = CFA_MPC_WR_WRITE_BACK;
		break;
	default:
		LOG_RC(-EINVAL);
		return -EINVAL;
	}
	return 0;
}

/* Map global evict mode value to p70 specific value */
static int evict_mode_map(uint64_t val, uint64_t *mapped_val)
{
	switch (val) {
	case CFA_BLD_MPC_EV_EVICT_LINE:
		*mapped_val = CFA_MPC_EV_EVICT_LINE;
		break;
	case CFA_BLD_MPC_EV_EVICT_SCOPE_ADDRESS:
		*mapped_val = CFA_MPC_EV_EVICT_SCOPE_ADDRESS;
		break;
	case CFA_BLD_MPC_EV_EVICT_CLEAN_LINES:
		*mapped_val = CFA_MPC_EV_EVICT_CLEAN_LINES;
		break;
	case CFA_BLD_MPC_EV_EVICT_CLEAN_FAST_EVICT_LINES:
		*mapped_val = CFA_MPC_EV_EVICT_CLEAN_FAST_EVICT_LINES;
		break;
	case CFA_BLD_MPC_EV_EVICT_CLEAN_AND_CLEAN_FAST_EVICT_LINES:
		*mapped_val = CFA_MPC_EV_EVICT_CLEAN_AND_CLEAN_FAST_EVICT_LINES;
		break;
	case CFA_BLD_MPC_EV_EVICT_TABLE_SCOPE:
		*mapped_val = CFA_MPC_EV_EVICT_TABLE_SCOPE;
		break;
	default:
		LOG_RC(-EINVAL);
		return -EINVAL;
	}
	return 0;
}

/* Map device specific response status code to global value */
static int status_code_map(uint64_t val, uint64_t *mapped_val)
{
	switch (val) {
	case CFA_MPC_OK:
		*mapped_val = CFA_BLD_MPC_OK;
		break;
	case CFA_MPC_UNSPRT_ERR:
		*mapped_val = CFA_BLD_MPC_UNSPRT_ERR;
		break;
	case CFA_MPC_FMT_ERR:
		*mapped_val = CFA_BLD_MPC_FMT_ERR;
		break;
	case CFA_MPC_SCOPE_ERR:
		*mapped_val = CFA_BLD_MPC_SCOPE_ERR;
		break;
	case CFA_MPC_ADDR_ERR:
		*mapped_val = CFA_BLD_MPC_ADDR_ERR;
		break;
	case CFA_MPC_CACHE_ERR:
		*mapped_val = CFA_BLD_MPC_CACHE_ERR;
		break;
	case CFA_MPC_EM_MISS:
		*mapped_val = CFA_BLD_MPC_EM_MISS;
		break;
	case CFA_MPC_EM_DUPLICATE:
		*mapped_val = CFA_BLD_MPC_EM_DUPLICATE;
		break;
	case CFA_MPC_EM_EVENT_COLLECTION_FAIL:
		*mapped_val = CFA_BLD_MPC_EM_EVENT_COLLECTION_FAIL;
		break;
	case CFA_MPC_EM_ABORT:
		*mapped_val = CFA_BLD_MPC_EM_ABORT;
		break;
	default:
		LOG_RC(-EINVAL);
		return -EINVAL;
	}
	return 0;
}

static bool has_unsupported_fields(struct cfa_mpc_data_obj *fields,
				   uint16_t len, uint16_t *unsup_flds,
				   uint16_t unsup_flds_len)
{
	int i, j;

	for (i = 0; i < len; i++) {
		/* Skip invalid fields */
		if (fields[i].field_id == INVALID_U16)
			continue;

		for (j = 0; j < unsup_flds_len; j++) {
			if (fields[i].field_id == unsup_flds[j])
				return true;
		}
	}

	return false;
}

int cfa_bld_p70_mpc_build_cache_read(uint8_t *cmd, uint32_t *cmd_buff_len,
				     struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_cache_axs_params parms = { 0 };

	/* Parameters check */
	if (!cmd || !cmd_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_READ_CMD_MAX_FLD,
			  cfa_p70_mpc_read_cmd_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Prepare parameters structure */
	SET_PARM_VALUE(opaque, uint32_t, CFA_BLD_MPC_READ_CMD_OPAQUE_FLD,
		       fields);
	SET_PARM_VALUE(tbl_scope, uint8_t, CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD,
		       fields);
	SET_PARM_VALUE(tbl_index, uint32_t,
		       CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD, fields);
	SET_PARM_VALUE(data_size, uint8_t, CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD,
		       fields);
	SET_PARM_VALUE(read.host_address, uint64_t,
		       CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD, fields);
	rc = SET_PARM_MAPPED_VALUE(tbl_type, enum cfa_hw_table_type,
				   CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD, fields,
				   table_type_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	rc = SET_PARM_MAPPED_VALUE(read.mode, enum cfa_mpc_read_mode,
				   CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD,
				   fields, read_mode_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	return cfa_mpc_build_cache_axs_cmd(CFA_MPC_READ, cmd, cmd_buff_len,
					   &parms);
}

int cfa_bld_p70_mpc_build_cache_write(uint8_t *cmd, uint32_t *cmd_buff_len,
				      const uint8_t *data,
				      struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_cache_axs_params parms = { 0 };

	/* Parameters check */
	if (!cmd || !cmd_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_WRITE_CMD_MAX_FLD,
			  cfa_p70_mpc_write_cmd_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Prepare parameters structure */
	SET_PARM_VALUE(opaque, uint32_t, CFA_BLD_MPC_WRITE_CMD_OPAQUE_FLD,
		       fields);
	SET_PARM_VALUE(tbl_scope, uint8_t,
		       CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD, fields);
	SET_PARM_VALUE(tbl_index, uint32_t,
		       CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD, fields);
	SET_PARM_VALUE(data_size, uint8_t, CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD,
		       fields);
	rc = SET_PARM_MAPPED_VALUE(tbl_type, enum cfa_hw_table_type,
				   CFA_BLD_MPC_WRITE_CMD_TABLE_TYPE_FLD, fields,
				   table_type_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	parms.write.data_ptr = data;
	rc = SET_PARM_MAPPED_VALUE(write.mode, enum cfa_mpc_write_mode,
				   CFA_BLD_MPC_WRITE_CMD_CACHE_OPTION_FLD,
				   fields, write_mode_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	return cfa_mpc_build_cache_axs_cmd(CFA_MPC_WRITE, cmd, cmd_buff_len,
					   &parms);
}

int cfa_bld_p70_mpc_build_cache_evict(uint8_t *cmd, uint32_t *cmd_buff_len,
				      struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_cache_axs_params parms = { 0 };

	/* Parameters check */
	if (!cmd || !cmd_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_INVALIDATE_CMD_MAX_FLD,
			  cfa_p70_mpc_invalidate_cmd_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Prepare parameters structure */
	SET_PARM_VALUE(opaque, uint32_t, CFA_BLD_MPC_INVALIDATE_CMD_OPAQUE_FLD,
		       fields);
	SET_PARM_VALUE(tbl_scope, uint8_t,
		       CFA_BLD_MPC_INVALIDATE_CMD_TABLE_SCOPE_FLD, fields);
	SET_PARM_VALUE(tbl_index, uint32_t,
		       CFA_BLD_MPC_INVALIDATE_CMD_TABLE_INDEX_FLD, fields);
	SET_PARM_VALUE(data_size, uint8_t,
		       CFA_BLD_MPC_INVALIDATE_CMD_DATA_SIZE_FLD, fields);
	rc = SET_PARM_MAPPED_VALUE(tbl_type, enum cfa_hw_table_type,
				   CFA_BLD_MPC_INVALIDATE_CMD_TABLE_TYPE_FLD,
				   fields, table_type_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	rc = SET_PARM_MAPPED_VALUE(evict.mode, enum cfa_mpc_evict_mode,
				   CFA_BLD_MPC_INVALIDATE_CMD_CACHE_OPTION_FLD,
				   fields, evict_mode_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	return cfa_mpc_build_cache_axs_cmd(CFA_MPC_INVALIDATE, cmd,
					   cmd_buff_len, &parms);
}

int cfa_bld_p70_mpc_build_cache_rdclr(uint8_t *cmd, uint32_t *cmd_buff_len,
				      struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_cache_axs_params parms = { 0 };

	/* Parameters check */
	if (!cmd || !cmd_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_READ_CLR_CMD_MAX_FLD,
			  cfa_p70_mpc_read_clr_cmd_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Prepare parameters structure */
	SET_PARM_VALUE(opaque, uint32_t, CFA_BLD_MPC_READ_CLR_CMD_OPAQUE_FLD,
		       fields);
	SET_PARM_VALUE(tbl_scope, uint8_t,
		       CFA_BLD_MPC_READ_CLR_CMD_TABLE_SCOPE_FLD, fields);
	SET_PARM_VALUE(tbl_index, uint32_t,
		       CFA_BLD_MPC_READ_CLR_CMD_TABLE_INDEX_FLD, fields);
	SET_PARM_VALUE(data_size, uint8_t,
		       CFA_BLD_MPC_READ_CLR_CMD_DATA_SIZE_FLD, fields);
	SET_PARM_VALUE(read.host_address, uint64_t,
		       CFA_BLD_MPC_READ_CLR_CMD_HOST_ADDRESS_FLD, fields);
	rc = SET_PARM_MAPPED_VALUE(tbl_type, enum cfa_hw_table_type,
				   CFA_BLD_MPC_READ_CLR_CMD_TABLE_TYPE_FLD,
				   fields, table_type_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	SET_PARM_VALUE(read.clear_mask, uint16_t,
		       CFA_BLD_MPC_READ_CLR_CMD_CLEAR_MASK_FLD, fields);
	rc = SET_PARM_MAPPED_VALUE(read.mode, enum cfa_mpc_read_mode,
				   CFA_BLD_MPC_READ_CLR_CMD_CACHE_OPTION_FLD,
				   fields, read_mode_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	return cfa_mpc_build_cache_axs_cmd(CFA_MPC_READ_CLR, cmd, cmd_buff_len,
					   &parms);
}

int cfa_bld_p70_mpc_build_em_search(uint8_t *cmd, uint32_t *cmd_buff_len,
				    uint8_t *em_entry,
				    struct cfa_mpc_data_obj *fields)
{
	struct cfa_mpc_em_op_params parms = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_EM_SEARCH_CMD_CACHE_OPTION_FLD,
	};

	/* Parameters check */
	if (!cmd || !cmd_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_EM_SEARCH_CMD_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_EM_SEARCH_CMD_MAX_FLD,
			  cfa_p70_mpc_em_search_cmd_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Prepare parameters structure */
	SET_PARM_VALUE(opaque, uint32_t, CFA_BLD_MPC_EM_SEARCH_CMD_OPAQUE_FLD,
		       fields);
	SET_PARM_VALUE(tbl_scope, uint8_t,
		       CFA_BLD_MPC_EM_SEARCH_CMD_TABLE_SCOPE_FLD, fields);

	parms.search.em_entry = em_entry;
	SET_PARM_VALUE(search.data_size, uint8_t,
		       CFA_BLD_MPC_EM_SEARCH_CMD_DATA_SIZE_FLD, fields);

	return cfa_mpc_build_em_op_cmd(CFA_MPC_EM_SEARCH, cmd, cmd_buff_len,
				       &parms);
}

int cfa_bld_p70_mpc_build_em_insert(uint8_t *cmd, uint32_t *cmd_buff_len,
				    const uint8_t *em_entry,
				    struct cfa_mpc_data_obj *fields)
{
	struct cfa_mpc_em_op_params parms = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_EM_INSERT_CMD_WRITE_THROUGH_FLD,
		CFA_BLD_MPC_EM_INSERT_CMD_CACHE_OPTION_FLD,
		CFA_BLD_MPC_EM_INSERT_CMD_CACHE_OPTION2_FLD,
	};

	/* Parameters check */
	if (!cmd || !cmd_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_EM_INSERT_CMD_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_EM_INSERT_CMD_MAX_FLD,
			  cfa_p70_mpc_em_insert_cmd_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Prepare parameters structure */
	SET_PARM_VALUE(opaque, uint32_t, CFA_BLD_MPC_EM_INSERT_CMD_OPAQUE_FLD,
		       fields);
	SET_PARM_VALUE(tbl_scope, uint8_t,
		       CFA_BLD_MPC_EM_INSERT_CMD_TABLE_SCOPE_FLD, fields);

	parms.insert.em_entry = (const uint8_t *)em_entry;
	SET_PARM_VALUE(insert.replace, uint8_t,
		       CFA_BLD_MPC_EM_INSERT_CMD_REPLACE_FLD, fields);
	SET_PARM_VALUE(insert.entry_idx, uint32_t,
		       CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX_FLD, fields);
	SET_PARM_VALUE(insert.bucket_idx, uint32_t,
		       CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX2_FLD, fields);
	SET_PARM_VALUE(insert.data_size, uint8_t,
		       CFA_BLD_MPC_EM_INSERT_CMD_DATA_SIZE_FLD, fields);

	return cfa_mpc_build_em_op_cmd(CFA_MPC_EM_INSERT, cmd, cmd_buff_len,
				       &parms);
}

int cfa_bld_p70_mpc_build_em_delete(uint8_t *cmd, uint32_t *cmd_buff_len,
				    struct cfa_mpc_data_obj *fields)
{
	struct cfa_mpc_em_op_params parms = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_EM_DELETE_CMD_WRITE_THROUGH_FLD,
		CFA_BLD_MPC_EM_DELETE_CMD_CACHE_OPTION_FLD,
		CFA_BLD_MPC_EM_DELETE_CMD_CACHE_OPTION2_FLD,
	};

	/* Parameters check */
	if (!cmd || !cmd_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_EM_DELETE_CMD_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_EM_DELETE_CMD_MAX_FLD,
			  cfa_p70_mpc_em_delete_cmd_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Prepare parameters structure */
	SET_PARM_VALUE(opaque, uint32_t, CFA_BLD_MPC_EM_DELETE_CMD_OPAQUE_FLD,
		       fields);
	SET_PARM_VALUE(tbl_scope, uint8_t,
		       CFA_BLD_MPC_EM_DELETE_CMD_TABLE_SCOPE_FLD, fields);

	SET_PARM_VALUE(del.entry_idx, uint32_t,
		       CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX_FLD, fields);
	SET_PARM_VALUE(del.bucket_idx, uint32_t,
		       CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX2_FLD, fields);

	return cfa_mpc_build_em_op_cmd(CFA_MPC_EM_DELETE, cmd, cmd_buff_len,
				       &parms);
}

int cfa_bld_p70_mpc_build_em_chain(uint8_t *cmd, uint32_t *cmd_buff_len,
				   struct cfa_mpc_data_obj *fields)
{
	struct cfa_mpc_em_op_params parms = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_EM_CHAIN_CMD_WRITE_THROUGH_FLD,
		CFA_BLD_MPC_EM_CHAIN_CMD_CACHE_OPTION_FLD,
		CFA_BLD_MPC_EM_CHAIN_CMD_CACHE_OPTION2_FLD,
	};

	/* Parameters check */
	if (!cmd || !cmd_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_EM_CHAIN_CMD_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_EM_CHAIN_CMD_MAX_FLD,
			  cfa_p70_mpc_em_chain_cmd_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Prepare parameters structure */
	SET_PARM_VALUE(opaque, uint32_t, CFA_BLD_MPC_EM_CHAIN_CMD_OPAQUE_FLD,
		       fields);
	SET_PARM_VALUE(tbl_scope, uint8_t,
		       CFA_BLD_MPC_EM_CHAIN_CMD_TABLE_SCOPE_FLD, fields);

	SET_PARM_VALUE(chain.entry_idx, uint32_t,
		       CFA_BLD_MPC_EM_CHAIN_CMD_TABLE_INDEX_FLD, fields);
	SET_PARM_VALUE(chain.bucket_idx, uint32_t,
		       CFA_BLD_MPC_EM_CHAIN_CMD_TABLE_INDEX2_FLD, fields);

	return cfa_mpc_build_em_op_cmd(CFA_MPC_EM_CHAIN, cmd, cmd_buff_len,
				       &parms);
}

int cfa_bld_p70_mpc_parse_cache_read(uint8_t *resp, uint32_t resp_buff_len,
				     uint8_t *rd_data, uint32_t rd_data_len,
				     struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_cache_axs_result result = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_READ_CMP_TYPE_FLD,
		CFA_BLD_MPC_READ_CMP_MP_CLIENT_FLD,
		CFA_BLD_MPC_READ_CMP_DMA_LENGTH_FLD,
		CFA_BLD_MPC_READ_CMP_OPCODE_FLD,
		CFA_BLD_MPC_READ_CMP_V_FLD,
		CFA_BLD_MPC_READ_CMP_TABLE_TYPE_FLD,
		CFA_BLD_MPC_READ_CMP_TABLE_SCOPE_FLD,
		CFA_BLD_MPC_READ_CMP_TABLE_INDEX_FLD,
	};

	/* Parameters check */
	if (!resp || !resp_buff_len || !fields || !rd_data) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_READ_CMP_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_READ_CMP_MAX_FLD,
			  cfa_p70_mpc_read_cmp_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Retrieve response parameters */
	result.rd_data = rd_data;
	result.data_len = rd_data_len;
	rc = cfa_mpc_parse_cache_axs_resp(CFA_MPC_READ, resp, resp_buff_len,
					  &result);
	if (rc)
		return rc;

	GET_RESP_VALUE(opaque, CFA_BLD_MPC_READ_CMP_OPAQUE_FLD, fields);
	GET_RESP_VALUE(error_data, CFA_BLD_MPC_READ_CMP_HASH_MSB_FLD, fields);
	rc = GET_RESP_MAPPED_VALUE(status, CFA_BLD_MPC_READ_CMP_STATUS_FLD,
				   fields, status_code_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	return 0;
}

int cfa_bld_p70_mpc_parse_cache_write(uint8_t *resp, uint32_t resp_buff_len,
				      struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_cache_axs_result result = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_WRITE_CMP_TYPE_FLD,
		CFA_BLD_MPC_WRITE_CMP_MP_CLIENT_FLD,
		CFA_BLD_MPC_WRITE_CMP_OPCODE_FLD,
		CFA_BLD_MPC_WRITE_CMP_V_FLD,
		CFA_BLD_MPC_WRITE_CMP_TABLE_TYPE_FLD,
		CFA_BLD_MPC_WRITE_CMP_TABLE_SCOPE_FLD,
		CFA_BLD_MPC_WRITE_CMP_TABLE_INDEX_FLD,
	};

	/* Parameters check */
	if (!resp || !resp_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_WRITE_CMP_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_WRITE_CMP_MAX_FLD,
			  cfa_p70_mpc_write_cmp_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Retrieve response parameters */
	rc = cfa_mpc_parse_cache_axs_resp(CFA_MPC_WRITE, resp, resp_buff_len,
					  &result);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	GET_RESP_VALUE(opaque, CFA_BLD_MPC_WRITE_CMP_OPAQUE_FLD, fields);
	GET_RESP_VALUE(error_data, CFA_BLD_MPC_WRITE_CMP_HASH_MSB_FLD, fields);
	rc = GET_RESP_MAPPED_VALUE(status, CFA_BLD_MPC_WRITE_CMP_STATUS_FLD,
				   fields, status_code_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	return 0;
}

int cfa_bld_p70_mpc_parse_cache_evict(uint8_t *resp, uint32_t resp_buff_len,
				      struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_cache_axs_result result = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_INVALIDATE_CMP_TYPE_FLD,
		CFA_BLD_MPC_INVALIDATE_CMP_MP_CLIENT_FLD,
		CFA_BLD_MPC_INVALIDATE_CMP_OPCODE_FLD,
		CFA_BLD_MPC_INVALIDATE_CMP_V_FLD,
		CFA_BLD_MPC_INVALIDATE_CMP_TABLE_TYPE_FLD,
		CFA_BLD_MPC_INVALIDATE_CMP_TABLE_SCOPE_FLD,
		CFA_BLD_MPC_INVALIDATE_CMP_TABLE_INDEX_FLD,
	};

	/* Parameters check */
	if (!resp || !resp_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_INVALIDATE_CMP_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_INVALIDATE_CMP_MAX_FLD,
			  cfa_p70_mpc_invalidate_cmp_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Retrieve response parameters */
	rc = cfa_mpc_parse_cache_axs_resp(CFA_MPC_INVALIDATE, resp,
					  resp_buff_len, &result);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	GET_RESP_VALUE(opaque, CFA_BLD_MPC_INVALIDATE_CMP_OPAQUE_FLD, fields);
	GET_RESP_VALUE(error_data, CFA_BLD_MPC_INVALIDATE_CMP_HASH_MSB_FLD,
		       fields);
	rc = GET_RESP_MAPPED_VALUE(status,
				   CFA_BLD_MPC_INVALIDATE_CMP_STATUS_FLD,
				   fields, status_code_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	return 0;
}

int cfa_bld_p70_mpc_parse_cache_rdclr(uint8_t *resp, uint32_t resp_buff_len,
				      uint8_t *rd_data, uint32_t rd_data_len,
				      struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_cache_axs_result result = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_READ_CMP_TYPE_FLD,
		CFA_BLD_MPC_READ_CMP_MP_CLIENT_FLD,
		CFA_BLD_MPC_READ_CMP_DMA_LENGTH_FLD,
		CFA_BLD_MPC_READ_CMP_OPCODE_FLD,
		CFA_BLD_MPC_READ_CMP_V_FLD,
		CFA_BLD_MPC_READ_CMP_TABLE_TYPE_FLD,
		CFA_BLD_MPC_READ_CMP_TABLE_SCOPE_FLD,
		CFA_BLD_MPC_READ_CMP_TABLE_INDEX_FLD,
	};

	/* Parameters check */
	if (!resp || !resp_buff_len || !fields || !rd_data) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_READ_CMP_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_READ_CMP_MAX_FLD,
			  cfa_p70_mpc_read_cmp_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Retrieve response parameters */
	result.rd_data = rd_data;
	result.data_len = rd_data_len;
	rc = cfa_mpc_parse_cache_axs_resp(CFA_MPC_READ_CLR, resp, resp_buff_len,
					  &result);
	if (rc)
		return rc;

	GET_RESP_VALUE(opaque, CFA_BLD_MPC_READ_CMP_OPAQUE_FLD, fields);
	GET_RESP_VALUE(error_data, CFA_BLD_MPC_READ_CMP_HASH_MSB_FLD, fields);
	rc = GET_RESP_MAPPED_VALUE(status, CFA_BLD_MPC_READ_CMP_STATUS_FLD,
				   fields, status_code_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	return 0;
}

int cfa_bld_p70_mpc_parse_em_search(uint8_t *resp, uint32_t resp_buff_len,
				    struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_em_op_result result = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_EM_SEARCH_CMP_TYPE_FLD,
		CFA_BLD_MPC_EM_SEARCH_CMP_MP_CLIENT_FLD,
		CFA_BLD_MPC_EM_SEARCH_CMP_OPCODE_FLD,
		CFA_BLD_MPC_EM_SEARCH_CMP_V1_FLD,
		CFA_BLD_MPC_EM_SEARCH_CMP_TABLE_SCOPE_FLD,
		CFA_BLD_MPC_EM_SEARCH_CMP_V2_FLD,
	};

	/* Parameters check */
	if (!resp || !resp_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_EM_SEARCH_CMP_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_EM_SEARCH_CMP_MAX_FLD,
			  cfa_p70_mpc_em_search_cmp_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Retrieve response parameters */
	rc = cfa_mpc_parse_em_op_resp(CFA_MPC_EM_SEARCH, resp, resp_buff_len,
				      &result);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	GET_RESP_VALUE(opaque, CFA_BLD_MPC_EM_SEARCH_CMP_OPAQUE_FLD, fields);
	GET_RESP_VALUE(error_data, CFA_BLD_MPC_EM_SEARCH_CMP_HASH_MSB_FLD,
		       fields);
	rc = GET_RESP_MAPPED_VALUE(status, CFA_BLD_MPC_EM_SEARCH_CMP_STATUS_FLD,
				   fields, status_code_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	GET_RESP_VALUE(search.bucket_num, CFA_BLD_MPC_EM_SEARCH_CMP_BKT_NUM_FLD,
		       fields);
	GET_RESP_VALUE(search.num_entries,
		       CFA_BLD_MPC_EM_SEARCH_CMP_NUM_ENTRIES_FLD, fields);
	GET_RESP_VALUE(search.hash_msb, CFA_BLD_MPC_EM_SEARCH_CMP_HASH_MSB_FLD,
		       fields);
	GET_RESP_VALUE(search.match_idx,
		       CFA_BLD_MPC_EM_SEARCH_CMP_TABLE_INDEX_FLD, fields);
	GET_RESP_VALUE(search.bucket_idx,
		       CFA_BLD_MPC_EM_SEARCH_CMP_TABLE_INDEX2_FLD, fields);

	return 0;
}

int cfa_bld_p70_mpc_parse_em_insert(uint8_t *resp, uint32_t resp_buff_len,
				    struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_em_op_result result = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_EM_INSERT_CMP_TYPE_FLD,
		CFA_BLD_MPC_EM_INSERT_CMP_MP_CLIENT_FLD,
		CFA_BLD_MPC_EM_INSERT_CMP_OPCODE_FLD,
		CFA_BLD_MPC_EM_INSERT_CMP_V1_FLD,
		CFA_BLD_MPC_EM_INSERT_CMP_TABLE_SCOPE_FLD,
		CFA_BLD_MPC_EM_INSERT_CMP_V2_FLD,
		CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX_FLD,
		CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX2_FLD,
	};

	/* Parameters check */
	if (!resp || !resp_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_EM_INSERT_CMP_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_EM_INSERT_CMP_MAX_FLD,
			  cfa_p70_mpc_em_insert_cmp_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Retrieve response parameters */
	rc = cfa_mpc_parse_em_op_resp(CFA_MPC_EM_INSERT, resp, resp_buff_len,
				      &result);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	GET_RESP_VALUE(opaque, CFA_BLD_MPC_EM_INSERT_CMP_OPAQUE_FLD, fields);
	GET_RESP_VALUE(error_data, CFA_BLD_MPC_EM_INSERT_CMP_HASH_MSB_FLD,
		       fields);
	rc = GET_RESP_MAPPED_VALUE(status, CFA_BLD_MPC_EM_INSERT_CMP_STATUS_FLD,
				   fields, status_code_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	GET_RESP_VALUE(insert.bucket_num, CFA_BLD_MPC_EM_INSERT_CMP_BKT_NUM_FLD,
		       fields);
	GET_RESP_VALUE(insert.num_entries,
		       CFA_BLD_MPC_EM_INSERT_CMP_NUM_ENTRIES_FLD, fields);
	GET_RESP_VALUE(insert.hash_msb, CFA_BLD_MPC_EM_INSERT_CMP_HASH_MSB_FLD,
		       fields);
	GET_RESP_VALUE(insert.match_idx,
		       CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX4_FLD, fields);
	GET_RESP_VALUE(insert.bucket_idx,
		       CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX3_FLD, fields);
	GET_RESP_VALUE(insert.replaced,
		       CFA_BLD_MPC_EM_INSERT_CMP_REPLACED_ENTRY_FLD, fields);
	GET_RESP_VALUE(insert.chain_update,
		       CFA_BLD_MPC_EM_INSERT_CMP_CHAIN_UPD_FLD, fields);

	return 0;
}

int cfa_bld_p70_mpc_parse_em_delete(uint8_t *resp, uint32_t resp_buff_len,
				    struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_em_op_result result = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_EM_DELETE_CMP_TYPE_FLD,
		CFA_BLD_MPC_EM_DELETE_CMP_MP_CLIENT_FLD,
		CFA_BLD_MPC_EM_DELETE_CMP_OPCODE_FLD,
		CFA_BLD_MPC_EM_DELETE_CMP_V1_FLD,
		CFA_BLD_MPC_EM_DELETE_CMP_TABLE_SCOPE_FLD,
		CFA_BLD_MPC_EM_DELETE_CMP_V2_FLD,
		CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX_FLD,
		CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX2_FLD,
	};

	/* Parameters check */
	if (!resp || !resp_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_EM_DELETE_CMP_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_EM_DELETE_CMP_MAX_FLD,
			  cfa_p70_mpc_em_delete_cmp_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Retrieve response parameters */
	rc = cfa_mpc_parse_em_op_resp(CFA_MPC_EM_DELETE, resp, resp_buff_len,
				      &result);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	GET_RESP_VALUE(opaque, CFA_BLD_MPC_EM_DELETE_CMP_OPAQUE_FLD, fields);
	GET_RESP_VALUE(error_data, CFA_BLD_MPC_EM_DELETE_CMP_HASH_MSB_FLD,
		       fields);
	rc = GET_RESP_MAPPED_VALUE(status, CFA_BLD_MPC_EM_DELETE_CMP_STATUS_FLD,
				   fields, status_code_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	GET_RESP_VALUE(del.new_tail, CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX4_FLD,
		       fields);
	GET_RESP_VALUE(del.prev_tail,
		       CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX3_FLD, fields);
	GET_RESP_VALUE(del.chain_update,
		       CFA_BLD_MPC_EM_DELETE_CMP_CHAIN_UPD_FLD, fields);
	GET_RESP_VALUE(del.bucket_num, CFA_BLD_MPC_EM_DELETE_CMP_BKT_NUM_FLD,
		       fields);
	GET_RESP_VALUE(del.num_entries,
		       CFA_BLD_MPC_EM_DELETE_CMP_NUM_ENTRIES_FLD, fields);
	return 0;
}

int cfa_bld_p70_mpc_parse_em_chain(uint8_t *resp, uint32_t resp_buff_len,
				   struct cfa_mpc_data_obj *fields)
{
	int rc;
	struct cfa_mpc_em_op_result result = { 0 };
	uint16_t unsupported_fields[] = {
		CFA_BLD_MPC_EM_CHAIN_CMP_TYPE_FLD,
		CFA_BLD_MPC_EM_CHAIN_CMP_MP_CLIENT_FLD,
		CFA_BLD_MPC_EM_CHAIN_CMP_OPCODE_FLD,
		CFA_BLD_MPC_EM_CHAIN_CMP_V1_FLD,
		CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_SCOPE_FLD,
		CFA_BLD_MPC_EM_CHAIN_CMP_V2_FLD,
		CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_INDEX_FLD,
		CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_INDEX2_FLD,
	};

	/* Parameters check */
	if (!resp || !resp_buff_len || !fields) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (has_unsupported_fields(fields, CFA_BLD_MPC_EM_CHAIN_CMP_MAX_FLD,
				   unsupported_fields,
				   ARRAY_SIZE(unsupported_fields))) {
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	if (!fields_valid(fields, CFA_BLD_MPC_EM_CHAIN_CMP_MAX_FLD,
			  cfa_p70_mpc_em_chain_cmp_gbl_to_dev)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Retrieve response parameters */
	rc = cfa_mpc_parse_em_op_resp(CFA_MPC_EM_CHAIN, resp, resp_buff_len,
				      &result);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	GET_RESP_VALUE(opaque, CFA_BLD_MPC_EM_CHAIN_CMP_OPAQUE_FLD, fields);
	GET_RESP_VALUE(error_data, CFA_BLD_MPC_EM_CHAIN_CMP_HASH_MSB_FLD,
		       fields);
	rc = GET_RESP_MAPPED_VALUE(status, CFA_BLD_MPC_EM_CHAIN_CMP_STATUS_FLD,
				   fields, status_code_map);
	if (rc) {
		LOG_RC(rc);
		return rc;
	}

	GET_RESP_VALUE(chain.bucket_num, CFA_BLD_MPC_EM_CHAIN_CMP_BKT_NUM_FLD,
		       fields);
	GET_RESP_VALUE(chain.num_entries,
		       CFA_BLD_MPC_EM_CHAIN_CMP_NUM_ENTRIES_FLD, fields);
	return 0;
}
