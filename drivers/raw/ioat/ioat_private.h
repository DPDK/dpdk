/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _IOAT_PRIVATE_H_
#define _IOAT_PRIVATE_H_

/**
 * @file idxd_private.h
 *
 * Private data structures for the idxd/DSA part of ioat device driver
 *
 * @warning
 * @b EXPERIMENTAL: these structures and APIs may change without prior notice
 */

extern int ioat_pmd_logtype;

#define IOAT_PMD_LOG(level, fmt, args...) rte_log(RTE_LOG_ ## level, \
		ioat_pmd_logtype, "%s(): " fmt "\n", __func__, ##args)

#define IOAT_PMD_DEBUG(fmt, args...)  IOAT_PMD_LOG(DEBUG, fmt, ## args)
#define IOAT_PMD_INFO(fmt, args...)   IOAT_PMD_LOG(INFO, fmt, ## args)
#define IOAT_PMD_ERR(fmt, args...)    IOAT_PMD_LOG(ERR, fmt, ## args)
#define IOAT_PMD_WARN(fmt, args...)   IOAT_PMD_LOG(WARNING, fmt, ## args)

#endif /* _IOAT_PRIVATE_H_ */
