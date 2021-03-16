; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2021 Intel Corporation

; This program is setting up an array of Two Rate Three Color Marker (trTCM) meters called "meters".
; Every input packet (Ethernet/IPv4) is metered by the meter at the location indexed by the IPv4
; header "Source Address" field. All green packets are sent out on port 0, the yellow ones on port 1
; and the red ones on port 2.
;
; The "meter stats" CLI command can be used to read the packet and byte statistics counters of any
; meter in the array.

//
// Headers.
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
	bit<32> port_in
	bit<32> port_out
}

metadata instanceof metadata_t

//
// Registers.
//
metarray meters size 65536

//
// Pipeline.
//
apply {
	rx m.port_in
	extract h.ethernet
	extract h.ipv4
	meter meters h.ipv4.src_addr h.ipv4.total_len 0 m.port_out
	emit h.ethernet
	emit h.ipv4
	tx m.port_out
}
