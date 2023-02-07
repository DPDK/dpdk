/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_OCM_H_
#define _CN10K_ML_OCM_H_

#include <rte_mldev.h>

/* Page size in bytes. */
#define ML_CN10K_OCM_PAGESIZE 0x4000

/* Number of OCM tiles. */
#define ML_CN10K_OCM_NUMTILES 0x8

/* OCM in bytes, per tile. */
#define ML_CN10K_OCM_TILESIZE 0x100000

/* OCM pages, per tile. */
#define ML_CN10K_OCM_NUMPAGES (ML_CN10K_OCM_TILESIZE / ML_CN10K_OCM_PAGESIZE)

/* Maximum OCM mask words, per tile, 8 bit words. */
#define ML_CN10K_OCM_MASKWORDS (ML_CN10K_OCM_NUMPAGES / 8)

/* OCM and Tile information structure */
struct cn10k_ml_ocm_tile_info {
	/* Mask of used / allotted pages on tile's OCM */
	uint8_t ocm_mask[ML_CN10K_OCM_MASKWORDS];

	/* Last pages in the tile's OCM used for weights and bias, default = -1 */
	int last_wb_page;

	/* Number pages used for scratch memory on the tile's OCM */
	uint16_t scratch_pages;
};

/* Model OCM map structure */
struct cn10k_ml_ocm_model_map {
	/* Status of OCM reservation */
	bool ocm_reserved;

	/* Mask of OCM tiles for the model */
	uint64_t tilemask;

	/* Start page for the model load, default = -1 */
	int wb_page_start;

	/* Number of pages required for weights and bias */
	uint16_t wb_pages;

	/* Number of pages required for scratch memory */
	uint16_t scratch_pages;
};

/* OCM state structure */
struct cn10k_ml_ocm {
	/* OCM spinlock, used to update OCM state */
	rte_spinlock_t lock;

	/* Number of OCM tiles */
	uint8_t num_tiles;

	/* OCM size per each tile */
	uint64_t size_per_tile;

	/* Size of OCM page */
	uint64_t page_size;

	/* Number of OCM pages */
	uint16_t num_pages;

	/* Words per OCM mask */
	uint16_t mask_words;

	/* OCM memory info and status*/
	struct cn10k_ml_ocm_tile_info tile_ocm_info[ML_CN10K_OCM_NUMTILES];
};

#endif /* _CN10K_ML_OCM_H_ */
