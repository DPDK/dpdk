/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>

#include <rte_common.h>
#include <rte_string_fns.h>

#include "tmgr.h"

static struct rte_sched_subport_profile_params
	subport_profile[TMGR_SUBPORT_PROFILE_MAX];

static uint32_t n_subport_profiles;

static struct rte_sched_pipe_params
	pipe_profile[TMGR_PIPE_PROFILE_MAX];

#ifdef RTE_SCHED_CMAN
static struct rte_sched_cman_params cman_params = {
	.red_params = {
		/* Traffic Class 0 Colors Green / Yellow / Red */
		[0][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[0][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[0][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 1 - Colors Green / Yellow / Red */
		[1][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[1][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[1][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 2 - Colors Green / Yellow / Red */
		[2][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[2][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[2][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 3 - Colors Green / Yellow / Red */
		[3][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[3][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[3][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 4 - Colors Green / Yellow / Red */
		[4][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[4][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[4][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 5 - Colors Green / Yellow / Red */
		[5][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[5][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[5][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 6 - Colors Green / Yellow / Red */
		[6][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[6][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[6][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 7 - Colors Green / Yellow / Red */
		[7][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[7][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[7][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 8 - Colors Green / Yellow / Red */
		[8][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[8][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[8][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 9 - Colors Green / Yellow / Red */
		[9][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[9][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[9][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 10 - Colors Green / Yellow / Red */
		[10][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[10][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[10][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 11 - Colors Green / Yellow / Red */
		[11][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[11][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[11][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 12 - Colors Green / Yellow / Red */
		[12][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[12][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[12][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		},
};
#endif /* RTE_SCHED_CMAN */

static uint32_t n_pipe_profiles;

static const struct rte_sched_subport_params subport_params_default = {
	.n_pipes_per_subport_enabled = 0, /* filled at runtime */
	.qsize = {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64},
	.pipe_profiles = pipe_profile,
	.n_pipe_profiles = 0, /* filled at run time */
	.n_max_pipe_profiles = RTE_DIM(pipe_profile),
#ifdef RTE_SCHED_CMAN
	.cman_params = &cman_params,
#endif /* RTE_SCHED_CMAN */
};

static struct tmgr_port_list tmgr_port_list;

int
tmgr_init(void)
{
	TAILQ_INIT(&tmgr_port_list);

	return 0;
}

struct tmgr_port *
tmgr_port_find(const char *name)
{
	struct tmgr_port *tmgr_port;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(tmgr_port, &tmgr_port_list, node)
		if (strcmp(tmgr_port->name, name) == 0)
			return tmgr_port;

	return NULL;
}

int
tmgr_subport_profile_add(struct rte_sched_subport_profile_params *params)
{
	/* Check input params */
	if (params == NULL)
		return -1;

	/* Save profile */
	memcpy(&subport_profile[n_subport_profiles],
		params,
		sizeof(*params));

	n_subport_profiles++;

	return 0;
}

int
tmgr_pipe_profile_add(struct rte_sched_pipe_params *p)
{
	/* Check input params */
	if (p == NULL)
		return -1;

	/* Save profile */
	memcpy(&pipe_profile[n_pipe_profiles],
		p,
		sizeof(*p));

	n_pipe_profiles++;

	return 0;
}

struct tmgr_port *
tmgr_port_create(const char *name, struct tmgr_port_params *params)
{
	struct rte_sched_subport_params subport_params;
	struct rte_sched_port_params p;
	struct tmgr_port *tmgr_port;
	struct rte_sched_port *s;
	uint32_t i, j;

	/* Check input params */
	if ((name == NULL) ||
		tmgr_port_find(name) ||
		(params == NULL) ||
		(params->n_subports_per_port == 0) ||
		(params->n_pipes_per_subport == 0) ||
		(params->cpu_id >= RTE_MAX_NUMA_NODES) ||
		(n_subport_profiles == 0) ||
		(n_pipe_profiles == 0))
		return NULL;

	/* Resource create */
	p.name = name;
	p.socket = (int) params->cpu_id;
	p.rate = params->rate;
	p.mtu = params->mtu;
	p.frame_overhead = params->frame_overhead;
	p.n_subports_per_port = params->n_subports_per_port;
	p.n_subport_profiles = n_subport_profiles;
	p.subport_profiles = subport_profile;
	p.n_max_subport_profiles = TMGR_SUBPORT_PROFILE_MAX;
	p.n_pipes_per_subport = params->n_pipes_per_subport;


	s = rte_sched_port_config(&p);
	if (s == NULL)
		return NULL;

	memcpy(&subport_params, &subport_params_default,
		sizeof(subport_params_default));

	subport_params.n_pipe_profiles = n_pipe_profiles;
	subport_params.n_pipes_per_subport_enabled =
						params->n_pipes_per_subport;

	for (i = 0; i < params->n_subports_per_port; i++) {
		int status;

		status = rte_sched_subport_config(
			s,
			i,
			&subport_params,
			0);

		if (status) {
			rte_sched_port_free(s);
			return NULL;
		}

		for (j = 0; j < params->n_pipes_per_subport; j++) {

			status = rte_sched_pipe_config(
				s,
				i,
				j,
				0);

			if (status) {
				rte_sched_port_free(s);
				return NULL;
			}
		}
	}

	/* Node allocation */
	tmgr_port = calloc(1, sizeof(struct tmgr_port));
	if (tmgr_port == NULL) {
		rte_sched_port_free(s);
		return NULL;
	}

	/* Node fill in */
	strlcpy(tmgr_port->name, name, sizeof(tmgr_port->name));
	tmgr_port->s = s;
	tmgr_port->n_subports_per_port = params->n_subports_per_port;
	tmgr_port->n_pipes_per_subport = params->n_pipes_per_subport;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&tmgr_port_list, tmgr_port, node);

	return tmgr_port;
}

int
tmgr_subport_config(const char *port_name,
	uint32_t subport_id,
	uint32_t subport_profile_id)
{
	struct tmgr_port *port;
	int status;

	/* Check input params */
	if (port_name == NULL)
		return -1;

	port = tmgr_port_find(port_name);
	if ((port == NULL) ||
		(subport_id >= port->n_subports_per_port) ||
		(subport_profile_id >= n_subport_profiles))
		return -1;

	/* Resource config */
	status = rte_sched_subport_config(
		port->s,
		subport_id,
		NULL,
		subport_profile_id);

	return status;
}

int
tmgr_pipe_config(const char *port_name,
	uint32_t subport_id,
	uint32_t pipe_id_first,
	uint32_t pipe_id_last,
	uint32_t pipe_profile_id)
{
	struct tmgr_port *port;
	uint32_t i;

	/* Check input params */
	if (port_name == NULL)
		return -1;

	port = tmgr_port_find(port_name);
	if ((port == NULL) ||
		(subport_id >= port->n_subports_per_port) ||
		(pipe_id_first >= port->n_pipes_per_subport) ||
		(pipe_id_last >= port->n_pipes_per_subport) ||
		(pipe_id_first > pipe_id_last) ||
		(pipe_profile_id >= n_pipe_profiles))
		return -1;

	/* Resource config */
	for (i = pipe_id_first; i <= pipe_id_last; i++) {
		int status;

		status = rte_sched_pipe_config(
			port->s,
			subport_id,
			i,
			(int) pipe_profile_id);

		if (status)
			return status;
	}

	return 0;
}
