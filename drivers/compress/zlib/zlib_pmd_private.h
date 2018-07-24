/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#ifndef _RTE_ZLIB_PMD_PRIVATE_H_
#define _RTE_ZLIB_PMD_PRIVATE_H_

#include <zlib.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#define COMPRESSDEV_NAME_ZLIB_PMD	compress_zlib
/**< ZLIB PMD device name */

#define DEF_MEM_LEVEL			8

int zlib_logtype_driver;
#define ZLIB_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, zlib_logtype_driver, "%s(): "fmt "\n", \
			__func__, ##args)

#define ZLIB_PMD_INFO(fmt, args...) \
	ZLIB_PMD_LOG(INFO, fmt, ## args)
#define ZLIB_PMD_ERR(fmt, args...) \
	ZLIB_PMD_LOG(ERR, fmt, ## args)
#define ZLIB_PMD_WARN(fmt, args...) \
	ZLIB_PMD_LOG(WARNING, fmt, ## args)

struct zlib_private {
};

#endif /* _RTE_ZLIB_PMD_PRIVATE_H_ */
