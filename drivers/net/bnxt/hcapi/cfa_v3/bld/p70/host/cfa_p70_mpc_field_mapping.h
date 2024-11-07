/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_P70_MPC_FIELD_MAPPING_H_
#define _CFA_P70_MPC_FIELD_MAPPING_H_

/* clang-format off */
/** Device specific Field ID mapping structure */
struct field_mapping {
	bool valid;
	uint16_t mapping;
};

/**
 * Global to device field id mapping for READ_CMD
 */
struct field_mapping cfa_p70_mpc_read_cmd_gbl_to_dev
	[CFA_BLD_MPC_READ_CMD_MAX_FLD] = {
	[CFA_BLD_MPC_READ_CMD_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMD_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMD_TABLE_TYPE_FLD,
	},
	[CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMD_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMD_DATA_SIZE_FLD,
	},
	[CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMD_CACHE_OPTION_FLD,
	},
	[CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMD_TABLE_INDEX_FLD,
	},
	[CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMD_HOST_ADDRESS_FLD,
	},
};

/**
 * Global to device field id mapping for WRITE_CMD
 */
struct field_mapping cfa_p70_mpc_write_cmd_gbl_to_dev
	[CFA_BLD_MPC_WRITE_CMD_MAX_FLD] = {
	[CFA_BLD_MPC_WRITE_CMD_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMD_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMD_TABLE_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMD_TABLE_TYPE_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMD_WRITE_THROUGH_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMD_WRITE_THROUGH_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMD_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMD_DATA_SIZE_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMD_CACHE_OPTION_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMD_CACHE_OPTION_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMD_TABLE_INDEX_FLD,
	},
};

/**
 * Global to device field id mapping for READ_CLR_CMD
 */
struct field_mapping cfa_p70_mpc_read_clr_cmd_gbl_to_dev
	[CFA_BLD_MPC_READ_CLR_CMD_MAX_FLD] = {
	[CFA_BLD_MPC_READ_CLR_CMD_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMD_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMD_TABLE_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMD_TABLE_TYPE_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMD_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMD_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMD_DATA_SIZE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMD_DATA_SIZE_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMD_CACHE_OPTION_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMD_CACHE_OPTION_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMD_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMD_TABLE_INDEX_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMD_HOST_ADDRESS_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMD_HOST_ADDRESS_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMD_CLEAR_MASK_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMD_CLEAR_MASK_FLD,
	},
};

/**
 * Global to device field id mapping for INVALIDATE_CMD
 */
struct field_mapping cfa_p70_mpc_invalidate_cmd_gbl_to_dev
	[CFA_BLD_MPC_INVALIDATE_CMD_MAX_FLD] = {
	[CFA_BLD_MPC_INVALIDATE_CMD_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMD_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMD_TABLE_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMD_TABLE_TYPE_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMD_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMD_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMD_DATA_SIZE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMD_DATA_SIZE_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMD_CACHE_OPTION_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMD_CACHE_OPTION_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMD_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMD_TABLE_INDEX_FLD,
	},
};

/**
 * Global to device field id mapping for EM_SEARCH_CMD
 */
struct field_mapping cfa_p70_mpc_em_search_cmd_gbl_to_dev
	[CFA_BLD_MPC_EM_SEARCH_CMD_MAX_FLD] = {
	[CFA_BLD_MPC_EM_SEARCH_CMD_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMD_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMD_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMD_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMD_DATA_SIZE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMD_DATA_SIZE_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMD_CACHE_OPTION_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMD_CACHE_OPTION_FLD,
	},
};

/**
 * Global to device field id mapping for EM_INSERT_CMD
 */
struct field_mapping cfa_p70_mpc_em_insert_cmd_gbl_to_dev
	[CFA_BLD_MPC_EM_INSERT_CMD_MAX_FLD] = {
	[CFA_BLD_MPC_EM_INSERT_CMD_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMD_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMD_WRITE_THROUGH_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMD_WRITE_THROUGH_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMD_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMD_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMD_DATA_SIZE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMD_DATA_SIZE_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMD_CACHE_OPTION_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMD_CACHE_OPTION_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMD_TABLE_INDEX_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMD_CACHE_OPTION2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMD_CACHE_OPTION2_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMD_TABLE_INDEX2_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMD_REPLACE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMD_REPLACE_FLD,
	},
};

/**
 * Global to device field id mapping for EM_DELETE_CMD
 */
struct field_mapping cfa_p70_mpc_em_delete_cmd_gbl_to_dev
	[CFA_BLD_MPC_EM_DELETE_CMD_MAX_FLD] = {
	[CFA_BLD_MPC_EM_DELETE_CMD_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMD_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMD_WRITE_THROUGH_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMD_WRITE_THROUGH_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMD_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMD_CACHE_OPTION_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMD_CACHE_OPTION_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMD_TABLE_INDEX_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMD_CACHE_OPTION2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMD_CACHE_OPTION2_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMD_TABLE_INDEX2_FLD,
	},
};

/**
 * Global to device field id mapping for EM_CHAIN_CMD
 */
struct field_mapping cfa_p70_mpc_em_chain_cmd_gbl_to_dev
	[CFA_BLD_MPC_EM_CHAIN_CMD_MAX_FLD] = {
	[CFA_BLD_MPC_EM_CHAIN_CMD_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMD_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMD_WRITE_THROUGH_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMD_WRITE_THROUGH_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMD_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMD_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMD_CACHE_OPTION_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMD_CACHE_OPTION_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMD_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMD_TABLE_INDEX_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMD_CACHE_OPTION2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMD_CACHE_OPTION2_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMD_TABLE_INDEX2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMD_TABLE_INDEX2_FLD,
	},
};

/**
 * Global to device field id mapping for READ_CMP
 */
struct field_mapping cfa_p70_mpc_read_cmp_gbl_to_dev
	[CFA_BLD_MPC_READ_CMP_MAX_FLD] = {
	[CFA_BLD_MPC_READ_CMP_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_TYPE_FLD,
	},
	[CFA_BLD_MPC_READ_CMP_STATUS_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_STATUS_FLD,
	},
	[CFA_BLD_MPC_READ_CMP_MP_CLIENT_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_MP_CLIENT_FLD,
	},
	[CFA_BLD_MPC_READ_CMP_OPCODE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_OPCODE_FLD,
	},
	[CFA_BLD_MPC_READ_CMP_DMA_LENGTH_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_DMA_LENGTH_FLD,
	},
	[CFA_BLD_MPC_READ_CMP_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_READ_CMP_V_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_V_FLD,
	},
	[CFA_BLD_MPC_READ_CMP_HASH_MSB_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_HASH_MSB_FLD,
	},
	[CFA_BLD_MPC_READ_CMP_TABLE_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_TABLE_TYPE_FLD,
	},
	[CFA_BLD_MPC_READ_CMP_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_READ_CMP_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CMP_TABLE_INDEX_FLD,
	},
};

/**
 * Global to device field id mapping for WRITE_CMP
 */
struct field_mapping cfa_p70_mpc_write_cmp_gbl_to_dev
	[CFA_BLD_MPC_WRITE_CMP_MAX_FLD] = {
	[CFA_BLD_MPC_WRITE_CMP_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMP_TYPE_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMP_STATUS_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMP_MP_CLIENT_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMP_MP_CLIENT_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMP_OPCODE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMP_OPCODE_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMP_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMP_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMP_V_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMP_V_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMP_HASH_MSB_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMP_HASH_MSB_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMP_TABLE_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMP_TABLE_TYPE_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMP_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMP_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_WRITE_CMP_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_WRITE_CMP_TABLE_INDEX_FLD,
	},
};

/**
 * Global to device field id mapping for READ_CLR_CMP
 */
struct field_mapping cfa_p70_mpc_read_clr_cmp_gbl_to_dev
	[CFA_BLD_MPC_READ_CLR_CMP_MAX_FLD] = {
	[CFA_BLD_MPC_READ_CLR_CMP_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_TYPE_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_STATUS_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMP_MP_CLIENT_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_MP_CLIENT_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMP_OPCODE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_OPCODE_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMP_DMA_LENGTH_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_DMA_LENGTH_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMP_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMP_V_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_V_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMP_HASH_MSB_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_HASH_MSB_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMP_TABLE_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_TABLE_TYPE_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMP_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_READ_CLR_CMP_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_READ_CLR_CMP_TABLE_INDEX_FLD,
	},
};

/**
 * Global to device field id mapping for INVALIDATE_CMP
 */
struct field_mapping cfa_p70_mpc_invalidate_cmp_gbl_to_dev
	[CFA_BLD_MPC_INVALIDATE_CMP_MAX_FLD] = {
	[CFA_BLD_MPC_INVALIDATE_CMP_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMP_TYPE_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMP_STATUS_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMP_STATUS_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMP_MP_CLIENT_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMP_MP_CLIENT_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMP_OPCODE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMP_OPCODE_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMP_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMP_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMP_V_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMP_V_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMP_HASH_MSB_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMP_HASH_MSB_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMP_TABLE_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMP_TABLE_TYPE_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMP_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMP_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_INVALIDATE_CMP_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_INVALIDATE_CMP_TABLE_INDEX_FLD,
	},
};

/**
 * Global to device field id mapping for EM_SEARCH_CMP
 */
struct field_mapping cfa_p70_mpc_em_search_cmp_gbl_to_dev
	[CFA_BLD_MPC_EM_SEARCH_CMP_MAX_FLD] = {
	[CFA_BLD_MPC_EM_SEARCH_CMP_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_TYPE_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_STATUS_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_STATUS_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_MP_CLIENT_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_MP_CLIENT_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_OPCODE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_OPCODE_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_V1_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_V1_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_HASH_MSB_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_HASH_MSB_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_TABLE_INDEX_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_TABLE_INDEX2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_TABLE_INDEX2_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_V2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_V2_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_BKT_NUM_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_BKT_NUM_FLD,
	},
	[CFA_BLD_MPC_EM_SEARCH_CMP_NUM_ENTRIES_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_SEARCH_CMP_NUM_ENTRIES_FLD,
	},
};

/**
 * Global to device field id mapping for EM_INSERT_CMP
 */
struct field_mapping cfa_p70_mpc_em_insert_cmp_gbl_to_dev
	[CFA_BLD_MPC_EM_INSERT_CMP_MAX_FLD] = {
	[CFA_BLD_MPC_EM_INSERT_CMP_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_TYPE_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_STATUS_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_STATUS_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_MP_CLIENT_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_MP_CLIENT_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_OPCODE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_OPCODE_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_V1_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_V1_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_HASH_MSB_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_HASH_MSB_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_TABLE_INDEX_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_TABLE_INDEX2_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX3_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_TABLE_INDEX3_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_V2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_V2_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX4_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_TABLE_INDEX4_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_BKT_NUM_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_BKT_NUM_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_NUM_ENTRIES_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_NUM_ENTRIES_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_CHAIN_UPD_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_CHAIN_UPD_FLD,
	},
	[CFA_BLD_MPC_EM_INSERT_CMP_REPLACED_ENTRY_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_INSERT_CMP_REPLACED_ENTRY_FLD,
	},
};

/**
 * Global to device field id mapping for EM_DELETE_CMP
 */
struct field_mapping cfa_p70_mpc_em_delete_cmp_gbl_to_dev
	[CFA_BLD_MPC_EM_DELETE_CMP_MAX_FLD] = {
	[CFA_BLD_MPC_EM_DELETE_CMP_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_TYPE_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_STATUS_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_STATUS_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_MP_CLIENT_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_MP_CLIENT_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_OPCODE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_OPCODE_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_V1_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_V1_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_HASH_MSB_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_HASH_MSB_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_TABLE_INDEX_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_TABLE_INDEX2_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX3_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_TABLE_INDEX3_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_V2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_V2_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX4_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_TABLE_INDEX4_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_BKT_NUM_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_BKT_NUM_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_NUM_ENTRIES_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_NUM_ENTRIES_FLD,
	},
	[CFA_BLD_MPC_EM_DELETE_CMP_CHAIN_UPD_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_DELETE_CMP_CHAIN_UPD_FLD,
	},
};

/**
 * Global to device field id mapping for EM_CHAIN_CMP
 */
struct field_mapping cfa_p70_mpc_em_chain_cmp_gbl_to_dev
	[CFA_BLD_MPC_EM_CHAIN_CMP_MAX_FLD] = {
	[CFA_BLD_MPC_EM_CHAIN_CMP_TYPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_TYPE_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_STATUS_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_STATUS_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_MP_CLIENT_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_MP_CLIENT_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_OPCODE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_OPCODE_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_OPAQUE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_OPAQUE_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_V1_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_V1_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_HASH_MSB_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_HASH_MSB_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_SCOPE_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_TABLE_SCOPE_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_INDEX_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_TABLE_INDEX_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_INDEX2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_TABLE_INDEX2_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_INDEX3_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_TABLE_INDEX3_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_V2_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_V2_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_BKT_NUM_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_BKT_NUM_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_NUM_ENTRIES_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_NUM_ENTRIES_FLD,
	},
	[CFA_BLD_MPC_EM_CHAIN_CMP_CHAIN_UPD_FLD] = {
		.valid = true,
		.mapping = CFA_P70_MPC_EM_CHAIN_CMP_CHAIN_UPD_FLD,
	},
};

/* clang-format on */

#endif /* _CFA_P70_MPC_FIELD_MAPPING_H_ */
