# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sources = files('bpf.c',
		'bpf_exec.c',
		'bpf_load.c',
		'bpf_pkt.c',
		'bpf_validate.c')

if arch_subdir == 'x86' and dpdk_conf.get('RTE_ARCH_64')
	sources += files('bpf_jit_x86.c')
elif dpdk_conf.has('RTE_ARCH_ARM64')
	sources += files('bpf_jit_arm64.c')
endif

headers = files('bpf_def.h',
		'rte_bpf.h',
		'rte_bpf_ethdev.h')

deps += ['mbuf', 'net', 'ethdev']

dep = dependency('libelf', required: false, method: 'pkg-config')
if dep.found()
	dpdk_conf.set('RTE_LIBRTE_BPF_ELF', 1)
	sources += files('bpf_load_elf.c')
	ext_deps += dep
endif
