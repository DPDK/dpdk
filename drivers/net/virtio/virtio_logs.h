/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _VIRTIO_LOGS_H_
#define _VIRTIO_LOGS_H_

#include <rte_log.h>

extern int virtio_logtype_init;
#define RTE_LOGTYPE_VIRTIO_INIT virtio_logtype_init
#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, VIRTIO_INIT, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#ifdef RTE_LIBRTE_VIRTIO_DEBUG_RX
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, VIRTIO_DRIVER, "%s() rx: ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_VIRTIO_DEBUG_TX
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, VIRTIO_DRIVER, "%s() tx: ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while(0)
#endif

extern int virtio_logtype_driver;
#define RTE_LOGTYPE_VIRTIO_DRIVER virtio_logtype_driver
#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, VIRTIO_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif /* _VIRTIO_LOGS_H_ */
