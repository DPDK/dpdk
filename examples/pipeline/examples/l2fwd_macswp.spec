; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2020 Intel Corporation

; Layer 2 Forwarding with MACADDR swapping (i.e. within the Ethernet header of the output packet,
; the destination MAC address is swapped with the source MAC address).

//
// Packet headers.
//
struct ethernet_h {
	bit<48> dst_addr
	bit<48> src_addr
	bit<16> ether_type
}

header ethernet instanceof ethernet_h

//
// Packet meta-data.
//
struct metadata_t {
	bit<32> port
	bit<48> addr
}

metadata instanceof metadata_t

//
// Pipeline.
//
apply {
	rx m.port
	extract h.ethernet

	//
	// Ethernet header: dst_addr swapped with src_addr.
	//
	mov m.addr h.ethernet.dst_addr
	mov h.ethernet.dst_addr h.ethernet.src_addr
	mov h.ethernet.src_addr m.addr

	xor m.port 1
	emit h.ethernet
	tx m.port
}
