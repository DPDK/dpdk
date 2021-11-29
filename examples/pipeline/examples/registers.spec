; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2021 Intel Corporation

; This program is setting up two register arrays called "pkt_counters" and "byte_counters".
; On every input packet (Ethernet/IPv4), the "pkt_counters" register at location indexed by
; the IPv4 header "Source Address" field is incremented, while the same location in the
; "byte_counters" array accumulates the value of the IPv4 header "Total Length" field.
;
; The "regrd" and "regwr" CLI commands can be used to read and write the current value of
; any register array location.

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
regarray pkt_counters size 65536 initval 0
regarray byte_counters size 65536 initval 0

//
// Pipeline.
//
apply {
	rx m.port_in
	extract h.ethernet
	extract h.ipv4
	regadd pkt_counters h.ipv4.src_addr 1
	regadd byte_counters h.ipv4.src_addr h.ipv4.total_len
	mov m.port_out m.port_in
	xor m.port_out 1
	emit h.ethernet
	emit h.ipv4
	tx m.port_out
}
