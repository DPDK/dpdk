; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2022 Intel Corporation

; This simple example illustrates how to perform packet mirroring. The "mirror" instruction is used
; to flag the current packet for mirroring, which means that at TX time, before the current packet
; is sent out, it will first be cloned (using either the fast or the slow/deep cloning method) and
; the clone packet sent out on the output port specified by the mirror session.
;
; In this example, the UDP packets with destination port 5000 are mirrored to the output port
; specified by the mirror session 0, while the rest of the packets are not mirrored. Therefore, for
; every UDP input packet with this specific destination port there will be two output packets (the
; current packet and its clone packet), while for every other input packet there will be a single
; output packet.

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
	bit<32> mirror_slot
	bit<32> mirror_session
}

metadata instanceof metadata_t

//
// Pipeline.
//
apply {
	rx m.port
	extract h.ethernet
	extract h.ipv4
	extract h.udp

	//
	// Mark for mirroring all packets with UDP destination port of 5000.
	//
	MIRROR_UDP_DST_PORT_5000 : jmpneq EMIT h.udp.dst_port 5000
	mov m.mirror_slot 0
	mov m.mirror_session 0
	mirror m.mirror_slot m.mirror_session

	EMIT : emit h.ethernet
	emit h.ipv4
	emit h.udp
	tx m.port
}
