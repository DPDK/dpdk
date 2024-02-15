; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2020 Intel Corporation

; The simplest pipeline processing with just packet reception and transmission. No header parsing,
; table lookup or action execution involved. Packets received on port 0 are sent out on port 1,
; those received on port 1 are sent out on port 0, etc.

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
	rx m.port
	xor m.port 1
	tx m.port
}
