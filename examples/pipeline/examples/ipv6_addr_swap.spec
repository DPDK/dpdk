; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2022 Intel Corporation

; This simple example swaps the two halves of the IPv6 source address.
;
; It demonstrates how to operate with 128-bit fields (used to represent IPv6 addresses) by using
; 64-bit operations.
;
; Notes:
;
; 1. The 128-bit fields are always stored in network byte order (NBO), even when these fields are
;    meta-data fields, so the instructions like "mov" or "movh" that accept a 128-bit operand and
;    a 64-bit (or lesser size) operand require the latter to be also stored in NBO, i.e. it must
;    be a header field. Hence, the 64-bit upper_half and lower_half fields below are stored in a
;    header, and not in meta-data.
;
; 2. To write a IPv6 address, the lower half (bits 63-0) must be written first using the mov
;    instruction, as it also fills the upper half (bits 127-64) with zeros, and then the upper
;    half must me written using the movh instruction, as the movh instruction does not change the
;    lower half.

//
// Headers.
//
struct ethernet_h {
	bit<48> dst_addr
	bit<48> src_addr
	bit<16> ethertype
}

struct ipv6_h {
	bit<32> version_traffic_class_flow_label
	bit<16> payload_length
	bit<8> next_header
	bit<8> hop_limit
	bit<128> src_addr
	bit<128> dst_addr
}

struct sandbox_h {
	bit<64> upper_half
	bit<64> lower_half
}

header ethernet instanceof ethernet_h
header ipv6 instanceof ipv6_h
header tmp instanceof sandbox_h

//
// Meta-data.
//
struct metadata_t {
	bit<32> port
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
	extract h.ipv6

	//
	// Swap the two halves of the IPv6 source address.
	//
	movh h.tmp.upper_half h.ipv6.src_addr
	mov h.tmp.lower_half h.ipv6.src_addr
	mov h.ipv6.src_addr h.tmp.upper_half
	movh h.ipv6.src_addr h.tmp.lower_half

	//
	// De-parse and TX.
	//
	emit h.ethernet
	emit h.ipv6
	tx m.port
}
