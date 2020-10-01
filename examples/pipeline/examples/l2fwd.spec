; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2020 Intel Corporation

//
// Meta-data.
//
struct metadata_t {
	bit<32> port_in
	bit<32> port_out
}

metadata instanceof metadata_t

//
// Actions.
//
action NoAction args none {
	return
}

//
// Tables.
//
table stub {
	key {
	}

	actions {
		NoAction
	}

	default_action NoAction args none const
}

//
// Pipeline.
//
apply {
	rx m.port_in
	table stub
	tx m.port_in
}
