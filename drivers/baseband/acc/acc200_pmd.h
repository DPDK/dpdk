/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _RTE_ACC200_PMD_H_
#define _RTE_ACC200_PMD_H_

#include "acc_common.h"

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, acc200_logtype, fmt "\n", \
		##__VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(fmt, ...) \
		rte_bbdev_log(DEBUG, "acc200_pmd: " fmt, \
		##__VA_ARGS__)
#else
#define rte_bbdev_log_debug(fmt, ...)
#endif

/* ACC200 PF and VF driver names */
#define ACC200PF_DRIVER_NAME           intel_acc200_pf
#define ACC200VF_DRIVER_NAME           intel_acc200_vf

/* ACC200 PCI vendor & device IDs */
#define RTE_ACC200_VENDOR_ID           (0x8086)
#define RTE_ACC200_PF_DEVICE_ID        (0x57C0)
#define RTE_ACC200_VF_DEVICE_ID        (0x57C1)

#endif /* _RTE_ACC200_PMD_H_ */
