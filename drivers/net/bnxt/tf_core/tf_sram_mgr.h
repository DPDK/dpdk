/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_SRAM_MGR_H_
#define _TF_SRAM_MGR_H_

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include "tf_core.h"
#include "tf_rm.h"

/* When special access registers are used to access the SRAM, stats can be
 * automatically cleared on read by the hardware.  This requires additional
 * support to be added in the firmware to use these registers for statistics.
 * The support entails using the special access registers to read the stats.
 * These are stored in bank 3 currently but may move depending upon the
 * policy defined in tf_device_p58.h
 */
#define STATS_CLEAR_ON_READ_SUPPORT 0

#define TF_SRAM_MGR_BLOCK_SZ_BYTES 64
#define TF_SRAM_MGR_MIN_SLICE_BYTES 8

/**
 * TF slice size.
 *
 * A slice is part of a 64B row
 *
 * Each slice is a multiple of 8B
 */
enum tf_sram_slice_size {
	TF_SRAM_SLICE_SIZE_8B,	/**< 8 byte SRAM slice */
	TF_SRAM_SLICE_SIZE_16B,	/**< 16 byte SRAM slice */
	TF_SRAM_SLICE_SIZE_32B,	/**< 32 byte SRAM slice */
	TF_SRAM_SLICE_SIZE_64B,	/**< 64 byte SRAM slice */
	TF_SRAM_SLICE_SIZE_MAX  /**< slice limit */
};


/** Initialize the SRAM slice manager
 *
 *  The SRAM slice manager manages slices within 64B rows. Slices are of size
 *  tf_sram_slice_size.  This function provides a handle to the SRAM manager
 *  data.
 *
 *  SRAM manager data may dynamically allocate data upon initialization if
 *  running on the host.
 *
 * [in/out] sram_handle
 *   Pointer to SRAM handle
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 * Returns the handle for the SRAM slice manager
 */
int tf_sram_mgr_bind(void **sram_handle);

/** Uninitialize the SRAM slice manager
 *
 * Frees any dynamically allocated data structures for SRAM slice management.
 *
 * [in] sram_handle
 *   Pointer to SRAM handle
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 */
int tf_sram_mgr_unbind(void *sram_handle);

/**
 * tf_sram_mgr_alloc_parms parameter definition
 */
struct tf_sram_mgr_alloc_parms {
	/**
	 * [in] dir
	 */
	enum tf_dir dir;
	/**
	 * [in] bank
	 *
	 *  the SRAM bank to allocate from
	 */
	enum tf_sram_bank_id bank_id;
	/**
	 * [in] slice_size
	 *
	 *  the slice size to allocate
	 */
	enum tf_sram_slice_size slice_size;
	/**
	 * [in/out] sram_slice
	 *
	 *  A pointer to be filled with an 8B sram slice offset
	 */
	uint16_t *sram_offset;
	/**
	 * [in] RM DB Handle required for RM allocation
	 */
	void *rm_db;
	/**
	 * [in] tf table type
	 */
	enum tf_tbl_type tbl_type;
};

/**
 * Allocate an SRAM Slice
 *
 * Allocate an SRAM slice from the indicated bank.  If successful an 8B SRAM
 * offset will be returned.  Slices are variable sized.  This may result in
 * a row being allocated from the RM SRAM bank pool if required.
 *
 * [in] sram_handle
 *   Pointer to SRAM handle
 *
 * [in] parms
 *   Pointer to the SRAM alloc parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
int tf_sram_mgr_alloc(void *sram_handle,
		      struct tf_sram_mgr_alloc_parms *parms);
/**
 * tf_sram_mgr_free_parms parameter definition
 */
struct tf_sram_mgr_free_parms {
	/**
	 * [in] dir
	 */
	enum tf_dir dir;
	/**
	 * [in] bank
	 *
	 *  the SRAM bank to free to
	 */
	enum tf_sram_bank_id bank_id;
	/**
	 * [in] slice_size
	 *
	 *  the slice size to be returned
	 */
	enum tf_sram_slice_size slice_size;
	/**
	 * [in] sram_offset
	 *
	 *  the SRAM slice offset (8B) to be returned
	 */
	uint16_t sram_offset;
	/**
	 * [in] RM DB Handle required for RM free
	 */
	void *rm_db;
	/**
	 * [in] tf table type
	 */
	enum tf_tbl_type tbl_type;
#if (STATS_CLEAR_ON_READ_SUPPORT == 0)
	/**
	 * [in] tfp
	 *
	 * A pointer to the tf handle
	 */
	void *tfp;
#endif
};

/**
 * Free an SRAM Slice
 *
 * Free an SRAM slice to the indicated bank.  This may result in a 64B row
 * being returned to the RM SRAM bank pool.
 *
 * [in] sram_handle
 *   Pointer to SRAM handle
 *
 * [in] parms
 *   Pointer to the SRAM free parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
int tf_sram_mgr_free(void *sram_handle,
		     struct tf_sram_mgr_free_parms *parms);

/**
 * tf_sram_mgr_dump_parms parameter definition
 */
struct tf_sram_mgr_dump_parms {
	/**
	 * [in] dir
	 */
	enum tf_dir dir;
	/**
	 * [in] bank
	 *
	 *  the SRAM bank to dump
	 */
	enum tf_sram_bank_id bank_id;
	/**
	 * [in] slice_size
	 *
	 *  the slice size list to be dumped
	 */
	enum tf_sram_slice_size slice_size;
};

/**
 * Dump a slice list
 *
 * Dump the slice list given the SRAM bank and the slice size
 *
 * [in] sram_handle
 *   Pointer to SRAM handle
 *
 * [in] parms
 *   Pointer to the SRAM free parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
int tf_sram_mgr_dump(void *sram_handle,
		     struct tf_sram_mgr_dump_parms *parms);

/**
 * tf_sram_mgr_is_allocated_parms parameter definition
 */
struct tf_sram_mgr_is_allocated_parms {
	/**
	 * [in] dir
	 */
	enum tf_dir dir;
	/**
	 * [in] bank
	 *
	 *  the SRAM bank to allocate from
	 */
	enum tf_sram_bank_id bank_id;
	/**
	 * [in] slice_size
	 *
	 *  the slice size which was allocated
	 */
	enum tf_sram_slice_size slice_size;
	/**
	 * [in] sram_offset
	 *
	 *  The sram slice offset to validate
	 */
	uint16_t sram_offset;
	/**
	 * [in/out] is_allocated
	 *
	 *  Pointer passed in to be filled with indication of allocation
	 */
	bool *is_allocated;
};

/**
 * Validate an SRAM Slice is allocated
 *
 * Validate whether the SRAM slice is allocated
 *
 * [in] sram_handle
 *   Pointer to SRAM handle
 *
 * [in] parms
 *   Pointer to the SRAM alloc parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
int tf_sram_mgr_is_allocated(void *sram_handle,
			     struct tf_sram_mgr_is_allocated_parms *parms);

/**
 * Given the slice size, return a char string
 */
const char
*tf_sram_slice_2_str(enum tf_sram_slice_size slice_size);

/**
 * Given the bank_id, return a char string
 */
const char
*tf_sram_bank_2_str(enum tf_sram_bank_id bank_id);

#endif /* _TF_SRAM_MGR_H_ */
