/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 6WIND S.A.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_GRE_H_
#define _RTE_GRE_H_

#include <stdint.h>
#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GRE Header
 */
struct gre_hdr {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint16_t res2:4; /**< Reserved */
	uint16_t s:1;    /**< Sequence Number Present bit */
	uint16_t k:1;    /**< Key Present bit */
	uint16_t res1:1; /**< Reserved */
	uint16_t c:1;    /**< Checksum Present bit */
	uint16_t ver:3;  /**< Version Number */
	uint16_t res3:5; /**< Reserved */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint16_t c:1;    /**< Checksum Present bit */
	uint16_t res1:1; /**< Reserved */
	uint16_t k:1;    /**< Key Present bit */
	uint16_t s:1;    /**< Sequence Number Present bit */
	uint16_t res2:4; /**< Reserved */
	uint16_t res3:5; /**< Reserved */
	uint16_t ver:3;  /**< Version Number */
#endif
	uint16_t proto;  /**< Protocol Type */
} __attribute__((__packed__));

#ifdef __cplusplus
}
#endif

#endif /* RTE_GRE_H_ */
