; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2020 Intel Corporation

; This simple example illustrates how to work with variable size headers. The assumed input packet
; is Ethernet/IPv4/UDP, with the IPv4 header containing between 0 and 40 bytes of options. To locate
; the start of the UDP header, the size of the IPv4 header needs to be detected first, which is done
; by reading the first byte of the IPv4 header that carries the 4-bit Internet Header Length (IHL)
; field; this read is done with the "lookahead" instruction, which does not advance the extract
; pointer within the input packet buffer. Once the size of the IPv4 header options is known for the
; current packet, the IPv4 header is extracted by using the two-argument "extract" instruction. Then
; the UDP header is extracted and modified.

//
// Headers
//
struct ethernet_h {
	bit<48> dst_addr
	bit<48> src_addr
	bit<16> ethertype
}

struct ipv4_top_h {
	bit<8> ver_ihl
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
	varbit<320> options
}

struct udp_h {
	bit<16> src_port
	bit<16> dst_port
	bit<16> length
	bit<16> checksum
}

header ethernet instanceof ethernet_h
header ipv4_top instanceof ipv4_top_h
header ipv4 instanceof ipv4_h
header udp instanceof udp_h

//
// Meta-data
//
struct metadata_t {
	bit<32> port
	bit<32> options_size
}

metadata instanceof metadata_t

//
// Pipeline.
//
apply {
	rx m.port

	// Extract the fixed size Ethernet header.
	extract h.ethernet

	// Extract the variable size IPv4 header with up to 10 options.
	lookahead h.ipv4_top
	mov m.options_size h.ipv4_top.ver_ihl
	and m.options_size 0xF
	sub m.options_size 5
	shl m.options_size 2
	extract h.ipv4 m.options_size

	// Extract the fixed size UDP header.
	extract h.udp

	// Modify the UDP header.
	mov h.udp.src_port 0xAABB
	mov h.udp.dst_port 0xCCDD

	// Decide the output port.
	xor m.port 1

	// Emit the Ethernet, IPv4 and UDP headers.
	emit h.ethernet
	emit h.ipv4
	emit h.udp

	tx m.port
}
