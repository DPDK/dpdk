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

#ifndef _DPAA2_HW_PVT_H_
#define _DPAA2_HW_PVT_H_

#include <mc/fsl_mc_sys.h>
#include <fsl_qbman_portal.h>

#define DPAA2_DQRR_RING_SIZE	16
	/** <Maximum number of slots available in RX ring*/

#define MC_PORTAL_INDEX		0
#define NUM_DPIO_REGIONS	2
#define NUM_DQS_PER_QUEUE       2

/* Maximum release/acquire from QBMAN */
#define DPAA2_MBUF_MAX_ACQ_REL	7

#define MAX_BPID 256

struct dpaa2_dpio_dev {
	TAILQ_ENTRY(dpaa2_dpio_dev) next;
		/**< Pointer to Next device instance */
	uint16_t index; /**< Index of a instance in the list */
	rte_atomic16_t ref_count;
		/**< How many thread contexts are sharing this.*/
	struct fsl_mc_io *dpio; /** handle to DPIO portal object */
	uint16_t token;
	struct qbman_swp *sw_portal; /** SW portal object */
	const struct qbman_result *dqrr[4];
		/**< DQRR Entry for this SW portal */
	void *mc_portal; /**< MC Portal for configuring this device */
	uintptr_t qbman_portal_ce_paddr;
		/**< Physical address of Cache Enabled Area */
	uintptr_t ce_size; /**< Size of the CE region */
	uintptr_t qbman_portal_ci_paddr;
		/**< Physical address of Cache Inhibit Area */
	uintptr_t ci_size; /**< Size of the CI region */
	int32_t	vfio_fd; /**< File descriptor received via VFIO */
	int32_t hw_id; /**< An unique ID of this DPIO device instance */
};

struct dpaa2_dpbp_dev {
	TAILQ_ENTRY(dpaa2_dpbp_dev) next;
		/**< Pointer to Next device instance */
	struct fsl_mc_io dpbp;  /** handle to DPBP portal object */
	uint16_t token;
	rte_atomic16_t in_use;
	uint32_t dpbp_id; /*HW ID for DPBP object */
};

struct queue_storage_info_t {
	struct qbman_result *dq_storage[NUM_DQS_PER_QUEUE];
};

struct dpaa2_queue {
	struct rte_mempool *mb_pool; /**< mbuf pool to populate RX ring. */
	void *dev;
	int32_t eventfd;	/*!< Event Fd of this queue */
	uint32_t fqid;		/*!< Unique ID of this queue */
	uint8_t tc_index;	/*!< traffic class identifier */
	uint16_t flow_id;	/*!< To be used by DPAA2 frmework */
	uint64_t rx_pkts;
	uint64_t tx_pkts;
	uint64_t err_pkts;
	struct queue_storage_info_t *q_storage;
};

/*! Global MCP list */
extern void *(*rte_mcp_ptr_list);

struct dpaa2_dpbp_dev *dpaa2_alloc_dpbp_dev(void);
void dpaa2_free_dpbp_dev(struct dpaa2_dpbp_dev *dpbp);

#endif
