/*
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

#ifndef __DPAA2_EVENTDEV_H__
#define __DPAA2_EVENTDEV_H__

#include <rte_eventdev_pmd.h>
#include <rte_eventdev_pmd_vdev.h>
#include <rte_atomic.h>
#include <mc/fsl_dpcon.h>
#include <mc/fsl_mc_sys.h>

#define EVENTDEV_NAME_DPAA2_PMD		event_dpaa2

#ifdef RTE_LIBRTE_PMD_DPAA2_EVENTDEV_DEBUG
#define PMD_DRV_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#define PMD_DRV_FUNC_TRACE() PMD_DRV_LOG(DEBUG, ">>")
#else
#define PMD_DRV_LOG(level, fmt, args...) do { } while (0)
#define PMD_DRV_FUNC_TRACE() do { } while (0)
#endif

#define PMD_DRV_ERR(fmt, args...) \
	RTE_LOG(ERR, PMD, "%s(): " fmt "\n", __func__, ## args)

#define DPAA2_EVENT_DEFAULT_DPCI_PRIO 0

#define DPAA2_EVENT_MAX_QUEUES			16

enum {
	DPAA2_EVENT_DPCI_PARALLEL_QUEUE,
	DPAA2_EVENT_DPCI_ATOMIC_QUEUE,
	DPAA2_EVENT_DPCI_MAX_QUEUES
};

struct dpaa2_dpcon_dev {
	TAILQ_ENTRY(dpaa2_dpcon_dev) next;
	struct fsl_mc_io dpcon;
	uint16_t token;
	rte_atomic16_t in_use;
	uint32_t dpcon_id;
	uint16_t qbman_ch_id;
	uint8_t num_priorities;
	uint8_t channel_index;
};

struct evq_info_t {
	/* DPcon device */
	struct dpaa2_dpcon_dev *dpcon;
	/* Attached DPCI device */
	struct dpaa2_dpci_dev *dpci;
};

struct dpaa2_eventdev {
	struct evq_info_t evq_info[DPAA2_EVENT_MAX_QUEUES];
	uint8_t max_event_queues;
};

struct dpaa2_dpcon_dev *rte_dpaa2_alloc_dpcon_dev(void);
void rte_dpaa2_free_dpcon_dev(struct dpaa2_dpcon_dev *dpcon);

#endif /* __DPAA2_EVENTDEV_H__ */
