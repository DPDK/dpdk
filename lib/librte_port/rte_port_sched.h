/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#ifndef __INCLUDE_RTE_PORT_SCHED_H__
#define __INCLUDE_RTE_PORT_SCHED_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port Hierarchical Scheduler
 *
 * sched_reader: input port built on top of pre-initialized rte_sched_port
 * sched_writer: output port built on top of pre-initialized rte_sched_port
 *
 ***/

#include <stdint.h>

#include <rte_sched.h>

#include "rte_port.h"

/** sched_reader port parameters */
struct rte_port_sched_reader_params {
	/** Underlying pre-initialized rte_sched_port */
	struct rte_sched_port *sched;
};

/** sched_reader port operations */
extern struct rte_port_in_ops rte_port_sched_reader_ops;

/** sched_writer port parameters */
struct rte_port_sched_writer_params {
	/** Underlying pre-initialized rte_sched_port */
	struct rte_sched_port *sched;

	/** Recommended burst size. The actual burst size can be bigger or
	smaller than this value. */
	uint32_t tx_burst_sz;
};

/** sched_writer port operations */
extern struct rte_port_out_ops rte_port_sched_writer_ops;

#ifdef __cplusplus
}
#endif

#endif
