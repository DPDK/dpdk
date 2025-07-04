/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _NITROX_COMP_H_
#define _NITROX_COMP_H_

#define COMPRESSDEV_NAME_NITROX_PMD	compress_nitrox
#define NITROX_DECOMP_CTX_SIZE 2048
#define NITROX_CONSTANTS_MAX_SEARCH_DEPTH 31744
#define NITROX_DEFAULT_DEFLATE_SEARCH_DEPTH 32768
#define NITROX_COMP_WINDOW_SIZE_MIN 1
#define NITROX_COMP_WINDOW_SIZE_MAX 15
#define NITROX_COMP_LEVEL_LOWEST_START 1
#define NITROX_COMP_LEVEL_LOWEST_END 2
#define NITROX_COMP_LEVEL_LOWER_START 3
#define NITROX_COMP_LEVEL_LOWER_END 4
#define NITROX_COMP_LEVEL_MEDIUM_START 5
#define NITROX_COMP_LEVEL_MEDIUM_END 6
#define NITROX_COMP_LEVEL_BEST_START 7
#define NITROX_COMP_LEVEL_BEST_END 9
#define ZIP_INSTR_SIZE 64

struct nitrox_comp_device {
	struct rte_compressdev *cdev;
	struct nitrox_device *ndev;
	struct rte_mempool *xform_pool;
};

struct nitrox_device;

int nitrox_comp_pmd_create(struct nitrox_device *ndev);
int nitrox_comp_pmd_destroy(struct nitrox_device *ndev);

#endif /* _NITROX_COMP_H_ */
