/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef RTE_DTLS_H
#define RTE_DTLS_H

/**
 * @file
 *
 * Datagram transport layer security (DTLS) related defines.
 */

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_DTLS_TYPE_INVALID               0 /**< Invalid DTLS message type. */
#define RTE_DTLS_TYPE_CHANGE_CIPHER_SPEC   20 /**< Change cipher spec message. */
#define RTE_DTLS_TYPE_ALERT                21 /**< Alert message. */
#define RTE_DTLS_TYPE_HANDSHAKE            22 /**< Handshake message for DTLS. */
#define RTE_DTLS_TYPE_APPDATA              23 /**< DTLS application data message. */
#define RTE_DTLS_TYPE_HEARTBEAT            24 /**< DTLS 1.3 heartbeat message. */
#define RTE_DTLS_TYPE_CIPHERTEXT_WITH_CID  25 /**< DTLS 1.3 ciphertext with CID message. */
#define RTE_DTLS_TYPE_ACK                  26 /**< DTLS 1.3 ACK message. */
#define RTE_DTLS_TYPE_MAX                 255 /**< Maximum value as DTLS content type. */

#define RTE_DTLS_VERSION_1_2    0xFEFD /**< DTLS 1.2 version. 1's complement of 1.2. */
#define RTE_DTLS_VERSION_1_3    0xFEFC /**< DTLS 1.3 version. 1's complement of 1.3. */

/**
 * DTLS Header
 */
__extension__
struct rte_dtls_hdr {
	/** Content type of DTLS packet. Defined as RTE_DTLS_TYPE_*. */
	uint8_t type;
	/** DTLS Version defined as RTE_DTLS_VERSION*. */
	rte_be16_t version;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	/** The sequence number for the DTLS record. */
	uint64_t sequence_number : 48;
	/** A counter value that is incremented on every cipher state change. */
	uint64_t epoch : 16;
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	/** A counter value that is incremented on every cipher state change. */
	uint64_t epoch : 16;
	/** The sequence number for the DTLS record. */
	uint64_t sequence_number : 48;
#endif
	/** The length (in bytes) of the following DTLS packet. */
	rte_be16_t length;
} __rte_packed;

#ifdef __cplusplus
}
#endif

#endif /* RTE_DTLS_H */
