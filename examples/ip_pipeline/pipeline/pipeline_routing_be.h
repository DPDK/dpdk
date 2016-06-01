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

#ifndef __INCLUDE_PIPELINE_ROUTING_BE_H__
#define __INCLUDE_PIPELINE_ROUTING_BE_H__

#include <rte_ether.h>

#include "pipeline_common_be.h"

/*
 * Pipeline argument parsing
 */
#ifndef PIPELINE_ROUTING_N_ROUTES_DEFAULT
#define PIPELINE_ROUTING_N_ROUTES_DEFAULT                  4096
#endif

enum pipeline_routing_encap {
	PIPELINE_ROUTING_ENCAP_ETHERNET = 0,
	PIPELINE_ROUTING_ENCAP_ETHERNET_QINQ,
	PIPELINE_ROUTING_ENCAP_ETHERNET_MPLS,
};

struct pipeline_routing_params {
	/* routing */
	uint32_t n_routes;
	uint32_t port_local_dest;

	/* routing packet encapsulation */
	enum pipeline_routing_encap encap;
	uint32_t qinq_sched;
	uint32_t mpls_color_mark;

	/* arp */
	uint32_t n_arp_entries;

	/* packet buffer offsets */
	uint32_t ip_hdr_offset;
	uint32_t arp_key_offset;
	uint32_t color_offset;

	/* debug */
	uint32_t dbg_ah_disable;
};

int
pipeline_routing_parse_args(struct pipeline_routing_params *p,
	struct pipeline_params *params);

/*
 * Route
 */
enum pipeline_routing_route_key_type {
	PIPELINE_ROUTING_ROUTE_IPV4,
};

struct pipeline_routing_route_key_ipv4 {
	uint32_t ip;
	uint32_t depth;
};

struct pipeline_routing_route_key {
	enum pipeline_routing_route_key_type type;
	union {
		struct pipeline_routing_route_key_ipv4 ipv4;
	} key;
};

enum pipeline_routing_route_flags {
	PIPELINE_ROUTING_ROUTE_LOCAL = 1 << 0, /* 0 = remote; 1 = local */
	PIPELINE_ROUTING_ROUTE_ARP = 1 << 1, /* 0 = ARP OFF; 1 = ARP ON */
	PIPELINE_ROUTING_ROUTE_QINQ = 1 << 2, /* 0 = QINQ OFF; 1 = QINQ ON */
	PIPELINE_ROUTING_ROUTE_MPLS = 1 << 3, /* 0 = MPLS OFF; 1 = MPLS ON */
};

#define PIPELINE_ROUTING_MPLS_LABELS_MAX         4

struct pipeline_routing_route_data {
	uint32_t flags;
	uint32_t port_id; /* Output port ID */

	union {
		/* Next hop IP (valid only when ARP is enabled) */
		uint32_t ip;

		/* Next hop MAC address (valid only when ARP disabled */
		struct ether_addr macaddr;
	} ethernet;

	union {
		struct {
			uint16_t svlan;
			uint16_t cvlan;
		} qinq;

		struct {
			uint32_t labels[PIPELINE_ROUTING_MPLS_LABELS_MAX];
			uint32_t n_labels;
		} mpls;
	} l2;
};

/*
 * ARP
 */
enum pipeline_routing_arp_key_type {
	PIPELINE_ROUTING_ARP_IPV4,
};

struct pipeline_routing_arp_key_ipv4 {
	uint32_t port_id;
	uint32_t ip;
};

struct pipeline_routing_arp_key {
	enum pipeline_routing_arp_key_type type;
	union {
		struct pipeline_routing_arp_key_ipv4 ipv4;
	} key;
};

/*
 * Messages
 */
enum pipeline_routing_msg_req_type {
	PIPELINE_ROUTING_MSG_REQ_ROUTE_ADD,
	PIPELINE_ROUTING_MSG_REQ_ROUTE_DEL,
	PIPELINE_ROUTING_MSG_REQ_ROUTE_ADD_DEFAULT,
	PIPELINE_ROUTING_MSG_REQ_ROUTE_DEL_DEFAULT,
	PIPELINE_ROUTING_MSG_REQ_ARP_ADD,
	PIPELINE_ROUTING_MSG_REQ_ARP_DEL,
	PIPELINE_ROUTING_MSG_REQ_ARP_ADD_DEFAULT,
	PIPELINE_ROUTING_MSG_REQ_ARP_DEL_DEFAULT,
	PIPELINE_ROUTING_MSG_REQ_SET_MACADDR,
	PIPELINE_ROUTING_MSG_REQS
};

/*
 * MSG ROUTE ADD
 */
struct pipeline_routing_route_add_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_routing_msg_req_type subtype;

	/* key */
	struct pipeline_routing_route_key key;

	/* data */
	struct pipeline_routing_route_data data;
};

struct pipeline_routing_route_add_msg_rsp {
	int status;
	int key_found;
	void *entry_ptr;
};

/*
 * MSG ROUTE DELETE
 */
struct pipeline_routing_route_delete_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_routing_msg_req_type subtype;

	/* key */
	struct pipeline_routing_route_key key;
};

struct pipeline_routing_route_delete_msg_rsp {
	int status;
	int key_found;
};

/*
 * MSG ROUTE ADD DEFAULT
 */
struct pipeline_routing_route_add_default_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_routing_msg_req_type subtype;

	/* data */
	uint32_t port_id;
};

struct pipeline_routing_route_add_default_msg_rsp {
	int status;
	void *entry_ptr;
};

/*
 * MSG ROUTE DELETE DEFAULT
 */
struct pipeline_routing_route_delete_default_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_routing_msg_req_type subtype;
};

struct pipeline_routing_route_delete_default_msg_rsp {
	int status;
};

/*
 * MSG ARP ADD
 */
struct pipeline_routing_arp_add_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_routing_msg_req_type subtype;

	/* key */
	struct pipeline_routing_arp_key key;

	/* data */
	struct ether_addr macaddr;
};

struct pipeline_routing_arp_add_msg_rsp {
	int status;
	int key_found;
	void *entry_ptr;
};

/*
 * MSG ARP DELETE
 */
struct pipeline_routing_arp_delete_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_routing_msg_req_type subtype;

	/* key */
	struct pipeline_routing_arp_key key;
};

struct pipeline_routing_arp_delete_msg_rsp {
	int status;
	int key_found;
};

/*
 * MSG ARP ADD DEFAULT
 */
struct pipeline_routing_arp_add_default_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_routing_msg_req_type subtype;

	/* data */
	uint32_t port_id;
};

struct pipeline_routing_arp_add_default_msg_rsp {
	int status;
	void *entry_ptr;
};

/*
 * MSG ARP DELETE DEFAULT
 */
struct pipeline_routing_arp_delete_default_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_routing_msg_req_type subtype;
};

struct pipeline_routing_arp_delete_default_msg_rsp {
	int status;
};

/*
 * MSG SET MACADDR
 */
struct pipeline_routing_set_macaddr_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_routing_msg_req_type subtype;

	uint64_t macaddr[PIPELINE_MAX_PORT_OUT];
};

struct pipeline_routing_set_macaddr_msg_rsp {
	int status;
};

extern struct pipeline_be_ops pipeline_routing_be_ops;

#endif
