/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_BLD_MPC_FIELD_IDS_H_
#define _CFA_BLD_MPC_FIELD_IDS_H_

/**
 * CFA Hardware Cache Table Type
 */
enum cfa_bld_mpc_hw_table_type {
	CFA_BLD_MPC_HW_TABLE_TYPE_ACTION, /**< CFA Action Record Table */
	CFA_BLD_MPC_HW_TABLE_TYPE_LOOKUP, /**< CFA EM Lookup Record Table */
	CFA_BLD_MPC_HW_TABLE_TYPE_MAX
};

/*
 * CFA MPC Cache access reading mode
 * To be used as a value for CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD
 */
enum cfa_bld_mpc_read_mode {
	CFA_BLD_MPC_RD_NORMAL, /**< Normal read mode */
	CFA_BLD_MPC_RD_EVICT,  /**< Read the cache and evict the cache line */
	CFA_BLD_MPC_RD_DEBUG_LINE,  /**< Debug read line mode */
	CFA_BLD_MPC_RD_DEBUG_TAG,   /**< Debug read tag mode */
	CFA_BLD_MPC_RD_MODE_MAX
};

/**
 * CFA MPC Cache access writing mode
 * To be used as a value for CFA_BLD_MPC_WRITE_CMD_CACHE_OPTION_FLD
 */
enum cfa_bld_mpc_write_mode {
	CFA_BLD_MPC_WR_WRITE_THRU, /**< Write to cache in Write through mode */
	CFA_BLD_MPC_WR_WRITE_BACK, /**< Write to cache in Write back mode */
	CFA_BLD_MPC_WR_MODE_MAX
};

/**
 * CFA MPC Cache access eviction mode
 * To be used as a value for CFA_BLD_MPC_INVALIDATE_CMD_CACHE_OPTION_FLD
 */
enum cfa_bld_mpc_evict_mode {
	/**
	 * Line evict: These modes evict a single cache line
	 * In these modes, the eviction occurs regardless of the cache line
	 * state (CLEAN/CLEAN_FAST_EVICT/DIRTY)
	 */
	/* Cache line addressed by set/way is evicted */
	CFA_BLD_MPC_EV_EVICT_LINE,
	/* Cache line hit with the table scope/address tuple is evicted */
	CFA_BLD_MPC_EV_EVICT_SCOPE_ADDRESS,

	/**
	 * Set Evict: These modes evict cache lines that meet certain criteria
	 * from the entire cache set.
	 */
	/*
	 * Cache lines only in CLEAN state are evicted from the set
	 * derived from the address
	 */
	CFA_BLD_MPC_EV_EVICT_CLEAN_LINES,
	/*
	 * Cache lines only in CLEAN_FAST_EVICT state are evicted from
	 * the set derived from the address
	 */
	CFA_BLD_MPC_EV_EVICT_CLEAN_FAST_EVICT_LINES,
	/*
	 * Cache lines in both CLEAN and CLEAN_FAST_EVICT states are
	 * evicted from the set derived from the address
	 */
	CFA_BLD_MPC_EV_EVICT_CLEAN_AND_CLEAN_FAST_EVICT_LINES,
	/*
	 * All Cache lines in the set identified by the address and
	 * belonging to the table scope are evicted.
	 */
	CFA_BLD_MPC_EV_EVICT_TABLE_SCOPE,
	CFA_BLD_MPC_EV_MODE_MAX,
};

/**
 * MPC CFA Command completion status
 */
enum cfa_bld_mpc_cmpl_status {
	/* Command success */
	CFA_BLD_MPC_OK,
	/* Unsupported CFA opcode */
	CFA_BLD_MPC_UNSPRT_ERR,
	/* CFA command format error */
	CFA_BLD_MPC_FMT_ERR,
	/* SVIF-Table Scope error */
	CFA_BLD_MPC_SCOPE_ERR,
	/* Address error: Only used if EM command or TABLE_TYPE=EM */
	CFA_BLD_MPC_ADDR_ERR,
	/* Cache operation error */
	CFA_BLD_MPC_CACHE_ERR,
	/* EM_SEARCH or EM_DELETE did not find a matching EM entry */
	CFA_BLD_MPC_EM_MISS,
	/* EM_INSERT found a matching EM entry and REPLACE=0 in the command */
	CFA_BLD_MPC_EM_DUPLICATE,
	/* EM_EVENT_COLLECTION_FAIL no events to return */
	CFA_BLD_MPC_EM_EVENT_COLLECTION_FAIL,
	/*
	 * EM_INSERT required a dynamic bucket to be added to the chain
	 * to successfully insert the EM entry, but the entry provided
	 * for use as dynamic bucket was invalid. (bucket_idx == 0)
	 */
	CFA_BLD_MPC_EM_ABORT,
};

/**
 * Field IDS for READ_CMD: This command reads 1-4 consecutive 32B words
 * from the specified address within a table scope.
 */
enum cfa_bld_mpc_read_cmd_fields {
	CFA_BLD_MPC_READ_CMD_OPAQUE_FLD = 0,
	/* This value selects the table type to be acted upon. */
	CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD = 1,
	/* Table scope to access. */
	CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD = 2,
	/*
	 * Number of 32B units in access. If value is outside the range [1, 4],
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD = 3,
	/*
	 * Test field for CFA MPC builder validation, added to introduce
	 * a hold in the field mapping array
	 */
	CFA_BLD_MPC_READ_CMD_RANDOM_TEST_FLD = 4,
	/*
	 * Determines setting of OPTION field for all cache requests while
	 * processing any command other than EM_INSERT, EM_DELETE, or EM_CHAIN.
	 * For these latter commands, CACHE_OPTION sets the OPTION field for all
	 * read requests, and CACHE_OPTION2 sets it for all write requests. CFA
	 * does not support posted write requests. Therefore, for WRITE
	 * commands, CACHE_OPTION[1] must be set to 0. And for EM commands that
	 * send write requests (all but EM_SEARCH), CACHE_OPTION2[1] must be set
	 * to 0.
	 */
	CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD = 5,
	/*
	 * A 32B index into the table identified by (TABLE_TYPE, TABLE_SCOPE):
	 */
	CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD = 6,
	/*
	 * The 64-bit host address to which to write the DMA data returned in
	 * the completion. The data will be written to the same function as the
	 * one that owns the SQ this command is read from. DATA_SIZE determines
	 * the maximum size of the data written. If HOST_ADDRESS[1:0] is not 0,
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD = 7,
	CFA_BLD_MPC_READ_CMD_MAX_FLD = 8,
};

/**
 * Field IDS for WRITE_CMD: This command writes 1-4 consecutive 32B
 * words to the specified address within a table scope.
 */
enum cfa_bld_mpc_write_cmd_fields {
	CFA_BLD_MPC_WRITE_CMD_OPAQUE_FLD = 0,
	/* This value selects the table type to be acted upon. */
	CFA_BLD_MPC_WRITE_CMD_TABLE_TYPE_FLD = 1,
	/*
	 * Sets the OPTION field on the cache interface to use write-through for
	 * EM entry writes while processing EM_INSERT commands. For all other
	 * cases (including EM_INSERT bucket writes), the OPTION field is set by
	 * the CACHE_OPTION and CACHE_OPTION2 fields.
	 */
	CFA_BLD_MPC_WRITE_CMD_WRITE_THROUGH_FLD = 2,
	/* Table scope to access. */
	CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD = 3,
	/*
	 * Number of 32B units in access. If value is outside the range [1, 4],
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD = 4,
	/*
	 * Determines setting of OPTION field for all cache requests while
	 * processing any command other than EM_INSERT, EM_DELETE, or EM_CHAIN.
	 * For these latter commands, CACHE_OPTION sets the OPTION field for all
	 * read requests, and CACHE_OPTION2 sets it for all write requests. CFA
	 * does not support posted write requests. Therefore, for WRITE
	 * commands, CACHE_OPTION[1] must be set to 0. And for EM commands that
	 * send write requests (all but EM_SEARCH), CACHE_OPTION2[1] must be set
	 * to 0.
	 */
	CFA_BLD_MPC_WRITE_CMD_CACHE_OPTION_FLD = 5,
	/*
	 * A 32B index into the table identified by (TABLE_TYPE, TABLE_SCOPE):
	 */
	CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD = 6,
	CFA_BLD_MPC_WRITE_CMD_MAX_FLD = 7,
};

/**
 * Field IDS for READ_CLR_CMD: This command performs a read-modify-write
 * to the specified 32B address using a 16b mask that specifies up to 16
 * 16b words to clear before writing the data back. It returns the 32B
 * data word read from cache (not the value written after the clear
 * operation).
 */
enum cfa_bld_mpc_read_clr_cmd_fields {
	CFA_BLD_MPC_READ_CLR_CMD_OPAQUE_FLD = 0,
	/* This value selects the table type to be acted upon. */
	CFA_BLD_MPC_READ_CLR_CMD_TABLE_TYPE_FLD = 1,
	/* Table scope to access. */
	CFA_BLD_MPC_READ_CLR_CMD_TABLE_SCOPE_FLD = 2,
	/*
	 * This field is no longer used. The READ_CLR command always reads (and
	 * does a mask-clear) on a single cache line. This field was added for
	 * SR2 A0 to avoid an ADDR_ERR when TABLE_INDEX=0 and TABLE_TYPE=EM (see
	 * CUMULUS-17872). That issue was fixed in SR2 B0.
	 */
	CFA_BLD_MPC_READ_CLR_CMD_DATA_SIZE_FLD = 3,
	/*
	 * Determines setting of OPTION field for all cache requests while
	 * processing any command other than EM_INSERT, EM_DELETE, or EM_CHAIN.
	 * For these latter commands, CACHE_OPTION sets the OPTION field for all
	 * read requests, and CACHE_OPTION2 sets it for all write requests. CFA
	 * does not support posted write requests. Therefore, for WRITE
	 * commands, CACHE_OPTION[1] must be set to 0. And for EM commands that
	 * send write requests (all but EM_SEARCH), CACHE_OPTION2[1] must be set
	 * to 0.
	 */
	CFA_BLD_MPC_READ_CLR_CMD_CACHE_OPTION_FLD = 4,
	/*
	 * A 32B index into the table identified by (TABLE_TYPE, TABLE_SCOPE):
	 */
	CFA_BLD_MPC_READ_CLR_CMD_TABLE_INDEX_FLD = 5,
	/*
	 * The 64-bit host address to which to write the DMA data returned in
	 * the completion. The data will be written to the same function as the
	 * one that owns the SQ this command is read from. DATA_SIZE determines
	 * the maximum size of the data written. If HOST_ADDRESS[1:0] is not 0,
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	CFA_BLD_MPC_READ_CLR_CMD_HOST_ADDRESS_FLD = 6,
	/*
	 * Specifies bits in 32B data word to clear. For x=0..15, when
	 * clear_mask[x]=1, data[x*16+15:x*16] is set to 0.
	 */
	CFA_BLD_MPC_READ_CLR_CMD_CLEAR_MASK_FLD = 7,
	CFA_BLD_MPC_READ_CLR_CMD_MAX_FLD = 8,
};

/**
 * Field IDS for INVALIDATE_CMD: This command forces an explicit evict
 * of 1-4 consecutive cache lines such that the next time the structure
 * is used it will be re-read from its backing store location.
 */
enum cfa_bld_mpc_invalidate_cmd_fields {
	CFA_BLD_MPC_INVALIDATE_CMD_OPAQUE_FLD = 0,
	/* This value selects the table type to be acted upon. */
	CFA_BLD_MPC_INVALIDATE_CMD_TABLE_TYPE_FLD = 1,
	/* Table scope to access. */
	CFA_BLD_MPC_INVALIDATE_CMD_TABLE_SCOPE_FLD = 2,
	/*
	 * This value identifies the number of cache lines to invalidate. A
	 * FMT_ERR is reported if the value is not in the range of [1, 4].
	 */
	CFA_BLD_MPC_INVALIDATE_CMD_DATA_SIZE_FLD = 3,
	/*
	 * Determines setting of OPTION field for all cache requests while
	 * processing any command other than EM_INSERT, EM_DELETE, or EM_CHAIN.
	 * For these latter commands, CACHE_OPTION sets the OPTION field for all
	 * read requests, and CACHE_OPTION2 sets it for all write requests. CFA
	 * does not support posted write requests. Therefore, for WRITE
	 * commands, CACHE_OPTION[1] must be set to 0. And for EM commands that
	 * send write requests (all but EM_SEARCH), CACHE_OPTION2[1] must be set
	 * to 0.
	 */
	CFA_BLD_MPC_INVALIDATE_CMD_CACHE_OPTION_FLD = 4,
	/*
	 * A 32B index into the table identified by (TABLE_TYPE, TABLE_SCOPE):
	 */
	CFA_BLD_MPC_INVALIDATE_CMD_TABLE_INDEX_FLD = 5,
	CFA_BLD_MPC_INVALIDATE_CMD_MAX_FLD = 6,
};

/**
 * Field IDS for EM_SEARCH_CMD: This command supplies an exact match
 * entry of 1-4 32B words to search for in the exact match table. CFA
 * first computes the hash value of the key in the entry, and determines
 * the static bucket address to search from the hash and the
 * (EM_BUCKETS, EM_SIZE) for TABLE_SCOPE. It then searches that static
 * bucket chain for an entry with a matching key (the LREC in the
 * command entry is ignored). If a matching entry is found, CFA reports
 * OK status in the completion. Otherwise, assuming no errors abort the
 * search before it completes, it reports EM_MISS status.
 */
enum cfa_bld_mpc_em_search_cmd_fields {
	CFA_BLD_MPC_EM_SEARCH_CMD_OPAQUE_FLD = 0,
	/* Table scope to access. */
	CFA_BLD_MPC_EM_SEARCH_CMD_TABLE_SCOPE_FLD = 1,
	/*
	 * Number of 32B units in access. If value is outside the range [1, 4],
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	CFA_BLD_MPC_EM_SEARCH_CMD_DATA_SIZE_FLD = 2,
	/*
	 * Determines setting of OPTION field for all cache requests while
	 * processing any command other than EM_INSERT, EM_DELETE, or EM_CHAIN.
	 * For these latter commands, CACHE_OPTION sets the OPTION field for all
	 * read requests, and CACHE_OPTION2 sets it for all write requests. CFA
	 * does not support posted write requests. Therefore, for WRITE
	 * commands, CACHE_OPTION[1] must be set to 0. And for EM commands that
	 * send write requests (all but EM_SEARCH), CACHE_OPTION2[1] must be set
	 * to 0.
	 */
	CFA_BLD_MPC_EM_SEARCH_CMD_CACHE_OPTION_FLD = 3,
	CFA_BLD_MPC_EM_SEARCH_CMD_MAX_FLD = 4,
};

/**
 * Field IDS for EM_INSERT_CMD: This command supplies an exact match
 * entry of 1-4 32B words to insert in the exact match table. CFA first
 * computes the hash value of the key in the entry, and determines the
 * static bucket address to search from the hash and the (EM_BUCKETS,
 * EM_SIZE) for TABLE_SCOPE. It then writes the 1-4 32B words of the
 * exact match entry starting at the TABLE_INDEX location in the
 * command. When the entry write completes, it searches the static
 * bucket chain for an existing entry with a key matching the key in the
 * insert entry (the LREC does not need to match). If a matching entry
 * is found: * If REPLACE=0, the CFA aborts the insert and returns
 * EM_DUPLICATE status. * If REPLACE=1, the CFA overwrites the matching
 * entry with the new entry. REPLACED_ENTRY=1 in the completion in this
 * case to signal that an entry was replaced. The location of the entry
 * is provided in the completion. If no match is found, CFA adds the new
 * entry to the lowest unused entry in the tail bucket. If the current
 * tail bucket is full, this requires adding a new bucket to the tail.
 * Then entry is then inserted at entry number 0. TABLE_INDEX2 provides
 * the address of the new tail bucket, if needed. If set to 0, the
 * insert is aborted and returns EM_ABORT status instead of adding a new
 * bucket to the tail. CHAIN_UPD in the completion indicates whether a
 * new bucket was added (1) or not (0). For locked scopes, if the read
 * of the static bucket gives a locked scope miss error, indicating that
 * the address is not in the cache, the static bucket is assumed empty.
 * In this case, TAI creates a new bucket, setting entry 0 to the new
 * entry fields and initializing all other fields to 0. It writes this
 * new bucket to the static bucket address, which installs it in the
 * cache.
 */
enum cfa_bld_mpc_em_insert_cmd_fields {
	CFA_BLD_MPC_EM_INSERT_CMD_OPAQUE_FLD = 0,
	/*
	 * Sets the OPTION field on the cache interface to use write-through for
	 * EM entry writes while processing EM_INSERT commands. For all other
	 * cases (including EM_INSERT bucket writes), the OPTION field is set by
	 * the CACHE_OPTION and CACHE_OPTION2 fields.
	 */
	CFA_BLD_MPC_EM_INSERT_CMD_WRITE_THROUGH_FLD = 1,
	/* Table scope to access. */
	CFA_BLD_MPC_EM_INSERT_CMD_TABLE_SCOPE_FLD = 2,
	/*
	 * Number of 32B units in access. If value is outside the range [1, 4],
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	CFA_BLD_MPC_EM_INSERT_CMD_DATA_SIZE_FLD = 3,
	/*
	 * Determines setting of OPTION field for all cache requests while
	 * processing any command other than EM_INSERT, EM_DELETE, or EM_CHAIN.
	 * For these latter commands, CACHE_OPTION sets the OPTION field for all
	 * read requests, and CACHE_OPTION2 sets it for all write requests. CFA
	 * does not support posted write requests. Therefore, for WRITE
	 * commands, CACHE_OPTION[1] must be set to 0. And for EM commands that
	 * send write requests (all but EM_SEARCH), CACHE_OPTION2[1] must be set
	 * to 0.
	 */
	CFA_BLD_MPC_EM_INSERT_CMD_CACHE_OPTION_FLD = 4,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. Starting
	 * address to write exact match entry being inserted.
	 */
	CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX_FLD = 5,
	/*
	 * Determines setting of OPTION field for all cache write requests for
	 * EM_INSERT, EM_DELETE, and EM_CHAIN commands. CFA does not support
	 * posted write requests. Therefore, CACHE_OPTION2[1] must be set to 0.
	 */
	CFA_BLD_MPC_EM_INSERT_CMD_CACHE_OPTION2_FLD = 6,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. Only used
	 * when no duplicate entry is found and the tail bucket in the chain
	 * searched has no unused entries. In this case, TABLE_INDEX2 provides
	 * the index to the 32B dynamic bucket to add to the tail of the chain
	 * (it is the new tail bucket). In this case, the CFA first writes
	 * TABLE_INDEX2 with a new bucket: * Entry 0 of the bucket sets the
	 * HASH_MSBS computed from the hash and ENTRY_PTR to TABLE_INDEX. *
	 * Entries 1-5 of the bucket set HASH_MSBS and ENTRY_PTR to 0. * CHAIN=0
	 * and CHAIN_PTR is set to CHAIN_PTR from to original tail bucket to
	 * maintain the background chaining. CFA then sets CHAIN=1 and
	 * CHAIN_PTR=TABLE_INDEX2 in the original tail bucket to link the new
	 * bucket to the chain. CHAIN_UPD=1 in the completion to signal that the
	 * new bucket at TABLE_INDEX2 was added to the tail of the chain.
	 */
	CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX2_FLD = 7,
	/*
	 * Only used if an entry is found whose key matches the exact match
	 * entry key in the command: * REPLACE=0: The insert is aborted and
	 * EM_DUPLICATE status is returned, signaling that the insert failed.
	 * The index of the matching entry that blocked the insertion is
	 * returned in the completion. * REPLACE=1: The matching entry is
	 * replaced with that from the command (ENTRY_PTR in the bucket is
	 * overwritten with TABLE_INDEX from the command). HASH_MSBS for the
	 * entry number never changes in this case since it had to match the new
	 * entry key HASH_MSBS to match. When an entry is replaced,
	 * REPLACED_ENTRY=1 in the completion and the index of the matching
	 * entry is returned in the completion so that software can de-allocate
	 * the entry.
	 */
	CFA_BLD_MPC_EM_INSERT_CMD_REPLACE_FLD = 8,
	CFA_BLD_MPC_EM_INSERT_CMD_MAX_FLD = 9,
};

/**
 * Field IDS for EM_DELETE_CMD: This command searches for an exact match
 * entry index in the static bucket chain and deletes it if found.
 * TABLE_INDEX give the entry index to delete and TABLE_INDEX2 gives the
 * static bucket index. If a matching entry is found: * If the matching
 * entry is the last valid entry in the tail bucket, its entry fields
 * (HASH_MSBS and ENTRY_PTR) are set to 0 to delete the entry. * If the
 * matching entry is not the last valid entry in the tail bucket, the
 * entry fields from that last entry are moved to the matching entry,
 * and the fields of that last entry are set to 0. * If any of the
 * previous processing results in the tail bucket not having any valid
 * entries, the tail bucket is the static bucket, the scope is a locked
 * scope, and CHAIN_PTR=0, hardware evicts the static bucket from the
 * cache and the completion signals this case with CHAIN_UPD=1. * If any
 * of the previous processing results in the tail bucket not having any
 * valid entries, and the tail bucket is not the static bucket, the tail
 * bucket is removed from the chain. In this case, the penultimate
 * bucket in the chain becomes the tail bucket. It has CHAIN set to 0 to
 * unlink the tail bucket, and CHAIN_PTR set to that from the original
 * tail bucket to preserve background chaining. The completion signals
 * this case with CHAIN_UPD=1 and returns the index to the bucket
 * removed so that software can de-allocate it. CFA returns OK status if
 * the entry was successfully deleted. Otherwise, it returns EM_MISS
 * status assuming there were no errors that caused processing to be
 * aborted.
 */
enum cfa_bld_mpc_em_delete_cmd_fields {
	CFA_BLD_MPC_EM_DELETE_CMD_OPAQUE_FLD = 0,
	/*
	 * Sets the OPTION field on the cache interface to use write-through for
	 * EM entry writes while processing EM_INSERT commands. For all other
	 * cases (including EM_INSERT bucket writes), the OPTION field is set by
	 * the CACHE_OPTION and CACHE_OPTION2 fields.
	 */
	CFA_BLD_MPC_EM_DELETE_CMD_WRITE_THROUGH_FLD = 1,
	/* Table scope to access. */
	CFA_BLD_MPC_EM_DELETE_CMD_TABLE_SCOPE_FLD = 2,
	/*
	 * Determines setting of OPTION field for all cache requests while
	 * processing any command other than EM_INSERT, EM_DELETE, or EM_CHAIN.
	 * For these latter commands, CACHE_OPTION sets the OPTION field for all
	 * read requests, and CACHE_OPTION2 sets it for all write requests. CFA
	 * does not support posted write requests. Therefore, for WRITE
	 * commands, CACHE_OPTION[1] must be set to 0. And for EM commands that
	 * send write requests (all but EM_SEARCH), CACHE_OPTION2[1] must be set
	 * to 0.
	 */
	CFA_BLD_MPC_EM_DELETE_CMD_CACHE_OPTION_FLD = 3,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. Entry index
	 * to delete.
	 */
	CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX_FLD = 4,
	/*
	 * Determines setting of OPTION field for all cache write requests for
	 * EM_INSERT, EM_DELETE, and EM_CHAIN commands. CFA does not support
	 * posted write requests. Therefore, CACHE_OPTION2[1] must be set to 0.
	 */
	CFA_BLD_MPC_EM_DELETE_CMD_CACHE_OPTION2_FLD = 5,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. Static
	 * bucket address for bucket chain.
	 */
	CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX2_FLD = 6,
	CFA_BLD_MPC_EM_DELETE_CMD_MAX_FLD = 7,
};

/**
 * Field IDS for EM_CHAIN_CMD: This command updates CHAIN_PTR in the
 * tail bucket of a static bucket chain, supplying both the static
 * bucket and the new CHAIN_PTR value. TABLE_INDEX is the new CHAIN_PTR
 * value and TABLE_INDEX2[23:0] is the static bucket. This command
 * provides software a means to update background chaining coherently
 * with other bucket updates. The value of CHAIN is unaffected (stays at
 * 0). For locked scopes, if the static bucket is the tail bucket, it is
 * empty (all of its ENTRY_PTR values are 0), and TABLE_INDEX=0 (the
 * CHAIN_PTR is being set to 0), instead of updating the static bucket
 * it is evicted from the cache. In this case, CHAIN_UPD=1 in the
 * completion.
 */
enum cfa_bld_mpc_em_chain_cmd_fields {
	CFA_BLD_MPC_EM_CHAIN_CMD_OPAQUE_FLD = 0,
	/*
	 * Sets the OPTION field on the cache interface to use write-through for
	 * EM entry writes while processing EM_INSERT commands. For all other
	 * cases (including EM_INSERT bucket writes), the OPTION field is set by
	 * the CACHE_OPTION and CACHE_OPTION2 fields.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMD_WRITE_THROUGH_FLD = 1,
	/* Table scope to access. */
	CFA_BLD_MPC_EM_CHAIN_CMD_TABLE_SCOPE_FLD = 2,
	/*
	 * Determines setting of OPTION field for all cache requests while
	 * processing any command other than EM_INSERT, EM_DELETE, or EM_CHAIN.
	 * For these latter commands, CACHE_OPTION sets the OPTION field for all
	 * read requests, and CACHE_OPTION2 sets it for all write requests. CFA
	 * does not support posted write requests. Therefore, for WRITE
	 * commands, CACHE_OPTION[1] must be set to 0. And for EM commands that
	 * send write requests (all but EM_SEARCH), CACHE_OPTION2[1] must be set
	 * to 0.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMD_CACHE_OPTION_FLD = 3,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. New
	 * CHAIN_PTR to write to tail bucket.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMD_TABLE_INDEX_FLD = 4,
	/*
	 * Determines setting of OPTION field for all cache write requests for
	 * EM_INSERT, EM_DELETE, and EM_CHAIN commands. CFA does not support
	 * posted write requests. Therefore, CACHE_OPTION2[1] must be set to 0.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMD_CACHE_OPTION2_FLD = 5,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. Static
	 * bucket address for bucket chain.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMD_TABLE_INDEX2_FLD = 6,
	CFA_BLD_MPC_EM_CHAIN_CMD_MAX_FLD = 7,
};

/**
 * Field IDS for READ_CMP: When no errors, teturns 1-4 consecutive 32B
 * words from the TABLE_INDEX within the TABLE_SCOPE specified in the
 * command, writing them to HOST_ADDRESS from the command.
 */
enum cfa_bld_mpc_read_cmp_fields {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	CFA_BLD_MPC_READ_CMP_TYPE_FLD = 0,
	/* The command processing status. */
	CFA_BLD_MPC_READ_CMP_STATUS_FLD = 1,
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	CFA_BLD_MPC_READ_CMP_MP_CLIENT_FLD = 2,
	/* OPCODE from the command. */
	CFA_BLD_MPC_READ_CMP_OPCODE_FLD = 3,
	/*
	 * The length of the DMA that accompanies the completion in units of
	 * DWORDs (32b). Valid values are [0, 128]. A value of zero indicates
	 * that there is no DMA that accompanies the completion.
	 */
	CFA_BLD_MPC_READ_CMP_DMA_LENGTH_FLD = 4,
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	CFA_BLD_MPC_READ_CMP_OPAQUE_FLD = 5,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_READ_CMP_V_FLD = 6,
	/*
	 * For EM_SEARCH and EM_INSERT commands without errors that abort the
	 * command processing prior to the hash computation, set to HASH[35:24]
	 * of the hash computed from the exact match entry key in the command.
	 * For all other cases, set to 0 except for the following error
	 * conditions, which carry debug information in this field as shown by
	 * error status below: * FMT_ERR: - Set to {7'd0, HOST_ADDRESS[1:0],
	 * DATA_SIZE[2:0]}. - If HOST_ADDRESS or DATA_SIZE field not present
	 * they are set to 0. * SCOPE_ERR: - Set to {1'b0, SVIF[10:0]}. *
	 * ADDR_ERR: - Only possible when TABLE_TYPE=EM or for EM* commands -
	 * Set to {1'b0, TABLE_INDEX[2:0], 5'd0, DATA_SIZE[2:0]} -
	 * TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error - TABLE_INDEX[1]=1 if
	 * TABLE_INDEX2 had an error - TABLE_INDEX[0]=1 if TABLE_INDEX had an
	 * error - TABLE_INDEX[n]=0 if the completion does not have the
	 * corresponding TABLE_INDEX field above. * CACHE_ERR: - Set to {9'd0,
	 * DATA_SIZE[2:0]}
	 */
	CFA_BLD_MPC_READ_CMP_HASH_MSB_FLD = 7,
	/* TABLE_TYPE from the command. */
	CFA_BLD_MPC_READ_CMP_TABLE_TYPE_FLD = 8,
	/* TABLE_SCOPE from the command. */
	CFA_BLD_MPC_READ_CMP_TABLE_SCOPE_FLD = 9,
	/* TABLE_INDEX from the command. */
	CFA_BLD_MPC_READ_CMP_TABLE_INDEX_FLD = 10,
	CFA_BLD_MPC_READ_CMP_MAX_FLD = 11,
};

/**
 * Field IDS for WRITE_CMP: Returns status of the write of 1-4
 * consecutive 32B words starting at TABLE_INDEX in the table specified
 * by (TABLE_TYPE, TABLE_SCOPE).
 */
enum cfa_bld_mpc_write_cmp_fields {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	CFA_BLD_MPC_WRITE_CMP_TYPE_FLD = 0,
	/* The command processing status. */
	CFA_BLD_MPC_WRITE_CMP_STATUS_FLD = 1,
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	CFA_BLD_MPC_WRITE_CMP_MP_CLIENT_FLD = 2,
	/* OPCODE from the command. */
	CFA_BLD_MPC_WRITE_CMP_OPCODE_FLD = 3,
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	CFA_BLD_MPC_WRITE_CMP_OPAQUE_FLD = 4,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_WRITE_CMP_V_FLD = 5,
	/*
	 * For EM_SEARCH and EM_INSERT commands without errors that abort the
	 * command processing prior to the hash computation, set to HASH[35:24]
	 * of the hash computed from the exact match entry key in the command.
	 * For all other cases, set to 0 except for the following error
	 * conditions, which carry debug information in this field as shown by
	 * error status below: * FMT_ERR: - Set to {7'd0, HOST_ADDRESS[1:0],
	 * DATA_SIZE[2:0]}. - If HOST_ADDRESS or DATA_SIZE field not present
	 * they are set to 0. * SCOPE_ERR: - Set to {1'b0, SVIF[10:0]}. *
	 * ADDR_ERR: - Only possible when TABLE_TYPE=EM or for EM* commands -
	 * Set to {1'b0, TABLE_INDEX[2:0], 5'd0, DATA_SIZE[2:0]} -
	 * TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error - TABLE_INDEX[1]=1 if
	 * TABLE_INDEX2 had an error - TABLE_INDEX[0]=1 if TABLE_INDEX had an
	 * error - TABLE_INDEX[n]=0 if the completion does not have the
	 * corresponding TABLE_INDEX field above. * CACHE_ERR: - Set to {9'd0,
	 * DATA_SIZE[2:0]}
	 */
	CFA_BLD_MPC_WRITE_CMP_HASH_MSB_FLD = 6,
	/* TABLE_TYPE from the command. */
	CFA_BLD_MPC_WRITE_CMP_TABLE_TYPE_FLD = 7,
	/* TABLE_SCOPE from the command. */
	CFA_BLD_MPC_WRITE_CMP_TABLE_SCOPE_FLD = 8,
	/* TABLE_INDEX from the command. */
	CFA_BLD_MPC_WRITE_CMP_TABLE_INDEX_FLD = 9,
	CFA_BLD_MPC_WRITE_CMP_MAX_FLD = 10,
};

/**
 * Field IDS for READ_CLR_CMP: When no errors, returns 1 32B word from
 * TABLE_INDEX in the table specified by (TABLE_TYPE, TABLE_SCOPE). The
 * data returned is the value prior to the clear.
 */
enum cfa_bld_mpc_read_clr_cmp_fields {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	CFA_BLD_MPC_READ_CLR_CMP_TYPE_FLD = 0,
	/* The command processing status. */
	CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD = 1,
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	CFA_BLD_MPC_READ_CLR_CMP_MP_CLIENT_FLD = 2,
	/* OPCODE from the command. */
	CFA_BLD_MPC_READ_CLR_CMP_OPCODE_FLD = 3,
	/*
	 * The length of the DMA that accompanies the completion in units of
	 * DWORDs (32b). Valid values are [0, 128]. A value of zero indicates
	 * that there is no DMA that accompanies the completion.
	 */
	CFA_BLD_MPC_READ_CLR_CMP_DMA_LENGTH_FLD = 4,
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	CFA_BLD_MPC_READ_CLR_CMP_OPAQUE_FLD = 5,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_READ_CLR_CMP_V_FLD = 6,
	/*
	 * For EM_SEARCH and EM_INSERT commands without errors that abort the
	 * command processing prior to the hash computation, set to HASH[35:24]
	 * of the hash computed from the exact match entry key in the command.
	 * For all other cases, set to 0 except for the following error
	 * conditions, which carry debug information in this field as shown by
	 * error status below: * FMT_ERR: - Set to {7'd0, HOST_ADDRESS[1:0],
	 * DATA_SIZE[2:0]}. - If HOST_ADDRESS or DATA_SIZE field not present
	 * they are set to 0. * SCOPE_ERR: - Set to {1'b0, SVIF[10:0]}. *
	 * ADDR_ERR: - Only possible when TABLE_TYPE=EM or for EM* commands -
	 * Set to {1'b0, TABLE_INDEX[2:0], 5'd0, DATA_SIZE[2:0]} -
	 * TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error - TABLE_INDEX[1]=1 if
	 * TABLE_INDEX2 had an error - TABLE_INDEX[0]=1 if TABLE_INDEX had an
	 * error - TABLE_INDEX[n]=0 if the completion does not have the
	 * corresponding TABLE_INDEX field above. * CACHE_ERR: - Set to {9'd0,
	 * DATA_SIZE[2:0]}
	 */
	CFA_BLD_MPC_READ_CLR_CMP_HASH_MSB_FLD = 7,
	/* TABLE_TYPE from the command. */
	CFA_BLD_MPC_READ_CLR_CMP_TABLE_TYPE_FLD = 8,
	/* TABLE_SCOPE from the command. */
	CFA_BLD_MPC_READ_CLR_CMP_TABLE_SCOPE_FLD = 9,
	/* TABLE_INDEX from the command. */
	CFA_BLD_MPC_READ_CLR_CMP_TABLE_INDEX_FLD = 10,
	CFA_BLD_MPC_READ_CLR_CMP_MAX_FLD = 11,
};

/**
 * Field IDS for INVALIDATE_CMP: Returns status for INVALIDATE commands.
 */
enum cfa_bld_mpc_invalidate_cmp_fields {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	CFA_BLD_MPC_INVALIDATE_CMP_TYPE_FLD = 0,
	/* The command processing status. */
	CFA_BLD_MPC_INVALIDATE_CMP_STATUS_FLD = 1,
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	CFA_BLD_MPC_INVALIDATE_CMP_MP_CLIENT_FLD = 2,
	/* OPCODE from the command. */
	CFA_BLD_MPC_INVALIDATE_CMP_OPCODE_FLD = 3,
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	CFA_BLD_MPC_INVALIDATE_CMP_OPAQUE_FLD = 4,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_INVALIDATE_CMP_V_FLD = 5,
	/*
	 * For EM_SEARCH and EM_INSERT commands without errors that abort the
	 * command processing prior to the hash computation, set to HASH[35:24]
	 * of the hash computed from the exact match entry key in the command.
	 * For all other cases, set to 0 except for the following error
	 * conditions, which carry debug information in this field as shown by
	 * error status below: * FMT_ERR: - Set to {7'd0, HOST_ADDRESS[1:0],
	 * DATA_SIZE[2:0]}. - If HOST_ADDRESS or DATA_SIZE field not present
	 * they are set to 0. * SCOPE_ERR: - Set to {1'b0, SVIF[10:0]}. *
	 * ADDR_ERR: - Only possible when TABLE_TYPE=EM or for EM* commands -
	 * Set to {1'b0, TABLE_INDEX[2:0], 5'd0, DATA_SIZE[2:0]} -
	 * TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error - TABLE_INDEX[1]=1 if
	 * TABLE_INDEX2 had an error - TABLE_INDEX[0]=1 if TABLE_INDEX had an
	 * error - TABLE_INDEX[n]=0 if the completion does not have the
	 * corresponding TABLE_INDEX field above. * CACHE_ERR: - Set to {9'd0,
	 * DATA_SIZE[2:0]}
	 */
	CFA_BLD_MPC_INVALIDATE_CMP_HASH_MSB_FLD = 6,
	/* TABLE_TYPE from the command. */
	CFA_BLD_MPC_INVALIDATE_CMP_TABLE_TYPE_FLD = 7,
	/* TABLE_SCOPE from the command. */
	CFA_BLD_MPC_INVALIDATE_CMP_TABLE_SCOPE_FLD = 8,
	/* TABLE_INDEX from the command. */
	CFA_BLD_MPC_INVALIDATE_CMP_TABLE_INDEX_FLD = 9,
	CFA_BLD_MPC_INVALIDATE_CMP_MAX_FLD = 10,
};

/**
 * Field IDS for EM_SEARCH_CMP: For OK status, returns the index of the
 * matching entry found for the EM key supplied in the command. Returns
 * EM_MISS status if no match was found.
 */
enum cfa_bld_mpc_em_search_cmp_fields {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	CFA_BLD_MPC_EM_SEARCH_CMP_TYPE_FLD = 0,
	/* The command processing status. */
	CFA_BLD_MPC_EM_SEARCH_CMP_STATUS_FLD = 1,
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	CFA_BLD_MPC_EM_SEARCH_CMP_MP_CLIENT_FLD = 2,
	/* OPCODE from the command. */
	CFA_BLD_MPC_EM_SEARCH_CMP_OPCODE_FLD = 3,
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	CFA_BLD_MPC_EM_SEARCH_CMP_OPAQUE_FLD = 4,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_EM_SEARCH_CMP_V1_FLD = 5,
	/*
	 * For EM_SEARCH and EM_INSERT commands without errors that abort the
	 * command processing prior to the hash computation, set to HASH[35:24]
	 * of the hash computed from the exact match entry key in the command.
	 * For all other cases, set to 0 except for the following error
	 * conditions, which carry debug information in this field as shown by
	 * error status below: * FMT_ERR: - Set to {7'd0, HOST_ADDRESS[1:0],
	 * DATA_SIZE[2:0]}. - If HOST_ADDRESS or DATA_SIZE field not present
	 * they are set to 0. * SCOPE_ERR: - Set to {1'b0, SVIF[10:0]}. *
	 * ADDR_ERR: - Only possible when TABLE_TYPE=EM or for EM* commands -
	 * Set to {1'b0, TABLE_INDEX[2:0], 5'd0, DATA_SIZE[2:0]} -
	 * TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error - TABLE_INDEX[1]=1 if
	 * TABLE_INDEX2 had an error - TABLE_INDEX[0]=1 if TABLE_INDEX had an
	 * error - TABLE_INDEX[n]=0 if the completion does not have the
	 * corresponding TABLE_INDEX field above. * CACHE_ERR: - Set to {9'd0,
	 * DATA_SIZE[2:0]}
	 */
	CFA_BLD_MPC_EM_SEARCH_CMP_HASH_MSB_FLD = 6,
	/* TABLE_SCOPE from the command. */
	CFA_BLD_MPC_EM_SEARCH_CMP_TABLE_SCOPE_FLD = 7,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. For OK
	 * status, gives ENTRY_PTR[25:0] of the matching entry found. Otherwise,
	 * set to 0.
	 */
	CFA_BLD_MPC_EM_SEARCH_CMP_TABLE_INDEX_FLD = 8,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. If the hash
	 * is computed (no errors during initial processing of the command),
	 * TABLE_INDEX2[23:0] is the static bucket address determined from the
	 * hash of the exact match entry key in the command and the (EM_SIZE,
	 * EM_BUCKETS) configuration for TABLE_SCOPE of the command. Bits 25:24
	 * in this case are set to 0. For any other status, it is always 0.
	 */
	CFA_BLD_MPC_EM_SEARCH_CMP_TABLE_INDEX2_FLD = 9,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_EM_SEARCH_CMP_V2_FLD = 10,
	/*
	 * BKT_NUM is the bucket number in chain of the tail bucket after
	 * finishing processing the command, except when the command stops
	 * processing before the tail bucket. NUM_ENTRIES is the number of valid
	 * entries in the BKT_NUM bucket. The following describes the cases
	 * where BKT_NUM and NUM_ENTRIES are not for the tail bucket after
	 * finishing processing of the command: * For UNSPRT_ERR, FMT_ERR,
	 * SCOPE_ERR, or ADDR_ERR completion status, BKT_NUM will be set to 0. *
	 * For CACHE_ERR completion status, BKT_NUM will be set to the bucket
	 * number that was last read without error. If ERR=1 in the response to
	 * the static bucket read, BKT_NUM and NUM_ENTRIES are set to 0. The
	 * static bucket is number 0, BKT_NUM increments for each new bucket in
	 * the chain, and saturates at 255. Therefore, if the value is 255,
	 * BKT_NUM may or may not be accurate. In this case, though, NUM_ENTRIES
	 * will still be the correct value as described above for the bucket.
	 */
	CFA_BLD_MPC_EM_SEARCH_CMP_BKT_NUM_FLD = 11,
	/* See BKT_NUM description. */
	CFA_BLD_MPC_EM_SEARCH_CMP_NUM_ENTRIES_FLD = 12,
	CFA_BLD_MPC_EM_SEARCH_CMP_MAX_FLD = 13,
};

/**
 * Field IDS for EM_INSERT_CMP: OK status indicates that the exact match
 * entry from the command was successfully inserted. EM_DUPLICATE status
 * indicates that the insert was aborted because an entry with the same
 * exact match key was found and REPLACE=0 in the command. EM_ABORT
 * status indicates that no duplicate was found, the tail bucket in the
 * chain was full, and TABLE_INDEX2=0. No changes are made to the
 * database in this case. TABLE_INDEX is the starting address at which
 * to insert the exact match entry (from the command). TABLE_INDEX2 is
 * the address at which to insert a new bucket at the tail of the static
 * bucket chain if needed (from the command). CHAIN_UPD=1 if a new
 * bucket was added at this address. TABLE_INDEX3 is the static bucket
 * address for the chain, determined from hashing the exact match entry.
 * Software needs this address and TABLE_INDEX in order to delete the
 * entry using an EM_DELETE command. TABLE_INDEX4 is the index of an
 * entry found that had a matching exact match key to the command entry
 * key. If no matching entry was found, it is set to 0. There are two
 * cases when there is a matching entry, depending on REPLACE from the
 * command: * REPLACE=0: EM_DUPLICATE status is reported and the insert
 * is aborted. Software can use the static bucket address
 * (TABLE_INDEX3[23:0]) and the matching entry (TABLE_INDEX4) in an
 * EM_DELETE command if it wishes to explicitly delete the matching
 * entry. * REPLACE=1: REPLACED_ENTRY=1 to signal that the entry at
 * TABLE_INDEX4 was replaced by the insert entry. REPLACED_ENTRY will
 * only be 1 if reporting OK status in this case. Software can de-
 * allocate the entry at TABLE_INDEX4.
 */
enum cfa_bld_mpc_em_insert_cmp_fields {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_TYPE_FLD = 0,
	/* The command processing status. */
	CFA_BLD_MPC_EM_INSERT_CMP_STATUS_FLD = 1,
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_MP_CLIENT_FLD = 2,
	/* OPCODE from the command. */
	CFA_BLD_MPC_EM_INSERT_CMP_OPCODE_FLD = 3,
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_OPAQUE_FLD = 4,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_V1_FLD = 5,
	/*
	 * For EM_SEARCH and EM_INSERT commands without errors that abort the
	 * command processing prior to the hash computation, set to HASH[35:24]
	 * of the hash computed from the exact match entry key in the command.
	 * For all other cases, set to 0 except for the following error
	 * conditions, which carry debug information in this field as shown by
	 * error status below: * FMT_ERR: - Set to {7'd0, HOST_ADDRESS[1:0],
	 * DATA_SIZE[2:0]}. - If HOST_ADDRESS or DATA_SIZE field not present
	 * they are set to 0. * SCOPE_ERR: - Set to {1'b0, SVIF[10:0]}. *
	 * ADDR_ERR: - Only possible when TABLE_TYPE=EM or for EM* commands -
	 * Set to {1'b0, TABLE_INDEX[2:0], 5'd0, DATA_SIZE[2:0]} -
	 * TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error - TABLE_INDEX[1]=1 if
	 * TABLE_INDEX2 had an error - TABLE_INDEX[0]=1 if TABLE_INDEX had an
	 * error - TABLE_INDEX[n]=0 if the completion does not have the
	 * corresponding TABLE_INDEX field above. * CACHE_ERR: - Set to {9'd0,
	 * DATA_SIZE[2:0]}
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_HASH_MSB_FLD = 6,
	/* TABLE_SCOPE from the command. */
	CFA_BLD_MPC_EM_INSERT_CMP_TABLE_SCOPE_FLD = 7,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX
	 * from the command, which is the starting address at which to insert
	 * the exact match entry.
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX_FLD = 8,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX2
	 * from the command, which is the index for the new tail bucket to add
	 * if needed (CHAIN_UPD=1 if it was used).
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX2_FLD = 9,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. If the hash
	 * is computed (no errors during initial processing of the command),
	 * TABLE_INDEX2[23:0] is the static bucket address determined from the
	 * hash of the exact match entry key in the command and the (EM_SIZE,
	 * EM_BUCKETS) configuration for TABLE_SCOPE of the command. Bits 25:24
	 * in this case are set to 0. For any other status, it is always 0.
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX3_FLD = 10,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_V2_FLD = 11,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. ENTRY_PTR of
	 * matching entry found. Set to 0 if no matching entry found. If
	 * REPLACED_ENTRY=1, that indicates a matching entry was found and
	 * REPLACE=1 in the command. In this case, the matching entry was
	 * replaced by the new entry in the command and this index can therefore
	 * by de-allocated.
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX4_FLD = 12,
	/*
	 * BKT_NUM is the bucket number in chain of the tail bucket after
	 * finishing processing the command, except when the command stops
	 * processing before the tail bucket. NUM_ENTRIES is the number of valid
	 * entries in the BKT_NUM bucket. The following describes the cases
	 * where BKT_NUM and NUM_ENTRIES are not for the tail bucket after
	 * finishing processing of the command: * For UNSPRT_ERR, FMT_ERR,
	 * SCOPE_ERR, or ADDR_ERR completion status, BKT_NUM will be set to 0. *
	 * For CACHE_ERR completion status, BKT_NUM will be set to the bucket
	 * number that was last read without error. If ERR=1 in the response to
	 * the static bucket read, BKT_NUM and NUM_ENTRIES are set to 0. The
	 * static bucket is number 0, BKT_NUM increments for each new bucket in
	 * the chain, and saturates at 255. Therefore, if the value is 255,
	 * BKT_NUM may or may not be accurate. In this case, though, NUM_ENTRIES
	 * will still be the correct value as described above for the bucket.
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_BKT_NUM_FLD = 13,
	/* See BKT_NUM description. */
	CFA_BLD_MPC_EM_INSERT_CMP_NUM_ENTRIES_FLD = 14,
	/*
	 * Specifies if the chain was updated while processing the command: Set
	 * to 1 when a new bucket is added to the tail of the static bucket
	 * chain at TABLE_INDEX2. This occurs if and only if the insert requires
	 * adding a new entry and the tail bucket is full. If set to 0,
	 * TABLE_INDEX2 was not used and is therefore still free.
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_CHAIN_UPD_FLD = 15,
	/*
	 * Set to 1 if a matching entry was found and REPLACE=1 in command. In
	 * the case, the entry starting at TABLE_INDEX4 was replaced and can
	 * therefore be de-allocated. Otherwise, this flag is set to 0.
	 */
	CFA_BLD_MPC_EM_INSERT_CMP_REPLACED_ENTRY_FLD = 16,
	CFA_BLD_MPC_EM_INSERT_CMP_MAX_FLD = 17,
};

/**
 * Field IDS for EM_DELETE_CMP: OK status indicates that an ENTRY_PTR
 * matching TABLE_INDEX was found in the static bucket chain specified
 * and was therefore deleted. EM_MISS status indicates that no match was
 * found. TABLE_INDEX is from the command. It is the index of the entry
 * to delete. TABLE_INDEX2 is from the command. It is the static bucket
 * address. TABLE_INDEX3 is the index of the tail bucket of the static
 * bucket chain prior to processing the command. TABLE_INDEX4 is the
 * index of the tail bucket of the static bucket chain after processing
 * the command. If CHAIN_UPD=1 and TABLE_INDEX4==TABLE_INDEX2, the
 * static bucket was the tail bucket, it became empty after the delete,
 * the scope is a locked scope, and CHAIN_PTR was 0. In this case, the
 * static bucket has been evicted from the cache. Otherwise, if
 * CHAIN_UPD=1, the original tail bucket given by TABLE_INDEX3 was
 * removed from the chain because it went empty. It can therefore be de-
 * allocated.
 */
enum cfa_bld_mpc_em_delete_cmp_fields {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_TYPE_FLD = 0,
	/* The command processing status. */
	CFA_BLD_MPC_EM_DELETE_CMP_STATUS_FLD = 1,
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_MP_CLIENT_FLD = 2,
	/* OPCODE from the command. */
	CFA_BLD_MPC_EM_DELETE_CMP_OPCODE_FLD = 3,
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_OPAQUE_FLD = 4,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_V1_FLD = 5,
	/*
	 * For EM_SEARCH and EM_INSERT commands without errors that abort the
	 * command processing prior to the hash computation, set to HASH[35:24]
	 * of the hash computed from the exact match entry key in the command.
	 * For all other cases, set to 0 except for the following error
	 * conditions, which carry debug information in this field as shown by
	 * error status below: * FMT_ERR: - Set to {7'd0, HOST_ADDRESS[1:0],
	 * DATA_SIZE[2:0]}. - If HOST_ADDRESS or DATA_SIZE field not present
	 * they are set to 0. * SCOPE_ERR: - Set to {1'b0, SVIF[10:0]}. *
	 * ADDR_ERR: - Only possible when TABLE_TYPE=EM or for EM* commands -
	 * Set to {1'b0, TABLE_INDEX[2:0], 5'd0, DATA_SIZE[2:0]} -
	 * TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error - TABLE_INDEX[1]=1 if
	 * TABLE_INDEX2 had an error - TABLE_INDEX[0]=1 if TABLE_INDEX had an
	 * error - TABLE_INDEX[n]=0 if the completion does not have the
	 * corresponding TABLE_INDEX field above. * CACHE_ERR: - Set to {9'd0,
	 * DATA_SIZE[2:0]}
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_HASH_MSB_FLD = 6,
	/* TABLE_SCOPE from the command. */
	CFA_BLD_MPC_EM_DELETE_CMP_TABLE_SCOPE_FLD = 7,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX
	 * from the command, which is the index of the entry to delete.
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX_FLD = 8,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX2
	 * from the command.
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX2_FLD = 9,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. For OK or
	 * EM_MISS status, the index of the tail bucket of the chain prior to
	 * processing the command. If CHAIN_UPD=1, the bucket was removed and
	 * this index can be de-allocated. For other status values, it is set to
	 * 0.
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX3_FLD = 10,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_V2_FLD = 11,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. For OK or
	 * EM_MISS status, the index of the tail bucket of the chain prior to
	 * after the command. If CHAIN_UPD=0 (always for EM_MISS status), it is
	 * always equal to TABLE_INDEX3 as the chain was not updated. For other
	 * status values, it is set to 0.
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX4_FLD = 12,
	/*
	 * BKT_NUM is the bucket number in chain of the tail bucket after
	 * finishing processing the command, except when the command stops
	 * processing before the tail bucket. NUM_ENTRIES is the number of valid
	 * entries in the BKT_NUM bucket. The following describes the cases
	 * where BKT_NUM and NUM_ENTRIES are not for the tail bucket after
	 * finishing processing of the command: * For UNSPRT_ERR, FMT_ERR,
	 * SCOPE_ERR, or ADDR_ERR completion status, BKT_NUM will be set to 0. *
	 * For CACHE_ERR completion status, BKT_NUM will be set to the bucket
	 * number that was last read without error. If ERR=1 in the response to
	 * the static bucket read, BKT_NUM and NUM_ENTRIES are set to 0. The
	 * static bucket is number 0, BKT_NUM increments for each new bucket in
	 * the chain, and saturates at 255. Therefore, if the value is 255,
	 * BKT_NUM may or may not be accurate. In this case, though, NUM_ENTRIES
	 * will still be the correct value as described above for the bucket.
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_BKT_NUM_FLD = 13,
	/* See BKT_NUM description. */
	CFA_BLD_MPC_EM_DELETE_CMP_NUM_ENTRIES_FLD = 14,
	/*
	 * Specifies if the chain was updated while processing the command: Set
	 * to 1 when a bucket is removed from the static bucket chain. This
	 * occurs if after the delete, the tail bucket is a dynamic bucket and
	 * no longer has any valid entries. In this case, software should de-
	 * allocate the dynamic bucket at TABLE_INDEX3. It is also set to 1 when
	 * the static bucket is evicted, which only occurs for locked scopes.
	 * See the EM_DELETE command description for details.
	 */
	CFA_BLD_MPC_EM_DELETE_CMP_CHAIN_UPD_FLD = 15,
	CFA_BLD_MPC_EM_DELETE_CMP_MAX_FLD = 16,
};

/**
 * Field IDS for EM_CHAIN_CMP: OK status indicates that the CHAIN_PTR of
 * the tail bucket was successfully updated. TABLE_INDEX is from the
 * command. It is the value of the new CHAIN_PTR. TABLE_INDEX2 is from
 * the command. TABLE_INDEX3 is the index of the tail bucket of the
 * static bucket chain.
 */
enum cfa_bld_mpc_em_chain_cmp_fields {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_TYPE_FLD = 0,
	/* The command processing status. */
	CFA_BLD_MPC_EM_CHAIN_CMP_STATUS_FLD = 1,
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_MP_CLIENT_FLD = 2,
	/* OPCODE from the command. */
	CFA_BLD_MPC_EM_CHAIN_CMP_OPCODE_FLD = 3,
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_OPAQUE_FLD = 4,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_V1_FLD = 5,
	/*
	 * For EM_SEARCH and EM_INSERT commands without errors that abort the
	 * command processing prior to the hash computation, set to HASH[35:24]
	 * of the hash computed from the exact match entry key in the command.
	 * For all other cases, set to 0 except for the following error
	 * conditions, which carry debug information in this field as shown by
	 * error status below: * FMT_ERR: - Set to {7'd0, HOST_ADDRESS[1:0],
	 * DATA_SIZE[2:0]}. - If HOST_ADDRESS or DATA_SIZE field not present
	 * they are set to 0. * SCOPE_ERR: - Set to {1'b0, SVIF[10:0]}. *
	 * ADDR_ERR: - Only possible when TABLE_TYPE=EM or for EM* commands -
	 * Set to {1'b0, TABLE_INDEX[2:0], 5'd0, DATA_SIZE[2:0]} -
	 * TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error - TABLE_INDEX[1]=1 if
	 * TABLE_INDEX2 had an error - TABLE_INDEX[0]=1 if TABLE_INDEX had an
	 * error - TABLE_INDEX[n]=0 if the completion does not have the
	 * corresponding TABLE_INDEX field above. * CACHE_ERR: - Set to {9'd0,
	 * DATA_SIZE[2:0]}
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_HASH_MSB_FLD = 6,
	/* TABLE_SCOPE from the command. */
	CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_SCOPE_FLD = 7,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX
	 * from the command, which is the new CHAIN_PTR for the tail bucket of
	 * the static bucket chain.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_INDEX_FLD = 8,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX2
	 * from the command.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_INDEX2_FLD = 9,
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. For OK
	 * status, the index of the tail bucket of the chain. Otherwise, set to
	 * 0.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_TABLE_INDEX3_FLD = 10,
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_V2_FLD = 11,
	/*
	 * BKT_NUM is the bucket number in chain of the tail bucket after
	 * finishing processing the command, except when the command stops
	 * processing before the tail bucket. NUM_ENTRIES is the number of valid
	 * entries in the BKT_NUM bucket. The following describes the cases
	 * where BKT_NUM and NUM_ENTRIES are not for the tail bucket after
	 * finishing processing of the command: * For UNSPRT_ERR, FMT_ERR,
	 * SCOPE_ERR, or ADDR_ERR completion status, BKT_NUM will be set to 0. *
	 * For CACHE_ERR completion status, BKT_NUM will be set to the bucket
	 * number that was last read without error. If ERR=1 in the response to
	 * the static bucket read, BKT_NUM and NUM_ENTRIES are set to 0. The
	 * static bucket is number 0, BKT_NUM increments for each new bucket in
	 * the chain, and saturates at 255. Therefore, if the value is 255,
	 * BKT_NUM may or may not be accurate. In this case, though, NUM_ENTRIES
	 * will still be the correct value as described above for the bucket.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_BKT_NUM_FLD = 12,
	/* See BKT_NUM description. */
	CFA_BLD_MPC_EM_CHAIN_CMP_NUM_ENTRIES_FLD = 13,
	/*
	 * Set to 1 when the scope is a locked scope, the tail bucket is the
	 * static bucket, the bucket is empty (all of its ENTRY_PTR values are
	 * 0), and TABLE_INDEX=0 in the command. In this case, the static bucket
	 * is evicted. For all other cases, it is set to 0.
	 */
	CFA_BLD_MPC_EM_CHAIN_CMP_CHAIN_UPD_FLD = 14,
	CFA_BLD_MPC_EM_CHAIN_CMP_MAX_FLD = 15,
};

#endif /* _CFA_BLD_MPC_FIELD_IDS_H_ */
