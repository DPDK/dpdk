/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#ifndef _RTE_PMD_ICE_H_
#define _RTE_PMD_ICE_H_

/**
 * @file rte_pmd_ice.h
 *
 * ICE PMD specific diagnostic functions.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dump the runtime DDP package configuration.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param buff
 *   Pointer to a `malloc()`-allocated buffer. On success, it will be freed by
 *   `free()` and replaced by a newly `malloc()`-allocated buffer; the caller
 *   must free the resulting buffer.
 * @param size
 *   Input buffer size in bytes; receives the size of the dump on success.
 * @return
 *   0 on success, -ENODEV for an invalid port, -ENOTSUP for a non-ICE port,
 *   or a negative error code on failure.
 */
__rte_experimental
int rte_pmd_ice_dump_package(uint16_t port, uint8_t **buff, uint32_t *size);

/**
 * Dump the runtime switch configuration.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param buff
 *   Pointer to a caller-allocated output buffer.
 * @param size
 *   Input buffer size in bytes; receives the size of the dump on success.
 * @return
 *   0 on success, -ENODEV for an invalid port, -ENOTSUP for a non-ICE port,
 *   or a negative error code on failure.
 */
__rte_experimental
int rte_pmd_ice_dump_switch(uint16_t port, uint8_t **buff, uint32_t *size);

/**
 * Dump the Tx scheduling tree to a stream in DOT format.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param detail
 *   Include node configuration parameters when true.
 * @param stream
 *   Output stream opened by the caller.
 * @return
 *   0 on success, -ENODEV for an invalid port, or -ENOTSUP for a non-ICE port.
 */
__rte_experimental
int rte_pmd_ice_dump_txsched(uint16_t port, bool detail, FILE *stream);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PMD_ICE_H_ */
