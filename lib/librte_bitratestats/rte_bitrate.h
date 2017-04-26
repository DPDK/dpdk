/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
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

#ifndef _RTE_BITRATE_H_
#define _RTE_BITRATE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  Bitrate statistics data structure.
 *  This data structure is intentionally opaque.
 */
struct rte_stats_bitrates;


/**
 * Allocate a bitrate statistics structure
 *
 * @return
 *   - Pointer to structure on success
 *   - NULL on error (zmalloc failure)
 */
struct rte_stats_bitrates *rte_stats_bitrate_create(void);


/**
 * Register bitrate statistics with the metric library.
 *
 * @param bitrate_data
 *   Pointer allocated by rte_stats_create()
 *
 * @return
 *   Zero on success
 *   Negative on error
 */
int rte_stats_bitrate_reg(struct rte_stats_bitrates *bitrate_data);


/**
 * Calculate statistics for current time window. The period with which
 * this function is called should be the intended sampling window width.
 *
 * @param bitrate_data
 *   Bitrate statistics data pointer
 *
 * @param port_id
 *   Port id to calculate statistics for
 *
 * @return
 *  - Zero on success
 *  - Negative value on error
 */
int rte_stats_bitrate_calc(struct rte_stats_bitrates *bitrate_data,
	uint8_t port_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BITRATE_H_ */
