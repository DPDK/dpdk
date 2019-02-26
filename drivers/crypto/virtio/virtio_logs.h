/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#ifndef _VIRTIO_LOGS_H_
#define _VIRTIO_LOGS_H_

#include <rte_log.h>

extern int virtio_crypto_logtype_init;

#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_crypto_logtype_init, \
		"PMD: %s(): " fmt "\n", __func__, ##args)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

extern int virtio_crypto_logtype_init;

#define VIRTIO_CRYPTO_INIT_LOG_IMPL(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_crypto_logtype_init, \
		"INIT: %s(): " fmt "\n", __func__, ##args)

#define VIRTIO_CRYPTO_INIT_LOG_INFO(fmt, args...) \
	VIRTIO_CRYPTO_INIT_LOG_IMPL(INFO, fmt, ## args)

#define VIRTIO_CRYPTO_INIT_LOG_DBG(fmt, args...) \
	VIRTIO_CRYPTO_INIT_LOG_IMPL(DEBUG, fmt, ## args)

#define VIRTIO_CRYPTO_INIT_LOG_ERR(fmt, args...) \
	VIRTIO_CRYPTO_INIT_LOG_IMPL(ERR, fmt, ## args)

extern int virtio_crypto_logtype_session;

#define VIRTIO_CRYPTO_SESSION_LOG_IMPL(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_crypto_logtype_session, \
		"SESSION: %s(): " fmt "\n", __func__, ##args)

#define VIRTIO_CRYPTO_SESSION_LOG_INFO(fmt, args...) \
	VIRTIO_CRYPTO_SESSION_LOG_IMPL(INFO, fmt, ## args)

#define VIRTIO_CRYPTO_SESSION_LOG_DBG(fmt, args...) \
	VIRTIO_CRYPTO_SESSION_LOG_IMPL(DEBUG, fmt, ## args)

#define VIRTIO_CRYPTO_SESSION_LOG_ERR(fmt, args...) \
	VIRTIO_CRYPTO_SESSION_LOG_IMPL(ERR, fmt, ## args)

extern int virtio_crypto_logtype_rx;

#define VIRTIO_CRYPTO_RX_LOG_IMPL(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_crypto_logtype_rx, \
		"RX: %s(): " fmt "\n", __func__, ##args)

#define VIRTIO_CRYPTO_RX_LOG_INFO(fmt, args...) \
	VIRTIO_CRYPTO_RX_LOG_IMPL(INFO, fmt, ## args)

#define VIRTIO_CRYPTO_RX_LOG_DBG(fmt, args...) \
	VIRTIO_CRYPTO_RX_LOG_IMPL(DEBUG, fmt, ## args)

#define VIRTIO_CRYPTO_RX_LOG_ERR(fmt, args...) \
	VIRTIO_CRYPTO_RX_LOG_IMPL(ERR, fmt, ## args)

extern int virtio_crypto_logtype_tx;

#define VIRTIO_CRYPTO_TX_LOG_IMPL(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_crypto_logtype_tx, \
		"TX: %s(): " fmt "\n", __func__, ##args)

#define VIRTIO_CRYPTO_TX_LOG_INFO(fmt, args...) \
	VIRTIO_CRYPTO_TX_LOG_IMPL(INFO, fmt, ## args)

#define VIRTIO_CRYPTO_TX_LOG_DBG(fmt, args...) \
	VIRTIO_CRYPTO_TX_LOG_IMPL(DEBUG, fmt, ## args)

#define VIRTIO_CRYPTO_TX_LOG_ERR(fmt, args...) \
	VIRTIO_CRYPTO_TX_LOG_IMPL(ERR, fmt, ## args)

extern int virtio_crypto_logtype_driver;

#define VIRTIO_CRYPTO_DRV_LOG_IMPL(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_crypto_logtype_driver, \
		"DRIVER: %s(): " fmt "\n", __func__, ##args)

#define VIRTIO_CRYPTO_DRV_LOG_INFO(fmt, args...) \
	VIRTIO_CRYPTO_DRV_LOG_IMPL(INFO, fmt, ## args)

#define VIRTIO_CRYPTO_DRV_LOG_DBG(fmt, args...) \
	VIRTIO_CRYPTO_DRV_LOG_IMPL(DEBUG, fmt, ## args)

#define VIRTIO_CRYPTO_DRV_LOG_ERR(fmt, args...) \
	VIRTIO_CRYPTO_DRV_LOG_IMPL(ERR, fmt, ## args)

#endif /* _VIRTIO_LOGS_H_ */
