/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright (c) 2016 NXP. All rights reserved.
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
 *     * Neither the name of Freescale Semiconductor, Inc nor the names of its
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

#ifndef _DPAA2_HW_DPIO_H_
#define _DPAA2_HW_DPIO_H_

#include <mc/fsl_dpio.h>
#include <mc/fsl_mc_sys.h>

struct dpaa2_io_portal_t {
	struct dpaa2_dpio_dev *dpio_dev;
	struct dpaa2_dpio_dev *sec_dpio_dev;
	uint64_t net_tid;
	uint64_t sec_tid;
};

/*! Global per thread DPIO portal */
RTE_DECLARE_PER_LCORE(struct dpaa2_io_portal_t, _dpaa2_io);

#define DPAA2_PER_LCORE_DPIO RTE_PER_LCORE(_dpaa2_io).dpio_dev
#define DPAA2_PER_LCORE_PORTAL DPAA2_PER_LCORE_DPIO->sw_portal

#define DPAA2_PER_LCORE_SEC_DPIO RTE_PER_LCORE(_dpaa2_io).sec_dpio_dev
#define DPAA2_PER_LCORE_SEC_PORTAL DPAA2_PER_LCORE_SEC_DPIO->sw_portal

/* Affine a DPIO portal to current processing thread */
int dpaa2_affine_qbman_swp(void);

/* Affine additional DPIO portal to current crypto processing thread */
int dpaa2_affine_qbman_swp_sec(void);

/* allocate memory for FQ - dq storage */
int
dpaa2_alloc_dq_storage(struct queue_storage_info_t *q_storage);

/* free memory for FQ- dq storage */
void
dpaa2_free_dq_storage(struct queue_storage_info_t *q_storage);

#endif /* _DPAA2_HW_DPIO_H_ */
