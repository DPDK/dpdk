/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _CNXK_RVU_LF_H_
#define _CNXK_RVU_LF_H_

#include <stdint.h>

#include <rte_common.h>

/**
 * @file cnxk_rvu_lf.h
 *
 * Marvell RVU LF raw PMD specific internal structures
 *
 */

extern int cnxk_logtype_rvu_lf;
#define RTE_LOGTYPE_CNXK_RVU_LF cnxk_logtype_rvu_lf
#define CNXK_RVU_LF_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, CNXK_RVU_LF, "%s(): ", __func__, __VA_ARGS__)

int rvu_lf_rawdev_selftest(uint16_t dev_id);

#endif /* _CNXK_RVU_LF_H_ */
