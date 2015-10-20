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

#ifndef __INCLUDE_PIPELINE_FIREWALL_BE_H__
#define __INCLUDE_PIPELINE_FIREWALL_BE_H__

#include "pipeline_common_be.h"

enum pipeline_firewall_key_type {
	PIPELINE_FIREWALL_IPV4_5TUPLE,
};

struct pipeline_firewall_key_ipv4_5tuple {
	uint32_t src_ip;
	uint32_t src_ip_mask;
	uint32_t dst_ip;
	uint32_t dst_ip_mask;
	uint16_t src_port_from;
	uint16_t src_port_to;
	uint16_t dst_port_from;
	uint16_t dst_port_to;
	uint8_t proto;
	uint8_t proto_mask;
};

struct pipeline_firewall_key {
	enum pipeline_firewall_key_type type;
	union {
		struct pipeline_firewall_key_ipv4_5tuple ipv4_5tuple;
	} key;
};

enum pipeline_firewall_msg_req_type {
	PIPELINE_FIREWALL_MSG_REQ_ADD = 0,
	PIPELINE_FIREWALL_MSG_REQ_DEL,
	PIPELINE_FIREWALL_MSG_REQ_ADD_BULK,
	PIPELINE_FIREWALL_MSG_REQ_DEL_BULK,
	PIPELINE_FIREWALL_MSG_REQ_ADD_DEFAULT,
	PIPELINE_FIREWALL_MSG_REQ_DEL_DEFAULT,
	PIPELINE_FIREWALL_MSG_REQS
};

/*
 * MSG ADD
 */
struct pipeline_firewall_add_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_firewall_msg_req_type subtype;

	/* key */
	struct pipeline_firewall_key key;

	/* data */
	int32_t priority;
	uint32_t port_id;
};

struct pipeline_firewall_add_msg_rsp {
	int status;
	int key_found;
	void *entry_ptr;
};

/*
 * MSG DEL
 */
struct pipeline_firewall_del_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_firewall_msg_req_type subtype;

	/* key */
	struct pipeline_firewall_key key;
};

struct pipeline_firewall_del_msg_rsp {
	int status;
	int key_found;
};

/*
 * MSG ADD BULK
 */
struct pipeline_firewall_add_bulk_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_firewall_msg_req_type subtype;

	struct pipeline_firewall_key *keys;
	uint32_t n_keys;

	uint32_t *priorities;
	uint32_t *port_ids;
	int *keys_found;
	void **entries_ptr;
};
struct pipeline_firewall_add_bulk_msg_rsp {
	int status;
};

/*
 * MSG DEL BULK
 */
struct pipeline_firewall_del_bulk_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_firewall_msg_req_type subtype;

	/* key */
	struct pipeline_firewall_key *keys;
	uint32_t n_keys;
	int *keys_found;
};

struct pipeline_firewall_del_bulk_msg_rsp {
	int status;
};

/*
 * MSG ADD DEFAULT
 */
struct pipeline_firewall_add_default_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_firewall_msg_req_type subtype;

	/* data */
	uint32_t port_id;
};

struct pipeline_firewall_add_default_msg_rsp {
	int status;
	void *entry_ptr;
};

/*
 * MSG DEL DEFAULT
 */
struct pipeline_firewall_del_default_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_firewall_msg_req_type subtype;
};

struct pipeline_firewall_del_default_msg_rsp {
	int status;
};

extern struct pipeline_be_ops pipeline_firewall_be_ops;

#endif
