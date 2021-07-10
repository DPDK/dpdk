; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2020 Intel Corporation

; A selector table is made out of groups of weighted members, with a given member potentially part
; of several groups. The select operation returns a member ID by first selecting a group based on an
; input group ID and then selecting a member within that group by hashing one or several input
; header or meta-data fields. It is very useful for implementing an Equal-Cost Multi-Path (ECMP) or
; Weighted-Cost Multi-Path (WCMP) enabled FIB or a load balancer. It is part of the action selector
; construct described by the P4 Portable Switch Architecture (PSA) specification.
;
; Normally, an action selector FIB is built with a routing table (the base table), a selector table
; (the group table) and a next hop table (the member table). One of the routing table actions sets
; up the group ID meta-data field used as the index into the group table, which produces the member
; ID meta-data field, i.e. the next hop ID that is used as the index into the next hop table. The
; next hop action prepares the output packet for being sent next hop in the network by prepending
; one or several headers to the packet (Ethernet at the very least), decrementing the TTL and
; recomputing the IPv4 checksum, etc. The selector allows for multiple next hops to be specified
; for any given route as opposed to a single next hop per route; for every packet, its next hop is
; picked out of the set of next hops defined for the route while preserving the packet ordering
; within the flow, with the flow defined by the selector n-tuple fields.
;
; In this simple example, the base table and the member table are striped out in order to focus
; exclusively on illustrating the selector table. The group_id is read from the destination MAC
; address and the selector n-tuple is represented by the Protocol, the source IP address and the
; destination IP address fields. The member_id produced by the selector table is used to identify
; the output port which facilitates the testing of different member weights by simply comparing the
; rates of output packets sent on different ports.

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
	bit<32> group_id
}

metadata instanceof metadata_t

//
// Selectors.
//
selector s {
	group_id m.group_id

	selector {
		h.ipv4.protocol
		h.ipv4.src_addr
		h.ipv4.dst_addr
	}

	member_id m.port_out

	n_groups_max 64
	n_members_per_group_max 16
}

//
// Pipeline.
//
apply {
	rx m.port_in
	extract h.ethernet
	extract h.ipv4
	mov m.group_id h.ethernet.dst_addr
	table s
	emit h.ethernet
	emit h.ipv4
	tx m.port_out
}
