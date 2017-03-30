/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

#ifndef _SW_EVDEV_H_
#define _SW_EVDEV_H_

#include <rte_eventdev.h>
#include <rte_eventdev_pmd.h>
#include <rte_atomic.h>

#define SW_DEFAULT_CREDIT_QUANTA 32
#define SW_DEFAULT_SCHED_QUANTA 128
#define SW_QID_NUM_FIDS 16384
#define SW_IQS_MAX 4
#define SW_Q_PRIORITY_MAX 255
#define SW_PORTS_MAX 64
#define MAX_SW_CONS_Q_DEPTH 128
#define SW_INFLIGHT_EVENTS_TOTAL 4096
/* allow for lots of over-provisioning */
#define MAX_SW_PROD_Q_DEPTH 4096
#define SW_FRAGMENTS_MAX 16

#define EVENTDEV_NAME_SW_PMD event_sw
#define SW_PMD_NAME RTE_STR(event_sw)

#ifdef RTE_LIBRTE_PMD_EVDEV_SW_DEBUG
#define SW_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, EVENTDEV, "[%s] %s() line %u: " fmt "\n", \
			SW_PMD_NAME, \
			__func__, __LINE__, ## args)

#define SW_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, EVENTDEV, "[%s] %s() line %u: " fmt "\n", \
			SW_PMD_NAME, \
			__func__, __LINE__, ## args)
#else
#define SW_LOG_INFO(fmt, args...)
#define SW_LOG_DBG(fmt, args...)
#endif

#define SW_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, EVENTDEV, "[%s] %s() line %u: " fmt "\n", \
			SW_PMD_NAME, \
			__func__, __LINE__, ## args)

/* Records basic event stats at a given point. Used in port and qid structs */
struct sw_point_stats {
	uint64_t rx_pkts;
	uint64_t rx_dropped;
	uint64_t tx_pkts;
};

/* structure used to track what port a flow (FID) is pinned to */
struct sw_fid_t {
	/* which CQ this FID is currently pinned to */
	int32_t cq;
	/* number of packets gone to the CQ with this FID */
	uint32_t pcount;
};

struct reorder_buffer_entry {
	uint16_t num_fragments;		/**< Number of packet fragments */
	uint16_t fragment_index;	/**< Points to the oldest valid frag */
	uint8_t ready;			/**< Entry is ready to be reordered */
	struct rte_event fragments[SW_FRAGMENTS_MAX];
};

struct sw_qid {
	/* set when the QID has been initialized */
	uint8_t initialized;
	/* The type of this QID */
	int8_t type;
	/* Integer ID representing the queue. This is used in history lists,
	 * to identify the stage of processing.
	 */
	uint32_t id;
	struct sw_point_stats stats;

	/* Internal priority rings for packets */
	struct iq_ring *iq[SW_IQS_MAX];
	uint32_t iq_pkt_mask; /* A mask to indicate packets in an IQ */
	uint64_t iq_pkt_count[SW_IQS_MAX];

	/* Information on what CQs are polling this IQ */
	uint32_t cq_num_mapped_cqs;
	uint32_t cq_next_tx; /* cq to write next (non-atomic) packet */
	uint32_t cq_map[SW_PORTS_MAX];

	/* Track flow ids for atomic load balancing */
	struct sw_fid_t fids[SW_QID_NUM_FIDS];

	/* Track packet order for reordering when needed */
	struct reorder_buffer_entry *reorder_buffer; /*< pkts await reorder */
	struct rte_ring *reorder_buffer_freelist; /* available reorder slots */
	uint32_t reorder_buffer_index; /* oldest valid reorder buffer entry */
	uint32_t window_size;          /* Used to wrap reorder_buffer_index */

	uint8_t priority;
};

struct sw_evdev {
	struct rte_eventdev_data *data;

	uint32_t port_count;
	uint32_t qid_count;

	/*
	 * max events in this instance. Cached here for performance.
	 * (also available in data->conf.nb_events_limit)
	 */
	uint32_t nb_events_limit;

	int32_t sched_quanta;

	uint32_t credit_update_quanta;
};

static inline struct sw_evdev *
sw_pmd_priv(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

static inline const struct sw_evdev *
sw_pmd_priv_const(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

#endif /* _SW_EVDEV_H_ */
