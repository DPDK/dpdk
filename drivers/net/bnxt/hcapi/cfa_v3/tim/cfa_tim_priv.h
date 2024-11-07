/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_TIM_PRIV_H_
#define _CFA_TIM_PRIV_H_

#include <cfa_types.h>

#define CFA_TIM_SIGNATURE 0xCFACEE11

/*
 *
 * Total index space is (MaxDir * MaxRegion * MaxTableScope), the
 * following macro satisfies that:
 *
 * (Dir# * (MaxRegionSpace + MaxTableScope)) +
 * (TableScope# * (MaxRegionSpace)) +
 * Region#
 *
 * Examples:
 *
 *  MaxD MaxR MaxT Total
 *     2    1    1     2
 *
 *  Dir Region TableScope  Index
 *    0      0          0      0
 *    1      0          0      1
 *
 *  MaxD MaxR MaxT Total
 *     2    2    1     4
 *
 *  Dir Region TableScope  Index
 *    0      0          0      0
 *    1      0          0      2
 *    0      1          0      1
 *    1      1          0      3
 *
 *  MaxD MaxR MaxT Total
 *     2    2    3    12
 *
 * Dir Region TableScope  Index
 *   0      0          0      0
 *   1      0          0      6
 *   0      1          0      1
 *   1      1          0      7
 *   0      0          1      2
 *   1      0          1      8
 *   0      1          1      3
 *   1      1          1      9
 *   0      0          2      4
 *   1      0          2      10
 *   0      1          2      5
 *   1      1          2      11
 *
 */
#define CFA_TIM_MAKE_INDEX(tsid, region, dir, max_regions, max_tsid) \
	(((dir) * (max_regions) * (max_tsid)) + ((tsid) * (max_regions)) + (region))

/**
 * CFA Table Scope Instance Manager Database
 *
 *  Structure used to store CFA Table Scope Instance Manager database info
 */
struct cfa_tim {
	/* Signature of the CFA Table Scope Instance Manager Database */
	uint32_t signature;
	/* Maximum number of Table Scope Ids */
	uint8_t max_tsid;
	/* Maximum number of regions per Table Scope */
	uint8_t max_regions;
	/* TPM instance table */
	void **tpm_tbl;
};

#endif /* _CFA_TIM_PRIV_H_ */
