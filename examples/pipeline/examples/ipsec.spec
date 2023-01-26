; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2020 Intel Corporation

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

struct ipsec_internal_h {
	bit<32> sa_id
}

header ethernet instanceof ethernet_h
header ipv4 instanceof ipv4_h
header udp instanceof udp_h
header ipsec_internal instanceof ipsec_internal_h

//
// Meta-data
//
struct metadata_t {
	bit<32> port_in
	bit<32> port_out

	bit<32> src_addr
	bit<32> dst_addr
	bit<8> protocol
	bit<16> src_port
	bit<16> dst_port
}

metadata instanceof metadata_t

//
// Actions
//
struct encrypt_args_t {
	bit<32> sa_id
}

action encrypt args instanceof encrypt_args_t {
	//Set the IPsec internal header.
	validate h.ipsec_internal
	mov h.ipsec_internal.sa_id t.sa_id

	return
}

action drop args none {
	drop
}

//
// Tables.
//
table policy_table {
	key {
		m.src_addr exact
		m.dst_addr exact
		m.protocol exact
		m.src_port exact
		m.dst_port exact
	}

	actions {
		encrypt
		drop
	}

	default_action encrypt args sa_id 0
	size 65536
}

//
// Pipeline.
//
apply {
	rx m.port_in

	jmpeq FROM_IPSEC_TO_NET m.port_in 1

FROM_NET_TO_IPSEC : extract h.ethernet

	extract h.ipv4
	mov m.src_addr h.ipv4.src_addr
	mov m.dst_addr h.ipv4.dst_addr
	mov m.protocol h.ipv4.protocol

	extract h.udp
	mov m.src_port h.udp.src_port
	mov m.dst_port h.udp.dst_port

	table policy_table

	mov m.port_out 1

	emit h.ipsec_internal
	emit h.ipv4
	emit h.udp
	tx m.port_out

FROM_IPSEC_TO_NET : extract h.ipv4

	validate h.ethernet
	mov h.ethernet.dst_addr 0xa0b0c0d0e0f0
	mov h.ethernet.src_addr 0xa1b1c1d1e1f1
	mov h.ethernet.ethertype 0x0800

	mov m.port_out 0

	emit h.ethernet
	emit h.ipv4
	tx m.port_out
}
