/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_MM_PRIV_H_
#define _CFA_MM_PRIV_H_

#define CFA_MM_SIGNATURE 0xCFA66C89

#define CFA_MM_INVALID8 0xFF
#define CFA_MM_INVALID16 0xFFFF
#define CFA_MM_INVALID32 0xFFFFFFFF
#define CFA_MM_INVALID64 0xFFFFFFFFFFFFFFFFULL

#define CFA_MM_MAX_RECORDS (64 * 1024 * 1024)
#define CFA_MM_MAX_CONTIG_RECORDS 8
#define CFA_MM_RECORDS_PER_BYTE 8
#define CFA_MM_MIN_RECORDS_PER_BLOCK 8

/**
 * CFA Records block
 *
 * Structure used to store the CFA record block info
 */
struct cfa_mm_blk {
	/* Index of the previous block in the list */
	uint32_t prev_blk_idx;
	/* Index of the next block in the list */
	uint32_t next_blk_idx;
	/* Number of free records available in the block */
	uint16_t num_free_records;
	/* Location of first free record in the block */
	uint16_t first_free_record;
	/* Number of contiguous records */
	uint16_t num_contig_records;
	/* Reserved for future use */
	uint16_t reserved;
};

/**
 * CFA Record block list
 *
 *  Structure used to store CFA Record block list info
 */
struct cfa_mm_blk_list {
	/* Index of the first block in the list */
	uint32_t first_blk_idx;
	/* Index of the current block having free records */
	uint32_t current_blk_idx;
};

/**
 * CFA memory manager Database
 *
 *  Structure used to store CFA memory manager database info
 */
struct cfa_mm {
	/* Signature of the CFA Memory Manager Database */
	uint32_t signature;
	/* Maximum number of CFA Records */
	uint32_t max_records;
	/* Number of CFA Records in use*/
	uint32_t records_in_use;
	/* Number of Records per block */
	uint16_t records_per_block;
	/* Maximum number of contiguous records */
	uint16_t max_contig_records;
	/**
	 * Block list table stores the info of lists of blocks
	 * for various numbers of contiguous records
	 */
	struct cfa_mm_blk_list *blk_list_tbl;
	/**
	 * Block table stores the info about the blocks of CFA Records
	 */
	struct cfa_mm_blk *blk_tbl;
	/**
	 * Block bitmap table stores bit maps for the blocks of CFA Records
	 */
	uint8_t *blk_bmap_tbl;
};

#endif /* _CFA_MM_PRIV_H_ */
