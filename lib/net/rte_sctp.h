/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright(c) 2010-2014 Intel Corporation.
 * All rights reserved.
 */

/**
 * @file
 *
 * SCTP-related defines
 */

#ifndef _RTE_SCTP_H_
#define _RTE_SCTP_H_

#include <stdint.h>

#include <rte_byteorder.h>

/**
 * SCTP Header
 */
struct __rte_packed_begin rte_sctp_hdr {
	rte_be16_t src_port; /**< Source port. */
	rte_be16_t dst_port; /**< Destin port. */
	rte_be32_t tag;      /**< Validation tag. */
	rte_be32_t cksum;    /**< Checksum. */
} __rte_packed_end;

#endif /* RTE_SCTP_H_ */
