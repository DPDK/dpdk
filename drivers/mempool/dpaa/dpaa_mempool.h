/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 NXP.
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
 *     * Neither the name of NXP nor the names of its
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
#ifndef __DPAA_MEMPOOL_H__
#define __DPAA_MEMPOOL_H__

/* System headers */
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>

#include <rte_mempool.h>

#include <rte_dpaa_bus.h>
#include <rte_dpaa_logs.h>

#include <fsl_usd.h>
#include <fsl_bman.h>

#define CPU_SPIN_BACKOFF_CYCLES               512

/* total number of bpools on SoC */
#define DPAA_MAX_BPOOLS	256

/* Maximum release/acquire from BMAN */
#define DPAA_MBUF_MAX_ACQ_REL  8

struct dpaa_bp_info {
	struct rte_mempool *mp;
	struct bman_pool *bp;
	uint32_t bpid;
	uint32_t size;
	uint32_t meta_data_size;
	int32_t dpaa_ops_index;
};

#define DPAA_MEMPOOL_TO_POOL_INFO(__mp) \
	((struct dpaa_bp_info *)__mp->pool_data)

#define DPAA_MEMPOOL_TO_BPID(__mp) \
	(((struct dpaa_bp_info *)__mp->pool_data)->bpid)

extern struct dpaa_bp_info rte_dpaa_bpid_info[DPAA_MAX_BPOOLS];

#define DPAA_BPID_TO_POOL_INFO(__bpid) (&rte_dpaa_bpid_info[__bpid])

#endif
