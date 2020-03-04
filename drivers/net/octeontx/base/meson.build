# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

sources = [
	'octeontx_pkovf.c',
	'octeontx_pkivf.c',
	'octeontx_bgx.c'
]

depends = ['ethdev', 'mempool_octeontx']
static_objs = []
foreach d: depends
	if not is_variable('shared_rte_' + d)
		subdir_done()
	endif
	static_objs += get_variable('static_rte_' + d)
endforeach

c_args = cflags
base_lib = static_library('octeontx_base', sources,
	c_args: c_args,
	dependencies: static_objs,
)

base_objs = base_lib.extract_all_objects()
