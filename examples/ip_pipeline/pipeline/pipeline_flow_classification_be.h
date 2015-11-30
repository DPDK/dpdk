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

#ifndef __INCLUDE_PIPELINE_FLOW_CLASSIFICATION_BE_H__
#define __INCLUDE_PIPELINE_FLOW_CLASSIFICATION_BE_H__

#include "pipeline_common_be.h"

enum pipeline_fc_msg_req_type {
	PIPELINE_FC_MSG_REQ_FLOW_ADD = 0,
	PIPELINE_FC_MSG_REQ_FLOW_ADD_BULK,
	PIPELINE_FC_MSG_REQ_FLOW_DEL,
	PIPELINE_FC_MSG_REQ_FLOW_ADD_DEFAULT,
	PIPELINE_FC_MSG_REQ_FLOW_DEL_DEFAULT,
	PIPELINE_FC_MSG_REQS,
};

#ifndef PIPELINE_FC_FLOW_KEY_MAX_SIZE
#define PIPELINE_FC_FLOW_KEY_MAX_SIZE            64
#endif

/*
 * MSG ADD
 */
struct pipeline_fc_add_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_fc_msg_req_type subtype;

	uint8_t key[PIPELINE_FC_FLOW_KEY_MAX_SIZE];

	uint32_t port_id;
	uint32_t flow_id;
};

struct pipeline_fc_add_msg_rsp {
	int status;
	int key_found;
	void *entry_ptr;
};

/*
 * MSG ADD BULK
 */
struct pipeline_fc_add_bulk_flow_req {
	uint8_t key[PIPELINE_FC_FLOW_KEY_MAX_SIZE];
	uint32_t port_id;
	uint32_t flow_id;
};

struct pipeline_fc_add_bulk_flow_rsp {
	int key_found;
	void *entry_ptr;
};

struct pipeline_fc_add_bulk_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_fc_msg_req_type subtype;

	struct pipeline_fc_add_bulk_flow_req *req;
	struct pipeline_fc_add_bulk_flow_rsp *rsp;
	uint32_t n_keys;
};

struct pipeline_fc_add_bulk_msg_rsp {
	uint32_t n_keys;
};

/*
 * MSG DEL
 */
struct pipeline_fc_del_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_fc_msg_req_type subtype;

	uint8_t key[PIPELINE_FC_FLOW_KEY_MAX_SIZE];
};

struct pipeline_fc_del_msg_rsp {
	int status;
	int key_found;
};

/*
 * MSG ADD DEFAULT
 */
struct pipeline_fc_add_default_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_fc_msg_req_type subtype;

	uint32_t port_id;
};

struct pipeline_fc_add_default_msg_rsp {
	int status;
	void *entry_ptr;
};

/*
 * MSG DEL DEFAULT
 */
struct pipeline_fc_del_default_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_fc_msg_req_type subtype;
};

struct pipeline_fc_del_default_msg_rsp {
	int status;
};

extern struct pipeline_be_ops pipeline_flow_classification_be_ops;

#endif
