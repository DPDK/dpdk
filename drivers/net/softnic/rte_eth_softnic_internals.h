/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__
#define __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_ethdev.h>
#include <rte_sched.h>
#include <rte_ethdev_driver.h>
#include <rte_tm_driver.h>

#include "rte_eth_softnic.h"

#define NAME_SIZE                                            64

/**
 * PMD Parameters
 */

struct pmd_params {
	const char *name;
	const char *firmware;
	uint32_t cpu_id;

	/** Traffic Management (TM) */
	struct {
		uint32_t n_queues; /**< Number of queues */
		uint16_t qsize[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	} tm;
};

/**
 * SWQ
 */
struct softnic_swq_params {
	uint32_t size;
};

struct softnic_swq {
	TAILQ_ENTRY(softnic_swq) node;
	char name[NAME_SIZE];
	struct rte_ring *r;
};

TAILQ_HEAD(softnic_swq_list, softnic_swq);

/**
 * LINK
 */
struct softnic_link_params {
	const char *dev_name;
	uint16_t port_id; /**< Valid only when *dev_name* is NULL. */
};

struct softnic_link {
	TAILQ_ENTRY(softnic_link) node;
	char name[NAME_SIZE];
	uint16_t port_id;
	uint32_t n_rxq;
	uint32_t n_txq;
};

TAILQ_HEAD(softnic_link_list, softnic_link);

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
	struct rte_sched_port *sched;
};

/**
 * PMD Internals
 */
struct pmd_internals {
	/** Params */
	struct pmd_params params;

	/** Soft device */
	struct {
		struct tm_internals tm; /**< Traffic Management */
	} soft;

	struct softnic_swq_list swq_list;
	struct softnic_link_list link_list;
};

/**
 * SWQ
 */
int
softnic_swq_init(struct pmd_internals *p);

void
softnic_swq_free(struct pmd_internals *p);

struct softnic_swq *
softnic_swq_find(struct pmd_internals *p,
	const char *name);

struct softnic_swq *
softnic_swq_create(struct pmd_internals *p,
	const char *name,
	struct softnic_swq_params *params);

/**
 * LINK
 */
int
softnic_link_init(struct pmd_internals *p);

void
softnic_link_free(struct pmd_internals *p);

struct softnic_link *
softnic_link_find(struct pmd_internals *p,
	const char *name);

struct softnic_link *
softnic_link_create(struct pmd_internals *p,
	const char *name,
	struct softnic_link_params *params);

/**
 * Traffic Management (TM) Operation
 */
extern const struct rte_tm_ops pmd_tm_ops;

int
tm_init(struct pmd_internals *p, struct pmd_params *params, int numa_node);

void
tm_free(struct pmd_internals *p);

int
tm_start(struct pmd_internals *p);

void
tm_stop(struct pmd_internals *p);

static inline int
tm_used(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

#endif /* __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__ */
