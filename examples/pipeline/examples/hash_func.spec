; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2022 Intel Corporation

; This simple example illustrates how to compute a hash signature over an n-tuple set of fields read
; from the packet headers and/or the packet meta-data by using the "hash" instruction. In this
; specific example, the n-tuple is the classical DiffServ 5-tuple.

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

struct udp_h {
	bit<16> src_port
	bit<16> dst_port
	bit<16> length
	bit<16> checksum
}

header ethernet instanceof ethernet_h
header ipv4 instanceof ipv4_h
header udp instanceof udp_h

//
// Meta-data.
//
struct metadata_t {
	bit<32> port
	bit<32> src_addr
	bit<32> dst_addr
	bit<8> protocol
	bit<16> src_port
	bit<16> dst_port
	bit<32> hash
}

metadata instanceof metadata_t

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
	extract h.udp

	//
	// Prepare the n-tuple to be hashed in meta-data.
	//
	// This is required when:
	//    a) The n-tuple fields are part of different headers;
	//    b) Some n-tuple fields come from headers and some from meta-data.
	//
	mov m.src_addr h.ipv4.src_addr
	mov m.dst_addr h.ipv4.dst_addr
	mov m.protocol h.ipv4.protocol
	mov m.src_port h.udp.src_port
	mov m.dst_port h.udp.dst_port

	//
	// Compute the hash over the n-tuple.
	//
	// Details:
	//    a) Hash function: jhash. Another available option is crc32.
	//    b) Destination (i.e. hash result): m.hash;
	//    c) Source (i.e. n-tuple to be hashed): The 5-tuple formed by the meta-data fields
	//       (m.src_addr, m.dst_addr, m.protocol, m.src_port, m.dst_port). Only the first and
	//       the last n-tuple fields are specified in the hash instruction, but all the fields
	//       in between are part of the n-tuple to be hashed.
	//
	hash jhash m.hash m.src_addr m.dst_port

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
	emit h.udp
	tx m.port
}
