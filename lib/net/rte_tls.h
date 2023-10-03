/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef RTE_TLS_H
#define RTE_TLS_H

/**
 * @file
 *
 * Transport layer security (TLS) related defines.
 */

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_TLS_TYPE_INVALID              0 /**< Invalid TLS message type. */
#define RTE_TLS_TYPE_CHANGE_CIPHER_SPEC  20 /**< Change cipher spec message. */
#define RTE_TLS_TYPE_ALERT               21 /**< Alert message. */
#define RTE_TLS_TYPE_HANDSHAKE           22 /**< Handshake message for TLS. */
#define RTE_TLS_TYPE_APPDATA             23 /**< TLS application data message. */
#define RTE_TLS_TYPE_HEARTBEAT           24 /**< TLS 1.3 heartbeat message. */
#define RTE_TLS_TYPE_MAX                255 /**< Maximum value as TLS content type. */

#define RTE_TLS_VERSION_1_2    0x0303 /**< TLS 1.2 version. */
#define RTE_TLS_VERSION_1_3    0x0304 /**< TLS 1.3 version. */

/**
 * TLS Header
 */
__extension__
struct rte_tls_hdr {
	/** Content type of TLS packet. Defined as RTE_TLS_TYPE_*. */
	uint8_t type;
	/** TLS Version defined as RTE_TLS_VERSION*. */
	rte_be16_t version;
	/** The length (in bytes) of the following TLS packet. */
	rte_be16_t length;
} __rte_packed;

#ifdef __cplusplus
}
#endif

#endif /* RTE_TLS_H */
