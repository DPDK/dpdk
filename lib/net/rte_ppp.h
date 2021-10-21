/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation.
 */

#ifndef _RTE_PPP_H_
#define _RTE_PPP_H_

/**
 * @file
 *
 * PPP headers definition.
 */

#include <stdint.h>
#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * PPP Header
 */
struct rte_ppp_hdr {
	uint8_t addr; /**< PPP address(8) */
	uint8_t ctrl; /**< PPP control(8) */
	rte_be16_t proto_id; /**< PPP protocol identifier(16) */
} __rte_packed;

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PPP_H_ */
