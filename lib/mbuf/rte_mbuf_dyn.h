/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 6WIND S.A.
 */

#ifndef _RTE_MBUF_DYN_H_
#define _RTE_MBUF_DYN_H_

/**
 * @file
 * RTE Mbuf dynamic fields and flags
 *
 * Many DPDK features require to store data inside the mbuf. As the room
 * in mbuf structure is limited, it is not possible to have a field for
 * each feature. Also, changing fields in the mbuf structure can break
 * the API or ABI.
 *
 * This module addresses this issue, by enabling the dynamic
 * registration of fields or flags:
 *
 * - a dynamic field is a named area in the rte_mbuf structure, with a
 *   given size (>= 1 byte) and alignment constraint.
 * - a dynamic flag is a named bit in the rte_mbuf structure, stored
 *   in mbuf->ol_flags.
 *
 * The placement of the field or flag can be automatic, in this case the
 * zones that have the smallest size and alignment constraint are
 * selected in priority. Else, a specific field offset or flag bit
 * number can be requested through the API.
 *
 * The typical use case is when a specific offload feature requires to
 * register a dedicated offload field in the mbuf structure, and adding
 * a static field or flag is not justified.
 *
 * Example of use:
 *
 * - A rte_mbuf_dynfield structure is defined, containing the parameters
 *   of the dynamic field to be registered:
 *   const struct rte_mbuf_dynfield rte_dynfield_my_feature = { ... };
 * - The application initializes the PMD, and asks for this feature
 *   at port initialization by passing RTE_ETH_RX_OFFLOAD_MY_FEATURE in
 *   rxconf. This will make the PMD to register the field by calling
 *   rte_mbuf_dynfield_register(&rte_dynfield_my_feature). The PMD
 *   stores the returned offset.
 * - The application that uses the offload feature also registers
 *   the field to retrieve the same offset.
 * - When the PMD receives a packet, it can set the field:
 *   *RTE_MBUF_DYNFIELD(m, offset, <type *>) = value;
 * - In the main loop, the application can retrieve the value with
 *   the same macro.
 *
 * To avoid wasting space, the dynamic fields or flags must only be
 * reserved on demand, when an application asks for the related feature.
 *
 * The registration can be done at any moment, but it is not possible
 * to unregister fields or flags for now.
 *
 * A dynamic field can be reserved and used by an application only.
 * It can for instance be a packet mark.
 *
 * To avoid namespace collisions, the dynamic mbuf field or flag names
 * have to be chosen with care. It is advised to use the same
 * conventions than function names in dpdk:
 * - "rte_mbuf_dynfield_<name>" if defined in mbuf library
 * - "rte_<libname>_dynfield_<name>" if defined in another library
 * - "rte_net_<pmd>_dynfield_<name>" if defined in a PMD
 * - any name that does not start with "rte_" in an application
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum length of the dynamic field or flag string.
 */
#define RTE_MBUF_DYN_NAMESIZE 64

/**
 * Structure describing the parameters of a mbuf dynamic field.
 */
struct rte_mbuf_dynfield {
	char name[RTE_MBUF_DYN_NAMESIZE]; /**< Name of the field. */
	size_t size;        /**< The number of bytes to reserve. */
	size_t align;       /**< The alignment constraint (power of 2). */
	unsigned int flags; /**< Reserved for future use, must be 0. */
};

/**
 * Structure describing the parameters of a mbuf dynamic flag.
 */
struct rte_mbuf_dynflag {
	char name[RTE_MBUF_DYN_NAMESIZE]; /**< Name of the dynamic flag. */
	unsigned int flags; /**< Reserved for future use, must be 0. */
};

/**
 * Register space for a dynamic field in the mbuf structure.
 *
 * If the field is already registered (same name and parameters), its
 * offset is returned.
 *
 * @param params
 *   A structure containing the requested parameters (name, size,
 *   alignment constraint and flags).
 * @return
 *   The offset in the mbuf structure, or -1 on error.
 *   Possible values for rte_errno:
 *   - EINVAL: invalid parameters (size, align, or flags).
 *   - EEXIST: this name is already register with different parameters.
 *   - EPERM: called from a secondary process.
 *   - ENOENT: not enough room in mbuf.
 *   - ENOMEM: allocation failure.
 *   - ENAMETOOLONG: name does not ends with \0.
 */
int rte_mbuf_dynfield_register(const struct rte_mbuf_dynfield *params);

/**
 * Register space for a dynamic field in the mbuf structure at offset.
 *
 * If the field is already registered (same name, parameters and offset),
 * the offset is returned.
 *
 * @param params
 *   A structure containing the requested parameters (name, size,
 *   alignment constraint and flags).
 * @param offset
 *   The requested offset. Ignored if SIZE_MAX is passed.
 * @return
 *   The offset in the mbuf structure, or -1 on error.
 *   Possible values for rte_errno:
 *   - EINVAL: invalid parameters (size, align, flags, or offset).
 *   - EEXIST: this name is already register with different parameters.
 *   - EBUSY: the requested offset cannot be used.
 *   - EPERM: called from a secondary process.
 *   - ENOENT: not enough room in mbuf.
 *   - ENOMEM: allocation failure.
 *   - ENAMETOOLONG: name does not ends with \0.
 */
int rte_mbuf_dynfield_register_offset(const struct rte_mbuf_dynfield *params,
				size_t offset);

/**
 * Lookup for a registered dynamic mbuf field.
 *
 * @param name
 *   A string identifying the dynamic field.
 * @param params
 *   If not NULL, and if the lookup is successful, the structure is
 *   filled with the parameters of the dynamic field.
 * @return
 *   The offset of this field in the mbuf structure, or -1 on error.
 *   Possible values for rte_errno:
 *   - ENOENT: no dynamic field matches this name.
 */
int rte_mbuf_dynfield_lookup(const char *name,
			struct rte_mbuf_dynfield *params);

/**
 * Register a dynamic flag in the mbuf structure.
 *
 * If the flag is already registered (same name and parameters), its
 * bitnum is returned.
 *
 * @param params
 *   A structure containing the requested parameters of the dynamic
 *   flag (name and options).
 * @return
 *   The number of the reserved bit, or -1 on error.
 *   Possible values for rte_errno:
 *   - EINVAL: invalid parameters (size, align, or flags).
 *   - EEXIST: this name is already register with different parameters.
 *   - EPERM: called from a secondary process.
 *   - ENOENT: no more flag available.
 *   - ENOMEM: allocation failure.
 *   - ENAMETOOLONG: name is longer than RTE_MBUF_DYN_NAMESIZE - 1.
 */
int rte_mbuf_dynflag_register(const struct rte_mbuf_dynflag *params);

/**
 * Register a dynamic flag in the mbuf structure specifying bitnum.
 *
 * If the flag is already registered (same name, parameters and bitnum),
 * the bitnum is returned.
 *
 * @param params
 *   A structure containing the requested parameters of the dynamic
 *   flag (name and options).
 * @param bitnum
 *   The requested bitnum. Ignored if UINT_MAX is passed.
 * @return
 *   The number of the reserved bit, or -1 on error.
 *   Possible values for rte_errno:
 *   - EINVAL: invalid parameters (size, align, or flags).
 *   - EEXIST: this name is already register with different parameters.
 *   - EBUSY: the requested bitnum cannot be used.
 *   - EPERM: called from a secondary process.
 *   - ENOENT: no more flag available.
 *   - ENOMEM: allocation failure.
 *   - ENAMETOOLONG: name is longer than RTE_MBUF_DYN_NAMESIZE - 1.
 */
int rte_mbuf_dynflag_register_bitnum(const struct rte_mbuf_dynflag *params,
				unsigned int bitnum);

/**
 * Lookup for a registered dynamic mbuf flag.
 *
 * @param name
 *   A string identifying the dynamic flag.
 * @param params
 *   If not NULL, and if the lookup is successful, the structure is
 *   filled with the parameters of the dynamic flag.
 * @return
 *   The offset of this flag in the mbuf structure, or -1 on error.
 *   Possible values for rte_errno:
 *   - ENOENT: no dynamic flag matches this name.
 */
int rte_mbuf_dynflag_lookup(const char *name,
			struct rte_mbuf_dynflag *params);

/**
 * Helper macro to access to a dynamic field.
 */
#define RTE_MBUF_DYNFIELD(m, offset, type) ((type)((uintptr_t)(m) + (offset)))

/**
 * Dump the status of dynamic fields and flags.
 *
 * @param out
 *   The stream where the status is displayed.
 */
void rte_mbuf_dyn_dump(FILE *out);

/*
 * Placeholder for dynamic fields and flags declarations.
 * This is centralizing point to gather all field names
 * and parameters together.
 */

/*
 * The metadata dynamic field provides some extra packet information
 * to interact with RTE Flow engine. The metadata in sent mbufs can be
 * used to match on some Flows. The metadata in received mbufs can
 * provide some feedback from the Flows. The metadata flag tells
 * whether the field contains actual value to send, or received one.
 */
#define RTE_MBUF_DYNFIELD_METADATA_NAME "rte_flow_dynfield_metadata"
#define RTE_MBUF_DYNFLAG_METADATA_NAME "rte_flow_dynflag_metadata"

/**
 * The timestamp dynamic field provides some timing information, the
 * units and time references (initial phase) are not explicitly defined
 * but are maintained always the same for a given port. Some devices allow
 * to query rte_eth_read_clock() that will return the current device
 * timestamp. The dynamic Tx timestamp flag tells whether the field contains
 * actual timestamp value for the packets being sent, this value can be
 * used by PMD to schedule packet sending.
 */
#define RTE_MBUF_DYNFIELD_TIMESTAMP_NAME "rte_dynfield_timestamp"
typedef uint64_t rte_mbuf_timestamp_t;

/**
 * Indicate that the timestamp field in the mbuf was filled by the driver.
 */
#define RTE_MBUF_DYNFLAG_RX_TIMESTAMP_NAME "rte_dynflag_rx_timestamp"

/**
 * Register dynamic mbuf field and flag for Rx timestamp.
 *
 * @param field_offset
 *   Pointer to the offset of the registered mbuf field, can be NULL.
 *   The same field is shared for Rx and Tx timestamp.
 * @param rx_flag
 *   Pointer to the mask of the registered offload flag, can be NULL.
 * @return
 *   0 on success, -1 otherwise.
 *   Possible values for rte_errno:
 *   - EEXIST: already registered with different parameters.
 *   - EPERM: called from a secondary process.
 *   - ENOENT: no more field or flag available.
 *   - ENOMEM: allocation failure.
 */
int rte_mbuf_dyn_rx_timestamp_register(int *field_offset, uint64_t *rx_flag);

/**
 * When PMD sees the RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME flag set on the
 * packet being sent it tries to synchronize the time of packet appearing
 * on the wire with the specified packet timestamp. If the specified one
 * is in the past it should be ignored, if one is in the distant future
 * it should be capped with some reasonable value (in range of seconds).
 *
 * There is no any packet reordering according to timestamps is supposed,
 * neither for packet within the burst, nor for the whole bursts, it is
 * an entirely application responsibility to generate packets and its
 * timestamps in desired order. The timestamps might be put only in
 * the first packet in the burst providing the entire burst scheduling.
 */
#define RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME "rte_dynflag_tx_timestamp"

/**
 * Register dynamic mbuf field and flag for Tx timestamp.
 *
 * @param field_offset
 *   Pointer to the offset of the registered mbuf field, can be NULL.
 *   The same field is shared for Rx and Tx timestamp.
 * @param tx_flag
 *   Pointer to the mask of the registered offload flag, can be NULL.
 * @return
 *   0 on success, -1 otherwise.
 *   Possible values for rte_errno:
 *   - EEXIST: already registered with different parameters.
 *   - EPERM: called from a secondary process.
 *   - ENOENT: no more field or flag available.
 *   - ENOMEM: allocation failure.
 */
int rte_mbuf_dyn_tx_timestamp_register(int *field_offset, uint64_t *tx_flag);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MBUF_DYN_H_ */
