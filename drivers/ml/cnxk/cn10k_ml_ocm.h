/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_OCM_H_
#define _CN10K_ML_OCM_H_

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

struct cnxk_ml_dev;

/* Number of OCM tiles. */
#define ML_CN10K_OCM_NUMTILES 0x8

/* OCM in bytes, per tile. */
#define ML_CN10K_OCM_TILESIZE 0x100000

/* OCM and Tile information structure */
struct cn10k_ml_ocm_tile_info {
	/* Mask of used / allotted pages on tile's OCM */
	uint8_t *ocm_mask;

	/* Last pages in the tile's OCM used for weights and bias, default = -1 */
	int last_wb_page;

	/* Number pages used for scratch memory on the tile's OCM */
	uint16_t scratch_pages;
};

/* Model OCM map structure */
struct cn10k_ml_ocm_layer_map {
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

	/* OCM allocation mode */
	const char *alloc_mode;

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

	/* Memory for ocm_mask */
	uint8_t *ocm_mask;
};

int cn10k_ml_ocm_tilecount(uint64_t tilemask, int *start, int *end);
int cn10k_ml_ocm_tilemask_find(struct cnxk_ml_dev *cnxk_mldev, uint8_t num_tiles, uint16_t wb_pages,
			       uint16_t scratch_pages, uint64_t *tilemask);
void cn10k_ml_ocm_reserve_pages(struct cnxk_ml_dev *cnxk_mldev, uint16_t model_id,
				uint16_t layer_id, uint64_t tilemask, int wb_page_start,
				uint16_t wb_pages, uint16_t scratch_pages);
void cn10k_ml_ocm_free_pages(struct cnxk_ml_dev *cnxk_mldev, uint16_t model_id, uint16_t layer_id);
void cn10k_ml_ocm_print(struct cnxk_ml_dev *cnxk_mldev, FILE *fp);

#endif /* _CN10K_ML_OCM_H_ */
