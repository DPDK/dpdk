/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#ifndef _RTE_ESP_H_
#define _RTE_ESP_H_

/**
 * @file
 *
 * ESP-related defines
 */

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ESP Header
 */
struct esp_hdr {
	rte_be32_t spi;  /**< Security Parameters Index */
	rte_be32_t seq;  /**< packet sequence number */
} __attribute__((__packed__));

/**
 * ESP Trailer
 */
struct esp_tail {
	uint8_t pad_len;     /**< number of pad bytes (0-255) */
	uint8_t next_proto;  /**< IPv4 or IPv6 or next layer header */
} __attribute__((__packed__));

#ifdef __cplusplus
}
#endif

#endif /* RTE_ESP_H_ */
