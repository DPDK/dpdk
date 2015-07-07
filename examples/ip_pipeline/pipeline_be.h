/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
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

#ifndef __INCLUDE_PIPELINE_BE_H__
#define __INCLUDE_PIPELINE_BE_H__

#include <rte_port_ethdev.h>
#include <rte_port_ring.h>
#include <rte_port_frag.h>
#include <rte_port_ras.h>
#include <rte_port_sched.h>
#include <rte_port_source_sink.h>
#include <rte_pipeline.h>

enum pipeline_port_in_type {
	PIPELINE_PORT_IN_ETHDEV_READER,
	PIPELINE_PORT_IN_RING_READER,
	PIPELINE_PORT_IN_RING_READER_IPV4_FRAG,
	PIPELINE_PORT_IN_RING_READER_IPV6_FRAG,
	PIPELINE_PORT_IN_SCHED_READER,
	PIPELINE_PORT_IN_SOURCE,
};

struct pipeline_port_in_params {
	enum pipeline_port_in_type type;
	union {
		struct rte_port_ethdev_reader_params ethdev;
		struct rte_port_ring_reader_params ring;
		struct rte_port_ring_reader_ipv4_frag_params ring_ipv4_frag;
		struct rte_port_ring_reader_ipv6_frag_params ring_ipv6_frag;
		struct rte_port_sched_reader_params sched;
		struct rte_port_source_params source;
	} params;
	uint32_t burst_size;
};

static inline void *
pipeline_port_in_params_convert(struct pipeline_port_in_params  *p)
{
	switch (p->type) {
	case PIPELINE_PORT_IN_ETHDEV_READER:
		return (void *) &p->params.ethdev;
	case PIPELINE_PORT_IN_RING_READER:
		return (void *) &p->params.ring;
	case PIPELINE_PORT_IN_RING_READER_IPV4_FRAG:
		return (void *) &p->params.ring_ipv4_frag;
	case PIPELINE_PORT_IN_RING_READER_IPV6_FRAG:
		return (void *) &p->params.ring_ipv6_frag;
	case PIPELINE_PORT_IN_SCHED_READER:
		return (void *) &p->params.sched;
	case PIPELINE_PORT_IN_SOURCE:
		return (void *) &p->params.source;
	default:
		return NULL;
	}
}

static inline struct rte_port_in_ops *
pipeline_port_in_params_get_ops(struct pipeline_port_in_params  *p)
{
	switch (p->type) {
	case PIPELINE_PORT_IN_ETHDEV_READER:
		return &rte_port_ethdev_reader_ops;
	case PIPELINE_PORT_IN_RING_READER:
		return &rte_port_ring_reader_ops;
	case PIPELINE_PORT_IN_RING_READER_IPV4_FRAG:
		return &rte_port_ring_reader_ipv4_frag_ops;
	case PIPELINE_PORT_IN_RING_READER_IPV6_FRAG:
		return &rte_port_ring_reader_ipv6_frag_ops;
	case PIPELINE_PORT_IN_SCHED_READER:
		return &rte_port_sched_reader_ops;
	case PIPELINE_PORT_IN_SOURCE:
		return &rte_port_source_ops;
	default:
		return NULL;
	}
}

enum pipeline_port_out_type {
	PIPELINE_PORT_OUT_ETHDEV_WRITER,
	PIPELINE_PORT_OUT_ETHDEV_WRITER_NODROP,
	PIPELINE_PORT_OUT_RING_WRITER,
	PIPELINE_PORT_OUT_RING_WRITER_NODROP,
	PIPELINE_PORT_OUT_RING_WRITER_IPV4_RAS,
	PIPELINE_PORT_OUT_RING_WRITER_IPV6_RAS,
	PIPELINE_PORT_OUT_SCHED_WRITER,
	PIPELINE_PORT_OUT_SINK,
};

struct pipeline_port_out_params {
	enum pipeline_port_out_type type;
	union {
		struct rte_port_ethdev_writer_params ethdev;
		struct rte_port_ethdev_writer_nodrop_params ethdev_nodrop;
		struct rte_port_ring_writer_params ring;
		struct rte_port_ring_writer_nodrop_params ring_nodrop;
		struct rte_port_ring_writer_ipv4_ras_params ring_ipv4_ras;
		struct rte_port_ring_writer_ipv6_ras_params ring_ipv6_ras;
		struct rte_port_sched_writer_params sched;
	} params;
};

static inline void *
pipeline_port_out_params_convert(struct pipeline_port_out_params  *p)
{
	switch (p->type) {
	case PIPELINE_PORT_OUT_ETHDEV_WRITER:
		return (void *) &p->params.ethdev;
	case PIPELINE_PORT_OUT_ETHDEV_WRITER_NODROP:
		return (void *) &p->params.ethdev_nodrop;
	case PIPELINE_PORT_OUT_RING_WRITER:
		return (void *) &p->params.ring;
	case PIPELINE_PORT_OUT_RING_WRITER_NODROP:
		return (void *) &p->params.ring_nodrop;
	case PIPELINE_PORT_OUT_RING_WRITER_IPV4_RAS:
		return (void *) &p->params.ring_ipv4_ras;
	case PIPELINE_PORT_OUT_RING_WRITER_IPV6_RAS:
		return (void *) &p->params.ring_ipv6_ras;
	case PIPELINE_PORT_OUT_SCHED_WRITER:
		return (void *) &p->params.sched;
	case PIPELINE_PORT_OUT_SINK:
	default:
		return NULL;
	}
}

static inline void *
pipeline_port_out_params_get_ops(struct pipeline_port_out_params  *p)
{
	switch (p->type) {
	case PIPELINE_PORT_OUT_ETHDEV_WRITER:
		return &rte_port_ethdev_writer_ops;
	case PIPELINE_PORT_OUT_ETHDEV_WRITER_NODROP:
		return &rte_port_ethdev_writer_nodrop_ops;
	case PIPELINE_PORT_OUT_RING_WRITER:
		return &rte_port_ring_writer_ops;
	case PIPELINE_PORT_OUT_RING_WRITER_NODROP:
		return &rte_port_ring_writer_nodrop_ops;
	case PIPELINE_PORT_OUT_RING_WRITER_IPV4_RAS:
		return &rte_port_ring_writer_ipv4_ras_ops;
	case PIPELINE_PORT_OUT_RING_WRITER_IPV6_RAS:
		return &rte_port_ring_writer_ipv6_ras_ops;
	case PIPELINE_PORT_OUT_SCHED_WRITER:
		return &rte_port_sched_writer_ops;
	case PIPELINE_PORT_OUT_SINK:
		return &rte_port_sink_ops;
	default:
		return NULL;
	}
}

#ifndef PIPELINE_NAME_SIZE
#define PIPELINE_NAME_SIZE                       32
#endif

#ifndef PIPELINE_MAX_PORT_IN
#define PIPELINE_MAX_PORT_IN                     16
#endif

#ifndef PIPELINE_MAX_PORT_OUT
#define PIPELINE_MAX_PORT_OUT                    16
#endif

#ifndef PIPELINE_MAX_TABLES
#define PIPELINE_MAX_TABLES                      16
#endif

#ifndef PIPELINE_MAX_MSGQ_IN
#define PIPELINE_MAX_MSGQ_IN                     16
#endif

#ifndef PIPELINE_MAX_MSGQ_OUT
#define PIPELINE_MAX_MSGQ_OUT                    16
#endif

#ifndef PIPELINE_MAX_ARGS
#define PIPELINE_MAX_ARGS                        32
#endif

struct pipeline_params {
	char name[PIPELINE_NAME_SIZE];

	struct pipeline_port_in_params port_in[PIPELINE_MAX_PORT_IN];
	struct pipeline_port_out_params port_out[PIPELINE_MAX_PORT_OUT];
	struct rte_ring *msgq_in[PIPELINE_MAX_MSGQ_IN];
	struct rte_ring *msgq_out[PIPELINE_MAX_MSGQ_OUT];

	uint32_t n_ports_in;
	uint32_t n_ports_out;
	uint32_t n_msgq;

	int socket_id;

	char *args_name[PIPELINE_MAX_ARGS];
	char *args_value[PIPELINE_MAX_ARGS];
	uint32_t n_args;

	uint32_t log_level;
};

/*
 * Pipeline type back-end operations
 */

typedef void* (*pipeline_be_op_init)(struct pipeline_params *params,
	void *arg);

typedef int (*pipeline_be_op_free)(void *pipeline);

typedef int (*pipeline_be_op_run)(void *pipeline);

typedef int (*pipeline_be_op_timer)(void *pipeline);

typedef int (*pipeline_be_op_track)(void *pipeline,
	uint32_t port_in,
	uint32_t *port_out);

struct pipeline_be_ops {
	pipeline_be_op_init f_init;
	pipeline_be_op_free f_free;
	pipeline_be_op_run f_run;
	pipeline_be_op_timer f_timer;
	pipeline_be_op_track f_track;
};

#endif
