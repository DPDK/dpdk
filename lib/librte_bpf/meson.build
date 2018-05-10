# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

allow_experimental_apis = true
sources = files('bpf.c',
		'bpf_exec.c',
		'bpf_load.c',
		'bpf_pkt.c',
		'bpf_validate.c')

if arch_subdir == 'x86'
	sources += files('bpf_jit_x86.c')
endif

install_headers = files('bpf_def.h',
			'rte_bpf.h',
			'rte_bpf_ethdev.h')

deps += ['mbuf', 'net', 'ethdev']

dep = cc.find_library('elf', required: false)
if dep.found() == true and cc.has_header('libelf.h', dependencies: dep)
	sources += files('bpf_load_elf.c')
	ext_deps += dep
endif
