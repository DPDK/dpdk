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

#ifndef THREAD_H_
#define THREAD_H_

#include "app.h"
#include "pipeline_be.h"

enum thread_msg_req_type {
	THREAD_MSG_REQ_PIPELINE_ENABLE = 0,
	THREAD_MSG_REQ_PIPELINE_DISABLE,
	THREAD_MSG_REQ_HEADROOM_READ,
	THREAD_MSG_REQS
};

struct thread_msg_req {
	enum thread_msg_req_type type;
};

struct thread_msg_rsp {
	int status;
};

/*
 * PIPELINE ENABLE
 */
struct thread_pipeline_enable_msg_req {
	enum thread_msg_req_type type;

	uint32_t pipeline_id;
	void *be;
	pipeline_be_op_run f_run;
	pipeline_be_op_timer f_timer;
	uint64_t timer_period;
};

struct thread_pipeline_enable_msg_rsp {
	int status;
};

/*
 * PIPELINE DISABLE
 */
struct thread_pipeline_disable_msg_req {
	enum thread_msg_req_type type;

	uint32_t pipeline_id;
};

struct thread_pipeline_disable_msg_rsp {
	int status;
};

/*
 * THREAD HEADROOM
 */
struct thread_headroom_read_msg_req {
	enum thread_msg_req_type type;
};

struct thread_headroom_read_msg_rsp {
	int status;

	double headroom_ratio;
};

#endif /* THREAD_H_ */
