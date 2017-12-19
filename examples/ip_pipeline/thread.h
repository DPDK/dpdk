/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
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
