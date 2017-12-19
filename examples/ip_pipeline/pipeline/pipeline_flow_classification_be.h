/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
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
