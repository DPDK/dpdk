; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2023 Intel Corporation

; This simple example illustrates how to compute an RSS hash signature over an n-tuple set of fields
; read from the packet headers and/or the packet meta-data by using the "rss" instruction. In this
; specific example, the n-tuple is the (IPv4 source address, IPv4 destination address) 2-tuple.

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
// Meta-data.
//
struct metadata_t {
	bit<32> port
	bit<32> hash
}

metadata instanceof metadata_t

//
// RSS.
//
rss rss0

//
// Pipeline.
//
apply {
	//
	// RX and parse.
	//
	rx m.port
	extract h.ethernet
	extract h.ipv4

	//
	// Compute the RSS hash over the n-tuple.
	//
	// Details:
	//    a) RSS object name: rss0;
	//    b) Destination (i.e. hash result): m.hash;
	//    c) Source (i.e. n-tuple to be hashed): The 2-tuple formed by the header fields
	//       (h.ipv4.src_addr, h.ipv4.dst_addr). Only the first and the last n-tuple fields are
	//       specified in the RSS instruction, but all the fields in between are part of the
	//       n-tuple to be hashed.
	//
	rss rss0 m.hash h.ipv4.src_addr h.ipv4.dst_addr

	//
	// Use the computed hash to create a uniform distribution of pkts across the 4 output ports.
	//
	and m.hash 3
	mov m.port m.hash

	//
	// De-parse and TX.
	//
	emit h.ethernet
	emit h.ipv4
	tx m.port
}
