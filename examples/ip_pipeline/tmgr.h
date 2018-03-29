/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _INCLUDE_TMGR_H_
#define _INCLUDE_TMGR_H_

#include <stdint.h>
#include <sys/queue.h>

#include <rte_sched.h>

#include "common.h"

#ifndef TMGR_SUBPORT_PROFILE_MAX
#define TMGR_SUBPORT_PROFILE_MAX                           256
#endif

#ifndef TMGR_PIPE_PROFILE_MAX
#define TMGR_PIPE_PROFILE_MAX                              256
#endif

struct tmgr_port {
	TAILQ_ENTRY(tmgr_port) node;
	char name[NAME_SIZE];
	struct rte_sched_port *s;
	uint32_t n_subports_per_port;
	uint32_t n_pipes_per_subport;
};

TAILQ_HEAD(tmgr_port_list, tmgr_port);

int
tmgr_init(void);

struct tmgr_port *
tmgr_port_find(const char *name);

struct tmgr_port_params {
	uint32_t rate;
	uint32_t n_subports_per_port;
	uint32_t n_pipes_per_subport;
	uint16_t qsize[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	uint32_t frame_overhead;
	uint32_t mtu;
	uint32_t cpu_id;
};

int
tmgr_subport_profile_add(struct rte_sched_subport_params *p);

int
tmgr_pipe_profile_add(struct rte_sched_pipe_params *p);

struct tmgr_port *
tmgr_port_create(const char *name, struct tmgr_port_params *params);

int
tmgr_subport_config(const char *port_name,
	uint32_t subport_id,
	uint32_t subport_profile_id);

int
tmgr_pipe_config(const char *port_name,
	uint32_t subport_id,
	uint32_t pipe_id_first,
	uint32_t pipe_id_last,
	uint32_t pipe_profile_id);

#endif /* _INCLUDE_TMGR_H_ */
