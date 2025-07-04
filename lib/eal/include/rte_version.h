/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

/**
 * @file
 * Definitions of DPDK version numbers
 */

#ifndef _RTE_VERSION_H_
#define _RTE_VERSION_H_

#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Macro to compute a version number usable for comparisons
 */
#define RTE_VERSION_NUM(a,b,c,d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))

/**
 * All version numbers in one to compare with RTE_VERSION_NUM()
 */
#define RTE_VERSION RTE_VERSION_NUM( \
			RTE_VER_YEAR, \
			RTE_VER_MONTH, \
			RTE_VER_MINOR, \
			RTE_VER_RELEASE)

/**
 * Function to return DPDK version prefix string
 */
const char *rte_version_prefix(void);

/**
 * Function to return DPDK version year
 */
unsigned int rte_version_year(void);

/**
 * Function to return DPDK version month
 */
unsigned int rte_version_month(void);

/**
 * Function to return DPDK minor version number
 */
unsigned int rte_version_minor(void);

/**
 * Function to return DPDK version suffix for any release candidates
 */
const char *rte_version_suffix(void);

/**
 * Function to return DPDK version release candidate value
 */
unsigned int rte_version_release(void);

/**
 * Function returning version string
 * @return
 *     DPDK version string
 */
const char *rte_version(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_VERSION_H */
