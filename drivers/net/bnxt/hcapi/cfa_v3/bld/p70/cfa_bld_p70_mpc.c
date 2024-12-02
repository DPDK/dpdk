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
#include "cfa_p70.h"
#include "cfa_bld_p70_mpc.h"
#include "cfa_bld_p70_mpc_defs.h"
#include "cfa_p70_mpc_structs.h"

/* CFA MPC client ids */
#define MP_CLIENT_TE_CFA READ_CMP_MP_CLIENT_TE_CFA
#define MP_CLIENT_RE_CFA READ_CMP_MP_CLIENT_RE_CFA

/* MPC Client id check in CFA completion messages */
#define ASSERT_CFA_MPC_CLIENT_ID(MPCID)                                        \
	do {                                                                   \
		if ((MPCID) != MP_CLIENT_TE_CFA &&                             \
		    (MPCID) != MP_CLIENT_RE_CFA) {                             \
			CFA_LOG_WARN(                                          \
				"Unexpected MPC client id in response: %d\n",  \
				(MPCID));                                      \
		}                                                              \
	} while (0)

#ifdef NXT_ENV_DEBUG
#define LOG_RC(ERRNO) PMD_DRV_LOG_LINE(ERR, "Returning error: %d", (ERRNO))
#else
#define LOG_RC(ERRNO)
#endif

/**
 * MPC header definition
 */
struct mpc_header {
	uint32_t type:6;
	uint32_t flags:10;
	uint32_t len:16;
	uint32_t opaque;
	uint64_t unused;
};

/*
 * For successful completions of read and read-clear MPC CFA
 * commands, the responses will contain this dma info structure
 * following the cfa_mpc_read(|clr)_cmp structure and preceding
 * the actual data read from the cache.
 */
struct mpc_cr_short_dma_data {
	uint32_t dma_length:8;
	uint32_t unused0:24;
	uint32_t dma_addr0;
	uint32_t dma_addr1;
};

/** Add MPC header information to MPC command message */
static int fill_mpc_header(uint8_t *cmd, uint32_t size, uint32_t opaque_val)
{
	struct mpc_header hdr = {
		.opaque = opaque_val,
	};

	if (size < sizeof(struct mpc_header)) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	memcpy(cmd, &hdr, sizeof(hdr));

	return 0;
}

/** Compose Table read-clear message */
static int compose_mpc_read_clr_msg(uint8_t *cmd_buff, uint32_t *cmd_buff_len,
				    struct cfa_mpc_cache_axs_params *parms)
{
	struct cfa_mpc_read_clr_cmd *cmd;
	struct cfa_mpc_cache_read_params *rd_parms = &parms->read;
	uint32_t cmd_size =
		sizeof(struct mpc_header) + sizeof(struct cfa_mpc_read_clr_cmd);

	if (parms->data_size != 1) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (parms->tbl_type >= CFA_HW_TABLE_MAX) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	cmd = (struct cfa_mpc_read_clr_cmd *)(cmd_buff +
					      sizeof(struct mpc_header));

	/* Populate CFA MPC command header */
	memset(cmd, 0, sizeof(struct cfa_mpc_read_clr_cmd));
	cmd->opcode = READ_CLR_CMD_OPCODE_READ_CLR;
	cmd->table_type = parms->tbl_type;
	cmd->table_scope = parms->tbl_scope;
	cmd->data_size = parms->data_size;
	cmd->table_index = parms->tbl_index;
	cmd->host_address_1 = (uint32_t)rd_parms->host_address;
	cmd->host_address_2 = (uint32_t)(rd_parms->host_address >> 32);
	switch (rd_parms->mode) {
	case CFA_MPC_RD_EVICT:
		cmd->cache_option = CACHE_READ_CLR_OPTION_EVICT;
		break;
	default:
	case CFA_MPC_RD_NORMAL:
		cmd->cache_option = CACHE_READ_CLR_OPTION_NORMAL;
		break;
	}
	cmd->clear_mask = rd_parms->clear_mask;
	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose Table read message */
static int compose_mpc_read_msg(uint8_t *cmd_buff, uint32_t *cmd_buff_len,
				struct cfa_mpc_cache_axs_params *parms)
{
	struct cfa_mpc_read_cmd *cmd;
	struct cfa_mpc_cache_read_params *rd_parms = &parms->read;
	uint32_t cmd_size =
		sizeof(struct mpc_header) + sizeof(struct cfa_mpc_read_cmd);

	if (parms->data_size < 1 || parms->data_size > 4) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (parms->tbl_type >= CFA_HW_TABLE_MAX) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	cmd = (struct cfa_mpc_read_cmd *)(cmd_buff + sizeof(struct mpc_header));

	/* Populate CFA MPC command header */
	memset(cmd, 0, sizeof(struct cfa_mpc_read_cmd));
	cmd->opcode = READ_CMD_OPCODE_READ;
	cmd->table_type = parms->tbl_type;
	cmd->table_scope = parms->tbl_scope;
	cmd->data_size = parms->data_size;
	cmd->table_index = parms->tbl_index;
	cmd->host_address_1 = (uint32_t)rd_parms->host_address;
	cmd->host_address_2 = (uint32_t)(rd_parms->host_address >> 32);
	switch (rd_parms->mode) {
	case CFA_MPC_RD_EVICT:
		cmd->cache_option = CACHE_READ_OPTION_EVICT;
		break;
	case CFA_MPC_RD_DEBUG_LINE:
		cmd->cache_option = CACHE_READ_OPTION_DEBUG_LINE;
		break;
	case CFA_MPC_RD_DEBUG_TAG:
		cmd->cache_option = CACHE_READ_OPTION_DEBUG_TAG;
		break;
	default:
	case CFA_MPC_RD_NORMAL:
		cmd->cache_option = CACHE_READ_OPTION_NORMAL;
		break;
	}
	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose Table write message */
static int compose_mpc_write_msg(uint8_t *cmd_buff, uint32_t *cmd_buff_len,
				 struct cfa_mpc_cache_axs_params *parms)
{
	struct cfa_mpc_write_cmd *cmd;
	struct cfa_mpc_cache_write_params *wr_parms = &parms->write;
	uint32_t cmd_size = sizeof(struct mpc_header) +
			    sizeof(struct cfa_mpc_write_cmd) +
			    parms->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE;

	if (parms->data_size < 1 || parms->data_size > 4) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (parms->tbl_type >= CFA_HW_TABLE_MAX) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (!parms->write.data_ptr) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	cmd = (struct cfa_mpc_write_cmd *)(cmd_buff +
					   sizeof(struct mpc_header));

	/* Populate CFA MPC command header */
	memset(cmd, 0, sizeof(struct cfa_mpc_write_cmd));
	cmd->opcode = WRITE_CMD_OPCODE_WRITE;
	cmd->table_type = parms->tbl_type;
	cmd->table_scope = parms->tbl_scope;
	cmd->data_size = parms->data_size;
	cmd->table_index = parms->tbl_index;
	switch (wr_parms->mode) {
	case CFA_MPC_WR_WRITE_THRU:
		cmd->cache_option = CACHE_WRITE_OPTION_WRITE_THRU;
		break;
	default:
	case CFA_MPC_WR_WRITE_BACK:
		cmd->cache_option = CACHE_WRITE_OPTION_WRITE_BACK;
		break;
	}

	/* Populate CFA MPC command payload following the header */
	memcpy(cmd + 1, wr_parms->data_ptr,
	       parms->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE);

	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose Invalidate message */
static int compose_mpc_evict_msg(uint8_t *cmd_buff, uint32_t *cmd_buff_len,
				 struct cfa_mpc_cache_axs_params *parms)
{
	struct cfa_mpc_invalidate_cmd *cmd;
	struct cfa_mpc_cache_evict_params *ev_parms = &parms->evict;
	uint32_t cmd_size = sizeof(struct mpc_header) +
			    sizeof(struct cfa_mpc_invalidate_cmd);

	if (parms->data_size < 1 || parms->data_size > 4) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (parms->tbl_type >= CFA_HW_TABLE_MAX) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	cmd = (struct cfa_mpc_invalidate_cmd *)(cmd_buff +
						sizeof(struct mpc_header));

	/* Populate CFA MPC command header */
	memset(cmd, 0, sizeof(struct cfa_mpc_invalidate_cmd));
	cmd->opcode = INVALIDATE_CMD_OPCODE_INVALIDATE;
	cmd->table_type = parms->tbl_type;
	cmd->table_scope = parms->tbl_scope;
	cmd->data_size = parms->data_size;
	cmd->table_index = parms->tbl_index;

	switch (ev_parms->mode) {
	case CFA_MPC_EV_EVICT_LINE:
		cmd->cache_option = CACHE_EVICT_OPTION_LINE;
		break;
	case CFA_MPC_EV_EVICT_CLEAN_LINES:
		cmd->cache_option = CACHE_EVICT_OPTION_CLEAN_LINES;
		break;
	case CFA_MPC_EV_EVICT_CLEAN_FAST_EVICT_LINES:
		cmd->cache_option = CACHE_EVICT_OPTION_CLEAN_FAST_LINES;
		break;
	case CFA_MPC_EV_EVICT_CLEAN_AND_CLEAN_FAST_EVICT_LINES:
		cmd->cache_option =
			CACHE_EVICT_OPTION_CLEAN_AND_CLEAN_FAST_EVICT_LINES;
		break;
	case CFA_MPC_EV_EVICT_TABLE_SCOPE:
		/* Not supported */
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	default:
	case CFA_MPC_EV_EVICT_SCOPE_ADDRESS:
		cmd->cache_option = CACHE_EVICT_OPTION_SCOPE_ADDRESS;
		break;
	}
	*cmd_buff_len = cmd_size;

	return 0;
}

/**
 * Build MPC CFA Cache access command
 *
 * @param [in] opc MPC opcode
 *
 * @param [out] cmd_buff Command data buffer to write the command to
 *
 * @param [in/out] cmd_buff_len Pointer to command buffer size param
 *        Set by caller to indicate the input cmd_buff size.
 *        Set to the actual size of the command generated by the api.
 *
 * @param [in] parms Pointer to MPC cache access command parameters
 *
 * @return 0 on Success, negative errno on failure
 */
int cfa_mpc_build_cache_axs_cmd(enum cfa_mpc_opcode opc, uint8_t *cmd_buff,
				uint32_t *cmd_buff_len,
				struct cfa_mpc_cache_axs_params *parms)
{
	int rc;
	if (!cmd_buff || !cmd_buff_len || *cmd_buff_len == 0 || !parms) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	rc = fill_mpc_header(cmd_buff, *cmd_buff_len, parms->opaque);
	if (rc)
		return rc;

	switch (opc) {
	case CFA_MPC_READ_CLR:
		return compose_mpc_read_clr_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_READ:
		return compose_mpc_read_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_WRITE:
		return compose_mpc_write_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_INVALIDATE:
		return compose_mpc_evict_msg(cmd_buff, cmd_buff_len, parms);
	default:
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}
}

/** Compose EM Search message */
static int compose_mpc_em_search_msg(uint8_t *cmd_buff, uint32_t *cmd_buff_len,
				     struct cfa_mpc_em_op_params *parms)
{
	struct cfa_mpc_em_search_cmd *cmd;
	struct cfa_mpc_em_search_params *e = &parms->search;
	uint32_t cmd_size = sizeof(struct mpc_header) +
			    sizeof(struct cfa_mpc_em_search_cmd) +
			    e->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE;

	if (e->data_size < 1 || e->data_size > 4) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (!e->em_entry) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	cmd = (struct cfa_mpc_em_search_cmd *)(cmd_buff +
					       sizeof(struct mpc_header));

	/* Populate CFA MPC command header */
	memset(cmd, 0, sizeof(struct cfa_mpc_em_search_cmd));
	cmd->opcode = EM_SEARCH_CMD_OPCODE_EM_SEARCH;
	cmd->table_scope = parms->tbl_scope;
	cmd->data_size = e->data_size;
	/* Default to normal read cache option for EM search */
	cmd->cache_option = CACHE_READ_OPTION_NORMAL;

	/* Populate CFA MPC command payload following the header */
	memcpy(cmd + 1, e->em_entry,
	       e->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE);

	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose EM Insert message */
static int compose_mpc_em_insert_msg(uint8_t *cmd_buff, uint32_t *cmd_buff_len,
				     struct cfa_mpc_em_op_params *parms)
{
	struct cfa_mpc_em_insert_cmd *cmd;
	struct cfa_mpc_em_insert_params *e = &parms->insert;
	uint32_t cmd_size = sizeof(struct mpc_header) +
			    sizeof(struct cfa_mpc_em_insert_cmd) +
			    e->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE;

	if (e->data_size < 1 || e->data_size > 4) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (!e->em_entry) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	cmd = (struct cfa_mpc_em_insert_cmd *)(cmd_buff +
					       sizeof(struct mpc_header));

	/* Populate CFA MPC command header */
	memset(cmd, 0, sizeof(struct cfa_mpc_em_insert_cmd));
	cmd->opcode = EM_INSERT_CMD_OPCODE_EM_INSERT;
	cmd->write_through = 1;
	cmd->table_scope = parms->tbl_scope;
	cmd->data_size = e->data_size;
	cmd->replace = e->replace;
	cmd->table_index = e->entry_idx;
	cmd->table_index2 = e->bucket_idx;
	/* Default to normal read cache option for EM insert */
	cmd->cache_option = CACHE_READ_OPTION_NORMAL;
	/* Default to write through cache write option for EM insert */
	cmd->cache_option2 = CACHE_WRITE_OPTION_WRITE_THRU;

	/* Populate CFA MPC command payload following the header */
	memcpy(cmd + 1, e->em_entry,
	       e->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE);

	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose EM Delete message */
static int compose_mpc_em_delete_msg(uint8_t *cmd_buff, uint32_t *cmd_buff_len,
				     struct cfa_mpc_em_op_params *parms)
{
	struct cfa_mpc_em_delete_cmd *cmd;
	struct cfa_mpc_em_delete_params *e = &parms->del;
	uint32_t cmd_size = sizeof(struct mpc_header) +
			    sizeof(struct cfa_mpc_em_delete_cmd);

	if (*cmd_buff_len < cmd_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Populate CFA MPC command header */
	cmd = (struct cfa_mpc_em_delete_cmd *)(cmd_buff +
					       sizeof(struct mpc_header));
	memset(cmd, 0, sizeof(struct cfa_mpc_em_delete_cmd));
	cmd->opcode = EM_DELETE_CMD_OPCODE_EM_DELETE;
	cmd->table_scope = parms->tbl_scope;
	cmd->table_index = e->entry_idx;
	cmd->table_index2 = e->bucket_idx;
	/* Default to normal read cache option for EM delete */
	cmd->cache_option = CACHE_READ_OPTION_NORMAL;
	/* Default to write through cache write option for EM delete */
	cmd->cache_option2 = CACHE_WRITE_OPTION_WRITE_THRU;

	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose EM Chain message */
static int compose_mpc_em_chain_msg(uint8_t *cmd_buff, uint32_t *cmd_buff_len,
				    struct cfa_mpc_em_op_params *parms)
{
	struct cfa_mpc_em_chain_cmd *cmd;
	struct cfa_mpc_em_chain_params *e = &parms->chain;
	uint32_t cmd_size =
		sizeof(struct mpc_header) + sizeof(struct cfa_mpc_em_chain_cmd);

	if (*cmd_buff_len < cmd_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	/* Populate CFA MPC command header */
	cmd = (struct cfa_mpc_em_chain_cmd *)(cmd_buff +
					      sizeof(struct mpc_header));
	memset(cmd, 0, sizeof(struct cfa_mpc_em_chain_cmd));
	cmd->opcode = EM_CHAIN_CMD_OPCODE_EM_CHAIN;
	cmd->table_scope = parms->tbl_scope;
	cmd->table_index = e->entry_idx;
	cmd->table_index2 = e->bucket_idx;
	/* Default to normal read cache option for EM delete */
	cmd->cache_option = CACHE_READ_OPTION_NORMAL;
	/* Default to write through cache write option for EM delete */
	cmd->cache_option2 = CACHE_WRITE_OPTION_WRITE_THRU;

	*cmd_buff_len = cmd_size;

	return 0;
}

/**
 * Build MPC CFA EM operation command
 *
 * @param [in] opc MPC EM opcode
 *
 * @param [in] cmd_buff Command data buffer to write the command to
 *
 * @param [in/out] cmd_buff_len Pointer to command buffer size param
 *        Set by caller to indicate the input cmd_buff size.
 *        Set to the actual size of the command generated by the api.
 *
 * @param [in] parms Pointer to MPC cache access command parameters
 *
 * @return 0 on Success, negative errno on failure
 */
int cfa_mpc_build_em_op_cmd(enum cfa_mpc_opcode opc, uint8_t *cmd_buff,
			    uint32_t *cmd_buff_len,
			    struct cfa_mpc_em_op_params *parms)
{
	int rc;
	if (!cmd_buff || !cmd_buff_len || *cmd_buff_len == 0 || !parms) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	rc = fill_mpc_header(cmd_buff, *cmd_buff_len, parms->opaque);
	if (rc)
		return rc;

	switch (opc) {
	case CFA_MPC_EM_SEARCH:
		return compose_mpc_em_search_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_EM_INSERT:
		return compose_mpc_em_insert_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_EM_DELETE:
		return compose_mpc_em_delete_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_EM_CHAIN:
		return compose_mpc_em_chain_msg(cmd_buff, cmd_buff_len, parms);
	default:
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}

	return 0;
}

/** Parse MPC read clear completion */
static int parse_mpc_read_clr_result(uint8_t *resp_buff, uint32_t resp_buff_len,
				     struct cfa_mpc_cache_axs_result *result)
{
	uint8_t *rd_data;
	uint32_t resp_size, rd_size;
	struct cfa_mpc_read_clr_cmp *cmp;

	/* Minimum data size = 1 32B unit */
	rd_size = MPC_CFA_CACHE_ACCESS_UNIT_SIZE;
	resp_size = sizeof(struct mpc_header) +
		    sizeof(struct cfa_mpc_read_clr_cmp) +
		    sizeof(struct mpc_cr_short_dma_data) + rd_size;
	cmp = (struct cfa_mpc_read_clr_cmp *)(resp_buff +
					      sizeof(struct mpc_header));

	if (resp_buff_len < resp_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (result->data_len < rd_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (!result->rd_data) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID(cmp->mp_client);

	result->status = cmp->status;
	result->error_data = cmp->hash_msb;
	result->opaque = cmp->opaque;

	/* No data to copy if there was an error, return early */
	if (cmp->status != READ_CLR_CMP_STATUS_OK)
		return 0;

	/* Copy the read data - starting at the end of the completion header including dma data */
	rd_data = resp_buff + sizeof(struct mpc_header) +
		  sizeof(struct cfa_mpc_read_clr_cmp) +
		  sizeof(struct mpc_cr_short_dma_data);
	memcpy(result->rd_data, rd_data, rd_size);

	return 0;
}

/** Parse MPC table read completion */
static int parse_mpc_read_result(uint8_t *resp_buff, uint32_t resp_buff_len,
				 struct cfa_mpc_cache_axs_result *result)
{
	uint8_t *rd_data;
	uint32_t resp_size, rd_size;
	struct cfa_mpc_read_cmp *cmp;

	/* Minimum data size = 1 32B unit */
	rd_size = MPC_CFA_CACHE_ACCESS_UNIT_SIZE;
	resp_size = sizeof(struct mpc_header) +
		    sizeof(struct cfa_mpc_read_cmp) +
		    sizeof(struct mpc_cr_short_dma_data) + rd_size;
	cmp = (struct cfa_mpc_read_cmp *)(resp_buff +
					  sizeof(struct mpc_header));

	if (resp_buff_len < resp_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (result->data_len < rd_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	if (!result->rd_data) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID(cmp->mp_client);

	result->status = cmp->status;
	result->error_data = cmp->hash_msb;
	result->opaque = cmp->opaque;

	/* No data to copy if there was an error, return early */
	if (cmp->status != READ_CMP_STATUS_OK)
		return 0;

	/* Copy max of 4 32B words that can fit into the return buffer */
	rd_size = MIN(4 * MPC_CFA_CACHE_ACCESS_UNIT_SIZE, result->data_len);

	/* Copy the read data - starting at the end of the completion header */
	rd_data = resp_buff + sizeof(struct mpc_header) +
		  sizeof(struct cfa_mpc_read_cmp) +
		  sizeof(struct mpc_cr_short_dma_data);
	memcpy(result->rd_data, rd_data, rd_size);

	return 0;
}

/** Parse MPC table write completion */
static int parse_mpc_write_result(uint8_t *resp_buff, uint32_t resp_buff_len,
				  struct cfa_mpc_cache_axs_result *result)
{
	uint32_t resp_size;
	struct cfa_mpc_write_cmp *cmp;

	resp_size =
		sizeof(struct mpc_header) + sizeof(struct cfa_mpc_write_cmp);
	cmp = (struct cfa_mpc_write_cmp *)(resp_buff +
					   sizeof(struct mpc_header));

	if (resp_buff_len < resp_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID(cmp->mp_client);

	result->status = cmp->status;
	result->error_data = cmp->hash_msb;
	result->opaque = cmp->opaque;
	return 0;
}

/** Parse MPC table evict completion */
static int parse_mpc_evict_result(uint8_t *resp_buff, uint32_t resp_buff_len,
				  struct cfa_mpc_cache_axs_result *result)
{
	uint32_t resp_size;
	struct cfa_mpc_invalidate_cmp *cmp;

	resp_size = sizeof(struct mpc_header) +
		    sizeof(struct cfa_mpc_invalidate_cmp);
	cmp = (struct cfa_mpc_invalidate_cmp *)(resp_buff +
						sizeof(struct mpc_header));

	if (resp_buff_len < resp_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID(cmp->mp_client);

	result->status = cmp->status;
	result->error_data = cmp->hash_msb;
	result->opaque = cmp->opaque;
	return 0;
}

/**
 * Parse MPC CFA Cache access command completion result
 *
 * @param [in] opc MPC cache access opcode
 *
 * @param [in] resp_buff Data buffer containing the response to parse
 *
 * @param [in] resp_buff_len Response buffer size
 *
 * @param [out] result Pointer to MPC cache access result object. This
 *        object will contain the fields parsed and extracted from the
 *        response buffer.
 *
 * @return 0 on Success, negative errno on failure
 */
int cfa_mpc_parse_cache_axs_resp(enum cfa_mpc_opcode opc, uint8_t *resp_buff,
				 uint32_t resp_buff_len,
				 struct cfa_mpc_cache_axs_result *result)
{
	if (!resp_buff || resp_buff_len == 0 || !result) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	switch (opc) {
	case CFA_MPC_READ_CLR:
		return parse_mpc_read_clr_result(resp_buff, resp_buff_len,
						 result);
	case CFA_MPC_READ:
		return parse_mpc_read_result(resp_buff, resp_buff_len, result);
	case CFA_MPC_WRITE:
		return parse_mpc_write_result(resp_buff, resp_buff_len, result);
	case CFA_MPC_INVALIDATE:
		return parse_mpc_evict_result(resp_buff, resp_buff_len, result);
	default:
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}
}

/** Parse MPC EM Search completion */
static int parse_mpc_em_search_result(uint8_t *resp_buff,
				      uint32_t resp_buff_len,
				      struct cfa_mpc_em_op_result *result)
{
	uint32_t resp_size;
	struct cfa_mpc_em_search_cmp *cmp;

	cmp = (struct cfa_mpc_em_search_cmp *)(resp_buff +
					       sizeof(struct mpc_header));
	resp_size = sizeof(struct mpc_header) +
		    sizeof(struct cfa_mpc_em_search_cmp);

	if (resp_buff_len < resp_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID(cmp->mp_client);

	result->status = cmp->status;
	result->error_data = cmp->status != CFA_MPC_OK ? cmp->hash_msb : 0;
	result->opaque = cmp->opaque;
	result->search.bucket_num = cmp->bkt_num;
	result->search.num_entries = cmp->num_entries;
	result->search.hash_msb = cmp->hash_msb;
	result->search.match_idx = cmp->table_index;
	result->search.bucket_idx = cmp->table_index2;

	return 0;
}

/** Parse MPC EM Insert completion */
static int parse_mpc_em_insert_result(uint8_t *resp_buff,
				      uint32_t resp_buff_len,
				      struct cfa_mpc_em_op_result *result)
{
	uint32_t resp_size;
	struct cfa_mpc_em_insert_cmp *cmp;

	cmp = (struct cfa_mpc_em_insert_cmp *)(resp_buff +
					       sizeof(struct mpc_header));
	resp_size = sizeof(struct mpc_header) +
		    sizeof(struct cfa_mpc_em_insert_cmp);

	if (resp_buff_len < resp_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID(cmp->mp_client);

	result->status = cmp->status;
	result->error_data = cmp->status != CFA_MPC_OK ? cmp->hash_msb : 0;
	result->opaque = cmp->opaque;
	result->insert.bucket_num = cmp->bkt_num;
	result->insert.num_entries = cmp->num_entries;
	result->insert.hash_msb = cmp->hash_msb;
	result->insert.match_idx = cmp->table_index4;
	result->insert.bucket_idx = cmp->table_index3;
	result->insert.replaced = cmp->replaced_entry;
	result->insert.chain_update = cmp->chain_upd;

	return 0;
}

/** Parse MPC EM Delete completion */
static int parse_mpc_em_delete_result(uint8_t *resp_buff,
				      uint32_t resp_buff_len,
				      struct cfa_mpc_em_op_result *result)
{
	uint32_t resp_size;
	struct cfa_mpc_em_delete_cmp *cmp;

	cmp = (struct cfa_mpc_em_delete_cmp *)(resp_buff +
					       sizeof(struct mpc_header));
	resp_size = sizeof(struct mpc_header) +
		    sizeof(struct cfa_mpc_em_delete_cmp);

	if (resp_buff_len < resp_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID(cmp->mp_client);

	result->status = cmp->status;
	result->error_data = cmp->hash_msb;
	result->opaque = cmp->opaque;
	result->del.bucket_num = cmp->bkt_num;
	result->del.num_entries = cmp->num_entries;
	result->del.prev_tail = cmp->table_index3;
	result->del.new_tail = cmp->table_index4;
	result->del.chain_update = cmp->chain_upd;

	return 0;
}

/** Parse MPC EM Chain completion */
static int parse_mpc_em_chain_result(uint8_t *resp_buff, uint32_t resp_buff_len,
				     struct cfa_mpc_em_op_result *result)
{
	uint32_t resp_size;
	struct cfa_mpc_em_chain_cmp *cmp;

	cmp = (struct cfa_mpc_em_chain_cmp *)(resp_buff +
					      sizeof(struct mpc_header));
	resp_size =
		sizeof(struct mpc_header) + sizeof(struct cfa_mpc_em_chain_cmp);

	if (resp_buff_len < resp_size) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID(cmp->mp_client);

	result->status = cmp->status;
	result->error_data = cmp->hash_msb;
	result->opaque = cmp->opaque;
	result->chain.bucket_num = cmp->bkt_num;
	result->chain.num_entries = cmp->num_entries;

	return 0;
}

/**
 * Parse MPC CFA EM operation command completion result
 *
 * @param [in] opc MPC cache access opcode
 *
 * @param [in] resp_buff Data buffer containing the response to parse
 *
 * @param [in] resp_buff_len Response buffer size
 *
 * @param [out] result Pointer to MPC EM operation result object. This
 *        object will contain the fields parsed and extracted from the
 *        response buffer.
 *
 * @return 0 on Success, negative errno on failure
 */
int cfa_mpc_parse_em_op_resp(enum cfa_mpc_opcode opc, uint8_t *resp_buff,
			     uint32_t resp_buff_len,
			     struct cfa_mpc_em_op_result *result)
{
	if (!resp_buff || resp_buff_len == 0 || !result) {
		LOG_RC(-EINVAL);
		return -EINVAL;
	}

	switch (opc) {
	case CFA_MPC_EM_SEARCH:
		return parse_mpc_em_search_result(resp_buff, resp_buff_len,
						  result);
	case CFA_MPC_EM_INSERT:
		return parse_mpc_em_insert_result(resp_buff, resp_buff_len,
						  result);
	case CFA_MPC_EM_DELETE:
		return parse_mpc_em_delete_result(resp_buff, resp_buff_len,
						  result);
	case CFA_MPC_EM_CHAIN:
		return parse_mpc_em_chain_result(resp_buff, resp_buff_len,
						 result);
	default:
		LOG_RC(-ENOTSUP);
		return -ENOTSUP;
	}
}
