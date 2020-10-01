; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2020 Intel Corporation

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
// Actions.
//
action macswp args none {
	mov m.addr h.ethernet.dst_addr
	mov h.ethernet.dst_addr h.ethernet.src_addr
	mov h.ethernet.src_addr m.addr
	return
}

//
// Tables.
//
table stub {
	key {
	}

	actions {
		macswp
	}

	default_action macswp args none const
}

//
// Pipeline.
//
apply {
	rx m.port
	extract h.ethernet
	table stub
	xor m.port 1
	emit h.ethernet
	tx m.port
}
