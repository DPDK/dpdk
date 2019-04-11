/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_string_fns.h>
#include <rte_cryptodev.h>

#include "rte_eth_softnic_internals.h"
#include "parser.h"

#ifndef CMD_MAX_TOKENS
#define CMD_MAX_TOKENS     256
#endif

#define MSG_OUT_OF_MEMORY   "Not enough memory.\n"
#define MSG_CMD_UNKNOWN     "Unknown command \"%s\".\n"
#define MSG_CMD_UNIMPLEM    "Command \"%s\" not implemented.\n"
#define MSG_ARG_NOT_ENOUGH  "Not enough arguments for command \"%s\".\n"
#define MSG_ARG_TOO_MANY    "Too many arguments for command \"%s\".\n"
#define MSG_ARG_MISMATCH    "Wrong number of arguments for command \"%s\".\n"
#define MSG_ARG_NOT_FOUND   "Argument \"%s\" not found.\n"
#define MSG_ARG_INVALID     "Invalid value for argument \"%s\".\n"
#define MSG_FILE_ERR        "Error in file \"%s\" at line %u.\n"
#define MSG_FILE_NOT_ENOUGH "Not enough rules in file \"%s\".\n"
#define MSG_CMD_FAIL        "Command \"%s\" failed.\n"

static int
is_comment(char *in)
{
	if ((strlen(in) && index("!#%;", in[0])) ||
		(strncmp(in, "//", 2) == 0) ||
		(strncmp(in, "--", 2) == 0))
		return 1;

	return 0;
}

/**
 * mempool <mempool_name>
 *  buffer <buffer_size>
 *  pool <pool_size>
 *  cache <cache_size>
 */
static void
cmd_mempool(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_mempool_params p;
	char *name;
	struct softnic_mempool *mempool;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "buffer") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "buffer");
		return;
	}

	if (softnic_parser_read_uint32(&p.buffer_size, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "buffer_size");
		return;
	}

	if (strcmp(tokens[4], "pool") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pool");
		return;
	}

	if (softnic_parser_read_uint32(&p.pool_size, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pool_size");
		return;
	}

	if (strcmp(tokens[6], "cache") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cache");
		return;
	}

	if (softnic_parser_read_uint32(&p.cache_size, tokens[7]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cache_size");
		return;
	}

	mempool = softnic_mempool_create(softnic, name, &p);
	if (mempool == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * link <link_name>
 *    dev <device_name> | port <port_id>
 */
static void
cmd_link(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_link_params p;
	struct softnic_link *link;
	char *name;

	memset(&p, 0, sizeof(p));

	if (n_tokens != 4) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}
	name = tokens[1];

	if (strcmp(tokens[2], "dev") == 0) {
		p.dev_name = tokens[3];
	} else if (strcmp(tokens[2], "port") == 0) {
		p.dev_name = NULL;

		if (softnic_parser_read_uint16(&p.port_id, tokens[3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
			return;
		}
	} else {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "dev or port");
		return;
	}

	link = softnic_link_create(softnic, name, &p);
	if (link == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * swq <swq_name>
 *  size <size>
 */
static void
cmd_swq(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_swq_params p;
	char *name;
	struct softnic_swq *swq;

	if (n_tokens != 4) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "size") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
		return;
	}

	if (softnic_parser_read_uint32(&p.size, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "size");
		return;
	}

	swq = softnic_swq_create(softnic, name, &p);
	if (swq == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tmgr shaper profile
 *  id <profile_id>
 *  rate <tb_rate> size <tb_size>
 *  adj <packet_length_adjust>
 */
static void
cmd_tmgr_shaper_profile(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_tm_shaper_params sp;
	struct rte_tm_error error;
	uint32_t shaper_profile_id;
	uint16_t port_id;
	int status;

	memset(&sp, 0, sizeof(struct rte_tm_shaper_params));

	if (n_tokens != 11) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "shaper") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "shaper");
		return;
	}

	if (strcmp(tokens[2], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	if (strcmp(tokens[3], "id") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "id");
		return;
	}

	if (softnic_parser_read_uint32(&shaper_profile_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "profile_id");
		return;
	}

	if (strcmp(tokens[5], "rate") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rate");
		return;
	}

	if (softnic_parser_read_uint64(&sp.peak.rate, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tb_rate");
		return;
	}

	if (strcmp(tokens[7], "size") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
		return;
	}

	if (softnic_parser_read_uint64(&sp.peak.size, tokens[8]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tb_size");
		return;
	}

	if (strcmp(tokens[9], "adj") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "adj");
		return;
	}

	if (softnic_parser_read_int32(&sp.pkt_length_adjust, tokens[10]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "packet_length_adjust");
		return;
	}

	status = rte_eth_dev_get_port_by_name(softnic->params.name, &port_id);
	if (status)
		return;

	status = rte_tm_shaper_profile_add(port_id, shaper_profile_id, &sp, &error);
	if (status != 0) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tmgr shared shaper
 *  id <shared_shaper_id>
 *  profile <shaper_profile_id>
 */
static void
cmd_tmgr_shared_shaper(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_tm_error error;
	uint32_t shared_shaper_id, shaper_profile_id;
	uint16_t port_id;
	int status;

	if (n_tokens != 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "shared") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "shared");
		return;
	}

	if (strcmp(tokens[2], "shaper") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "shaper");
		return;
	}

	if (strcmp(tokens[3], "id") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "id");
		return;
	}

	if (softnic_parser_read_uint32(&shared_shaper_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "shared_shaper_id");
		return;
	}

	if (strcmp(tokens[5], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	if (softnic_parser_read_uint32(&shaper_profile_id, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "shaper_profile_id");
		return;
	}

	status = rte_eth_dev_get_port_by_name(softnic->params.name, &port_id);
	if (status)
		return;

	status = rte_tm_shared_shaper_add_update(port_id,
		shared_shaper_id,
		shaper_profile_id,
		&error);
	if (status != 0) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tmgr node
 *   id <node_id>
 *   parent <parent_node_id | none>
 *   priority <priority>
 *   weight <weight>
 *   [shaper profile <shaper_profile_id>]
 *   [shared shaper <shared_shaper_id>]
 *   [nonleaf sp <n_sp_priorities>]
 */
static void
cmd_tmgr_node(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_tm_error error;
	struct rte_tm_node_params np;
	uint32_t node_id, parent_node_id, priority, weight, shared_shaper_id;
	uint16_t port_id;
	int status;

	memset(&np, 0, sizeof(struct rte_tm_node_params));
	np.shaper_profile_id = RTE_TM_SHAPER_PROFILE_ID_NONE;
	np.nonleaf.n_sp_priorities = 1;

	if (n_tokens < 10) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "node") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "node");
		return;
	}

	if (strcmp(tokens[2], "id") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "id");
		return;
	}

	if (softnic_parser_read_uint32(&node_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "node_id");
		return;
	}

	if (strcmp(tokens[4], "parent") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "parent");
		return;
	}

	if (strcmp(tokens[5], "none") == 0)
		parent_node_id = RTE_TM_NODE_ID_NULL;
	else {
		if (softnic_parser_read_uint32(&parent_node_id, tokens[5]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "parent_node_id");
			return;
		}
	}

	if (strcmp(tokens[6], "priority") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "priority");
		return;
	}

	if (softnic_parser_read_uint32(&priority, tokens[7]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "priority");
		return;
	}

	if (strcmp(tokens[8], "weight") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "weight");
		return;
	}

	if (softnic_parser_read_uint32(&weight, tokens[9]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "weight");
		return;
	}

	tokens += 10;
	n_tokens -= 10;

	if (n_tokens >= 2 &&
		(strcmp(tokens[0], "shaper") == 0) &&
		(strcmp(tokens[1], "profile") == 0)) {
		if (n_tokens < 3) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "tmgr node");
			return;
		}

		if (strcmp(tokens[2], "none") == 0) {
			np.shaper_profile_id = RTE_TM_SHAPER_PROFILE_ID_NONE;
		} else {
			if (softnic_parser_read_uint32(&np.shaper_profile_id, tokens[2]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "shaper_profile_id");
				return;
			}
		}

		tokens += 3;
		n_tokens -= 3;
	} /* shaper profile */

	if (n_tokens >= 2 &&
		(strcmp(tokens[0], "shared") == 0) &&
		(strcmp(tokens[1], "shaper") == 0)) {
		if (n_tokens < 3) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "tmgr node");
			return;
		}

		if (softnic_parser_read_uint32(&shared_shaper_id, tokens[2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "shared_shaper_id");
			return;
		}

		np.shared_shaper_id = &shared_shaper_id;
		np.n_shared_shapers = 1;

		tokens += 3;
		n_tokens -= 3;
	} /* shared shaper */

	if (n_tokens >= 2 &&
		(strcmp(tokens[0], "nonleaf") == 0) &&
		(strcmp(tokens[1], "sp") == 0)) {
		if (n_tokens < 3) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "tmgr node");
			return;
		}

		if (softnic_parser_read_uint32(&np.nonleaf.n_sp_priorities, tokens[2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_sp_priorities");
			return;
		}

		tokens += 3;
		n_tokens -= 3;
	} /* nonleaf sp <n_sp_priorities> */

	if (n_tokens) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	status = rte_eth_dev_get_port_by_name(softnic->params.name, &port_id);
	if (status != 0)
		return;

	status = rte_tm_node_add(port_id,
		node_id,
		parent_node_id,
		priority,
		weight,
		RTE_TM_NODE_LEVEL_ID_ANY,
		&np,
		&error);
	if (status != 0) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static uint32_t
root_node_id(uint32_t n_spp,
	uint32_t n_pps)
{
	uint32_t n_queues = n_spp * n_pps * RTE_SCHED_QUEUES_PER_PIPE;
	uint32_t n_tc = n_spp * n_pps * RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE;
	uint32_t n_pipes = n_spp * n_pps;

	return n_queues + n_tc + n_pipes + n_spp;
}

static uint32_t
subport_node_id(uint32_t n_spp,
	uint32_t n_pps,
	uint32_t subport_id)
{
	uint32_t n_pipes = n_spp * n_pps;
	uint32_t n_tc = n_pipes * RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE;
	uint32_t n_queues = n_pipes * RTE_SCHED_QUEUES_PER_PIPE;

	return n_queues + n_tc + n_pipes + subport_id;
}

static uint32_t
pipe_node_id(uint32_t n_spp,
	uint32_t n_pps,
	uint32_t subport_id,
	uint32_t pipe_id)
{
	uint32_t n_pipes = n_spp * n_pps;
	uint32_t n_tc = n_pipes * RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE;
	uint32_t n_queues = n_pipes * RTE_SCHED_QUEUES_PER_PIPE;

	return n_queues +
		n_tc +
		pipe_id +
		subport_id * n_pps;
}

static uint32_t
tc_node_id(uint32_t n_spp,
	uint32_t n_pps,
	uint32_t subport_id,
	uint32_t pipe_id,
	uint32_t tc_id)
{
	uint32_t n_pipes = n_spp * n_pps;
	uint32_t n_queues = n_pipes * RTE_SCHED_QUEUES_PER_PIPE;

	return n_queues +
		tc_id +
		(pipe_id + subport_id * n_pps) * RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE;
}

static uint32_t
queue_node_id(uint32_t n_spp __rte_unused,
	uint32_t n_pps,
	uint32_t subport_id,
	uint32_t pipe_id,
	uint32_t tc_id,
	uint32_t queue_id)
{
	return queue_id +
		tc_id * RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE +
		(pipe_id + subport_id * n_pps) * RTE_SCHED_QUEUES_PER_PIPE;
}

struct tmgr_hierarchy_default_params {
	uint32_t n_spp; /**< Number of subports per port. */
	uint32_t n_pps; /**< Number of pipes per subport. */

	struct {
		uint32_t port;
		uint32_t subport;
		uint32_t pipe;
		uint32_t tc[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	} shaper_profile_id;

	struct {
		uint32_t tc[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
		uint32_t tc_valid[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	} shared_shaper_id;

	struct {
		uint32_t queue[RTE_SCHED_QUEUES_PER_PIPE];
	} weight;
};

static int
tmgr_hierarchy_default(struct pmd_internals *softnic,
	struct tmgr_hierarchy_default_params *params)
{
	struct rte_tm_node_params root_node_params = {
		.shaper_profile_id = params->shaper_profile_id.port,
		.nonleaf = {
			.n_sp_priorities = 1,
		},
	};

	struct rte_tm_node_params subport_node_params = {
		.shaper_profile_id = params->shaper_profile_id.subport,
		.nonleaf = {
			.n_sp_priorities = 1,
		},
	};

	struct rte_tm_node_params pipe_node_params = {
		.shaper_profile_id = params->shaper_profile_id.pipe,
		.nonleaf = {
			.n_sp_priorities = RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE,
		},
	};

	struct rte_tm_node_params tc_node_params[] = {
		[0] = {
			.shaper_profile_id = params->shaper_profile_id.tc[0],
			.shared_shaper_id = &params->shared_shaper_id.tc[0],
			.n_shared_shapers =
				(&params->shared_shaper_id.tc_valid[0]) ? 1 : 0,
			.nonleaf = {
				.n_sp_priorities = 1,
			},
		},

		[1] = {
			.shaper_profile_id = params->shaper_profile_id.tc[1],
			.shared_shaper_id = &params->shared_shaper_id.tc[1],
			.n_shared_shapers =
				(&params->shared_shaper_id.tc_valid[1]) ? 1 : 0,
			.nonleaf = {
				.n_sp_priorities = 1,
			},
		},

		[2] = {
			.shaper_profile_id = params->shaper_profile_id.tc[2],
			.shared_shaper_id = &params->shared_shaper_id.tc[2],
			.n_shared_shapers =
				(&params->shared_shaper_id.tc_valid[2]) ? 1 : 0,
			.nonleaf = {
				.n_sp_priorities = 1,
			},
		},

		[3] = {
			.shaper_profile_id = params->shaper_profile_id.tc[3],
			.shared_shaper_id = &params->shared_shaper_id.tc[3],
			.n_shared_shapers =
				(&params->shared_shaper_id.tc_valid[3]) ? 1 : 0,
			.nonleaf = {
				.n_sp_priorities = 1,
			},
		},
	};

	struct rte_tm_node_params queue_node_params = {
		.shaper_profile_id = RTE_TM_SHAPER_PROFILE_ID_NONE,
	};

	struct rte_tm_error error;
	uint32_t n_spp = params->n_spp, n_pps = params->n_pps, s;
	int status;
	uint16_t port_id;

	status = rte_eth_dev_get_port_by_name(softnic->params.name, &port_id);
	if (status)
		return -1;

	/* Hierarchy level 0: Root node */
	status = rte_tm_node_add(port_id,
		root_node_id(n_spp, n_pps),
		RTE_TM_NODE_ID_NULL,
		0,
		1,
		RTE_TM_NODE_LEVEL_ID_ANY,
		&root_node_params,
		&error);
	if (status)
		return -1;

	/* Hierarchy level 1: Subport nodes */
	for (s = 0; s < params->n_spp; s++) {
		uint32_t p;

		status = rte_tm_node_add(port_id,
			subport_node_id(n_spp, n_pps, s),
			root_node_id(n_spp, n_pps),
			0,
			1,
			RTE_TM_NODE_LEVEL_ID_ANY,
			&subport_node_params,
			&error);
		if (status)
			return -1;

		/* Hierarchy level 2: Pipe nodes */
		for (p = 0; p < params->n_pps; p++) {
			uint32_t t;

			status = rte_tm_node_add(port_id,
				pipe_node_id(n_spp, n_pps, s, p),
				subport_node_id(n_spp, n_pps, s),
				0,
				1,
				RTE_TM_NODE_LEVEL_ID_ANY,
				&pipe_node_params,
				&error);
			if (status)
				return -1;

			/* Hierarchy level 3: Traffic class nodes */
			for (t = 0; t < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; t++) {
				uint32_t q;

				status = rte_tm_node_add(port_id,
					tc_node_id(n_spp, n_pps, s, p, t),
					pipe_node_id(n_spp, n_pps, s, p),
					t,
					1,
					RTE_TM_NODE_LEVEL_ID_ANY,
					&tc_node_params[t],
					&error);
				if (status)
					return -1;

				/* Hierarchy level 4: Queue nodes */
				for (q = 0; q < RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; q++) {
					status = rte_tm_node_add(port_id,
						queue_node_id(n_spp, n_pps, s, p, t, q),
						tc_node_id(n_spp, n_pps, s, p, t),
						0,
						params->weight.queue[q],
						RTE_TM_NODE_LEVEL_ID_ANY,
						&queue_node_params,
						&error);
					if (status)
						return -1;
				} /* Queue */
			} /* TC */
		} /* Pipe */
	} /* Subport */

	return 0;
}


/**
 * tmgr hierarchy-default
 *  spp <n_subports_per_port>
 *  pps <n_pipes_per_subport>
 *  shaper profile
 *   port <profile_id>
 *   subport <profile_id>
 *   pipe <profile_id>
 *   tc0 <profile_id>
 *   tc1 <profile_id>
 *   tc2 <profile_id>
 *   tc3 <profile_id>
 *  shared shaper
 *   tc0 <id | none>
 *   tc1 <id | none>
 *   tc2 <id | none>
 *   tc3 <id | none>
 *  weight
 *   queue  <q0> ... <q15>
 */
static void
cmd_tmgr_hierarchy_default(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct tmgr_hierarchy_default_params p;
	int i, status;

	memset(&p, 0, sizeof(p));

	if (n_tokens != 50) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "hierarchy-default") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "hierarchy-default");
		return;
	}

	if (strcmp(tokens[2], "spp") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "spp");
		return;
	}

	if (softnic_parser_read_uint32(&p.n_spp, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_subports_per_port");
		return;
	}

	if (strcmp(tokens[4], "pps") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pps");
		return;
	}

	if (softnic_parser_read_uint32(&p.n_pps, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_pipes_per_subport");
		return;
	}

	/* Shaper profile */

	if (strcmp(tokens[6], "shaper") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "shaper");
		return;
	}

	if (strcmp(tokens[7], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	if (strcmp(tokens[8], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (softnic_parser_read_uint32(&p.shaper_profile_id.port, tokens[9]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port profile id");
		return;
	}

	if (strcmp(tokens[10], "subport") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "subport");
		return;
	}

	if (softnic_parser_read_uint32(&p.shaper_profile_id.subport, tokens[11]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "subport profile id");
		return;
	}

	if (strcmp(tokens[12], "pipe") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipe");
		return;
	}

	if (softnic_parser_read_uint32(&p.shaper_profile_id.pipe, tokens[13]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipe_profile_id");
		return;
	}

	if (strcmp(tokens[14], "tc0") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc0");
		return;
	}

	if (softnic_parser_read_uint32(&p.shaper_profile_id.tc[0], tokens[15]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tc0 profile id");
		return;
	}

	if (strcmp(tokens[16], "tc1") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc1");
		return;
	}

	if (softnic_parser_read_uint32(&p.shaper_profile_id.tc[1], tokens[17]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tc1 profile id");
		return;
	}

	if (strcmp(tokens[18], "tc2") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc2");
		return;
	}

	if (softnic_parser_read_uint32(&p.shaper_profile_id.tc[2], tokens[19]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tc2 profile id");
		return;
	}

	if (strcmp(tokens[20], "tc3") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc3");
		return;
	}

	if (softnic_parser_read_uint32(&p.shaper_profile_id.tc[3], tokens[21]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tc3 profile id");
		return;
	}

	/* Shared shaper */

	if (strcmp(tokens[22], "shared") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "shared");
		return;
	}

	if (strcmp(tokens[23], "shaper") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "shaper");
		return;
	}

	if (strcmp(tokens[24], "tc0") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc0");
		return;
	}

	if (strcmp(tokens[25], "none") == 0)
		p.shared_shaper_id.tc_valid[0] = 0;
	else {
		if (softnic_parser_read_uint32(&p.shared_shaper_id.tc[0], tokens[25]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "shared shaper tc0");
			return;
		}

		p.shared_shaper_id.tc_valid[0] = 1;
	}

	if (strcmp(tokens[26], "tc1") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc1");
		return;
	}

	if (strcmp(tokens[27], "none") == 0)
		p.shared_shaper_id.tc_valid[1] = 0;
	else {
		if (softnic_parser_read_uint32(&p.shared_shaper_id.tc[1], tokens[27]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "shared shaper tc1");
			return;
		}

		p.shared_shaper_id.tc_valid[1] = 1;
	}

	if (strcmp(tokens[28], "tc2") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc2");
		return;
	}

	if (strcmp(tokens[29], "none") == 0)
		p.shared_shaper_id.tc_valid[2] = 0;
	else {
		if (softnic_parser_read_uint32(&p.shared_shaper_id.tc[2], tokens[29]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "shared shaper tc2");
			return;
		}

		p.shared_shaper_id.tc_valid[2] = 1;
	}

	if (strcmp(tokens[30], "tc3") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc3");
		return;
	}

	if (strcmp(tokens[31], "none") == 0)
		p.shared_shaper_id.tc_valid[3] = 0;
	else {
		if (softnic_parser_read_uint32(&p.shared_shaper_id.tc[3], tokens[31]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "shared shaper tc3");
			return;
		}

		p.shared_shaper_id.tc_valid[3] = 1;
	}

	/* Weight */

	if (strcmp(tokens[32], "weight") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "weight");
		return;
	}

	if (strcmp(tokens[33], "queue") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "queue");
		return;
	}

	for (i = 0; i < 16; i++) {
		if (softnic_parser_read_uint32(&p.weight.queue[i], tokens[34 + i]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "weight queue");
			return;
		}
	}

	status = tmgr_hierarchy_default(softnic, &p);
	if (status != 0) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tmgr hierarchy commit
 */
static void
cmd_tmgr_hierarchy_commit(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_tm_error error;
	uint16_t port_id;
	int status;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "hierarchy") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "hierarchy");
		return;
	}

	if (strcmp(tokens[2], "commit") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "commit");
		return;
	}

	status = rte_eth_dev_get_port_by_name(softnic->params.name, &port_id);
	if (status != 0)
		return;

	status = rte_tm_hierarchy_commit(port_id, 1, &error);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tmgr <tmgr_name>
 */
static void
cmd_tmgr(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *name;
	struct softnic_tmgr_port *tmgr_port;

	if (n_tokens != 2) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	tmgr_port = softnic_tmgr_port_create(softnic, name);
	if (tmgr_port == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tap <tap_name>
 */
static void
cmd_tap(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *name;
	struct softnic_tap *tap;

	if (n_tokens != 2) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	tap = softnic_tap_create(softnic, name);
	if (tap == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * cryptodev <tap_name> dev <device_name> | dev_id <device_id>
 * queue <n_queues> <queue_size> max_sessions <n_sessions>
 **/

static void
cmd_cryptodev(struct pmd_internals *softnic,
		char **tokens,
		uint32_t n_tokens,
		char *out,
		size_t out_size)
{
	struct softnic_cryptodev_params params;
	char *name;

	memset(&params, 0, sizeof(params));
	if (n_tokens != 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "dev") == 0)
		params.dev_name = tokens[3];
	else if (strcmp(tokens[2], "dev_id") == 0) {
		if (softnic_parser_read_uint32(&params.dev_id, tokens[3]) < 0) {
			snprintf(out, out_size,	MSG_ARG_INVALID,
				"dev_id");
			return;
		}
	} else {
		snprintf(out, out_size,	MSG_ARG_INVALID,
			"cryptodev");
		return;
	}

	if (strcmp(tokens[4], "queue")) {
		snprintf(out, out_size,	MSG_ARG_NOT_FOUND,
			"4");
		return;
	}

	if (softnic_parser_read_uint32(&params.n_queues, tokens[5]) < 0) {
		snprintf(out, out_size,	MSG_ARG_INVALID,
			"q");
		return;
	}

	if (softnic_parser_read_uint32(&params.queue_size, tokens[6]) < 0) {
		snprintf(out, out_size,	MSG_ARG_INVALID,
			"queue_size");
		return;
	}

	if (strcmp(tokens[7], "max_sessions")) {
		snprintf(out, out_size,	MSG_ARG_NOT_FOUND,
			"4");
		return;
	}

	if (softnic_parser_read_uint32(&params.session_pool_size, tokens[8])
			< 0) {
		snprintf(out, out_size,	MSG_ARG_INVALID,
			"q");
		return;
	}

	if (softnic_cryptodev_create(softnic, name, &params) == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * port in action profile <profile_name>
 *  [filter match | mismatch offset <key_offset> mask <key_mask> key <key_value> port <port_id>]
 *  [balance offset <key_offset> mask <key_mask> port <port_id0> ... <port_id15>]
 */
static void
cmd_port_in_action_profile(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_port_in_action_profile_params p;
	struct softnic_port_in_action_profile *ap;
	char *name;
	uint32_t t0;

	memset(&p, 0, sizeof(p));

	if (n_tokens < 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (strcmp(tokens[2], "action") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "action");
		return;
	}

	if (strcmp(tokens[3], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	name = tokens[4];

	t0 = 5;

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "filter") == 0)) {
		uint32_t size;

		if (n_tokens < t0 + 10) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "port in action profile filter");
			return;
		}

		if (strcmp(tokens[t0 + 1], "match") == 0) {
			p.fltr.filter_on_match = 1;
		} else if (strcmp(tokens[t0 + 1], "mismatch") == 0) {
			p.fltr.filter_on_match = 0;
		} else {
			snprintf(out, out_size, MSG_ARG_INVALID, "match or mismatch");
			return;
		}

		if (strcmp(tokens[t0 + 2], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.fltr.key_offset,
			tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 4], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		size = RTE_PORT_IN_ACTION_FLTR_KEY_SIZE;
		if ((softnic_parse_hex_string(tokens[t0 + 5],
			p.fltr.key_mask, &size) != 0) ||
			size != RTE_PORT_IN_ACTION_FLTR_KEY_SIZE) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}

		if (strcmp(tokens[t0 + 6], "key") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "key");
			return;
		}

		size = RTE_PORT_IN_ACTION_FLTR_KEY_SIZE;
		if ((softnic_parse_hex_string(tokens[t0 + 7],
			p.fltr.key, &size) != 0) ||
			size != RTE_PORT_IN_ACTION_FLTR_KEY_SIZE) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_value");
			return;
		}

		if (strcmp(tokens[t0 + 8], "port") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
			return;
		}

		if (softnic_parser_read_uint32(&p.fltr.port_id,
			tokens[t0 + 9]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
			return;
		}

		p.action_mask |= 1LLU << RTE_PORT_IN_ACTION_FLTR;
		t0 += 10;
	} /* filter */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "balance") == 0)) {
		uint32_t i;

		if (n_tokens < t0 + 22) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"port in action profile balance");
			return;
		}

		if (strcmp(tokens[t0 + 1], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.lb.key_offset,
			tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 3], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		p.lb.key_size = RTE_PORT_IN_ACTION_LB_KEY_SIZE_MAX;
		if (softnic_parse_hex_string(tokens[t0 + 4],
			p.lb.key_mask, &p.lb.key_size) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}

		if (strcmp(tokens[t0 + 5], "port") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
			return;
		}

		for (i = 0; i < 16; i++)
			if (softnic_parser_read_uint32(&p.lb.port_id[i],
			tokens[t0 + 6 + i]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
				return;
			}

		p.action_mask |= 1LLU << RTE_PORT_IN_ACTION_LB;
		t0 += 22;
	} /* balance */

	if (t0 < n_tokens) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	ap = softnic_port_in_action_profile_create(softnic, name, &p);
	if (ap == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * table action profile <profile_name>
 *  ipv4 | ipv6
 *  offset <ip_offset>
 *  fwd
 *  [balance offset <key_offset> mask <key_mask> outoffset <out_offset>]
 *  [meter srtcm | trtcm
 *      tc <n_tc>
 *      stats none | pkts | bytes | both]
 *  [tm spp <n_subports_per_port> pps <n_pipes_per_subport>]
 *  [encap ether | vlan | qinq | mpls | pppoe | qinq_pppoe |
 *      vxlan offset <ether_offset> ipv4 | ipv6 vlan on | off]
 *  [nat src | dst
 *      proto udp | tcp]
 *  [ttl drop | fwd
 *      stats none | pkts]
 *  [stats pkts | bytes | both]
 *  [time]
 *  [tag]
 *  [decap]
 *
 */
static void
cmd_table_action_profile(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_table_action_profile_params p;
	struct softnic_table_action_profile *ap;
	char *name;
	uint32_t t0;

	memset(&p, 0, sizeof(p));

	if (n_tokens < 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "action") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "action");
		return;
	}

	if (strcmp(tokens[2], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "ipv4") == 0) {
		p.common.ip_version = 1;
	} else if (strcmp(tokens[4], "ipv6") == 0) {
		p.common.ip_version = 0;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, "ipv4 or ipv6");
		return;
	}

	if (strcmp(tokens[5], "offset") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
		return;
	}

	if (softnic_parser_read_uint32(&p.common.ip_offset,
		tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "ip_offset");
		return;
	}

	if (strcmp(tokens[7], "fwd") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "fwd");
		return;
	}

	p.action_mask |= 1LLU << RTE_TABLE_ACTION_FWD;

	t0 = 8;
	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "balance") == 0)) {
		if (n_tokens < t0 + 7) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "table action profile balance");
			return;
		}

		if (strcmp(tokens[t0 + 1], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.lb.key_offset,
			tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 3], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		p.lb.key_size = RTE_PORT_IN_ACTION_LB_KEY_SIZE_MAX;
		if (softnic_parse_hex_string(tokens[t0 + 4],
			p.lb.key_mask, &p.lb.key_size) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}

		if (strcmp(tokens[t0 + 5], "outoffset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "outoffset");
			return;
		}

		if (softnic_parser_read_uint32(&p.lb.out_offset,
			tokens[t0 + 6]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "out_offset");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_LB;
		t0 += 7;
	} /* balance */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "meter") == 0)) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile meter");
			return;
		}

		if (strcmp(tokens[t0 + 1], "srtcm") == 0) {
			p.mtr.alg = RTE_TABLE_ACTION_METER_SRTCM;
		} else if (strcmp(tokens[t0 + 1], "trtcm") == 0) {
			p.mtr.alg = RTE_TABLE_ACTION_METER_TRTCM;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"srtcm or trtcm");
			return;
		}

		if (strcmp(tokens[t0 + 2], "tc") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc");
			return;
		}

		if (softnic_parser_read_uint32(&p.mtr.n_tc,
			tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_tc");
			return;
		}

		if (strcmp(tokens[t0 + 4], "stats") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
			return;
		}

		if (strcmp(tokens[t0 + 5], "none") == 0) {
			p.mtr.n_packets_enabled = 0;
			p.mtr.n_bytes_enabled = 0;
		} else if (strcmp(tokens[t0 + 5], "pkts") == 0) {
			p.mtr.n_packets_enabled = 1;
			p.mtr.n_bytes_enabled = 0;
		} else if (strcmp(tokens[t0 + 5], "bytes") == 0) {
			p.mtr.n_packets_enabled = 0;
			p.mtr.n_bytes_enabled = 1;
		} else if (strcmp(tokens[t0 + 5], "both") == 0) {
			p.mtr.n_packets_enabled = 1;
			p.mtr.n_bytes_enabled = 1;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"none or pkts or bytes or both");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_MTR;
		t0 += 6;
	} /* meter */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "tm") == 0)) {
		if (n_tokens < t0 + 5) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile tm");
			return;
		}

		if (strcmp(tokens[t0 + 1], "spp") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "spp");
			return;
		}

		if (softnic_parser_read_uint32(&p.tm.n_subports_per_port,
			tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"n_subports_per_port");
			return;
		}

		if (strcmp(tokens[t0 + 3], "pps") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pps");
			return;
		}

		if (softnic_parser_read_uint32(&p.tm.n_pipes_per_subport,
			tokens[t0 + 4]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"n_pipes_per_subport");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TM;
		t0 += 5;
	} /* tm */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "encap") == 0)) {
		uint32_t n_extra_tokens = 0;

		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"action profile encap");
			return;
		}

		if (strcmp(tokens[t0 + 1], "ether") == 0) {
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_ETHER;
		} else if (strcmp(tokens[t0 + 1], "vlan") == 0) {
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_VLAN;
		} else if (strcmp(tokens[t0 + 1], "qinq") == 0) {
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_QINQ;
		} else if (strcmp(tokens[t0 + 1], "mpls") == 0) {
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_MPLS;
		} else if (strcmp(tokens[t0 + 1], "pppoe") == 0) {
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_PPPOE;
		} else if (strcmp(tokens[t0 + 1], "vxlan") == 0) {
			if (n_tokens < t0 + 2 + 5) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					"action profile encap vxlan");
				return;
			}

			if (strcmp(tokens[t0 + 2], "offset") != 0) {
				snprintf(out, out_size, MSG_ARG_NOT_FOUND,
					"vxlan: offset");
				return;
			}

			if (softnic_parser_read_uint32(&p.encap.vxlan.data_offset,
				tokens[t0 + 2 + 1]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"vxlan: ether_offset");
				return;
			}

			if (strcmp(tokens[t0 + 2 + 2], "ipv4") == 0)
				p.encap.vxlan.ip_version = 1;
			else if (strcmp(tokens[t0 + 2 + 2], "ipv6") == 0)
				p.encap.vxlan.ip_version = 0;
			else {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"vxlan: ipv4 or ipv6");
				return;
			}

			if (strcmp(tokens[t0 + 2 + 3], "vlan") != 0) {
				snprintf(out, out_size, MSG_ARG_NOT_FOUND,
					"vxlan: vlan");
				return;
			}

			if (strcmp(tokens[t0 + 2 + 4], "on") == 0)
				p.encap.vxlan.vlan = 1;
			else if (strcmp(tokens[t0 + 2 + 4], "off") == 0)
				p.encap.vxlan.vlan = 0;
			else {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"vxlan: on or off");
				return;
			}

			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_VXLAN;
			n_extra_tokens = 5;

		} else if (strcmp(tokens[t0 + 1], "qinq_pppoe") == 0) {
			p.encap.encap_mask =
				1LLU << RTE_TABLE_ACTION_ENCAP_QINQ_PPPOE;
		} else {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "encap");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_ENCAP;
		t0 += 2 + n_extra_tokens;
	} /* encap */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "nat") == 0)) {
		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile nat");
			return;
		}

		if (strcmp(tokens[t0 + 1], "src") == 0) {
			p.nat.source_nat = 1;
		} else if (strcmp(tokens[t0 + 1], "dst") == 0) {
			p.nat.source_nat = 0;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"src or dst");
			return;
		}

		if (strcmp(tokens[t0 + 2], "proto") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "proto");
			return;
		}

		if (strcmp(tokens[t0 + 3], "tcp") == 0) {
			p.nat.proto = 0x06;
		} else if (strcmp(tokens[t0 + 3], "udp") == 0) {
			p.nat.proto = 0x11;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"tcp or udp");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_NAT;
		t0 += 4;
	} /* nat */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "ttl") == 0)) {
		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile ttl");
			return;
		}

		if (strcmp(tokens[t0 + 1], "drop") == 0) {
			p.ttl.drop = 1;
		} else if (strcmp(tokens[t0 + 1], "fwd") == 0) {
			p.ttl.drop = 0;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"drop or fwd");
			return;
		}

		if (strcmp(tokens[t0 + 2], "stats") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
			return;
		}

		if (strcmp(tokens[t0 + 3], "none") == 0) {
			p.ttl.n_packets_enabled = 0;
		} else if (strcmp(tokens[t0 + 3], "pkts") == 0) {
			p.ttl.n_packets_enabled = 1;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"none or pkts");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TTL;
		t0 += 4;
	} /* ttl */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "stats") == 0)) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile stats");
			return;
		}

		if (strcmp(tokens[t0 + 1], "pkts") == 0) {
			p.stats.n_packets_enabled = 1;
			p.stats.n_bytes_enabled = 0;
		} else if (strcmp(tokens[t0 + 1], "bytes") == 0) {
			p.stats.n_packets_enabled = 0;
			p.stats.n_bytes_enabled = 1;
		} else if (strcmp(tokens[t0 + 1], "both") == 0) {
			p.stats.n_packets_enabled = 1;
			p.stats.n_bytes_enabled = 1;
		} else {
			snprintf(out, out_size,	MSG_ARG_NOT_FOUND,
				"pkts or bytes or both");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_STATS;
		t0 += 2;
	} /* stats */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "time") == 0)) {
		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TIME;
		t0 += 1;
	} /* time */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "tag") == 0)) {
		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TAG;
		t0 += 1;
	} /* tag */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "decap") == 0)) {
		p.action_mask |= 1LLU << RTE_TABLE_ACTION_DECAP;
		t0 += 1;
	} /* decap */

	if (t0 < n_tokens && (strcmp(tokens[t0], "sym_crypto") == 0)) {
		struct softnic_cryptodev *cryptodev;

		if (n_tokens < t0 + 5 ||
				strcmp(tokens[t0 + 1], "dev") ||
				strcmp(tokens[t0 + 3], "offset")) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile sym_crypto");
			return;
		}

		cryptodev = softnic_cryptodev_find(softnic, tokens[t0 + 2]);
		if (cryptodev == NULL) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"table action profile sym_crypto");
			return;
		}

		p.sym_crypto.cryptodev_id = cryptodev->dev_id;

		if (softnic_parser_read_uint32(&p.sym_crypto.op_offset,
				tokens[t0 + 4]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
					"table action profile sym_crypto");
			return;
		}

		p.sym_crypto.mp_create = cryptodev->mp_create;
		p.sym_crypto.mp_init = cryptodev->mp_init;

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_SYM_CRYPTO;

		t0 += 5;
	} /* sym_crypto */

	if (t0 < n_tokens) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	ap = softnic_table_action_profile_create(softnic, name, &p);
	if (ap == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name>
 *  period <timer_period_ms>
 *  offset_port_id <offset_port_id>
 */
static void
cmd_pipeline(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline_params p;
	char *name;
	struct pipeline *pipeline;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "period") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "period");
		return;
	}

	if (softnic_parser_read_uint32(&p.timer_period_ms,
		tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "timer_period_ms");
		return;
	}

	if (strcmp(tokens[4], "offset_port_id") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset_port_id");
		return;
	}

	if (softnic_parser_read_uint32(&p.offset_port_id,
		tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "offset_port_id");
		return;
	}

	pipeline = softnic_pipeline_create(softnic, name, &p);
	if (pipeline == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in
 *  bsz <burst_size>
 *  link <link_name> rxq <queue_id>
 *  | swq <swq_name>
 *  | tmgr <tmgr_name>
 *  | tap <tap_name> mempool <mempool_name> mtu <mtu>
 *  | source mempool <mempool_name> file <file_name> bpp <n_bytes_per_pkt>
 *  | cryptodev <cryptodev_name> rxq <queue_id>
 *  [action <port_in_action_profile_name>]
 *  [disabled]
 */
static void
cmd_pipeline_port_in(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_port_in_params p;
	char *pipeline_name;
	uint32_t t0;
	int enabled, status;

	memset(&p, 0, sizeof(p));

	if (n_tokens < 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (strcmp(tokens[4], "bsz") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
		return;
	}

	if (softnic_parser_read_uint32(&p.burst_size, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "burst_size");
		return;
	}

	t0 = 6;

	if (strcmp(tokens[t0], "link") == 0) {
		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in link");
			return;
		}

		p.type = PORT_IN_RXQ;

		strlcpy(p.dev_name, tokens[t0 + 1], sizeof(p.dev_name));

		if (strcmp(tokens[t0 + 2], "rxq") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rxq");
			return;
		}

		if (softnic_parser_read_uint16(&p.rxq.queue_id,
			tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"queue_id");
			return;
		}
		t0 += 4;
	} else if (strcmp(tokens[t0], "swq") == 0) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in swq");
			return;
		}

		p.type = PORT_IN_SWQ;

		strlcpy(p.dev_name, tokens[t0 + 1], sizeof(p.dev_name));

		t0 += 2;
	} else if (strcmp(tokens[t0], "tmgr") == 0) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in tmgr");
			return;
		}

		p.type = PORT_IN_TMGR;

		strlcpy(p.dev_name, tokens[t0 + 1], sizeof(p.dev_name));

		t0 += 2;
	} else if (strcmp(tokens[t0], "tap") == 0) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in tap");
			return;
		}

		p.type = PORT_IN_TAP;

		strlcpy(p.dev_name, tokens[t0 + 1], sizeof(p.dev_name));

		if (strcmp(tokens[t0 + 2], "mempool") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"mempool");
			return;
		}

		p.tap.mempool_name = tokens[t0 + 3];

		if (strcmp(tokens[t0 + 4], "mtu") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"mtu");
			return;
		}

		if (softnic_parser_read_uint32(&p.tap.mtu,
			tokens[t0 + 5]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "mtu");
			return;
		}

		t0 += 6;
	} else if (strcmp(tokens[t0], "source") == 0) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in source");
			return;
		}

		p.type = PORT_IN_SOURCE;

		if (strcmp(tokens[t0 + 1], "mempool") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"mempool");
			return;
		}

		p.source.mempool_name = tokens[t0 + 2];

		if (strcmp(tokens[t0 + 3], "file") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"file");
			return;
		}

		p.source.file_name = tokens[t0 + 4];

		if (strcmp(tokens[t0 + 5], "bpp") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"bpp");
			return;
		}

		if (softnic_parser_read_uint32(&p.source.n_bytes_per_pkt,
			tokens[t0 + 6]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"n_bytes_per_pkt");
			return;
		}

		t0 += 7;
	} else if (strcmp(tokens[t0], "cryptodev") == 0) {
		if (n_tokens < t0 + 3) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in cryptodev");
			return;
		}

		p.type = PORT_IN_CRYPTODEV;

		strlcpy(p.dev_name, tokens[t0 + 1], sizeof(p.dev_name));
		if (softnic_parser_read_uint16(&p.rxq.queue_id,
				tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"rxq");
			return;
		}

		p.cryptodev.arg_callback = NULL;
		p.cryptodev.f_callback = NULL;

		t0 += 4;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	if (n_tokens > t0 &&
		(strcmp(tokens[t0], "action") == 0)) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "action");
			return;
		}

		strlcpy(p.action_profile_name, tokens[t0 + 1],
			sizeof(p.action_profile_name));

		t0 += 2;
	}

	enabled = 1;
	if (n_tokens > t0 &&
		(strcmp(tokens[t0], "disabled") == 0)) {
		enabled = 0;

		t0 += 1;
	}

	if (n_tokens != t0) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	status = softnic_pipeline_port_in_create(softnic,
		pipeline_name,
		&p,
		enabled);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port out
 *  bsz <burst_size>
 *  link <link_name> txq <txq_id>
 *  | swq <swq_name>
 *  | tmgr <tmgr_name>
 *  | tap <tap_name>
 *  | sink [file <file_name> pkts <max_n_pkts>]
 *  | cryptodev <cryptodev_name> txq <txq_id> offset <crypto_op_offset>
 */
static void
cmd_pipeline_port_out(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_port_out_params p;
	char *pipeline_name;
	int status;

	memset(&p, 0, sizeof(p));

	if (n_tokens < 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "out") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "out");
		return;
	}

	if (strcmp(tokens[4], "bsz") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
		return;
	}

	if (softnic_parser_read_uint32(&p.burst_size, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "burst_size");
		return;
	}

	if (strcmp(tokens[6], "link") == 0) {
		if (n_tokens != 10) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out link");
			return;
		}

		p.type = PORT_OUT_TXQ;

		strlcpy(p.dev_name, tokens[7], sizeof(p.dev_name));

		if (strcmp(tokens[8], "txq") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "txq");
			return;
		}

		if (softnic_parser_read_uint16(&p.txq.queue_id,
			tokens[9]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "queue_id");
			return;
		}
	} else if (strcmp(tokens[6], "swq") == 0) {
		if (n_tokens != 8) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out swq");
			return;
		}

		p.type = PORT_OUT_SWQ;

		strlcpy(p.dev_name, tokens[7], sizeof(p.dev_name));
	} else if (strcmp(tokens[6], "tmgr") == 0) {
		if (n_tokens != 8) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out tmgr");
			return;
		}

		p.type = PORT_OUT_TMGR;

		strlcpy(p.dev_name, tokens[7], sizeof(p.dev_name));
	} else if (strcmp(tokens[6], "tap") == 0) {
		if (n_tokens != 8) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out tap");
			return;
		}

		p.type = PORT_OUT_TAP;

		strlcpy(p.dev_name, tokens[7], sizeof(p.dev_name));
	} else if (strcmp(tokens[6], "sink") == 0) {
		if ((n_tokens != 7) && (n_tokens != 11)) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out sink");
			return;
		}

		p.type = PORT_OUT_SINK;

		if (n_tokens == 7) {
			p.sink.file_name = NULL;
			p.sink.max_n_pkts = 0;
		} else {
			if (strcmp(tokens[7], "file") != 0) {
				snprintf(out, out_size, MSG_ARG_NOT_FOUND,
					"file");
				return;
			}

			p.sink.file_name = tokens[8];

			if (strcmp(tokens[9], "pkts") != 0) {
				snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pkts");
				return;
			}

			if (softnic_parser_read_uint32(&p.sink.max_n_pkts,
				tokens[10]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "max_n_pkts");
				return;
			}
		}
	} else if (strcmp(tokens[6], "cryptodev") == 0) {
		if (n_tokens != 12) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out cryptodev");
			return;
		}

		p.type = PORT_OUT_CRYPTODEV;

		strlcpy(p.dev_name, tokens[7], sizeof(p.dev_name));

		if (strcmp(tokens[8], "txq")) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out cryptodev");
			return;
		}

		if (softnic_parser_read_uint16(&p.cryptodev.queue_id, tokens[9])
				!= 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "queue_id");
			return;
		}

		if (strcmp(tokens[10], "offset")) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out cryptodev");
			return;
		}

		if (softnic_parser_read_uint32(&p.cryptodev.op_offset,
				tokens[11]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "queue_id");
			return;
		}
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	status = softnic_pipeline_port_out_create(softnic, pipeline_name, &p);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> table
 *      match
 *      acl
 *          ipv4 | ipv6
 *          offset <ip_header_offset>
 *          size <n_rules>
 *      | array
 *          offset <key_offset>
 *          size <n_keys>
 *      | hash
 *          ext | lru
 *          key <key_size>
 *          mask <key_mask>
 *          offset <key_offset>
 *          buckets <n_buckets>
 *          size <n_keys>
 *      | lpm
 *          ipv4 | ipv6
 *          offset <ip_header_offset>
 *          size <n_rules>
 *      | stub
 *  [action <table_action_profile_name>]
 */
static void
cmd_pipeline_table(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_table_params p;
	char *pipeline_name;
	uint32_t t0;
	int status;

	memset(&p, 0, sizeof(p));

	if (n_tokens < 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (strcmp(tokens[3], "match") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
		return;
	}

	t0 = 4;
	if (strcmp(tokens[t0], "acl") == 0) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline table acl");
			return;
		}

		p.match_type = TABLE_ACL;

		if (strcmp(tokens[t0 + 1], "ipv4") == 0) {
			p.match.acl.ip_version = 1;
		} else if (strcmp(tokens[t0 + 1], "ipv6") == 0) {
			p.match.acl.ip_version = 0;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ipv4 or ipv6");
			return;
		}

		if (strcmp(tokens[t0 + 2], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.acl.ip_header_offset,
			tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"ip_header_offset");
			return;
		}

		if (strcmp(tokens[t0 + 4], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.acl.n_rules,
			tokens[t0 + 5]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_rules");
			return;
		}

		t0 += 6;
	} else if (strcmp(tokens[t0], "array") == 0) {
		if (n_tokens < t0 + 5) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline table array");
			return;
		}

		p.match_type = TABLE_ARRAY;

		if (strcmp(tokens[t0 + 1], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.array.key_offset,
			tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 3], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.array.n_keys,
			tokens[t0 + 4]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_keys");
			return;
		}

		t0 += 5;
	} else if (strcmp(tokens[t0], "hash") == 0) {
		uint32_t key_mask_size = TABLE_RULE_MATCH_SIZE_MAX;

		if (n_tokens < t0 + 12) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline table hash");
			return;
		}

		p.match_type = TABLE_HASH;

		if (strcmp(tokens[t0 + 1], "ext") == 0) {
			p.match.hash.extendable_bucket = 1;
		} else if (strcmp(tokens[t0 + 1], "lru") == 0) {
			p.match.hash.extendable_bucket = 0;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ext or lru");
			return;
		}

		if (strcmp(tokens[t0 + 2], "key") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "key");
			return;
		}

		if ((softnic_parser_read_uint32(&p.match.hash.key_size,
			tokens[t0 + 3]) != 0) ||
			p.match.hash.key_size == 0 ||
			p.match.hash.key_size > TABLE_RULE_MATCH_SIZE_MAX) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_size");
			return;
		}

		if (strcmp(tokens[t0 + 4], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		if ((softnic_parse_hex_string(tokens[t0 + 5],
			p.match.hash.key_mask, &key_mask_size) != 0) ||
			key_mask_size != p.match.hash.key_size) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}

		if (strcmp(tokens[t0 + 6], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.hash.key_offset,
			tokens[t0 + 7]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 8], "buckets") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "buckets");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.hash.n_buckets,
			tokens[t0 + 9]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_buckets");
			return;
		}

		if (strcmp(tokens[t0 + 10], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.hash.n_keys,
			tokens[t0 + 11]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_keys");
			return;
		}

		t0 += 12;
	} else if (strcmp(tokens[t0], "lpm") == 0) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline table lpm");
			return;
		}

		p.match_type = TABLE_LPM;

		if (strcmp(tokens[t0 + 1], "ipv4") == 0) {
			p.match.lpm.key_size = 4;
		} else if (strcmp(tokens[t0 + 1], "ipv6") == 0) {
			p.match.lpm.key_size = 16;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ipv4 or ipv6");
			return;
		}

		if (strcmp(tokens[t0 + 2], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.lpm.key_offset,
			tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 4], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.lpm.n_rules,
			tokens[t0 + 5]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_rules");
			return;
		}

		t0 += 6;
	} else if (strcmp(tokens[t0], "stub") == 0) {
		p.match_type = TABLE_STUB;

		t0 += 1;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	if (n_tokens > t0 &&
		(strcmp(tokens[t0], "action") == 0)) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "action");
			return;
		}

		strlcpy(p.action_profile_name, tokens[t0 + 1],
			sizeof(p.action_profile_name));

		t0 += 2;
	}

	if (n_tokens > t0) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	status = softnic_pipeline_table_create(softnic, pipeline_name, &p);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in <port_id> table <table_id>
 */
static void
cmd_pipeline_port_in_table(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t port_id, table_id;
	int status;

	if (n_tokens != 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (softnic_parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	status = softnic_pipeline_port_in_connect_to_table(softnic,
		pipeline_name,
		port_id,
		table_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in <port_id> stats read [clear]
 */

#define MSG_PIPELINE_PORT_IN_STATS                         \
	"Pkts in: %" PRIu64 "\n"                           \
	"Pkts dropped by AH: %" PRIu64 "\n"                \
	"Pkts dropped by other: %" PRIu64 "\n"

static void
cmd_pipeline_port_in_stats(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_pipeline_port_in_stats stats;
	char *pipeline_name;
	uint32_t port_id;
	int clear, status;

	if (n_tokens != 7 &&
		n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (softnic_parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "stats") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	if (strcmp(tokens[6], "read") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "read");
		return;
	}

	clear = 0;
	if (n_tokens == 8) {
		if (strcmp(tokens[7], "clear") != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "clear");
			return;
		}

		clear = 1;
	}

	status = softnic_pipeline_port_in_stats_read(softnic,
		pipeline_name,
		port_id,
		&stats,
		clear);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	snprintf(out, out_size, MSG_PIPELINE_PORT_IN_STATS,
		stats.stats.n_pkts_in,
		stats.n_pkts_dropped_by_ah,
		stats.stats.n_pkts_drop);
}

/**
 * pipeline <pipeline_name> port in <port_id> enable
 */
static void
cmd_softnic_pipeline_port_in_enable(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t port_id;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (softnic_parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "enable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "enable");
		return;
	}

	status = softnic_pipeline_port_in_enable(softnic, pipeline_name, port_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in <port_id> disable
 */
static void
cmd_softnic_pipeline_port_in_disable(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t port_id;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (softnic_parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "disable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "disable");
		return;
	}

	status = softnic_pipeline_port_in_disable(softnic, pipeline_name, port_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port out <port_id> stats read [clear]
 */
#define MSG_PIPELINE_PORT_OUT_STATS                        \
	"Pkts in: %" PRIu64 "\n"                           \
	"Pkts dropped by AH: %" PRIu64 "\n"                \
	"Pkts dropped by other: %" PRIu64 "\n"

static void
cmd_pipeline_port_out_stats(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_pipeline_port_out_stats stats;
	char *pipeline_name;
	uint32_t port_id;
	int clear, status;

	if (n_tokens != 7 &&
		n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "out") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "out");
		return;
	}

	if (softnic_parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "stats") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	if (strcmp(tokens[6], "read") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "read");
		return;
	}

	clear = 0;
	if (n_tokens == 8) {
		if (strcmp(tokens[7], "clear") != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "clear");
			return;
		}

		clear = 1;
	}

	status = softnic_pipeline_port_out_stats_read(softnic,
		pipeline_name,
		port_id,
		&stats,
		clear);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	snprintf(out, out_size, MSG_PIPELINE_PORT_OUT_STATS,
		stats.stats.n_pkts_in,
		stats.n_pkts_dropped_by_ah,
		stats.stats.n_pkts_drop);
}

/**
 * pipeline <pipeline_name> table <table_id> stats read [clear]
 */
#define MSG_PIPELINE_TABLE_STATS                                     \
	"Pkts in: %" PRIu64 "\n"                                     \
	"Pkts in with lookup miss: %" PRIu64 "\n"                    \
	"Pkts in with lookup hit dropped by AH: %" PRIu64 "\n"       \
	"Pkts in with lookup hit dropped by others: %" PRIu64 "\n"   \
	"Pkts in with lookup miss dropped by AH: %" PRIu64 "\n"      \
	"Pkts in with lookup miss dropped by others: %" PRIu64 "\n"

static void
cmd_pipeline_table_stats(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_pipeline_table_stats stats;
	char *pipeline_name;
	uint32_t table_id;
	int clear, status;

	if (n_tokens != 6 &&
		n_tokens != 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "stats") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	if (strcmp(tokens[5], "read") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "read");
		return;
	}

	clear = 0;
	if (n_tokens == 7) {
		if (strcmp(tokens[6], "clear") != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "clear");
			return;
		}

		clear = 1;
	}

	status = softnic_pipeline_table_stats_read(softnic,
		pipeline_name,
		table_id,
		&stats,
		clear);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	snprintf(out, out_size, MSG_PIPELINE_TABLE_STATS,
		stats.stats.n_pkts_in,
		stats.stats.n_pkts_lookup_miss,
		stats.n_pkts_dropped_by_lkp_hit_ah,
		stats.n_pkts_dropped_lkp_hit,
		stats.n_pkts_dropped_by_lkp_miss_ah,
		stats.n_pkts_dropped_lkp_miss);
}

/**
 * <match> ::=
 *
 * match
 *    acl
 *       priority <priority>
 *       ipv4 | ipv6 <sa> <sa_depth> <da> <da_depth>
 *       <sp0> <sp1> <dp0> <dp1> <proto>
 *    | array <pos>
 *    | hash
 *       raw <key>
 *       | ipv4_5tuple <sa> <da> <sp> <dp> <proto>
 *       | ipv6_5tuple <sa> <da> <sp> <dp> <proto>
 *       | ipv4_addr <addr>
 *       | ipv6_addr <addr>
 *       | qinq <svlan> <cvlan>
 *    | lpm
 *       ipv4 | ipv6 <addr> <depth>
 */
struct pkt_key_qinq {
	uint16_t ethertype_svlan;
	uint16_t svlan;
	uint16_t ethertype_cvlan;
	uint16_t cvlan;
} __attribute__((__packed__));

struct pkt_key_ipv4_5tuple {
	uint8_t time_to_live;
	uint8_t proto;
	uint16_t hdr_checksum;
	uint32_t sa;
	uint32_t da;
	uint16_t sp;
	uint16_t dp;
} __attribute__((__packed__));

struct pkt_key_ipv6_5tuple {
	uint16_t payload_length;
	uint8_t proto;
	uint8_t hop_limit;
	uint8_t sa[16];
	uint8_t da[16];
	uint16_t sp;
	uint16_t dp;
} __attribute__((__packed__));

struct pkt_key_ipv4_addr {
	uint32_t addr;
} __attribute__((__packed__));

struct pkt_key_ipv6_addr {
	uint8_t addr[16];
} __attribute__((__packed__));

static uint32_t
parse_match(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	struct softnic_table_rule_match *m)
{
	memset(m, 0, sizeof(*m));

	if (n_tokens < 2)
		return 0;

	if (strcmp(tokens[0], "match") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
		return 0;
	}

	if (strcmp(tokens[1], "acl") == 0) {
		if (n_tokens < 14) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return 0;
		}

		m->match_type = TABLE_ACL;

		if (strcmp(tokens[2], "priority") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "priority");
			return 0;
		}

		if (softnic_parser_read_uint32(&m->match.acl.priority,
			tokens[3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "priority");
			return 0;
		}

		if (strcmp(tokens[4], "ipv4") == 0) {
			struct in_addr saddr, daddr;

			m->match.acl.ip_version = 1;

			if (softnic_parse_ipv4_addr(tokens[5], &saddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sa");
				return 0;
			}
			m->match.acl.ipv4.sa = rte_be_to_cpu_32(saddr.s_addr);

			if (softnic_parse_ipv4_addr(tokens[7], &daddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "da");
				return 0;
			}
			m->match.acl.ipv4.da = rte_be_to_cpu_32(daddr.s_addr);
		} else if (strcmp(tokens[4], "ipv6") == 0) {
			struct in6_addr saddr, daddr;

			m->match.acl.ip_version = 0;

			if (softnic_parse_ipv6_addr(tokens[5], &saddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sa");
				return 0;
			}
			memcpy(m->match.acl.ipv6.sa, saddr.s6_addr, 16);

			if (softnic_parse_ipv6_addr(tokens[7], &daddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "da");
				return 0;
			}
			memcpy(m->match.acl.ipv6.da, daddr.s6_addr, 16);
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ipv4 or ipv6");
			return 0;
		}

		if (softnic_parser_read_uint32(&m->match.acl.sa_depth,
			tokens[6]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "sa_depth");
			return 0;
		}

		if (softnic_parser_read_uint32(&m->match.acl.da_depth,
			tokens[8]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "da_depth");
			return 0;
		}

		if (softnic_parser_read_uint16(&m->match.acl.sp0, tokens[9]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "sp0");
			return 0;
		}

		if (softnic_parser_read_uint16(&m->match.acl.sp1, tokens[10]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "sp1");
			return 0;
		}

		if (softnic_parser_read_uint16(&m->match.acl.dp0, tokens[11]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "dp0");
			return 0;
		}

		if (softnic_parser_read_uint16(&m->match.acl.dp1, tokens[12]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "dp1");
			return 0;
		}

		if (softnic_parser_read_uint8(&m->match.acl.proto, tokens[13]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "proto");
			return 0;
		}

		m->match.acl.proto_mask = 0xff;

		return 14;
	} /* acl */

	if (strcmp(tokens[1], "array") == 0) {
		if (n_tokens < 3) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return 0;
		}

		m->match_type = TABLE_ARRAY;

		if (softnic_parser_read_uint32(&m->match.array.pos, tokens[2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "pos");
			return 0;
		}

		return 3;
	} /* array */

	if (strcmp(tokens[1], "hash") == 0) {
		if (n_tokens < 3) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return 0;
		}

		m->match_type = TABLE_HASH;

		if (strcmp(tokens[2], "raw") == 0) {
			uint32_t key_size = TABLE_RULE_MATCH_SIZE_MAX;

			if (n_tokens < 4) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if (softnic_parse_hex_string(tokens[3],
				m->match.hash.key, &key_size) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "key");
				return 0;
			}

			return 4;
		} /* hash raw */

		if (strcmp(tokens[2], "ipv4_5tuple") == 0) {
			struct pkt_key_ipv4_5tuple *ipv4 =
				(struct pkt_key_ipv4_5tuple *)m->match.hash.key;
			struct in_addr saddr, daddr;
			uint16_t sp, dp;
			uint8_t proto;

			if (n_tokens < 8) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if (softnic_parse_ipv4_addr(tokens[3], &saddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sa");
				return 0;
			}

			if (softnic_parse_ipv4_addr(tokens[4], &daddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "da");
				return 0;
			}

			if (softnic_parser_read_uint16(&sp, tokens[5]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sp");
				return 0;
			}

			if (softnic_parser_read_uint16(&dp, tokens[6]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "dp");
				return 0;
			}

			if (softnic_parser_read_uint8(&proto, tokens[7]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"proto");
				return 0;
			}

			ipv4->sa = saddr.s_addr;
			ipv4->da = daddr.s_addr;
			ipv4->sp = rte_cpu_to_be_16(sp);
			ipv4->dp = rte_cpu_to_be_16(dp);
			ipv4->proto = proto;

			return 8;
		} /* hash ipv4_5tuple */

		if (strcmp(tokens[2], "ipv6_5tuple") == 0) {
			struct pkt_key_ipv6_5tuple *ipv6 =
				(struct pkt_key_ipv6_5tuple *)m->match.hash.key;
			struct in6_addr saddr, daddr;
			uint16_t sp, dp;
			uint8_t proto;

			if (n_tokens < 8) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if (softnic_parse_ipv6_addr(tokens[3], &saddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sa");
				return 0;
			}

			if (softnic_parse_ipv6_addr(tokens[4], &daddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "da");
				return 0;
			}

			if (softnic_parser_read_uint16(&sp, tokens[5]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sp");
				return 0;
			}

			if (softnic_parser_read_uint16(&dp, tokens[6]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "dp");
				return 0;
			}

			if (softnic_parser_read_uint8(&proto, tokens[7]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"proto");
				return 0;
			}

			memcpy(ipv6->sa, saddr.s6_addr, 16);
			memcpy(ipv6->da, daddr.s6_addr, 16);
			ipv6->sp = rte_cpu_to_be_16(sp);
			ipv6->dp = rte_cpu_to_be_16(dp);
			ipv6->proto = proto;

			return 8;
		} /* hash ipv6_5tuple */

		if (strcmp(tokens[2], "ipv4_addr") == 0) {
			struct pkt_key_ipv4_addr *ipv4_addr =
				(struct pkt_key_ipv4_addr *)m->match.hash.key;
			struct in_addr addr;

			if (n_tokens < 4) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if (softnic_parse_ipv4_addr(tokens[3], &addr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"addr");
				return 0;
			}

			ipv4_addr->addr = addr.s_addr;

			return 4;
		} /* hash ipv4_addr */

		if (strcmp(tokens[2], "ipv6_addr") == 0) {
			struct pkt_key_ipv6_addr *ipv6_addr =
				(struct pkt_key_ipv6_addr *)m->match.hash.key;
			struct in6_addr addr;

			if (n_tokens < 4) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if (softnic_parse_ipv6_addr(tokens[3], &addr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"addr");
				return 0;
			}

			memcpy(ipv6_addr->addr, addr.s6_addr, 16);

			return 4;
		} /* hash ipv6_5tuple */

		if (strcmp(tokens[2], "qinq") == 0) {
			struct pkt_key_qinq *qinq =
				(struct pkt_key_qinq *)m->match.hash.key;
			uint16_t svlan, cvlan;

			if (n_tokens < 5) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if ((softnic_parser_read_uint16(&svlan, tokens[3]) != 0) ||
				svlan > 0xFFF) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"svlan");
				return 0;
			}

			if ((softnic_parser_read_uint16(&cvlan, tokens[4]) != 0) ||
				cvlan > 0xFFF) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"cvlan");
				return 0;
			}

			qinq->svlan = rte_cpu_to_be_16(svlan);
			qinq->cvlan = rte_cpu_to_be_16(cvlan);

			return 5;
		} /* hash qinq */

		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return 0;
	} /* hash */

	if (strcmp(tokens[1], "lpm") == 0) {
		if (n_tokens < 5) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return 0;
		}

		m->match_type = TABLE_LPM;

		if (strcmp(tokens[2], "ipv4") == 0) {
			struct in_addr addr;

			m->match.lpm.ip_version = 1;

			if (softnic_parse_ipv4_addr(tokens[3], &addr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"addr");
				return 0;
			}

			m->match.lpm.ipv4 = rte_be_to_cpu_32(addr.s_addr);
		} else if (strcmp(tokens[2], "ipv6") == 0) {
			struct in6_addr addr;

			m->match.lpm.ip_version = 0;

			if (softnic_parse_ipv6_addr(tokens[3], &addr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"addr");
				return 0;
			}

			memcpy(m->match.lpm.ipv6, addr.s6_addr, 16);
		} else {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"ipv4 or ipv6");
			return 0;
		}

		if (softnic_parser_read_uint8(&m->match.lpm.depth, tokens[4]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "depth");
			return 0;
		}

		return 5;
	} /* lpm */

	snprintf(out, out_size, MSG_ARG_MISMATCH,
		"acl or array or hash or lpm");
	return 0;
}

/**
 * table_action ::=
 *
 * action
 *    fwd
 *       drop
 *       | port <port_id>
 *       | meta
 *       | table <table_id>
 *    [balance <out0> ... <out7>]
 *    [meter
 *       tc0 meter <meter_profile_id> policer g <pa> y <pa> r <pa>
 *       [tc1 meter <meter_profile_id> policer g <pa> y <pa> r <pa>
 *       tc2 meter <meter_profile_id> policer g <pa> y <pa> r <pa>
 *       tc3 meter <meter_profile_id> policer g <pa> y <pa> r <pa>]]
 *    [tm subport <subport_id> pipe <pipe_id>]
 *    [encap
 *       ether <da> <sa>
 *       | vlan <da> <sa> <pcp> <dei> <vid>
 *       | qinq <da> <sa> <pcp> <dei> <vid> <pcp> <dei> <vid>
 *       | qinq_pppoe <da> <sa> <pcp> <dei> <vid> <pcp> <dei> <vid> <session_id>
 *       | mpls unicast | multicast
 *          <da> <sa>
 *          label0 <label> <tc> <ttl>
 *          [label1 <label> <tc> <ttl>
 *          [label2 <label> <tc> <ttl>
 *          [label3 <label> <tc> <ttl>]]]
 *       | pppoe <da> <sa> <session_id>]
 *       | vxlan ether <da> <sa>
 *          [vlan <pcp> <dei> <vid>]
 *          ipv4 <sa> <da> <dscp> <ttl>
 *          | ipv6 <sa> <da> <flow_label> <dscp> <hop_limit>
 *          udp <sp> <dp>
 *          vxlan <vni>]
 *    [nat ipv4 | ipv6 <addr> <port>]
 *    [ttl dec | keep]
 *    [stats]
 *    [time]
 *    [tag <tag>]
 *    [decap <n>]
 *    [sym_crypto
 *       encrypt | decrypt
 *       type
 *       | cipher
 *          cipher_algo <algo> cipher_key <key> cipher_iv <iv>
 *       | cipher_auth
 *          cipher_algo <algo> cipher_key <key> cipher_iv <iv>
 *          auth_algo <algo> auth_key <key> digest_size <size>
 *       | aead
 *          aead_algo <algo> aead_key <key> aead_iv <iv> aead_aad <aad>
 *          digest_size <size>
 *       data_offset <data_offset>]
 *
 * where:
 *    <pa> ::= g | y | r | drop
 */
static uint32_t
parse_table_action_fwd(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	if (n_tokens == 0 ||
		(strcmp(tokens[0], "fwd") != 0))
		return 0;

	tokens++;
	n_tokens--;

	if (n_tokens && (strcmp(tokens[0], "drop") == 0)) {
		a->fwd.action = RTE_PIPELINE_ACTION_DROP;
		a->action_mask |= 1 << RTE_TABLE_ACTION_FWD;
		return 1 + 1;
	}

	if (n_tokens && (strcmp(tokens[0], "port") == 0)) {
		uint32_t id;

		if (n_tokens < 2 ||
			softnic_parser_read_uint32(&id, tokens[1]))
			return 0;

		a->fwd.action = RTE_PIPELINE_ACTION_PORT;
		a->fwd.id = id;
		a->action_mask |= 1 << RTE_TABLE_ACTION_FWD;
		return 1 + 2;
	}

	if (n_tokens && (strcmp(tokens[0], "meta") == 0)) {
		a->fwd.action = RTE_PIPELINE_ACTION_PORT_META;
		a->action_mask |= 1 << RTE_TABLE_ACTION_FWD;
		return 1 + 1;
	}

	if (n_tokens && (strcmp(tokens[0], "table") == 0)) {
		uint32_t id;

		if (n_tokens < 2 ||
			softnic_parser_read_uint32(&id, tokens[1]))
			return 0;

		a->fwd.action = RTE_PIPELINE_ACTION_TABLE;
		a->fwd.id = id;
		a->action_mask |= 1 << RTE_TABLE_ACTION_FWD;
		return 1 + 2;
	}

	return 0;
}

static uint32_t
parse_table_action_balance(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	uint32_t i;

	if (n_tokens == 0 ||
		(strcmp(tokens[0], "balance") != 0))
		return 0;

	tokens++;
	n_tokens--;

	if (n_tokens < RTE_TABLE_ACTION_LB_TABLE_SIZE)
		return 0;

	for (i = 0; i < RTE_TABLE_ACTION_LB_TABLE_SIZE; i++)
		if (softnic_parser_read_uint32(&a->lb.out[i], tokens[i]) != 0)
			return 0;

	a->action_mask |= 1 << RTE_TABLE_ACTION_LB;
	return 1 + RTE_TABLE_ACTION_LB_TABLE_SIZE;
}

static int
parse_policer_action(char *token, enum rte_table_action_policer *a)
{
	if (strcmp(token, "g") == 0) {
		*a = RTE_TABLE_ACTION_POLICER_COLOR_GREEN;
		return 0;
	}

	if (strcmp(token, "y") == 0) {
		*a = RTE_TABLE_ACTION_POLICER_COLOR_YELLOW;
		return 0;
	}

	if (strcmp(token, "r") == 0) {
		*a = RTE_TABLE_ACTION_POLICER_COLOR_RED;
		return 0;
	}

	if (strcmp(token, "drop") == 0) {
		*a = RTE_TABLE_ACTION_POLICER_DROP;
		return 0;
	}

	return -1;
}

static uint32_t
parse_table_action_meter_tc(char **tokens,
	uint32_t n_tokens,
	struct rte_table_action_mtr_tc_params *mtr)
{
	if (n_tokens < 9 ||
		strcmp(tokens[0], "meter") ||
		softnic_parser_read_uint32(&mtr->meter_profile_id, tokens[1]) ||
		strcmp(tokens[2], "policer") ||
		strcmp(tokens[3], "g") ||
		parse_policer_action(tokens[4], &mtr->policer[RTE_COLOR_GREEN]) ||
		strcmp(tokens[5], "y") ||
		parse_policer_action(tokens[6], &mtr->policer[RTE_COLOR_YELLOW]) ||
		strcmp(tokens[7], "r") ||
		parse_policer_action(tokens[8], &mtr->policer[RTE_COLOR_RED]))
		return 0;

	return 9;
}

static uint32_t
parse_table_action_meter(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	if (n_tokens == 0 ||
		strcmp(tokens[0], "meter"))
		return 0;

	tokens++;
	n_tokens--;

	if (n_tokens < 10 ||
		strcmp(tokens[0], "tc0") ||
		(parse_table_action_meter_tc(tokens + 1,
			n_tokens - 1,
			&a->mtr.mtr[0]) == 0))
		return 0;

	tokens += 10;
	n_tokens -= 10;

	if (n_tokens == 0 ||
		strcmp(tokens[0], "tc1")) {
		a->mtr.tc_mask = 1;
		a->action_mask |= 1 << RTE_TABLE_ACTION_MTR;
		return 1 + 10;
	}

	if (n_tokens < 30 ||
		(parse_table_action_meter_tc(tokens + 1,
			n_tokens - 1, &a->mtr.mtr[1]) == 0) ||
		strcmp(tokens[10], "tc2") ||
		(parse_table_action_meter_tc(tokens + 11,
			n_tokens - 11, &a->mtr.mtr[2]) == 0) ||
		strcmp(tokens[20], "tc3") ||
		(parse_table_action_meter_tc(tokens + 21,
			n_tokens - 21, &a->mtr.mtr[3]) == 0))
		return 0;

	a->mtr.tc_mask = 0xF;
	a->action_mask |= 1 << RTE_TABLE_ACTION_MTR;
	return 1 + 10 + 3 * 10;
}

static uint32_t
parse_table_action_tm(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	uint32_t subport_id, pipe_id;

	if (n_tokens < 5 ||
		strcmp(tokens[0], "tm") ||
		strcmp(tokens[1], "subport") ||
		softnic_parser_read_uint32(&subport_id, tokens[2]) ||
		strcmp(tokens[3], "pipe") ||
		softnic_parser_read_uint32(&pipe_id, tokens[4]))
		return 0;

	a->tm.subport_id = subport_id;
	a->tm.pipe_id = pipe_id;
	a->action_mask |= 1 << RTE_TABLE_ACTION_TM;
	return 5;
}

static uint32_t
parse_table_action_encap(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	if (n_tokens == 0 ||
		strcmp(tokens[0], "encap"))
		return 0;

	tokens++;
	n_tokens--;

	/* ether */
	if (n_tokens && (strcmp(tokens[0], "ether") == 0)) {
		if (n_tokens < 3 ||
			softnic_parse_mac_addr(tokens[1], &a->encap.ether.ether.da) ||
			softnic_parse_mac_addr(tokens[2], &a->encap.ether.ether.sa))
			return 0;

		a->encap.type = RTE_TABLE_ACTION_ENCAP_ETHER;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 3;
	}

	/* vlan */
	if (n_tokens && (strcmp(tokens[0], "vlan") == 0)) {
		uint32_t pcp, dei, vid;

		if (n_tokens < 6 ||
			softnic_parse_mac_addr(tokens[1], &a->encap.vlan.ether.da) ||
			softnic_parse_mac_addr(tokens[2], &a->encap.vlan.ether.sa) ||
			softnic_parser_read_uint32(&pcp, tokens[3]) ||
			pcp > 0x7 ||
			softnic_parser_read_uint32(&dei, tokens[4]) ||
			dei > 0x1 ||
			softnic_parser_read_uint32(&vid, tokens[5]) ||
			vid > 0xFFF)
			return 0;

		a->encap.vlan.vlan.pcp = pcp & 0x7;
		a->encap.vlan.vlan.dei = dei & 0x1;
		a->encap.vlan.vlan.vid = vid & 0xFFF;
		a->encap.type = RTE_TABLE_ACTION_ENCAP_VLAN;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 6;
	}

	/* qinq */
	if (n_tokens && (strcmp(tokens[0], "qinq") == 0)) {
		uint32_t svlan_pcp, svlan_dei, svlan_vid;
		uint32_t cvlan_pcp, cvlan_dei, cvlan_vid;

		if (n_tokens < 9 ||
			softnic_parse_mac_addr(tokens[1], &a->encap.qinq.ether.da) ||
			softnic_parse_mac_addr(tokens[2], &a->encap.qinq.ether.sa) ||
			softnic_parser_read_uint32(&svlan_pcp, tokens[3]) ||
			svlan_pcp > 0x7 ||
			softnic_parser_read_uint32(&svlan_dei, tokens[4]) ||
			svlan_dei > 0x1 ||
			softnic_parser_read_uint32(&svlan_vid, tokens[5]) ||
			svlan_vid > 0xFFF ||
			softnic_parser_read_uint32(&cvlan_pcp, tokens[6]) ||
			cvlan_pcp > 0x7 ||
			softnic_parser_read_uint32(&cvlan_dei, tokens[7]) ||
			cvlan_dei > 0x1 ||
			softnic_parser_read_uint32(&cvlan_vid, tokens[8]) ||
			cvlan_vid > 0xFFF)
			return 0;

		a->encap.qinq.svlan.pcp = svlan_pcp & 0x7;
		a->encap.qinq.svlan.dei = svlan_dei & 0x1;
		a->encap.qinq.svlan.vid = svlan_vid & 0xFFF;
		a->encap.qinq.cvlan.pcp = cvlan_pcp & 0x7;
		a->encap.qinq.cvlan.dei = cvlan_dei & 0x1;
		a->encap.qinq.cvlan.vid = cvlan_vid & 0xFFF;
		a->encap.type = RTE_TABLE_ACTION_ENCAP_QINQ;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 9;
	}

	/* qinq_pppoe */
	if (n_tokens && (strcmp(tokens[0], "qinq_pppoe") == 0)) {
		uint32_t svlan_pcp, svlan_dei, svlan_vid;
		uint32_t cvlan_pcp, cvlan_dei, cvlan_vid;

		if (n_tokens < 10 ||
			softnic_parse_mac_addr(tokens[1],
				&a->encap.qinq_pppoe.ether.da) ||
			softnic_parse_mac_addr(tokens[2],
				&a->encap.qinq_pppoe.ether.sa) ||
			softnic_parser_read_uint32(&svlan_pcp, tokens[3]) ||
			svlan_pcp > 0x7 ||
			softnic_parser_read_uint32(&svlan_dei, tokens[4]) ||
			svlan_dei > 0x1 ||
			softnic_parser_read_uint32(&svlan_vid, tokens[5]) ||
			svlan_vid > 0xFFF ||
			softnic_parser_read_uint32(&cvlan_pcp, tokens[6]) ||
			cvlan_pcp > 0x7 ||
			softnic_parser_read_uint32(&cvlan_dei, tokens[7]) ||
			cvlan_dei > 0x1 ||
			softnic_parser_read_uint32(&cvlan_vid, tokens[8]) ||
			cvlan_vid > 0xFFF ||
			softnic_parser_read_uint16(&a->encap.qinq_pppoe.pppoe.session_id,
				tokens[9]))
			return 0;

		a->encap.qinq_pppoe.svlan.pcp = svlan_pcp & 0x7;
		a->encap.qinq_pppoe.svlan.dei = svlan_dei & 0x1;
		a->encap.qinq_pppoe.svlan.vid = svlan_vid & 0xFFF;
		a->encap.qinq_pppoe.cvlan.pcp = cvlan_pcp & 0x7;
		a->encap.qinq_pppoe.cvlan.dei = cvlan_dei & 0x1;
		a->encap.qinq_pppoe.cvlan.vid = cvlan_vid & 0xFFF;
		a->encap.type = RTE_TABLE_ACTION_ENCAP_QINQ_PPPOE;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 10;
	}

	/* mpls */
	if (n_tokens && (strcmp(tokens[0], "mpls") == 0)) {
		uint32_t label, tc, ttl;

		if (n_tokens < 8)
			return 0;

		if (strcmp(tokens[1], "unicast") == 0)
			a->encap.mpls.unicast = 1;
		else if (strcmp(tokens[1], "multicast") == 0)
			a->encap.mpls.unicast = 0;
		else
			return 0;

		if (softnic_parse_mac_addr(tokens[2], &a->encap.mpls.ether.da) ||
			softnic_parse_mac_addr(tokens[3], &a->encap.mpls.ether.sa) ||
			strcmp(tokens[4], "label0") ||
			softnic_parser_read_uint32(&label, tokens[5]) ||
			label > 0xFFFFF ||
			softnic_parser_read_uint32(&tc, tokens[6]) ||
			tc > 0x7 ||
			softnic_parser_read_uint32(&ttl, tokens[7]) ||
			ttl > 0x3F)
			return 0;

		a->encap.mpls.mpls[0].label = label;
		a->encap.mpls.mpls[0].tc = tc;
		a->encap.mpls.mpls[0].ttl = ttl;

		tokens += 8;
		n_tokens -= 8;

		if (n_tokens == 0 ||
			strcmp(tokens[0], "label1")) {
			a->encap.mpls.mpls_count = 1;
			a->encap.type = RTE_TABLE_ACTION_ENCAP_MPLS;
			a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
			return 1 + 8;
		}

		if (n_tokens < 4 ||
			softnic_parser_read_uint32(&label, tokens[1]) ||
			label > 0xFFFFF ||
			softnic_parser_read_uint32(&tc, tokens[2]) ||
			tc > 0x7 ||
			softnic_parser_read_uint32(&ttl, tokens[3]) ||
			ttl > 0x3F)
			return 0;

		a->encap.mpls.mpls[1].label = label;
		a->encap.mpls.mpls[1].tc = tc;
		a->encap.mpls.mpls[1].ttl = ttl;

		tokens += 4;
		n_tokens -= 4;

		if (n_tokens == 0 ||
			strcmp(tokens[0], "label2")) {
			a->encap.mpls.mpls_count = 2;
			a->encap.type = RTE_TABLE_ACTION_ENCAP_MPLS;
			a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
			return 1 + 8 + 4;
		}

		if (n_tokens < 4 ||
			softnic_parser_read_uint32(&label, tokens[1]) ||
			label > 0xFFFFF ||
			softnic_parser_read_uint32(&tc, tokens[2]) ||
			tc > 0x7 ||
			softnic_parser_read_uint32(&ttl, tokens[3]) ||
			ttl > 0x3F)
			return 0;

		a->encap.mpls.mpls[2].label = label;
		a->encap.mpls.mpls[2].tc = tc;
		a->encap.mpls.mpls[2].ttl = ttl;

		tokens += 4;
		n_tokens -= 4;

		if (n_tokens == 0 ||
			strcmp(tokens[0], "label3")) {
			a->encap.mpls.mpls_count = 3;
			a->encap.type = RTE_TABLE_ACTION_ENCAP_MPLS;
			a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
			return 1 + 8 + 4 + 4;
		}

		if (n_tokens < 4 ||
			softnic_parser_read_uint32(&label, tokens[1]) ||
			label > 0xFFFFF ||
			softnic_parser_read_uint32(&tc, tokens[2]) ||
			tc > 0x7 ||
			softnic_parser_read_uint32(&ttl, tokens[3]) ||
			ttl > 0x3F)
			return 0;

		a->encap.mpls.mpls[3].label = label;
		a->encap.mpls.mpls[3].tc = tc;
		a->encap.mpls.mpls[3].ttl = ttl;

		a->encap.mpls.mpls_count = 4;
		a->encap.type = RTE_TABLE_ACTION_ENCAP_MPLS;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 8 + 4 + 4 + 4;
	}

	/* pppoe */
	if (n_tokens && (strcmp(tokens[0], "pppoe") == 0)) {
		if (n_tokens < 4 ||
			softnic_parse_mac_addr(tokens[1], &a->encap.pppoe.ether.da) ||
			softnic_parse_mac_addr(tokens[2], &a->encap.pppoe.ether.sa) ||
			softnic_parser_read_uint16(&a->encap.pppoe.pppoe.session_id,
				tokens[3]))
			return 0;

		a->encap.type = RTE_TABLE_ACTION_ENCAP_PPPOE;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 4;
	}

	/* vxlan */
	if (n_tokens && (strcmp(tokens[0], "vxlan") == 0)) {
		uint32_t n = 0;

		n_tokens--;
		tokens++;
		n++;

		/* ether <da> <sa> */
		if ((n_tokens < 3) ||
			strcmp(tokens[0], "ether") ||
			softnic_parse_mac_addr(tokens[1], &a->encap.vxlan.ether.da) ||
			softnic_parse_mac_addr(tokens[2], &a->encap.vxlan.ether.sa))
			return 0;

		n_tokens -= 3;
		tokens += 3;
		n += 3;

		/* [vlan <pcp> <dei> <vid>] */
		if (strcmp(tokens[0], "vlan") == 0) {
			uint32_t pcp, dei, vid;

			if ((n_tokens < 4) ||
				softnic_parser_read_uint32(&pcp, tokens[1]) ||
				(pcp > 7) ||
				softnic_parser_read_uint32(&dei, tokens[2]) ||
				(dei > 1) ||
				softnic_parser_read_uint32(&vid, tokens[3]) ||
				(vid > 0xFFF))
				return 0;

			a->encap.vxlan.vlan.pcp = pcp;
			a->encap.vxlan.vlan.dei = dei;
			a->encap.vxlan.vlan.vid = vid;

			n_tokens -= 4;
			tokens += 4;
			n += 4;
		}

		/* ipv4 <sa> <da> <dscp> <ttl>
		   | ipv6 <sa> <da> <flow_label> <dscp> <hop_limit> */
		if (strcmp(tokens[0], "ipv4") == 0) {
			struct in_addr sa, da;
			uint8_t dscp, ttl;

			if ((n_tokens < 5) ||
				softnic_parse_ipv4_addr(tokens[1], &sa) ||
				softnic_parse_ipv4_addr(tokens[2], &da) ||
				softnic_parser_read_uint8(&dscp, tokens[3]) ||
				(dscp > 64) ||
				softnic_parser_read_uint8(&ttl, tokens[4]))
				return 0;

			a->encap.vxlan.ipv4.sa = rte_be_to_cpu_32(sa.s_addr);
			a->encap.vxlan.ipv4.da = rte_be_to_cpu_32(da.s_addr);
			a->encap.vxlan.ipv4.dscp = dscp;
			a->encap.vxlan.ipv4.ttl = ttl;

			n_tokens -= 5;
			tokens += 5;
			n += 5;
		} else if (strcmp(tokens[0], "ipv6") == 0) {
			struct in6_addr sa, da;
			uint32_t flow_label;
			uint8_t dscp, hop_limit;

			if ((n_tokens < 6) ||
				softnic_parse_ipv6_addr(tokens[1], &sa) ||
				softnic_parse_ipv6_addr(tokens[2], &da) ||
				softnic_parser_read_uint32(&flow_label, tokens[3]) ||
				softnic_parser_read_uint8(&dscp, tokens[4]) ||
				(dscp > 64) ||
				softnic_parser_read_uint8(&hop_limit, tokens[5]))
				return 0;

			memcpy(a->encap.vxlan.ipv6.sa, sa.s6_addr, 16);
			memcpy(a->encap.vxlan.ipv6.da, da.s6_addr, 16);
			a->encap.vxlan.ipv6.flow_label = flow_label;
			a->encap.vxlan.ipv6.dscp = dscp;
			a->encap.vxlan.ipv6.hop_limit = hop_limit;

			n_tokens -= 6;
			tokens += 6;
			n += 6;
		} else
			return 0;

		/* udp <sp> <dp> */
		if ((n_tokens < 3) ||
			strcmp(tokens[0], "udp") ||
			softnic_parser_read_uint16(&a->encap.vxlan.udp.sp, tokens[1]) ||
			softnic_parser_read_uint16(&a->encap.vxlan.udp.dp, tokens[2]))
			return 0;

		n_tokens -= 3;
		tokens += 3;
		n += 3;

		/* vxlan <vni> */
		if ((n_tokens < 2) ||
			strcmp(tokens[0], "vxlan") ||
			softnic_parser_read_uint32(&a->encap.vxlan.vxlan.vni, tokens[1]) ||
			(a->encap.vxlan.vxlan.vni > 0xFFFFFF))
			return 0;

		n_tokens -= 2;
		tokens += 2;
		n += 2;

		a->encap.type = RTE_TABLE_ACTION_ENCAP_VXLAN;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + n;
	}

	return 0;
}

static uint32_t
parse_table_action_nat(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	if (n_tokens < 4 ||
		strcmp(tokens[0], "nat"))
		return 0;

	if (strcmp(tokens[1], "ipv4") == 0) {
		struct in_addr addr;
		uint16_t port;

		if (softnic_parse_ipv4_addr(tokens[2], &addr) ||
			softnic_parser_read_uint16(&port, tokens[3]))
			return 0;

		a->nat.ip_version = 1;
		a->nat.addr.ipv4 = rte_be_to_cpu_32(addr.s_addr);
		a->nat.port = port;
		a->action_mask |= 1 << RTE_TABLE_ACTION_NAT;
		return 4;
	}

	if (strcmp(tokens[1], "ipv6") == 0) {
		struct in6_addr addr;
		uint16_t port;

		if (softnic_parse_ipv6_addr(tokens[2], &addr) ||
			softnic_parser_read_uint16(&port, tokens[3]))
			return 0;

		a->nat.ip_version = 0;
		memcpy(a->nat.addr.ipv6, addr.s6_addr, 16);
		a->nat.port = port;
		a->action_mask |= 1 << RTE_TABLE_ACTION_NAT;
		return 4;
	}

	return 0;
}

static uint32_t
parse_table_action_ttl(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	if (n_tokens < 2 ||
		strcmp(tokens[0], "ttl"))
		return 0;

	if (strcmp(tokens[1], "dec") == 0)
		a->ttl.decrement = 1;
	else if (strcmp(tokens[1], "keep") == 0)
		a->ttl.decrement = 0;
	else
		return 0;

	a->action_mask |= 1 << RTE_TABLE_ACTION_TTL;
	return 2;
}

static uint32_t
parse_table_action_stats(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	if (n_tokens < 1 ||
		strcmp(tokens[0], "stats"))
		return 0;

	a->stats.n_packets = 0;
	a->stats.n_bytes = 0;
	a->action_mask |= 1 << RTE_TABLE_ACTION_STATS;
	return 1;
}

static uint32_t
parse_table_action_time(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	if (n_tokens < 1 ||
		strcmp(tokens[0], "time"))
		return 0;

	a->time.time = rte_rdtsc();
	a->action_mask |= 1 << RTE_TABLE_ACTION_TIME;
	return 1;
}

static void
parse_free_sym_crypto_param_data(struct rte_table_action_sym_crypto_params *p)
{
	struct rte_crypto_sym_xform *xform[2] = {NULL};
	uint32_t i;

	xform[0] = p->xform;
	if (xform[0])
		xform[1] = xform[0]->next;

	for (i = 0; i < 2; i++) {
		if (xform[i] == NULL)
			continue;

		switch (xform[i]->type) {
		case RTE_CRYPTO_SYM_XFORM_CIPHER:
			if (xform[i]->cipher.key.data)
				free(xform[i]->cipher.key.data);
			if (p->cipher_auth.cipher_iv.val)
				free(p->cipher_auth.cipher_iv.val);
			if (p->cipher_auth.cipher_iv_update.val)
				free(p->cipher_auth.cipher_iv_update.val);
			break;
		case RTE_CRYPTO_SYM_XFORM_AUTH:
			if (xform[i]->auth.key.data)
				free(xform[i]->cipher.key.data);
			if (p->cipher_auth.auth_iv.val)
				free(p->cipher_auth.cipher_iv.val);
			if (p->cipher_auth.auth_iv_update.val)
				free(p->cipher_auth.cipher_iv_update.val);
			break;
		case RTE_CRYPTO_SYM_XFORM_AEAD:
			if (xform[i]->aead.key.data)
				free(xform[i]->cipher.key.data);
			if (p->aead.iv.val)
				free(p->aead.iv.val);
			if (p->aead.aad.val)
				free(p->aead.aad.val);
			break;
		default:
			continue;
		}
	}

}

static struct rte_crypto_sym_xform *
parse_table_action_cipher(struct rte_table_action_sym_crypto_params *p,
		char **tokens, uint32_t n_tokens, uint32_t encrypt,
		uint32_t *used_n_tokens)
{
	struct rte_crypto_sym_xform *xform_cipher;
	int status;
	size_t len;

	if (n_tokens < 7 || strcmp(tokens[1], "cipher_algo") ||
			strcmp(tokens[3], "cipher_key") ||
			strcmp(tokens[5], "cipher_iv"))
		return NULL;

	xform_cipher = calloc(1, sizeof(*xform_cipher));
	if (xform_cipher == NULL)
		return NULL;

	xform_cipher->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	xform_cipher->cipher.op = encrypt ? RTE_CRYPTO_CIPHER_OP_ENCRYPT :
			RTE_CRYPTO_CIPHER_OP_DECRYPT;

	/* cipher_algo */
	status = rte_cryptodev_get_cipher_algo_enum(
			&xform_cipher->cipher.algo, tokens[2]);
	if (status < 0)
		goto error_exit;

	/* cipher_key */
	len = strlen(tokens[4]);
	xform_cipher->cipher.key.data = calloc(1, len / 2 + 1);
	if (xform_cipher->cipher.key.data == NULL)
		goto error_exit;

	status = softnic_parse_hex_string(tokens[4],
			xform_cipher->cipher.key.data,
			(uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_cipher->cipher.key.length = (uint16_t)len;

	/* cipher_iv */
	len = strlen(tokens[6]);

	p->cipher_auth.cipher_iv.val = calloc(1, len / 2 + 1);
	if (p->cipher_auth.cipher_iv.val == NULL)
		goto error_exit;

	status = softnic_parse_hex_string(tokens[6],
			p->cipher_auth.cipher_iv.val,
			(uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_cipher->cipher.iv.length = (uint16_t)len;
	xform_cipher->cipher.iv.offset = RTE_TABLE_ACTION_SYM_CRYPTO_IV_OFFSET;
	p->cipher_auth.cipher_iv.length = (uint32_t)len;
	*used_n_tokens = 7;

	return xform_cipher;

error_exit:
	if (xform_cipher->cipher.key.data)
		free(xform_cipher->cipher.key.data);

	if (p->cipher_auth.cipher_iv.val) {
		free(p->cipher_auth.cipher_iv.val);
		p->cipher_auth.cipher_iv.val = NULL;
	}

	free(xform_cipher);

	return NULL;
}

static struct rte_crypto_sym_xform *
parse_table_action_cipher_auth(struct rte_table_action_sym_crypto_params *p,
		char **tokens, uint32_t n_tokens, uint32_t encrypt,
		uint32_t *used_n_tokens)
{
	struct rte_crypto_sym_xform *xform_cipher;
	struct rte_crypto_sym_xform *xform_auth;
	int status;
	size_t len;

	if (n_tokens < 13 ||
			strcmp(tokens[7], "auth_algo") ||
			strcmp(tokens[9], "auth_key") ||
			strcmp(tokens[11], "digest_size"))
		return NULL;

	xform_auth = calloc(1, sizeof(*xform_auth));
	if (xform_auth == NULL)
		return NULL;

	xform_auth->type = RTE_CRYPTO_SYM_XFORM_AUTH;
	xform_auth->auth.op = encrypt ? RTE_CRYPTO_AUTH_OP_GENERATE :
			RTE_CRYPTO_AUTH_OP_VERIFY;

	/* auth_algo */
	status = rte_cryptodev_get_auth_algo_enum(&xform_auth->auth.algo,
			tokens[8]);
	if (status < 0)
		goto error_exit;

	/* auth_key */
	len = strlen(tokens[10]);
	xform_auth->auth.key.data = calloc(1, len / 2 + 1);
	if (xform_auth->auth.key.data == NULL)
		goto error_exit;

	status = softnic_parse_hex_string(tokens[10],
			xform_auth->auth.key.data, (uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_auth->auth.key.length = (uint16_t)len;

	if (strcmp(tokens[11], "digest_size"))
		goto error_exit;

	status = softnic_parser_read_uint16(&xform_auth->auth.digest_length,
			tokens[12]);
	if (status < 0)
		goto error_exit;

	xform_cipher = parse_table_action_cipher(p, tokens, 7, encrypt,
			used_n_tokens);
	if (xform_cipher == NULL)
		goto error_exit;

	*used_n_tokens += 6;

	if (encrypt) {
		xform_cipher->next = xform_auth;
		return xform_cipher;
	} else {
		xform_auth->next = xform_cipher;
		return xform_auth;
	}

error_exit:
	if (xform_auth->auth.key.data)
		free(xform_auth->auth.key.data);
	if (p->cipher_auth.auth_iv.val) {
		free(p->cipher_auth.auth_iv.val);
		p->cipher_auth.auth_iv.val = 0;
	}

	free(xform_auth);

	return NULL;
}

static struct rte_crypto_sym_xform *
parse_table_action_aead(struct rte_table_action_sym_crypto_params *p,
		char **tokens, uint32_t n_tokens, uint32_t encrypt,
		uint32_t *used_n_tokens)
{
	struct rte_crypto_sym_xform *xform_aead;
	int status;
	size_t len;

	if (n_tokens < 11 || strcmp(tokens[1], "aead_algo") ||
			strcmp(tokens[3], "aead_key") ||
			strcmp(tokens[5], "aead_iv") ||
			strcmp(tokens[7], "aead_aad") ||
			strcmp(tokens[9], "digest_size"))
		return NULL;

	xform_aead = calloc(1, sizeof(*xform_aead));
	if (xform_aead == NULL)
		return NULL;

	xform_aead->type = RTE_CRYPTO_SYM_XFORM_AEAD;
	xform_aead->aead.op = encrypt ? RTE_CRYPTO_AEAD_OP_ENCRYPT :
			RTE_CRYPTO_AEAD_OP_DECRYPT;

	/* aead_algo */
	status = rte_cryptodev_get_aead_algo_enum(&xform_aead->aead.algo,
			tokens[2]);
	if (status < 0)
		goto error_exit;

	/* aead_key */
	len = strlen(tokens[4]);
	xform_aead->aead.key.data = calloc(1, len / 2 + 1);
	if (xform_aead->aead.key.data == NULL)
		goto error_exit;

	status = softnic_parse_hex_string(tokens[4], xform_aead->aead.key.data,
			(uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_aead->aead.key.length = (uint16_t)len;

	/* aead_iv */
	len = strlen(tokens[6]);
	p->aead.iv.val = calloc(1, len / 2 + 1);
	if (p->aead.iv.val == NULL)
		goto error_exit;

	status = softnic_parse_hex_string(tokens[6], p->aead.iv.val,
			(uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_aead->aead.iv.length = (uint16_t)len;
	xform_aead->aead.iv.offset = RTE_TABLE_ACTION_SYM_CRYPTO_IV_OFFSET;
	p->aead.iv.length = (uint32_t)len;

	/* aead_aad */
	len = strlen(tokens[8]);
	p->aead.aad.val = calloc(1, len / 2 + 1);
	if (p->aead.aad.val == NULL)
		goto error_exit;

	status = softnic_parse_hex_string(tokens[8], p->aead.aad.val, (uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_aead->aead.aad_length = (uint16_t)len;
	p->aead.aad.length = (uint32_t)len;

	/* digest_size */
	status = softnic_parser_read_uint16(&xform_aead->aead.digest_length,
			tokens[10]);
	if (status < 0)
		goto error_exit;

	*used_n_tokens = 11;

	return xform_aead;

error_exit:
	if (xform_aead->aead.key.data)
		free(xform_aead->aead.key.data);
	if (p->aead.iv.val) {
		free(p->aead.iv.val);
		p->aead.iv.val = NULL;
	}
	if (p->aead.aad.val) {
		free(p->aead.aad.val);
		p->aead.aad.val = NULL;
	}

	free(xform_aead);

	return NULL;
}


static uint32_t
parse_table_action_sym_crypto(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	struct rte_table_action_sym_crypto_params *p = &a->sym_crypto;
	struct rte_crypto_sym_xform *xform = NULL;
	uint32_t used_n_tokens;
	uint32_t encrypt;
	int status;

	if ((n_tokens < 12) ||
		strcmp(tokens[0], "sym_crypto") ||
		strcmp(tokens[2], "type"))
		return 0;

	memset(p, 0, sizeof(*p));

	if (strcmp(tokens[1], "encrypt") == 0)
		encrypt = 1;
	else
		encrypt = 0;

	status = softnic_parser_read_uint32(&p->data_offset, tokens[n_tokens - 1]);
	if (status < 0)
		return 0;

	if (strcmp(tokens[3], "cipher") == 0) {
		tokens += 3;
		n_tokens -= 3;

		xform = parse_table_action_cipher(p, tokens, n_tokens, encrypt,
				&used_n_tokens);
	} else if (strcmp(tokens[3], "cipher_auth") == 0) {
		tokens += 3;
		n_tokens -= 3;

		xform = parse_table_action_cipher_auth(p, tokens, n_tokens,
				encrypt, &used_n_tokens);
	} else if (strcmp(tokens[3], "aead") == 0) {
		tokens += 3;
		n_tokens -= 3;

		xform = parse_table_action_aead(p, tokens, n_tokens, encrypt,
				&used_n_tokens);
	}

	if (xform == NULL)
		return 0;

	p->xform = xform;

	if (strcmp(tokens[used_n_tokens], "data_offset")) {
		parse_free_sym_crypto_param_data(p);
		return 0;
	}

	a->action_mask |= 1 << RTE_TABLE_ACTION_SYM_CRYPTO;

	return used_n_tokens + 5;
}

static uint32_t
parse_table_action_tag(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	if (n_tokens < 2 ||
		strcmp(tokens[0], "tag"))
		return 0;

	if (softnic_parser_read_uint32(&a->tag.tag, tokens[1]))
		return 0;

	a->action_mask |= 1 << RTE_TABLE_ACTION_TAG;
	return 2;
}

static uint32_t
parse_table_action_decap(char **tokens,
	uint32_t n_tokens,
	struct softnic_table_rule_action *a)
{
	if (n_tokens < 2 ||
		strcmp(tokens[0], "decap"))
		return 0;

	if (softnic_parser_read_uint16(&a->decap.n, tokens[1]))
		return 0;

	a->action_mask |= 1 << RTE_TABLE_ACTION_DECAP;
	return 2;
}

static uint32_t
parse_table_action(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	struct softnic_table_rule_action *a)
{
	uint32_t n_tokens0 = n_tokens;

	memset(a, 0, sizeof(*a));

	if (n_tokens < 2 ||
		strcmp(tokens[0], "action"))
		return 0;

	tokens++;
	n_tokens--;

	if (n_tokens && (strcmp(tokens[0], "fwd") == 0)) {
		uint32_t n;

		n = parse_table_action_fwd(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action fwd");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "balance") == 0)) {
		uint32_t n;

		n = parse_table_action_balance(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action balance");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "meter") == 0)) {
		uint32_t n;

		n = parse_table_action_meter(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action meter");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "tm") == 0)) {
		uint32_t n;

		n = parse_table_action_tm(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action tm");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "encap") == 0)) {
		uint32_t n;

		n = parse_table_action_encap(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action encap");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "nat") == 0)) {
		uint32_t n;

		n = parse_table_action_nat(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action nat");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "ttl") == 0)) {
		uint32_t n;

		n = parse_table_action_ttl(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action ttl");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "stats") == 0)) {
		uint32_t n;

		n = parse_table_action_stats(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action stats");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "time") == 0)) {
		uint32_t n;

		n = parse_table_action_time(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action time");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "tag") == 0)) {
		uint32_t n;

		n = parse_table_action_tag(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action tag");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "decap") == 0)) {
		uint32_t n;

		n = parse_table_action_decap(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action decap");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "sym_crypto") == 0)) {
		uint32_t n;

		n = parse_table_action_sym_crypto(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action sym_crypto");
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens0 - n_tokens == 1) {
		snprintf(out, out_size, MSG_ARG_INVALID, "action");
		return 0;
	}

	return n_tokens0 - n_tokens;
}

/**
 * pipeline <pipeline_name> table <table_id> rule add
 *    match <match>
 *    action <table_action>
 */
static void
cmd_softnic_pipeline_table_rule_add(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_table_rule_match m;
	struct softnic_table_rule_action a;
	char *pipeline_name;
	void *data;
	uint32_t table_id, t0, n_tokens_parsed;
	int status;

	if (n_tokens < 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "add") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		return;
	}

	t0 = 6;

	/* match */
	n_tokens_parsed = parse_match(tokens + t0,
		n_tokens - t0,
		out,
		out_size,
		&m);
	if (n_tokens_parsed == 0)
		return;
	t0 += n_tokens_parsed;

	/* action */
	n_tokens_parsed = parse_table_action(tokens + t0,
		n_tokens - t0,
		out,
		out_size,
		&a);
	if (n_tokens_parsed == 0)
		return;
	t0 += n_tokens_parsed;

	if (t0 != n_tokens) {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	status = softnic_pipeline_table_rule_add(softnic,
		pipeline_name,
		table_id,
		&m,
		&a,
		&data);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> table <table_id> rule add
 *    match
 *       default
 *    action
 *       fwd
 *          drop
 *          | port <port_id>
 *          | meta
 *          | table <table_id>
 */
static void
cmd_softnic_pipeline_table_rule_add_default(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_table_rule_action action;
	void *data;
	char *pipeline_name;
	uint32_t table_id;
	int status;

	if (n_tokens != 11 &&
		n_tokens != 12) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "add") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		return;
	}

	if (strcmp(tokens[6], "match") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "match");
		return;
	}

	if (strcmp(tokens[7], "default") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "default");
		return;
	}

	if (strcmp(tokens[8], "action") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "action");
		return;
	}

	if (strcmp(tokens[9], "fwd") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "fwd");
		return;
	}

	action.action_mask = 1 << RTE_TABLE_ACTION_FWD;

	if (strcmp(tokens[10], "drop") == 0) {
		if (n_tokens != 11) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		action.fwd.action = RTE_PIPELINE_ACTION_DROP;
	} else if (strcmp(tokens[10], "port") == 0) {
		uint32_t id;

		if (n_tokens != 12) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		if (softnic_parser_read_uint32(&id, tokens[11]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
			return;
		}

		action.fwd.action = RTE_PIPELINE_ACTION_PORT;
		action.fwd.id = id;
	} else if (strcmp(tokens[10], "meta") == 0) {
		if (n_tokens != 11) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		action.fwd.action = RTE_PIPELINE_ACTION_PORT_META;
	} else if (strcmp(tokens[10], "table") == 0) {
		uint32_t id;

		if (n_tokens != 12) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		if (softnic_parser_read_uint32(&id, tokens[11]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
			return;
		}

		action.fwd.action = RTE_PIPELINE_ACTION_TABLE;
		action.fwd.id = id;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID,
			"drop or port or meta or table");
		return;
	}

	status = softnic_pipeline_table_rule_add_default(softnic,
		pipeline_name,
		table_id,
		&action,
		&data);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> table <table_id> rule add bulk <file_name> <n_rules>
 *
 * File <file_name>:
 * - line format: match <match> action <action>
 */
static int
cli_rule_file_process(const char *file_name,
	size_t line_len_max,
	struct softnic_table_rule_match *m,
	struct softnic_table_rule_action *a,
	uint32_t *n_rules,
	uint32_t *line_number,
	char *out,
	size_t out_size);

static void
cmd_softnic_pipeline_table_rule_add_bulk(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_table_rule_match *match;
	struct softnic_table_rule_action *action;
	void **data;
	char *pipeline_name, *file_name;
	uint32_t table_id, n_rules, n_rules_parsed, line_number;
	int status;

	if (n_tokens != 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "add") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		return;
	}

	if (strcmp(tokens[6], "bulk") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "bulk");
		return;
	}

	file_name = tokens[7];

	if ((softnic_parser_read_uint32(&n_rules, tokens[8]) != 0) ||
		n_rules == 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_rules");
		return;
	}

	/* Memory allocation. */
	match = calloc(n_rules, sizeof(struct softnic_table_rule_match));
	action = calloc(n_rules, sizeof(struct softnic_table_rule_action));
	data = calloc(n_rules, sizeof(void *));
	if (match == NULL ||
		action == NULL ||
		data == NULL) {
		snprintf(out, out_size, MSG_OUT_OF_MEMORY);
		free(data);
		free(action);
		free(match);
		return;
	}

	/* Load rule file */
	n_rules_parsed = n_rules;
	status = cli_rule_file_process(file_name,
		1024,
		match,
		action,
		&n_rules_parsed,
		&line_number,
		out,
		out_size);
	if (status) {
		snprintf(out, out_size, MSG_FILE_ERR, file_name, line_number);
		free(data);
		free(action);
		free(match);
		return;
	}
	if (n_rules_parsed != n_rules) {
		snprintf(out, out_size, MSG_FILE_NOT_ENOUGH, file_name);
		free(data);
		free(action);
		free(match);
		return;
	}

	/* Rule bulk add */
	status = softnic_pipeline_table_rule_add_bulk(softnic,
		pipeline_name,
		table_id,
		match,
		action,
		data,
		&n_rules);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		free(data);
		free(action);
		free(match);
		return;
	}

	/* Memory free */
	free(data);
	free(action);
	free(match);
}

/**
 * pipeline <pipeline_name> table <table_id> rule delete
 *    match <match>
 */
static void
cmd_softnic_pipeline_table_rule_delete(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_table_rule_match m;
	char *pipeline_name;
	uint32_t table_id, n_tokens_parsed, t0;
	int status;

	if (n_tokens < 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "delete") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "delete");
		return;
	}

	t0 = 6;

	/* match */
	n_tokens_parsed = parse_match(tokens + t0,
		n_tokens - t0,
		out,
		out_size,
		&m);
	if (n_tokens_parsed == 0)
		return;
	t0 += n_tokens_parsed;

	if (n_tokens != t0) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	status = softnic_pipeline_table_rule_delete(softnic,
		pipeline_name,
		table_id,
		&m);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> table <table_id> rule delete
 *    match
 *       default
 */
static void
cmd_softnic_pipeline_table_rule_delete_default(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t table_id;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "delete") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "delete");
		return;
	}

	if (strcmp(tokens[6], "match") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "match");
		return;
	}

	if (strcmp(tokens[7], "default") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "default");
		return;
	}

	status = softnic_pipeline_table_rule_delete_default(softnic,
		pipeline_name,
		table_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> table <table_id> rule read stats [clear]
 */
static void
cmd_softnic_pipeline_table_rule_stats_read(struct pmd_internals *softnic __rte_unused,
	char **tokens,
	uint32_t n_tokens __rte_unused,
	char *out,
	size_t out_size)
{
	snprintf(out, out_size, MSG_CMD_UNIMPLEM, tokens[0]);
}

/**
 * pipeline <pipeline_name> table <table_id> meter profile <meter_profile_id>
 *  add srtcm cir <cir> cbs <cbs> ebs <ebs>
 *  | trtcm cir <cir> pir <pir> cbs <cbs> pbs <pbs>
 */
static void
cmd_pipeline_table_meter_profile_add(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_table_action_meter_profile p;
	char *pipeline_name;
	uint32_t table_id, meter_profile_id;
	int status;

	if (n_tokens < 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "meter") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	if (strcmp(tokens[5], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	if (softnic_parser_read_uint32(&meter_profile_id, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "meter_profile_id");
		return;
	}

	if (strcmp(tokens[7], "add") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		return;
	}

	if (strcmp(tokens[8], "srtcm") == 0) {
		if (n_tokens != 15) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				tokens[0]);
			return;
		}

		p.alg = RTE_TABLE_ACTION_METER_SRTCM;

		if (strcmp(tokens[9], "cir") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cir");
			return;
		}

		if (softnic_parser_read_uint64(&p.srtcm.cir, tokens[10]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "cir");
			return;
		}

		if (strcmp(tokens[11], "cbs") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cbs");
			return;
		}

		if (softnic_parser_read_uint64(&p.srtcm.cbs, tokens[12]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "cbs");
			return;
		}

		if (strcmp(tokens[13], "ebs") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "ebs");
			return;
		}

		if (softnic_parser_read_uint64(&p.srtcm.ebs, tokens[14]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "ebs");
			return;
		}
	} else if (strcmp(tokens[8], "trtcm") == 0) {
		if (n_tokens != 17) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		p.alg = RTE_TABLE_ACTION_METER_TRTCM;

		if (strcmp(tokens[9], "cir") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cir");
			return;
		}

		if (softnic_parser_read_uint64(&p.trtcm.cir, tokens[10]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "cir");
			return;
		}

		if (strcmp(tokens[11], "pir") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pir");
			return;
		}

		if (softnic_parser_read_uint64(&p.trtcm.pir, tokens[12]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "pir");
			return;
		}
		if (strcmp(tokens[13], "cbs") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cbs");
			return;
		}

		if (softnic_parser_read_uint64(&p.trtcm.cbs, tokens[14]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "cbs");
			return;
		}

		if (strcmp(tokens[15], "pbs") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pbs");
			return;
		}

		if (softnic_parser_read_uint64(&p.trtcm.pbs, tokens[16]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "pbs");
			return;
		}
	} else {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	status = softnic_pipeline_table_mtr_profile_add(softnic,
		pipeline_name,
		table_id,
		meter_profile_id,
		&p);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> table <table_id>
 *  meter profile <meter_profile_id> delete
 */
static void
cmd_pipeline_table_meter_profile_delete(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t table_id, meter_profile_id;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "meter") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	if (strcmp(tokens[5], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	if (softnic_parser_read_uint32(&meter_profile_id, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "meter_profile_id");
		return;
	}

	if (strcmp(tokens[7], "delete") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "delete");
		return;
	}

	status = softnic_pipeline_table_mtr_profile_delete(softnic,
		pipeline_name,
		table_id,
		meter_profile_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> table <table_id> rule read meter [clear]
 */
static void
cmd_pipeline_table_rule_meter_read(struct pmd_internals *softnic __rte_unused,
	char **tokens,
	uint32_t n_tokens __rte_unused,
	char *out,
	size_t out_size)
{
	snprintf(out, out_size, MSG_CMD_UNIMPLEM, tokens[0]);
}

/**
 * pipeline <pipeline_name> table <table_id> dscp <file_name>
 *
 * File <file_name>:
 *  - exactly 64 lines
 *  - line format: <tc_id> <tc_queue_id> <color>, with <color> as: g | y | r
 */
static int
load_dscp_table(struct rte_table_action_dscp_table *dscp_table,
	const char *file_name,
	uint32_t *line_number)
{
	FILE *f = NULL;
	uint32_t dscp, l;

	/* Check input arguments */
	if (dscp_table == NULL ||
		file_name == NULL ||
		line_number == NULL) {
		if (line_number)
			*line_number = 0;
		return -EINVAL;
	}

	/* Open input file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		*line_number = 0;
		return -EINVAL;
	}

	/* Read file */
	for (dscp = 0, l = 1; ; l++) {
		char line[64];
		char *tokens[3];
		enum rte_color color;
		uint32_t tc_id, tc_queue_id, n_tokens = RTE_DIM(tokens);

		if (fgets(line, sizeof(line), f) == NULL)
			break;

		if (is_comment(line))
			continue;

		if (softnic_parse_tokenize_string(line, tokens, &n_tokens)) {
			*line_number = l;
			fclose(f);
			return -EINVAL;
		}

		if (n_tokens == 0)
			continue;

		if (dscp >= RTE_DIM(dscp_table->entry) ||
			n_tokens != RTE_DIM(tokens) ||
			softnic_parser_read_uint32(&tc_id, tokens[0]) ||
			tc_id >= RTE_TABLE_ACTION_TC_MAX ||
			softnic_parser_read_uint32(&tc_queue_id, tokens[1]) ||
			tc_queue_id >= RTE_TABLE_ACTION_TC_QUEUE_MAX ||
			(strlen(tokens[2]) != 1)) {
			*line_number = l;
			fclose(f);
			return -EINVAL;
		}

		switch (tokens[2][0]) {
		case 'g':
		case 'G':
			color = RTE_COLOR_GREEN;
			break;

		case 'y':
		case 'Y':
			color = RTE_COLOR_YELLOW;
			break;

		case 'r':
		case 'R':
			color = RTE_COLOR_RED;
			break;

		default:
			*line_number = l;
			fclose(f);
			return -EINVAL;
		}

		dscp_table->entry[dscp].tc_id = tc_id;
		dscp_table->entry[dscp].tc_queue_id = tc_queue_id;
		dscp_table->entry[dscp].color = color;
		dscp++;
	}

	/* Close file */
	fclose(f);
	return 0;
}

static void
cmd_pipeline_table_dscp(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_table_action_dscp_table dscp_table;
	char *pipeline_name, *file_name;
	uint32_t table_id, line_number;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "dscp") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "dscp");
		return;
	}

	file_name = tokens[5];

	status = load_dscp_table(&dscp_table, file_name, &line_number);
	if (status) {
		snprintf(out, out_size, MSG_FILE_ERR, file_name, line_number);
		return;
	}

	status = softnic_pipeline_table_dscp_table_update(softnic,
		pipeline_name,
		table_id,
		UINT64_MAX,
		&dscp_table);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> table <table_id> rule read ttl [clear]
 */
static void
cmd_softnic_pipeline_table_rule_ttl_read(struct pmd_internals *softnic __rte_unused,
	char **tokens,
	uint32_t n_tokens __rte_unused,
	char *out,
	size_t out_size)
{
	snprintf(out, out_size, MSG_CMD_UNIMPLEM, tokens[0]);
}

/**
 * thread <thread_id> pipeline <pipeline_name> enable
 */
static void
cmd_softnic_thread_pipeline_enable(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t thread_id;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (softnic_parser_read_uint32(&thread_id, tokens[1]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "thread_id");
		return;
	}

	if (strcmp(tokens[2], "pipeline") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipeline");
		return;
	}

	pipeline_name = tokens[3];

	if (strcmp(tokens[4], "enable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "enable");
		return;
	}

	status = softnic_thread_pipeline_enable(softnic, thread_id, pipeline_name);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, "thread pipeline enable");
		return;
	}
}

/**
 * thread <thread_id> pipeline <pipeline_name> disable
 */
static void
cmd_softnic_thread_pipeline_disable(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t thread_id;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (softnic_parser_read_uint32(&thread_id, tokens[1]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "thread_id");
		return;
	}

	if (strcmp(tokens[2], "pipeline") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipeline");
		return;
	}

	pipeline_name = tokens[3];

	if (strcmp(tokens[4], "disable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "disable");
		return;
	}

	status = softnic_thread_pipeline_disable(softnic, thread_id, pipeline_name);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL,
			"thread pipeline disable");
		return;
	}
}

/**
 * flowapi map
 *  group <group_id>
 *  ingress | egress
 *  pipeline <pipeline_name>
 *  table <table_id>
 */
static void
cmd_softnic_flowapi_map(struct pmd_internals *softnic,
		char **tokens,
		uint32_t n_tokens,
		char *out,
		size_t out_size)
{
	char *pipeline_name;
	uint32_t group_id, table_id;
	int ingress, status;

	if (n_tokens != 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "map") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "map");
		return;
	}

	if (strcmp(tokens[2], "group") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "group");
		return;
	}

	if (softnic_parser_read_uint32(&group_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "group_id");
		return;
	}

	if (strcmp(tokens[4], "ingress") == 0) {
		ingress = 1;
	} else if (strcmp(tokens[4], "egress") == 0) {
		ingress = 0;
	} else {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "ingress | egress");
		return;
	}

	if (strcmp(tokens[5], "pipeline") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipeline");
		return;
	}

	pipeline_name = tokens[6];

	if (strcmp(tokens[7], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[8]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	status = flow_attr_map_set(softnic,
			group_id,
			ingress,
			pipeline_name,
			table_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

void
softnic_cli_process(char *in, char *out, size_t out_size, void *arg)
{
	char *tokens[CMD_MAX_TOKENS];
	uint32_t n_tokens = RTE_DIM(tokens);
	struct pmd_internals *softnic = arg;
	int status;

	if (is_comment(in))
		return;

	status = softnic_parse_tokenize_string(in, tokens, &n_tokens);
	if (status) {
		snprintf(out, out_size, MSG_ARG_TOO_MANY, "");
		return;
	}

	if (n_tokens == 0)
		return;

	if (strcmp(tokens[0], "mempool") == 0) {
		cmd_mempool(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "link") == 0) {
		cmd_link(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "swq") == 0) {
		cmd_swq(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "tmgr") == 0) {
		if (n_tokens == 2) {
			cmd_tmgr(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 3 &&
			(strcmp(tokens[1], "shaper") == 0) &&
			(strcmp(tokens[2], "profile") == 0)) {
			cmd_tmgr_shaper_profile(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 3 &&
			(strcmp(tokens[1], "shared") == 0) &&
			(strcmp(tokens[2], "shaper") == 0)) {
			cmd_tmgr_shared_shaper(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 2 &&
			(strcmp(tokens[1], "node") == 0)) {
			cmd_tmgr_node(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 2 &&
			(strcmp(tokens[1], "hierarchy-default") == 0)) {
			cmd_tmgr_hierarchy_default(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 3 &&
			(strcmp(tokens[1], "hierarchy") == 0) &&
			(strcmp(tokens[2], "commit") == 0)) {
			cmd_tmgr_hierarchy_commit(softnic, tokens, n_tokens, out, out_size);
			return;
		}
	}

	if (strcmp(tokens[0], "tap") == 0) {
		cmd_tap(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "cryptodev") == 0) {
		cmd_cryptodev(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "port") == 0) {
		cmd_port_in_action_profile(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "table") == 0) {
		cmd_table_action_profile(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "pipeline") == 0) {
		if (n_tokens >= 3 &&
			(strcmp(tokens[2], "period") == 0)) {
			cmd_pipeline(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 5 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[4], "bsz") == 0)) {
			cmd_pipeline_port_in(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 5 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "out") == 0) &&
			(strcmp(tokens[4], "bsz") == 0)) {
			cmd_pipeline_port_out(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 4 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[3], "match") == 0)) {
			cmd_pipeline_table(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "table") == 0)) {
			cmd_pipeline_port_in_table(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "stats") == 0)) {
			cmd_pipeline_port_in_stats(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "enable") == 0)) {
			cmd_softnic_pipeline_port_in_enable(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "disable") == 0)) {
			cmd_softnic_pipeline_port_in_disable(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "out") == 0) &&
			(strcmp(tokens[5], "stats") == 0)) {
			cmd_pipeline_port_out_stats(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 5 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "stats") == 0)) {
			cmd_pipeline_table_stats(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 7 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "add") == 0) &&
			(strcmp(tokens[6], "match") == 0)) {
			if (n_tokens >= 8 &&
				(strcmp(tokens[7], "default") == 0)) {
				cmd_softnic_pipeline_table_rule_add_default(softnic, tokens,
					n_tokens, out, out_size);
				return;
			}

			cmd_softnic_pipeline_table_rule_add(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 7 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "add") == 0) &&
			(strcmp(tokens[6], "bulk") == 0)) {
			cmd_softnic_pipeline_table_rule_add_bulk(softnic, tokens,
				n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 7 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "delete") == 0) &&
			(strcmp(tokens[6], "match") == 0)) {
			if (n_tokens >= 8 &&
				(strcmp(tokens[7], "default") == 0)) {
				cmd_softnic_pipeline_table_rule_delete_default(softnic, tokens,
					n_tokens, out, out_size);
				return;
				}

			cmd_softnic_pipeline_table_rule_delete(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 7 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "read") == 0) &&
			(strcmp(tokens[6], "stats") == 0)) {
			cmd_softnic_pipeline_table_rule_stats_read(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 8 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "meter") == 0) &&
			(strcmp(tokens[5], "profile") == 0) &&
			(strcmp(tokens[7], "add") == 0)) {
			cmd_pipeline_table_meter_profile_add(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 8 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "meter") == 0) &&
			(strcmp(tokens[5], "profile") == 0) &&
			(strcmp(tokens[7], "delete") == 0)) {
			cmd_pipeline_table_meter_profile_delete(softnic, tokens,
				n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 7 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "read") == 0) &&
			(strcmp(tokens[6], "meter") == 0)) {
			cmd_pipeline_table_rule_meter_read(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 5 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "dscp") == 0)) {
			cmd_pipeline_table_dscp(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 7 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "read") == 0) &&
			(strcmp(tokens[6], "ttl") == 0)) {
			cmd_softnic_pipeline_table_rule_ttl_read(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}
	}

	if (strcmp(tokens[0], "thread") == 0) {
		if (n_tokens >= 5 &&
			(strcmp(tokens[4], "enable") == 0)) {
			cmd_softnic_thread_pipeline_enable(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 5 &&
			(strcmp(tokens[4], "disable") == 0)) {
			cmd_softnic_thread_pipeline_disable(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}
	}

	if (strcmp(tokens[0], "flowapi") == 0) {
		cmd_softnic_flowapi_map(softnic, tokens, n_tokens, out,
					out_size);
		return;
	}

	snprintf(out, out_size, MSG_CMD_UNKNOWN, tokens[0]);
}

int
softnic_cli_script_process(struct pmd_internals *softnic,
	const char *file_name,
	size_t msg_in_len_max,
	size_t msg_out_len_max)
{
	char *msg_in = NULL, *msg_out = NULL;
	FILE *f = NULL;

	/* Check input arguments */
	if (file_name == NULL ||
		(strlen(file_name) == 0) ||
		msg_in_len_max == 0 ||
		msg_out_len_max == 0)
		return -EINVAL;

	msg_in = malloc(msg_in_len_max + 1);
	msg_out = malloc(msg_out_len_max + 1);
	if (msg_in == NULL ||
		msg_out == NULL) {
		free(msg_out);
		free(msg_in);
		return -ENOMEM;
	}

	/* Open input file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		free(msg_out);
		free(msg_in);
		return -EIO;
	}

	/* Read file */
	for ( ; ; ) {
		if (fgets(msg_in, msg_in_len_max + 1, f) == NULL)
			break;

		printf("%s", msg_in);
		msg_out[0] = 0;

		softnic_cli_process(msg_in,
			msg_out,
			msg_out_len_max,
			softnic);

		if (strlen(msg_out))
			printf("%s", msg_out);
	}

	/* Close file */
	fclose(f);
	free(msg_out);
	free(msg_in);
	return 0;
}

static int
cli_rule_file_process(const char *file_name,
	size_t line_len_max,
	struct softnic_table_rule_match *m,
	struct softnic_table_rule_action *a,
	uint32_t *n_rules,
	uint32_t *line_number,
	char *out,
	size_t out_size)
{
	FILE *f = NULL;
	char *line = NULL;
	uint32_t rule_id, line_id;
	int status = 0;

	/* Check input arguments */
	if (file_name == NULL ||
		(strlen(file_name) == 0) ||
		line_len_max == 0) {
		*line_number = 0;
		return -EINVAL;
	}

	/* Memory allocation */
	line = malloc(line_len_max + 1);
	if (line == NULL) {
		*line_number = 0;
		return -ENOMEM;
	}

	/* Open file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		*line_number = 0;
		free(line);
		return -EIO;
	}

	/* Read file */
	for (line_id = 1, rule_id = 0; rule_id < *n_rules; line_id++) {
		char *tokens[CMD_MAX_TOKENS];
		uint32_t n_tokens, n_tokens_parsed, t0;

		/* Read next line from file. */
		if (fgets(line, line_len_max + 1, f) == NULL)
			break;

		/* Comment. */
		if (is_comment(line))
			continue;

		/* Parse line. */
		n_tokens = RTE_DIM(tokens);
		status = softnic_parse_tokenize_string(line, tokens, &n_tokens);
		if (status) {
			status = -EINVAL;
			break;
		}

		/* Empty line. */
		if (n_tokens == 0)
			continue;
		t0 = 0;

		/* Rule match. */
		n_tokens_parsed = parse_match(tokens + t0,
			n_tokens - t0,
			out,
			out_size,
			&m[rule_id]);
		if (n_tokens_parsed == 0) {
			status = -EINVAL;
			break;
		}
		t0 += n_tokens_parsed;

		/* Rule action. */
		n_tokens_parsed = parse_table_action(tokens + t0,
			n_tokens - t0,
			out,
			out_size,
			&a[rule_id]);
		if (n_tokens_parsed == 0) {
			status = -EINVAL;
			break;
		}
		t0 += n_tokens_parsed;

		/* Line completed. */
		if (t0 < n_tokens) {
			status = -EINVAL;
			break;
		}

		/* Increment rule count */
		rule_id++;
	}

	/* Close file */
	fclose(f);

	/* Memory free */
	free(line);

	*n_rules = rule_id;
	*line_number = line_id;
	return status;
}
