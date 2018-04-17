/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>

#include <rte_string_fns.h>

#include "tmgr.h"

static struct rte_sched_subport_params
	subport_profile[TMGR_SUBPORT_PROFILE_MAX];

static uint32_t n_subport_profiles;

static struct rte_sched_pipe_params
	pipe_profile[TMGR_PIPE_PROFILE_MAX];

static uint32_t n_pipe_profiles;

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
tmgr_subport_profile_add(struct rte_sched_subport_params *p)
{
	/* Check input params */
	if (p == NULL)
		return -1;

	/* Save profile */
	memcpy(&subport_profile[n_subport_profiles],
		p,
		sizeof(*p));

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
	p.n_pipes_per_subport = params->n_pipes_per_subport;

	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++)
		p.qsize[i] = params->qsize[i];

	p.pipe_profiles = pipe_profile;
	p.n_pipe_profiles = n_pipe_profiles;

	s = rte_sched_port_config(&p);
	if (s == NULL)
		return NULL;

	for (i = 0; i < params->n_subports_per_port; i++) {
		int status;

		status = rte_sched_subport_config(
			s,
			i,
			&subport_profile[0]);

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
		&subport_profile[subport_profile_id]);

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
