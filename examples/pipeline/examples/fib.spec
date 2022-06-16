; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2021 Intel Corporation

; This example illustrates a FIB [1] with VRF [2] and ECMP [3] support. A FIB essentially is the
; data plane copy of the routing table. The VRF support allows for multiple logical routing tables
; to co-exist as part of the same "physical" routing table; the VRF ID typically identifies the
; logical table to provide the matching route for the IP destination address of the input packet.
; The ECMP provides a load balancing mechanism for the packet forwarding by allowing for multiple
; next hops (of equal or different weights, in case of WCMP [4]) to be provided for each route.
;
; In this example, the VRF ID is read from the IP source address of the input packet as opposed to a
; more complex classification scheme being used. The routing table produces the ID of the group of
; next hops associated with the current route, out of which a single next hop is selected based on a
; hashing scheme that preserves the packet order within each flow (with the flow defined here by a
; typical 3-tuple) by always selecting the same next hop for packets that are part of the same flow.
; The next hop provides the Ethernet header and the output port for the outgoing packet.
;
; [1] Forwarding Information Base (FIB):
;        https://en.wikipedia.org/wiki/Forwarding_information_base
; [2] Virtual Routing and Forwarding (VRF):
;        https://en.wikipedia.org/wiki/Virtual_routing_and_forwarding
; [3] Equal-Cost Multi-Path (ECMP) routing:
;        https://en.wikipedia.org/wiki/Equal-cost_multi-path_routing
; [4] Weighted-Cost Multi-Path (WCMP) routing.

//
// Headers
//
struct ethernet_h {
	bit<48> dst_addr
	bit<48> src_addr
	bit<16> ethertype
}

struct ipv4_h {
	bit<8> ver_ihl
	bit<8> diffserv
	bit<16> total_len
	bit<16> identification
	bit<16> flags_offset
	bit<8> ttl
	bit<8> protocol
	bit<16> hdr_checksum
	bit<32> src_addr
	bit<32> dst_addr
}

header ethernet instanceof ethernet_h
header ipv4 instanceof ipv4_h

//
// Meta-data
//
struct metadata_t {
	bit<32> port_in
	bit<32> port_out
	bit<32> vrf_id
	bit<32> dst_addr
	bit<32> nexthop_group_id
	bit<32> nexthop_id
}

metadata instanceof metadata_t

//
// Actions
//
struct nexthop_group_action_args_t {
	bit<32> nexthop_group_id
}

action nexthop_group_action args instanceof nexthop_group_action_args_t {
	mov m.nexthop_group_id t.nexthop_group_id
	return
}

struct nexthop_action_args_t {
	bit<48> ethernet_dst_addr
	bit<48> ethernet_src_addr
	bit<16> ethernet_ethertype
	bit<32> port_out
}

action nexthop_action args instanceof nexthop_action_args_t {
	//Set Ethernet header.
	validate h.ethernet
	mov h.ethernet.dst_addr t.ethernet_dst_addr
	mov h.ethernet.src_addr t.ethernet_src_addr
	mov h.ethernet.ethertype t.ethernet_ethertype

	//Decrement the TTL and update the checksum within the IPv4 header.
	cksub h.ipv4.hdr_checksum h.ipv4.ttl
	sub h.ipv4.ttl 0x1
	ckadd h.ipv4.hdr_checksum h.ipv4.ttl

	//Set the output port.
	mov m.port_out t.port_out

	return
}

action drop args none {
	drop
}

//
// Tables
//
table routing_table {
	key {
		m.vrf_id exact
		m.dst_addr lpm
	}

	actions {
		nexthop_group_action
		drop
	}

	default_action drop args none

	size 1048576
}

selector nexthop_group_table {
	group_id m.nexthop_group_id

	selector {
		h.ipv4.protocol
		h.ipv4.src_addr
		h.ipv4.dst_addr
	}

	member_id m.nexthop_id

	n_groups_max 65536

	n_members_per_group_max 64
}

table nexthop_table {
	key {
		m.nexthop_id exact
	}

	actions {
		nexthop_action
		drop
	}

	default_action drop args none

	size 1048576
}

//
// Pipeline
//
apply {
	rx m.port_in
	extract h.ethernet
	extract h.ipv4
	mov m.vrf_id h.ipv4.src_addr
	mov m.dst_addr h.ipv4.dst_addr
	table routing_table
	table nexthop_group_table
	table nexthop_table
	emit h.ethernet
	emit h.ipv4
	tx m.port_out
}
