; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2022 Intel Corporation

; This simple example illustrates how to perform packet recirculation. The "recirculate" instruction
; is used to mark the current packet for recirculation, which means that at TX time the packet is
; reinjected into the pipeline for another full pass as opposed to being sent to the output port.
;
; The same packet can be recirculated multiple times, with the recirculation pass ID retrieved by
; the "recircid" instruction. The pass ID can be used by the program to execute different code on
; the same packet in different pipeline passes. The packet meta-data is preserved between the
; pipeline passes.

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
	bit<32> pass_id
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
	// State machine based on the recirculation pass ID.
	//
	// During each of the first 5 passes through the pipeline (m.pass_id is 0 .. 4), the UDP
	// source port is incremented and the packet is marked for recirculation, while on the final
	// iteration (m.pass_id is 5) the packet is sent out.
	//
	recircid m.pass_id
	jmpgt EMIT m.pass_id 4
	add h.udp.src_port 1
	recirculate

	EMIT : emit h.ethernet
	emit h.ipv4
	emit h.udp
	tx m.port
}
