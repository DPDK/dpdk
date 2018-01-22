/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__
#define __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_sched.h>
#include <rte_ethdev_driver.h>
#include <rte_tm_driver.h>

#include "rte_eth_softnic.h"

/**
 * PMD Parameters
 */

enum pmd_feature {
	PMD_FEATURE_TM = 1, /**< Traffic Management (TM) */
};

#ifndef INTRUSIVE
#define INTRUSIVE					0
#endif

struct pmd_params {
	/** Parameters for the soft device (to be created) */
	struct {
		const char *name; /**< Name */
		uint32_t flags; /**< Flags */

		/** 0 = Access hard device though API only (potentially slower,
		 *      but safer);
		 *  1 = Access hard device private data structures is allowed
		 *      (potentially faster).
		 */
		int intrusive;

		/** Traffic Management (TM) */
		struct {
			uint32_t rate; /**< Rate (bytes/second) */
			uint32_t nb_queues; /**< Number of queues */
			uint16_t qsize[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
			/**< Queue size per traffic class */
			uint32_t enq_bsz; /**< Enqueue burst size */
			uint32_t deq_bsz; /**< Dequeue burst size */
		} tm;
	} soft;

	/** Parameters for the hard device (existing) */
	struct {
		char *name; /**< Name */
		uint16_t tx_queue_id; /**< TX queue ID */
	} hard;
};

/**
 * Default Internals
 */

#ifndef DEFAULT_BURST_SIZE
#define DEFAULT_BURST_SIZE				32
#endif

#ifndef FLUSH_COUNT_THRESHOLD
#define FLUSH_COUNT_THRESHOLD			(1 << 17)
#endif

struct default_internals {
	struct rte_mbuf **pkts;
	uint32_t pkts_len;
	uint32_t txq_pos;
	uint32_t flush_count;
};

/**
 * Traffic Management (TM) Internals
 */

#ifndef TM_MAX_SUBPORTS
#define TM_MAX_SUBPORTS					8
#endif

#ifndef TM_MAX_PIPES_PER_SUBPORT
#define TM_MAX_PIPES_PER_SUBPORT			4096
#endif

struct tm_params {
	struct rte_sched_port_params port_params;

	struct rte_sched_subport_params subport_params[TM_MAX_SUBPORTS];

	struct rte_sched_pipe_params
		pipe_profiles[RTE_SCHED_PIPE_PROFILES_PER_PORT];
	uint32_t n_pipe_profiles;
	uint32_t pipe_to_profile[TM_MAX_SUBPORTS * TM_MAX_PIPES_PER_SUBPORT];
};

/* TM Levels */
enum tm_node_level {
	TM_NODE_LEVEL_PORT = 0,
	TM_NODE_LEVEL_SUBPORT,
	TM_NODE_LEVEL_PIPE,
	TM_NODE_LEVEL_TC,
	TM_NODE_LEVEL_QUEUE,
	TM_NODE_LEVEL_MAX,
};

/* TM Shaper Profile */
struct tm_shaper_profile {
	TAILQ_ENTRY(tm_shaper_profile) node;
	uint32_t shaper_profile_id;
	uint32_t n_users;
	struct rte_tm_shaper_params params;
};

TAILQ_HEAD(tm_shaper_profile_list, tm_shaper_profile);

/* TM Shared Shaper */
struct tm_shared_shaper {
	TAILQ_ENTRY(tm_shared_shaper) node;
	uint32_t shared_shaper_id;
	uint32_t n_users;
	uint32_t shaper_profile_id;
};

TAILQ_HEAD(tm_shared_shaper_list, tm_shared_shaper);

/* TM WRED Profile */
struct tm_wred_profile {
	TAILQ_ENTRY(tm_wred_profile) node;
	uint32_t wred_profile_id;
	uint32_t n_users;
	struct rte_tm_wred_params params;
};

TAILQ_HEAD(tm_wred_profile_list, tm_wred_profile);

/* TM Node */
struct tm_node {
	TAILQ_ENTRY(tm_node) node;
	uint32_t node_id;
	uint32_t parent_node_id;
	uint32_t priority;
	uint32_t weight;
	uint32_t level;
	struct tm_node *parent_node;
	struct tm_shaper_profile *shaper_profile;
	struct tm_wred_profile *wred_profile;
	struct rte_tm_node_params params;
	struct rte_tm_node_stats stats;
	uint32_t n_children;
};

TAILQ_HEAD(tm_node_list, tm_node);

/* TM Hierarchy Specification */
struct tm_hierarchy {
	struct tm_shaper_profile_list shaper_profiles;
	struct tm_shared_shaper_list shared_shapers;
	struct tm_wred_profile_list wred_profiles;
	struct tm_node_list nodes;

	uint32_t n_shaper_profiles;
	uint32_t n_shared_shapers;
	uint32_t n_wred_profiles;
	uint32_t n_nodes;

	uint32_t n_tm_nodes[TM_NODE_LEVEL_MAX];
};

struct tm_internals {
	/** Hierarchy specification
	 *
	 *     -Hierarchy is unfrozen at init and when port is stopped.
	 *     -Hierarchy is frozen on successful hierarchy commit.
	 *     -Run-time hierarchy changes are not allowed, therefore it makes
	 *      sense to keep the hierarchy frozen after the port is started.
	 */
	struct tm_hierarchy h;
	int hierarchy_frozen;

	/** Blueprints */
	struct tm_params params;

	/** Run-time */
	struct rte_sched_port *sched;
	struct rte_mbuf **pkts_enq;
	struct rte_mbuf **pkts_deq;
	uint32_t pkts_enq_len;
	uint32_t txq_pos;
	uint32_t flush_count;
};

/**
 * PMD Internals
 */
struct pmd_internals {
	/** Params */
	struct pmd_params params;

	/** Soft device */
	struct {
		struct default_internals def; /**< Default */
		struct tm_internals tm; /**< Traffic Management */
	} soft;

	/** Hard device */
	struct {
		uint16_t port_id;
	} hard;
};

struct pmd_rx_queue {
	/** Hard device */
	struct {
		uint16_t port_id;
		uint16_t rx_queue_id;
	} hard;
};

/**
 * Traffic Management (TM) Operation
 */
extern const struct rte_tm_ops pmd_tm_ops;

int
tm_params_check(struct pmd_params *params, uint32_t hard_rate);

int
tm_init(struct pmd_internals *p, struct pmd_params *params, int numa_node);

void
tm_free(struct pmd_internals *p);

int
tm_start(struct pmd_internals *p);

void
tm_stop(struct pmd_internals *p);

static inline int
tm_enabled(struct rte_eth_dev *dev)
{
	struct pmd_internals *p = dev->data->dev_private;

	return (p->params.soft.flags & PMD_FEATURE_TM);
}

static inline int
tm_used(struct rte_eth_dev *dev)
{
	struct pmd_internals *p = dev->data->dev_private;

	return (p->params.soft.flags & PMD_FEATURE_TM) &&
		p->soft.tm.h.n_tm_nodes[TM_NODE_LEVEL_PORT];
}

#endif /* __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__ */
