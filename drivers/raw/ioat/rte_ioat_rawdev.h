/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_IOAT_RAWDEV_H_
#define _RTE_IOAT_RAWDEV_H_

/**
 * @file rte_ioat_rawdev.h
 *
 * Definitions for using the ioat rawdev device driver
 *
 * @warning
 * @b EXPERIMENTAL: these structures and APIs may change without prior notice
 */

/** Name of the device driver */
#define IOAT_PMD_RAWDEV_NAME rawdev_ioat
/** String reported as the device driver name by rte_rawdev_info_get() */
#define IOAT_PMD_RAWDEV_NAME_STR "rawdev_ioat"
/** Name used to adjust the log level for this driver */
#define IOAT_PMD_LOG_NAME "rawdev.ioat"

#endif
