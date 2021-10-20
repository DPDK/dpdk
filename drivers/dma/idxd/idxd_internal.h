/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Intel Corporation
 */

#ifndef _IDXD_INTERNAL_H_
#define _IDXD_INTERNAL_H_

/**
 * @file idxd_internal.h
 *
 * Internal data structures for the idxd/DSA driver for dmadev
 *
 * @warning
 * @b EXPERIMENTAL: these structures and APIs may change without prior notice
 */

extern int idxd_pmd_logtype;

#define IDXD_PMD_LOG(level, fmt, args...) rte_log(RTE_LOG_ ## level, \
		idxd_pmd_logtype, "IDXD: %s(): " fmt "\n", __func__, ##args)

#define IDXD_PMD_DEBUG(fmt, args...)  IDXD_PMD_LOG(DEBUG, fmt, ## args)
#define IDXD_PMD_INFO(fmt, args...)   IDXD_PMD_LOG(INFO, fmt, ## args)
#define IDXD_PMD_ERR(fmt, args...)    IDXD_PMD_LOG(ERR, fmt, ## args)
#define IDXD_PMD_WARN(fmt, args...)   IDXD_PMD_LOG(WARNING, fmt, ## args)

#endif /* _IDXD_INTERNAL_H_ */
