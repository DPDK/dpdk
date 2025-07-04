/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_P70_MPC_STRUCTS_H_
#define _CFA_P70_MPC_STRUCTS_H_

/* clang-format off */

/**
 * READ_CMD: This command reads 1-4 consecutive 32B words from the
 * specified address within a table scope.
 */
struct cfa_mpc_read_cmd {
	/*
	 * This value selects the format for the mid-path command for the CFA.
	 */
	uint32_t opcode:8;
		#define READ_CMD_OPCODE_READ 0
	/* This value selects the table type to be acted upon. */
	uint32_t table_type:4;
		#define READ_CMD_TABLE_TYPE_ACTION 0
		#define READ_CMD_TABLE_TYPE_EM 1
	/* Unused field [4] */
	uint32_t unused0:4;
	/* Table scope to access. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused1:3;
	/*
	 * Number of 32B units in access. If value is outside the range [1, 4],
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	uint32_t data_size:3;
	/* Unused field [1] */
	uint32_t unused2:1;
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
	uint32_t cache_option:4;
	/*
	 * A 32B index into the table identified by (TABLE_TYPE, TABLE_SCOPE):
	 */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused3:6;
	/*
	 * The 64-bit host address to which to write the DMA data returned in
	 * the completion. The data will be written to the same function as the
	 * one that owns the SQ this command is read from. DATA_SIZE determines
	 * the maximum size of the data written. If HOST_ADDRESS[1:0] is not 0,
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	uint32_t host_address_1:32;
	uint32_t host_address_2:32;
};

/**
 * WRITE_CMD: This command writes 1-4 consecutive 32B words to the
 * specified address within a table scope.
 */
struct cfa_mpc_write_cmd {
	/*
	 * This value selects the format for the mid-path command for the CFA.
	 */
	uint32_t opcode:8;
		#define WRITE_CMD_OPCODE_WRITE 1
	/* This value selects the table type to be acted upon. */
	uint32_t table_type:4;
		#define WRITE_CMD_TABLE_TYPE_ACTION 0
		#define WRITE_CMD_TABLE_TYPE_EM 1
	/*
	 * Sets the OPTION field on the cache interface to use write-through for
	 * EM entry writes while processing EM_INSERT commands. For all other
	 * cases (including EM_INSERT bucket writes), the OPTION field is set by
	 * the CACHE_OPTION and CACHE_OPTION2 fields.
	 */
	uint32_t write_through:1;
	/* Unused field [3] */
	uint32_t unused0:3;
	/* Table scope to access. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused1:3;
	/*
	 * Number of 32B units in access. If value is outside the range [1, 4],
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	uint32_t data_size:3;
	/* Unused field [1] */
	uint32_t unused2:1;
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
	uint32_t cache_option:4;
	/*
	 * A 32B index into the table identified by (TABLE_TYPE, TABLE_SCOPE):
	 */
	uint32_t table_index:26;
	/* Unused field [70] */
	uint32_t unused3_1:6;
	uint32_t unused3_2:32;
	uint32_t unused3_3:32;
};

/**
 * READ_CLR_CMD: This command performs a read-modify-write to the
 * specified 32B address using a 16b mask that specifies up to 16 16b
 * words to clear before writing the data back. It returns the 32B data
 * word read from cache (not the value written after the clear
 * operation).
 */
struct cfa_mpc_read_clr_cmd {
	/*
	 * This value selects the format for the mid-path command for the CFA.
	 */
	uint32_t opcode:8;
		#define READ_CLR_CMD_OPCODE_READ_CLR 2
	/* This value selects the table type to be acted upon. */
	uint32_t table_type:4;
		#define READ_CLR_CMD_TABLE_TYPE_ACTION 0
		#define READ_CLR_CMD_TABLE_TYPE_EM 1
	/* Unused field [4] */
	uint32_t unused0:4;
	/* Table scope to access. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused1:3;
	/*
	 * This field is no longer used. The READ_CLR command always reads (and
	 * does a mask-clear) on a single cache line. This field was added for
	 * SR2 A0 to avoid an ADDR_ERR when TABLE_INDEX=0 and TABLE_TYPE=EM (see
	 * CUMULUS-17872). That issue was fixed in SR2 B0.
	 */
	uint32_t data_size:3;
	/* Unused field [1] */
	uint32_t unused2:1;
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
	uint32_t cache_option:4;
	/*
	 * A 32B index into the table identified by (TABLE_TYPE, TABLE_SCOPE):
	 */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused3:6;
	/*
	 * The 64-bit host address to which to write the DMA data returned in
	 * the completion. The data will be written to the same function as the
	 * one that owns the SQ this command is read from. DATA_SIZE determines
	 * the maximum size of the data written. If HOST_ADDRESS[1:0] is not 0,
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	uint32_t host_address_1:32;
	uint32_t host_address_2:32;
	/*
	 * Specifies bits in 32B data word to clear. For x=0..15, when
	 * clear_mask[x]=1, data[x*16+15:x*16] is set to 0.
	 */
	uint32_t clear_mask:16;
	/* Unused field [16] */
	uint32_t unused4:16;
};

/**
 * INVALIDATE_CMD: This command forces an explicit evict of 1-4
 * consecutive cache lines such that the next time the structure is used
 * it will be re-read from its backing store location.
 */
struct cfa_mpc_invalidate_cmd {
	/*
	 * This value selects the format for the mid-path command for the CFA.
	 */
	uint32_t opcode:8;
		#define INVALIDATE_CMD_OPCODE_INVALIDATE 5
	/* This value selects the table type to be acted upon. */
	uint32_t table_type:4;
		#define INVALIDATE_CMD_TABLE_TYPE_ACTION 0
		#define INVALIDATE_CMD_TABLE_TYPE_EM 1
	/* Unused field [4] */
	uint32_t unused0:4;
	/* Table scope to access. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused1:3;
	/*
	 * This value identifies the number of cache lines to invalidate. A
	 * FMT_ERR is reported if the value is not in the range of [1, 4].
	 */
	uint32_t data_size:3;
	/* Unused field [1] */
	uint32_t unused2:1;
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
	uint32_t cache_option:4;
	/*
	 * A 32B index into the table identified by (TABLE_TYPE, TABLE_SCOPE):
	 */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused3:6;
};

/**
 * EM_SEARCH_CMD: This command supplies an exact match entry of 1-4 32B
 * words to search for in the exact match table. CFA first computes the
 * hash value of the key in the entry, and determines the static bucket
 * address to search from the hash and the (EM_BUCKETS, EM_SIZE) for
 * TABLE_SCOPE. It then searches that static bucket chain for an entry
 * with a matching key (the LREC in the command entry is ignored). If a
 * matching entry is found, CFA reports OK status in the completion.
 * Otherwise, assuming no errors abort the search before it completes,
 * it reports EM_MISS status.
 */
struct cfa_mpc_em_search_cmd {
	/*
	 * This value selects the format for the mid-path command for the CFA.
	 */
	uint32_t opcode:8;
		#define EM_SEARCH_CMD_OPCODE_EM_SEARCH 8
	/* Unused field [8] */
	uint32_t unused0:8;
	/* Table scope to access. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused1:3;
	/*
	 * Number of 32B units in access. If value is outside the range [1, 4],
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	uint32_t data_size:3;
	/* Unused field [1] */
	uint32_t unused2:1;
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
	uint32_t cache_option:4;
	/* Unused field [96] */
	uint32_t unused3_1:32;
	uint32_t unused3_2:32;
	uint32_t unused3_3:32;
};

/**
 * EM_INSERT_CMD: This command supplies an exact match entry of 1-4 32B
 * words to insert in the exact match table. CFA first computes the hash
 * value of the key in the entry, and determines the static bucket
 * address to search from the hash and the (EM_BUCKETS, EM_SIZE) for
 * TABLE_SCOPE. It then writes the 1-4 32B words of the exact match
 * entry starting at the TABLE_INDEX location in the command. When the
 * entry write completes, it searches the static bucket chain for an
 * existing entry with a key matching the key in the insert entry (the
 * LREC does not need to match). If a matching entry is found: * If
 * REPLACE=0, the CFA aborts the insert and returns EM_DUPLICATE status.
 * * If REPLACE=1, the CFA overwrites the matching entry with the new
 * entry. REPLACED_ENTRY=1 in the completion in this case to signal that
 * an entry was replaced. The location of the entry is provided in the
 * completion. If no match is found, CFA adds the new entry to the
 * lowest unused entry in the tail bucket. If the current tail bucket is
 * full, this requires adding a new bucket to the tail. Then entry is
 * then inserted at entry number 0. TABLE_INDEX2 provides the address of
 * the new tail bucket, if needed. If set to 0, the insert is aborted
 * and returns EM_ABORT status instead of adding a new bucket to the
 * tail. CHAIN_UPD in the completion indicates whether a new bucket was
 * added (1) or not (0). For locked scopes, if the read of the static
 * bucket gives a locked scope miss error, indicating that the address
 * is not in the cache, the static bucket is assumed empty. In this
 * case, TAI creates a new bucket, setting entry 0 to the new entry
 * fields and initializing all other fields to 0. It writes this new
 * bucket to the static bucket address, which installs it in the cache.
 */
struct cfa_mpc_em_insert_cmd {
	/*
	 * This value selects the format for the mid-path command for the CFA.
	 */
	uint32_t opcode:8;
		#define EM_INSERT_CMD_OPCODE_EM_INSERT 9
	/* Unused field [4] */
	uint32_t unused0:4;
	/*
	 * Sets the OPTION field on the cache interface to use write-through for
	 * EM entry writes while processing EM_INSERT commands. For all other
	 * cases (including EM_INSERT bucket writes), the OPTION field is set by
	 * the CACHE_OPTION and CACHE_OPTION2 fields.
	 */
	uint32_t write_through:1;
	/* Unused field [3] */
	uint32_t unused1:3;
	/* Table scope to access. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused2:3;
	/*
	 * Number of 32B units in access. If value is outside the range [1, 4],
	 * CFA aborts processing and reports FMT_ERR status.
	 */
	uint32_t data_size:3;
	/* Unused field [1] */
	uint32_t unused3:1;
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
	uint32_t cache_option:4;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. Starting
	 * address to write exact match entry being inserted.
	 */
	uint32_t table_index:26;
	/* Unused field [2] */
	uint32_t unused4:2;
	/*
	 * Determines setting of OPTION field for all cache write requests for
	 * EM_INSERT, EM_DELETE, and EM_CHAIN commands. CFA does not support
	 * posted write requests. Therefore, CACHE_OPTION2[1] must be set to 0.
	 */
	uint32_t cache_option2:4;
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
	uint32_t table_index2:26;
	/* Unused field [5] */
	uint32_t unused5:5;
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
	uint32_t replace:1;
	/* Unused field [32] */
	uint32_t unused6:32;
};

/**
 * EM_DELETE_CMD: This command searches for an exact match entry index
 * in the static bucket chain and deletes it if found. TABLE_INDEX give
 * the entry index to delete and TABLE_INDEX2 gives the static bucket
 * index. If a matching entry is found: * If the matching entry is the
 * last valid entry in the tail bucket, its entry fields (HASH_MSBS and
 * ENTRY_PTR) are set to 0 to delete the entry. * If the matching entry
 * is not the last valid entry in the tail bucket, the entry fields from
 * that last entry are moved to the matching entry, and the fields of
 * that last entry are set to 0. * If any of the previous processing
 * results in the tail bucket not having any valid entries, the tail
 * bucket is the static bucket, the scope is a locked scope, and
 * CHAIN_PTR=0, hardware evicts the static bucket from the cache and the
 * completion signals this case with CHAIN_UPD=1. * If any of the
 * previous processing results in the tail bucket not having any valid
 * entries, and the tail bucket is not the static bucket, the tail
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
struct cfa_mpc_em_delete_cmd {
	/*
	 * This value selects the format for the mid-path command for the CFA.
	 */
	uint32_t opcode:8;
		#define EM_DELETE_CMD_OPCODE_EM_DELETE 10
	/* Unused field [4] */
	uint32_t unused0:4;
	/*
	 * Sets the OPTION field on the cache interface to use write-through for
	 * EM entry writes while processing EM_INSERT commands. For all other
	 * cases (including EM_INSERT bucket writes), the OPTION field is set by
	 * the CACHE_OPTION and CACHE_OPTION2 fields.
	 */
	uint32_t write_through:1;
	/* Unused field [3] */
	uint32_t unused1:3;
	/* Table scope to access. */
	uint32_t table_scope:5;
	/* Unused field [7] */
	uint32_t unused2:7;
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
	uint32_t cache_option:4;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. Entry index
	 * to delete.
	 */
	uint32_t table_index:26;
	/* Unused field [2] */
	uint32_t unused3:2;
	/*
	 * Determines setting of OPTION field for all cache write requests for
	 * EM_INSERT, EM_DELETE, and EM_CHAIN commands. CFA does not support
	 * posted write requests. Therefore, CACHE_OPTION2[1] must be set to 0.
	 */
	uint32_t cache_option2:4;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. Static
	 * bucket address for bucket chain.
	 */
	uint32_t table_index2:26;
	/* Unused field [6] */
	uint32_t unused4:6;
};

/**
 * EM_CHAIN_CMD: This command updates CHAIN_PTR in the tail bucket of a
 * static bucket chain, supplying both the static bucket and the new
 * CHAIN_PTR value. TABLE_INDEX is the new CHAIN_PTR value and
 * TABLE_INDEX2[23:0] is the static bucket. This command provides
 * software a means to update background chaining coherently with other
 * bucket updates. The value of CHAIN is unaffected (stays at 0). For
 * locked scopes, if the static bucket is the tail bucket, it is empty
 * (all of its ENTRY_PTR values are 0), and TABLE_INDEX=0 (the CHAIN_PTR
 * is being set to 0), instead of updating the static bucket it is
 * evicted from the cache. In this case, CHAIN_UPD=1 in the completion.
 */
struct cfa_mpc_em_chain_cmd {
	/*
	 * This value selects the format for the mid-path command for the CFA.
	 */
	uint32_t opcode:8;
		#define EM_CHAIN_CMD_OPCODE_EM_CHAIN 11
	/* Unused field [4] */
	uint32_t unused0:4;
	/*
	 * Sets the OPTION field on the cache interface to use write-through for
	 * EM entry writes while processing EM_INSERT commands. For all other
	 * cases (including EM_INSERT bucket writes), the OPTION field is set by
	 * the CACHE_OPTION and CACHE_OPTION2 fields.
	 */
	uint32_t write_through:1;
	/* Unused field [3] */
	uint32_t unused1:3;
	/* Table scope to access. */
	uint32_t table_scope:5;
	/* Unused field [7] */
	uint32_t unused2:7;
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
	uint32_t cache_option:4;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. New
	 * CHAIN_PTR to write to tail bucket.
	 */
	uint32_t table_index:26;
	/* Unused field [2] */
	uint32_t unused3:2;
	/*
	 * Determines setting of OPTION field for all cache write requests for
	 * EM_INSERT, EM_DELETE, and EM_CHAIN commands. CFA does not support
	 * posted write requests. Therefore, CACHE_OPTION2[1] must be set to 0.
	 */
	uint32_t cache_option2:4;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. Static
	 * bucket address for bucket chain.
	 */
	uint32_t table_index2:26;
	/* Unused field [6] */
	uint32_t unused4:6;
};

/**
 * READ_CMP: When no errors, teturns 1-4 consecutive 32B words from the
 * TABLE_INDEX within the TABLE_SCOPE specified in the command, writing
 * them to HOST_ADDRESS from the command.
 */
struct cfa_mpc_read_cmp {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	uint32_t type:6;
		#define READ_CMP_TYPE_MID_PATH_SHORT 30
	/* Unused field [2] */
	uint32_t unused0:2;
	/* The command processing status. */
	uint32_t status:4;
		#define READ_CMP_STATUS_OK 0
		#define READ_CMP_STATUS_UNSPRT_ERR 1
		#define READ_CMP_STATUS_FMT_ERR 2
		#define READ_CMP_STATUS_SCOPE_ERR 3
		#define READ_CMP_STATUS_ADDR_ERR 4
		#define READ_CMP_STATUS_CACHE_ERR 5
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	uint32_t mp_client:4;
		#define READ_CMP_MP_CLIENT_TE_CFA 2
		#define READ_CMP_MP_CLIENT_RE_CFA 3
	/* OPCODE from the command. */
	uint32_t opcode:8;
		#define READ_CMP_OPCODE_READ 0
	/*
	 * The length of the DMA that accompanies the completion in units of
	 * DWORDs (32b). Valid values are [0, 128]. A value of zero indicates
	 * that there is no DMA that accompanies the completion.
	 */
	uint32_t dma_length:8;
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	uint32_t opaque:32;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v:1;
	/* Unused field [3] */
	uint32_t unused1:3;
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
	uint32_t hash_msb:12;
	/* Unused field [4] */
	uint32_t unused2:4;
	/* TABLE_TYPE from the command. */
	uint32_t table_type:4;
		#define READ_CMP_TABLE_TYPE_ACTION 0
		#define READ_CMP_TABLE_TYPE_EM 1
	/* TABLE_SCOPE from the command. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused3:3;
	/* TABLE_INDEX from the command. */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused4:6;
};

/**
 * WRITE_CMP: Returns status of the write of 1-4 consecutive 32B words
 * starting at TABLE_INDEX in the table specified by (TABLE_TYPE,
 * TABLE_SCOPE).
 */
struct cfa_mpc_write_cmp {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	uint32_t type:6;
		#define WRITE_CMP_TYPE_MID_PATH_SHORT 30
	/* Unused field [2] */
	uint32_t unused0:2;
	/* The command processing status. */
	uint32_t status:4;
		#define WRITE_CMP_STATUS_OK 0
		#define WRITE_CMP_STATUS_UNSPRT_ERR 1
		#define WRITE_CMP_STATUS_FMT_ERR 2
		#define WRITE_CMP_STATUS_SCOPE_ERR 3
		#define WRITE_CMP_STATUS_ADDR_ERR 4
		#define WRITE_CMP_STATUS_CACHE_ERR 5
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	uint32_t mp_client:4;
		#define WRITE_CMP_MP_CLIENT_TE_CFA 2
		#define WRITE_CMP_MP_CLIENT_RE_CFA 3
	/* OPCODE from the command. */
	uint32_t opcode:8;
		#define WRITE_CMP_OPCODE_WRITE 1
	/* Unused field [8] */
	uint32_t unused1:8;
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	uint32_t opaque:32;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v:1;
	/* Unused field [3] */
	uint32_t unused2:3;
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
	uint32_t hash_msb:12;
	/* Unused field [4] */
	uint32_t unused3:4;
	/* TABLE_TYPE from the command. */
	uint32_t table_type:4;
		#define WRITE_CMP_TABLE_TYPE_ACTION 0
		#define WRITE_CMP_TABLE_TYPE_EM 1
	/* TABLE_SCOPE from the command. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused4:3;
	/* TABLE_INDEX from the command. */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused5:6;
};

/**
 * READ_CLR_CMP: When no errors, returns 1 32B word from TABLE_INDEX in
 * the table specified by (TABLE_TYPE, TABLE_SCOPE). The data returned
 * is the value prior to the clear.
 */
struct cfa_mpc_read_clr_cmp {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	uint32_t type:6;
		#define READ_CLR_CMP_TYPE_MID_PATH_SHORT 30
	/* Unused field [2] */
	uint32_t unused0:2;
	/* The command processing status. */
	uint32_t status:4;
		#define READ_CLR_CMP_STATUS_OK 0
		#define READ_CLR_CMP_STATUS_UNSPRT_ERR 1
		#define READ_CLR_CMP_STATUS_FMT_ERR 2
		#define READ_CLR_CMP_STATUS_SCOPE_ERR 3
		#define READ_CLR_CMP_STATUS_ADDR_ERR 4
		#define READ_CLR_CMP_STATUS_CACHE_ERR 5
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	uint32_t mp_client:4;
		#define READ_CLR_CMP_MP_CLIENT_TE_CFA 2
		#define READ_CLR_CMP_MP_CLIENT_RE_CFA 3
	/* OPCODE from the command. */
	uint32_t opcode:8;
		#define READ_CLR_CMP_OPCODE_READ_CLR 2
	/*
	 * The length of the DMA that accompanies the completion in units of
	 * DWORDs (32b). Valid values are [0, 128]. A value of zero indicates
	 * that there is no DMA that accompanies the completion.
	 */
	uint32_t dma_length:8;
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	uint32_t opaque:32;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v:1;
	/* Unused field [3] */
	uint32_t unused1:3;
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
	uint32_t hash_msb:12;
	/* Unused field [4] */
	uint32_t unused2:4;
	/* TABLE_TYPE from the command. */
	uint32_t table_type:4;
		#define READ_CLR_CMP_TABLE_TYPE_ACTION 0
		#define READ_CLR_CMP_TABLE_TYPE_EM 1
	/* TABLE_SCOPE from the command. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused3:3;
	/* TABLE_INDEX from the command. */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused4:6;
};

/**
 * INVALIDATE_CMP: Returns status for INVALIDATE commands.
 */
struct cfa_mpc_invalidate_cmp {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	uint32_t type:6;
		#define INVALIDATE_CMP_TYPE_MID_PATH_SHORT 30
	/* Unused field [2] */
	uint32_t unused0:2;
	/* The command processing status. */
	uint32_t status:4;
		#define INVALIDATE_CMP_STATUS_OK 0
		#define INVALIDATE_CMP_STATUS_UNSPRT_ERR 1
		#define INVALIDATE_CMP_STATUS_FMT_ERR 2
		#define INVALIDATE_CMP_STATUS_SCOPE_ERR 3
		#define INVALIDATE_CMP_STATUS_ADDR_ERR 4
		#define INVALIDATE_CMP_STATUS_CACHE_ERR 5
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	uint32_t mp_client:4;
		#define INVALIDATE_CMP_MP_CLIENT_TE_CFA 2
		#define INVALIDATE_CMP_MP_CLIENT_RE_CFA 3
	/* OPCODE from the command. */
	uint32_t opcode:8;
		#define INVALIDATE_CMP_OPCODE_INVALIDATE 5
	/* Unused field [8] */
	uint32_t unused1:8;
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	uint32_t opaque:32;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v:1;
	/* Unused field [3] */
	uint32_t unused2:3;
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
	uint32_t hash_msb:12;
	/* Unused field [4] */
	uint32_t unused3:4;
	/* TABLE_TYPE from the command. */
	uint32_t table_type:4;
		#define INVALIDATE_CMP_TABLE_TYPE_ACTION 0
		#define INVALIDATE_CMP_TABLE_TYPE_EM 1
	/* TABLE_SCOPE from the command. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused4:3;
	/* TABLE_INDEX from the command. */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused5:6;
};

/**
 * EM_SEARCH_CMP: For OK status, returns the index of the matching entry
 * found for the EM key supplied in the command. Returns EM_MISS status
 * if no match was found.
 */
struct cfa_mpc_em_search_cmp {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	uint32_t type:6;
		#define EM_SEARCH_CMP_TYPE_MID_PATH_LONG 31
	/* Unused field [2] */
	uint32_t unused0:2;
	/* The command processing status. */
	uint32_t status:4;
		#define EM_SEARCH_CMP_STATUS_OK 0
		#define EM_SEARCH_CMP_STATUS_UNSPRT_ERR 1
		#define EM_SEARCH_CMP_STATUS_FMT_ERR 2
		#define EM_SEARCH_CMP_STATUS_SCOPE_ERR 3
		#define EM_SEARCH_CMP_STATUS_ADDR_ERR 4
		#define EM_SEARCH_CMP_STATUS_CACHE_ERR 5
		#define EM_SEARCH_CMP_STATUS_EM_MISS 6
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	uint32_t mp_client:4;
		#define EM_SEARCH_CMP_MP_CLIENT_TE_CFA 2
		#define EM_SEARCH_CMP_MP_CLIENT_RE_CFA 3
	/* OPCODE from the command. */
	uint32_t opcode:8;
		#define EM_SEARCH_CMP_OPCODE_EM_SEARCH 8
	/* Unused field [8] */
	uint32_t unused1:8;
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	uint32_t opaque:32;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v1:1;
	/* Unused field [3] */
	uint32_t unused2:3;
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
	uint32_t hash_msb:12;
	/* Unused field [8] */
	uint32_t unused3:8;
	/* TABLE_SCOPE from the command. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused4:3;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. For OK
	 * status, gives ENTRY_PTR[25:0] of the matching entry found. Otherwise,
	 * set to 0.
	 */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused5:6;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. If the hash
	 * is computed (no errors during initial processing of the command),
	 * TABLE_INDEX2[23:0] is the static bucket address determined from the
	 * hash of the exact match entry key in the command and the (EM_SIZE,
	 * EM_BUCKETS) configuration for TABLE_SCOPE of the command. Bits 25:24
	 * in this case are set to 0. For any other status, it is always 0.
	 */
	uint32_t table_index2:26;
	/* Unused field [38] */
	uint32_t unused6_1:6;
	uint32_t unused6_2:32;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v2:1;
	/* Unused field [31] */
	uint32_t unused7:31;
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
	uint32_t bkt_num:8;
	/* See BKT_NUM description. */
	uint32_t num_entries:3;
	/* Unused field [21] */
	uint32_t unused8:21;
};

/**
 * EM_INSERT_CMP: OK status indicates that the exact match entry from
 * the command was successfully inserted. EM_DUPLICATE status indicates
 * that the insert was aborted because an entry with the same exact
 * match key was found and REPLACE=0 in the command. EM_ABORT status
 * indicates that no duplicate was found, the tail bucket in the chain
 * was full, and TABLE_INDEX2=0. No changes are made to the database in
 * this case. TABLE_INDEX is the starting address at which to insert the
 * exact match entry (from the command). TABLE_INDEX2 is the address at
 * which to insert a new bucket at the tail of the static bucket chain
 * if needed (from the command). CHAIN_UPD=1 if a new bucket was added
 * at this address. TABLE_INDEX3 is the static bucket address for the
 * chain, determined from hashing the exact match entry. Software needs
 * this address and TABLE_INDEX in order to delete the entry using an
 * EM_DELETE command. TABLE_INDEX4 is the index of an entry found that
 * had a matching exact match key to the command entry key. If no
 * matching entry was found, it is set to 0. There are two cases when
 * there is a matching entry, depending on REPLACE from the command: *
 * REPLACE=0: EM_DUPLICATE status is reported and the insert is aborted.
 * Software can use the static bucket address (TABLE_INDEX3[23:0]) and
 * the matching entry (TABLE_INDEX4) in an EM_DELETE command if it
 * wishes to explicitly delete the matching entry. * REPLACE=1:
 * REPLACED_ENTRY=1 to signal that the entry at TABLE_INDEX4 was
 * replaced by the insert entry. REPLACED_ENTRY will only be 1 if
 * reporting OK status in this case. Software can de-allocate the entry
 * at TABLE_INDEX4.
 */
struct cfa_mpc_em_insert_cmp {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	uint32_t type:6;
		#define EM_INSERT_CMP_TYPE_MID_PATH_LONG 31
	/* Unused field [2] */
	uint32_t unused0:2;
	/* The command processing status. */
	uint32_t status:4;
		#define EM_INSERT_CMP_STATUS_OK 0
		#define EM_INSERT_CMP_STATUS_UNSPRT_ERR 1
		#define EM_INSERT_CMP_STATUS_FMT_ERR 2
		#define EM_INSERT_CMP_STATUS_SCOPE_ERR 3
		#define EM_INSERT_CMP_STATUS_ADDR_ERR 4
		#define EM_INSERT_CMP_STATUS_CACHE_ERR 5
		#define EM_INSERT_CMP_STATUS_EM_DUPLICATE 7
		#define EM_INSERT_CMP_STATUS_EM_ABORT 9
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	uint32_t mp_client:4;
		#define EM_INSERT_CMP_MP_CLIENT_TE_CFA 2
		#define EM_INSERT_CMP_MP_CLIENT_RE_CFA 3
	/* OPCODE from the command. */
	uint32_t opcode:8;
		#define EM_INSERT_CMP_OPCODE_EM_INSERT 9
	/* Unused field [8] */
	uint32_t unused1:8;
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	uint32_t opaque:32;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v1:1;
	/* Unused field [3] */
	uint32_t unused2:3;
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
	uint32_t hash_msb:12;
	/* Unused field [8] */
	uint32_t unused3:8;
	/* TABLE_SCOPE from the command. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused4:3;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX
	 * from the command, which is the starting address at which to insert
	 * the exact match entry.
	 */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused5:6;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX2
	 * from the command, which is the index for the new tail bucket to add
	 * if needed (CHAIN_UPD=1 if it was used).
	 */
	uint32_t table_index2:26;
	/* Unused field [6] */
	uint32_t unused6:6;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. If the hash
	 * is computed (no errors during initial processing of the command),
	 * TABLE_INDEX2[23:0] is the static bucket address determined from the
	 * hash of the exact match entry key in the command and the (EM_SIZE,
	 * EM_BUCKETS) configuration for TABLE_SCOPE of the command. Bits 25:24
	 * in this case are set to 0. For any other status, it is always 0.
	 */
	uint32_t table_index3:26;
	/* Unused field [6] */
	uint32_t unused7:6;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v2:1;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. ENTRY_PTR of
	 * matching entry found. Set to 0 if no matching entry found. If
	 * REPLACED_ENTRY=1, that indicates a matching entry was found and
	 * REPLACE=1 in the command. In this case, the matching entry was
	 * replaced by the new entry in the command and this index can therefore
	 * by de-allocated.
	 */
	uint32_t table_index4:26;
	/* Unused field [5] */
	uint32_t unused8:5;
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
	uint32_t bkt_num:8;
	/* See BKT_NUM description. */
	uint32_t num_entries:3;
	/*
	 * Specifies if the chain was updated while processing the command: Set
	 * to 1 when a new bucket is added to the tail of the static bucket
	 * chain at TABLE_INDEX2. This occurs if and only if the insert requires
	 * adding a new entry and the tail bucket is full. If set to 0,
	 * TABLE_INDEX2 was not used and is therefore still free.
	 */
	uint32_t chain_upd:1;
	/*
	 * Set to 1 if a matching entry was found and REPLACE=1 in command. In
	 * the case, the entry starting at TABLE_INDEX4 was replaced and can
	 * therefore be de-allocated. Otherwise, this flag is set to 0.
	 */
	uint32_t replaced_entry:1;
	/* Unused field [19] */
	uint32_t unused9:19;
};

/**
 * EM_DELETE_CMP: OK status indicates that an ENTRY_PTR matching
 * TABLE_INDEX was found in the static bucket chain specified and was
 * therefore deleted. EM_MISS status indicates that no match was found.
 * TABLE_INDEX is from the command. It is the index of the entry to
 * delete. TABLE_INDEX2 is from the command. It is the static bucket
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
struct cfa_mpc_em_delete_cmp {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	uint32_t type:6;
		#define EM_DELETE_CMP_TYPE_MID_PATH_LONG 31
	/* Unused field [2] */
	uint32_t unused0:2;
	/* The command processing status. */
	uint32_t status:4;
		#define EM_DELETE_CMP_STATUS_OK 0
		#define EM_DELETE_CMP_STATUS_UNSPRT_ERR 1
		#define EM_DELETE_CMP_STATUS_FMT_ERR 2
		#define EM_DELETE_CMP_STATUS_SCOPE_ERR 3
		#define EM_DELETE_CMP_STATUS_ADDR_ERR 4
		#define EM_DELETE_CMP_STATUS_CACHE_ERR 5
		#define EM_DELETE_CMP_STATUS_EM_MISS 6
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	uint32_t mp_client:4;
		#define EM_DELETE_CMP_MP_CLIENT_TE_CFA 2
		#define EM_DELETE_CMP_MP_CLIENT_RE_CFA 3
	/* OPCODE from the command. */
	uint32_t opcode:8;
		#define EM_DELETE_CMP_OPCODE_EM_DELETE 10
	/* Unused field [8] */
	uint32_t unused1:8;
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	uint32_t opaque:32;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v1:1;
	/* Unused field [3] */
	uint32_t unused2:3;
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
	uint32_t hash_msb:12;
	/* Unused field [8] */
	uint32_t unused3:8;
	/* TABLE_SCOPE from the command. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused4:3;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX
	 * from the command, which is the index of the entry to delete.
	 */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused5:6;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX2
	 * from the command.
	 */
	uint32_t table_index2:26;
	/* Unused field [6] */
	uint32_t unused6:6;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. For OK or
	 * EM_MISS status, the index of the tail bucket of the chain prior to
	 * processing the command. If CHAIN_UPD=1, the bucket was removed and
	 * this index can be de-allocated. For other status values, it is set to
	 * 0.
	 */
	uint32_t table_index3:26;
	/* Unused field [6] */
	uint32_t unused7:6;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v2:1;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. For OK or
	 * EM_MISS status, the index of the tail bucket of the chain prior to
	 * after the command. If CHAIN_UPD=0 (always for EM_MISS status), it is
	 * always equal to TABLE_INDEX3 as the chain was not updated. For other
	 * status values, it is set to 0.
	 */
	uint32_t table_index4:26;
	/* Unused field [5] */
	uint32_t unused8:5;
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
	uint32_t bkt_num:8;
	/* See BKT_NUM description. */
	uint32_t num_entries:3;
	/*
	 * Specifies if the chain was updated while processing the command: Set
	 * to 1 when a bucket is removed from the static bucket chain. This
	 * occurs if after the delete, the tail bucket is a dynamic bucket and
	 * no longer has any valid entries. In this case, software should de-
	 * allocate the dynamic bucket at TABLE_INDEX3. It is also set to 1 when
	 * the static bucket is evicted, which only occurs for locked scopes.
	 * See the EM_DELETE command description for details.
	 */
	uint32_t chain_upd:1;
	/* Unused field [20] */
	uint32_t unused9:20;
};

/**
 * EM_CHAIN_CMP: OK status indicates that the CHAIN_PTR of the tail
 * bucket was successfully updated. TABLE_INDEX is from the command. It
 * is the value of the new CHAIN_PTR. TABLE_INDEX2 is from the command.
 * TABLE_INDEX3 is the index of the tail bucket of the static bucket
 * chain.
 */
struct cfa_mpc_em_chain_cmp {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records **(EXCEPT
	 * no_op!!!!)** .
	 */
	uint32_t type:6;
		#define EM_CHAIN_CMP_TYPE_MID_PATH_LONG 31
	/* Unused field [2] */
	uint32_t unused0:2;
	/* The command processing status. */
	uint32_t status:4;
		#define EM_CHAIN_CMP_STATUS_OK 0
		#define EM_CHAIN_CMP_STATUS_UNSPRT_ERR 1
		#define EM_CHAIN_CMP_STATUS_FMT_ERR 2
		#define EM_CHAIN_CMP_STATUS_SCOPE_ERR 3
		#define EM_CHAIN_CMP_STATUS_ADDR_ERR 4
		#define EM_CHAIN_CMP_STATUS_CACHE_ERR 5
	/*
	 * This field represents the Mid-Path client that generated the
	 * completion.
	 */
	uint32_t mp_client:4;
		#define EM_CHAIN_CMP_MP_CLIENT_TE_CFA 2
		#define EM_CHAIN_CMP_MP_CLIENT_RE_CFA 3
	/* OPCODE from the command. */
	uint32_t opcode:8;
		#define EM_CHAIN_CMP_OPCODE_EM_CHAIN 11
	/* Unused field [8] */
	uint32_t unused1:8;
	/*
	 * This is a copy of the opaque field from the mid path BD of this
	 * command.
	 */
	uint32_t opaque:32;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v1:1;
	/* Unused field [3] */
	uint32_t unused2:3;
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
	uint32_t hash_msb:12;
	/* Unused field [8] */
	uint32_t unused3:8;
	/* TABLE_SCOPE from the command. */
	uint32_t table_scope:5;
	/* Unused field [3] */
	uint32_t unused4:3;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX
	 * from the command, which is the new CHAIN_PTR for the tail bucket of
	 * the static bucket chain.
	 */
	uint32_t table_index:26;
	/* Unused field [6] */
	uint32_t unused5:6;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. TABLE_INDEX2
	 * from the command.
	 */
	uint32_t table_index2:26;
	/* Unused field [6] */
	uint32_t unused6:6;
	/*
	 * A 32B index into the EM table identified by TABLE_SCOPE. For OK
	 * status, the index of the tail bucket of the chain. Otherwise, set to
	 * 0.
	 */
	uint32_t table_index3:26;
	/* Unused field [6] */
	uint32_t unused7:6;
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	uint32_t v2:1;
	/* Unused field [31] */
	uint32_t unused8:31;
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
	uint32_t bkt_num:8;
	/* See BKT_NUM description. */
	uint32_t num_entries:3;
	/*
	 * Set to 1 when the scope is a locked scope, the tail bucket is the
	 * static bucket, the bucket is empty (all of its ENTRY_PTR values are
	 * 0), and TABLE_INDEX=0 in the command. In this case, the static bucket
	 * is evicted. For all other cases, it is set to 0.
	 */
	uint32_t chain_upd:1;
	/* Unused field [20] */
	uint32_t unused9:20;
};

/* clang-format on */

#endif /* _CFA_P70_MPC_STRUCTS_H_ */
