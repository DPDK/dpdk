/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_BLD_P70_MPC_H_
#define _CFA_BLD_P70_MPC_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * CFA Mid-Path Command (MPC) opcodes. The MPC CFA operations
 * are divided into 2 sub groups. Cache access operations
 * and EM update operations.
 */
enum cfa_mpc_opcode {
	/**
	 * MPC Cache access commands
	 */
	/* MPC Command to read Action/Lookup cache (up to 4 lines) */
	CFA_MPC_READ,
	/* MPC Command to write to Action/Lookup cache (up to 4 lines) */
	CFA_MPC_WRITE,
	/* MPC Cmd to Read and Clear Action/Lookup cache line (max 1 line) */
	CFA_MPC_READ_CLR,
	/* MPC Cmd to Invalidate Action/Lkup cache lines (up to 4 lines) */
	CFA_MPC_INVALIDATE,

	/**
	 * MPC EM update commands
	 */
	/**
	 * MPC Command to search for an EM entry by its key in the
	 * EM bucket chain
	 */
	CFA_MPC_EM_SEARCH,
	/* MPC command to insert a new EM entry to the EM bucket chain */
	CFA_MPC_EM_INSERT,
	/* MPC Command to delete an EM entry from the EM bucket chain */
	CFA_MPC_EM_DELETE,
	/* MPC Command to add an EM bucket to the tail of EM bucket chain */
	CFA_MPC_EM_CHAIN,
	CFA_MPC_OPC_MAX,
};

/**
 * CFA MPC Cache access reading mode
 */
enum cfa_mpc_read_mode {
	CFA_MPC_RD_NORMAL, /**< Normal read mode */
	CFA_MPC_RD_EVICT,  /**< Read the cache and evict the cache line */
	CFA_MPC_RD_DEBUG_LINE,  /**< Debug read mode line */
	CFA_MPC_RD_DEBUG_TAG,   /**< Debug read mode tag */
	CFA_MPC_RD_MODE_MAX
};

/**
 * CFA MPC Cache access writing mode
 */
enum cfa_mpc_write_mode {
	CFA_MPC_WR_WRITE_THRU, /**< Write to cache in Write through mode */
	CFA_MPC_WR_WRITE_BACK, /**< Write to cache in Write back mode */
	CFA_MPC_WR_MODE_MAX
};

/**
 * CFA MPC Cache access eviction mode
 */
enum cfa_mpc_evict_mode {
	/**
	 * Line evict: These modes evict a single cache line
	 * In these modes, the eviction occurs regardless of the cache line
	 * state (CLEAN/CLEAN_FAST_EVICT/DIRTY)
	 */
	/* Cache line addressed by set/way is evicted */
	CFA_MPC_EV_EVICT_LINE,
	/* Cache line hit with the table scope/address tuple is evicted */
	CFA_MPC_EV_EVICT_SCOPE_ADDRESS,

	/**
	 * Set Evict: These modes evict cache lines that meet certain criteria
	 * from the entire cache set.
	 */
	/*
	 * Cache lines only in CLEAN state are evicted from the set
	 * derived from the address
	 */
	CFA_MPC_EV_EVICT_CLEAN_LINES,
	/*
	 * Cache lines only in CLEAN_FAST_EVICT state are evicted from
	 * the set derived from the address
	 */
	CFA_MPC_EV_EVICT_CLEAN_FAST_EVICT_LINES,
	/*
	 * Cache lines in both CLEAN and CLEAN_FAST_EVICT states are
	 * evicted from the set derived from the address
	 */
	CFA_MPC_EV_EVICT_CLEAN_AND_CLEAN_FAST_EVICT_LINES,
	/*
	 * All Cache lines in the set identified by the address and
	 * belonging to the table scope are evicted.
	 */
	CFA_MPC_EV_EVICT_TABLE_SCOPE,
	CFA_MPC_EV_MODE_MAX,
};

/**
 * CFA Hardware Cache Table Type
 */
enum cfa_hw_table_type {
	CFA_HW_TABLE_ACTION, /**< CFA Action Record Table */
	CFA_HW_TABLE_LOOKUP, /**< CFA EM Lookup Record Table */
	CFA_HW_TABLE_MAX
};

/**
 * MPC Command parameters specific to Cache read operations
 */
struct cfa_mpc_cache_read_params {
	/* Specifies the cache option for reading the cache lines */
	enum cfa_mpc_read_mode mode;
	/**
	 * Clear mask to use for the Read-Clear operation
	 * Each bit in the mask correspond to 2 bytes in the
	 * cache line. Setting the corresponding mask bit, clears
	 * the corresponding data bytes in the cache line AFTER
	 * the read. This field is ignored for Read CMD.
	 */
	uint16_t clear_mask;
	/**
	 * External host memory address
	 *
	 * The 64-bit IOVA host address to which to write the DMA data returned
	 * in the completion. The data will be written to the same function as
	 * the one that owns the queue this command is read from.  Address must
	 * be 4 byte aligned.
	 */
	uint64_t host_address;
};

/**
 * MPC Command parameters specific to Cache write operation
 */
struct cfa_mpc_cache_write_params {
	/* Specifies the cache option for the write access */
	enum cfa_mpc_write_mode mode;
	/* Pointer to data to be written to cache */
	const uint8_t *data_ptr;
};

/**
 * MPC Command parameters specific to Cache evict/invalidate operation
 */
struct cfa_mpc_cache_evict_params {
	/* Specifies the cache option for Invalidation operation */
	enum cfa_mpc_evict_mode mode;
};

/**
 * MPC CFA Command parameters for cache related operations
 */
struct cfa_mpc_cache_axs_params {
	/** Common parameters for cache operations */
	/*
	 * Opaque value that will be returned in the MPC CFA
	 * Completion message. This can be used by the caller to associate
	 * completions with commands.
	 */
	uint32_t opaque;
	/*
	 * Table Scope to address the cache line. For Thor2
	 * the table scope goes for 0 - 31.
	 */
	uint8_t tbl_scope;
	/*
	 * Table Index to address the cache line. Note that
	 * this is the offset to the 32B record in the table
	 * scope backing store, expressed in 32B units.
	 */
	uint32_t tbl_index;
	/*
	 * Number of cache lines (32B word) in the access
	 * This should be set to 1 for READ-CLEAR command and between 1 and
	 * 4 for all other cache access commands (READ/WRITE/INVALIDATE)
	 */
	uint8_t data_size;
	/* CFA table type for which this Host IF hw operation is intended for */
	enum cfa_hw_table_type tbl_type;

	/* Cache operation specific params */
	union {
		/** Read and Read clear specific parameters */
		struct cfa_mpc_cache_read_params read;
		/** Cache write specific parameters */
		struct cfa_mpc_cache_write_params write;
		/** Cache invalidate operation specific parameters */
		struct cfa_mpc_cache_evict_params evict;
	};
};

/**
 * MPC CFA command parameters specific to EM insert operation
 */
struct cfa_mpc_em_insert_params {
	/*
	 * Pointer to the Exact Match entry to search. The
	 * EM Key in the entry is used to for the search
	 */
	const uint8_t *em_entry;
	/* Size of the EM entry in 32B words (1- 4) */
	uint8_t data_size;
	/* Flag to indicate if a matching entry (if found) should be replaced */
	bool replace;
	/* Table index to write the EM entry being inserted */
	uint32_t entry_idx;
	/*
	 * Table index to the EM record that can be used to
	 * create a new EM bucket, if the insertion results
	 * in a EM bucket chain's tail update.
	 */
	uint32_t bucket_idx;
};

/**
 * MPC CFA command parameters specific to EM search operation
 */
struct cfa_mpc_em_search_params {
	/*
	 * Pointer to the Exact Match entry to search. The
	 * EM Key in the entry is used to for the search
	 */
	uint8_t *em_entry;
	/* Size of the EM entry in 32B words (1- 4) */
	uint8_t data_size;
};

/**
 * MPC CFA command parameters specific to EM delete operation
 */
struct cfa_mpc_em_delete_params {
	/* Table index to the EM record to delete */
	uint32_t entry_idx;
	/*
	 * Table index to the static bucket for the EM bucket chain.
	 * As part of EM Delete processing, the hw walks the EM bucket
	 * chain to determine if the entry_idx is part of the chain.
	 * If the entry_idx is found to be a part of the chain, it is
	 * deleted from the chain and the EM bucket is repacked. If the
	 * tail of the bucket has only one valid entry, then the delete
	 * operation results in a tail update and one free EM entry
	 */
	uint32_t bucket_idx;
};

/**
 * MPC CFA command parameters specific to EM chain operation
 */
struct cfa_mpc_em_chain_params {
	/*
	 * Table index that will form the chain
	 * pointer to the tail bucket in the EM bucket chain
	 */
	uint32_t entry_idx;
	/*
	 * Table index to the static bucket for
	 * EM bucket chain to be updated.
	 */
	uint32_t bucket_idx;
};

/**
 * MPC CFA Command parameters for EM operations
 */
struct cfa_mpc_em_op_params {
	/** Common parameters for EM update operations */
	/*
	 * Opaque value that will be returned in the MPC CFA
	 * Completion message. This can be used by the caller to associate
	 * completions with commands.
	 */
	uint32_t opaque;
	/*
	 * Table Scope to address the cache line. For Thor2
	 * the table scope goes for 0 - 31.
	 */
	uint8_t tbl_scope;
	/** EM update operation specific params */
	union {
		/** EM Search operation params */
		struct cfa_mpc_em_search_params search;
		/** EM Insert operation params */
		struct cfa_mpc_em_insert_params insert;
		/** EM Delete operation params */
		struct cfa_mpc_em_delete_params del;
		/** EM Chain operation params */
		struct cfa_mpc_em_chain_params chain;
	};
};

/**
 * MPC CFA Command completion status
 */
enum cfa_mpc_cmpl_status {
	/* Command success */
	CFA_MPC_OK = 0,
	/* Unsupported CFA opcode */
	CFA_MPC_UNSPRT_ERR = 1,
	/* CFA command format error */
	CFA_MPC_FMT_ERR = 2,
	/* SVIF-Table Scope error */
	CFA_MPC_SCOPE_ERR = 3,
	/* Address error: Only used if EM command or TABLE_TYPE=EM */
	CFA_MPC_ADDR_ERR = 4,
	/* Cache operation error */
	CFA_MPC_CACHE_ERR = 5,
	/* EM_SEARCH or EM_DELETE did not find a matching EM entry */
	CFA_MPC_EM_MISS = 6,
	/* EM_INSERT found a matching EM entry and REPLACE=0 in the command */
	CFA_MPC_EM_DUPLICATE = 7,
	/* EM_EVENT_COLLECTION_FAIL no events to return */
	CFA_MPC_EM_EVENT_COLLECTION_FAIL = 8,
	/*
	 * EM_INSERT required a dynamic bucket to be added to the chain
	 * to successfully insert the EM entry, but the entry provided
	 * for use as dynamic bucket was invalid. (bucket_idx == 0)
	 */
	CFA_MPC_EM_ABORT = 9,
};

/**
 * MPC Cache access command completion result
 */
struct cfa_mpc_cache_axs_result {
	/*
	 * Opaque value returned in the completion message. This can
	 * be used by the caller to associate completions with commands.
	 */
	uint32_t opaque;
	/* MPC Command completion status code */
	enum cfa_mpc_cmpl_status status;
	/*
	 * Additional error information
	 * when status code is one of FMT, SCOPE, ADDR or CACHE error
	 */
	uint32_t error_data;
	/*
	 * Pointer to buffer to copy read data to.
	 * Needs to be valid for READ, READ-CLEAR operations
	 * Not set for write and evict operations
	 */
	uint8_t *rd_data;
	/*
	 * Size of the data buffer in Bytes. Should be at least
	 * be data_size * 32 for MPC cache reads
	 */
	uint16_t data_len;
};

/**
 * MPC EM search operation result
 */
struct cfa_mpc_em_search_result {
	uint32_t bucket_num;  /**< See CFA EAS */
	uint32_t num_entries; /**< See CFA EAS */
	/* Set to HASH[35:24] of the hash computed from the EM entry key. */
	uint32_t hash_msb;
	/*
	 * IF a match is found, this field is set
	 * to the table index of the matching EM entry
	 */
	uint32_t match_idx;
	/*
	 * Table index to the static bucket determined by hashing the EM entry
	 * key
	 */
	uint32_t bucket_idx;
};

/**
 * MPC EM insert operation result
 */
struct cfa_mpc_em_insert_result {
	uint32_t bucket_num;  /**< See CFA EAS */
	uint32_t num_entries; /**< See CFA EAS */
	/* Set to HASH[35:24] of the hash computed from the EM entry key. */
	uint32_t hash_msb;
	/*
	 * If replace = 1 and a matchng entry is found, this field is
	 * updated with the table index of the replaced entry. This table
	 * index is therefore free for use.
	 */
	uint32_t match_idx;
	/*
	 * Table index to the static bucket determined by hashing the EM entry
	 * key
	 */
	uint32_t bucket_idx;
	/* Flag: Matching entry was found and replace */
	uint8_t replaced:1;
	/* Flag: EM bucket chain was updated */
	uint8_t chain_update:1;
};

/**
 * MPC EM delete operation result
 */
struct cfa_mpc_em_delete_result {
	uint32_t bucket_num;  /**< See CFA EAS */
	uint32_t num_entries; /**< See CFA EAS */
	/*
	 * Table index to EM bucket tail BEFORE the delete command
	 * was processed with a OK or EM_MISS status. If chain update = 1, then
	 * this bucket can be freed
	 */
	uint32_t prev_tail;
	/*
	 * Table index to EM bucket tail AFTER the delete command
	 * was processed with a OK or EM_MISS status. Same as prev_tail
	 * if chain_update = 0.
	 */
	uint32_t new_tail;
	/* Flag: EM bucket chain was updated */
	uint8_t chain_update:1;
};

/**
 * MPC EM chain operation result
 */
struct cfa_mpc_em_chain_result {
	uint32_t bucket_num;  /**< See CFA EAS */
	uint32_t num_entries; /**< See CFA EAS */
};

/**
 * MPC EM operation completion result
 */
struct cfa_mpc_em_op_result {
	/*
	 * Opaque value returned in the completion message. This can
	 * be used by the caller to associate completions with commands.
	 */
	uint32_t opaque;
	/* MPC Command completion status code */
	enum cfa_mpc_cmpl_status status;
	/*
	 * Additional error information
	 * when status code is one of FMT, SCOPE, ADDR or CACHE error
	 */
	uint32_t error_data;
	union {
		/** EM Search specific results */
		struct cfa_mpc_em_search_result search;
		/** EM Insert specific results */
		struct cfa_mpc_em_insert_result insert;
		/** EM Delete specific results */
		struct cfa_mpc_em_delete_result del;
		/** EM Chain specific results */
		struct cfa_mpc_em_chain_result chain;
	};
};

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
				struct cfa_mpc_cache_axs_params *parms);

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
				 struct cfa_mpc_cache_axs_result *result);

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
			    struct cfa_mpc_em_op_params *parms);

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
			     struct cfa_mpc_em_op_result *result);

#endif /* _CFA_BLD_P70_MPC_H_ */
