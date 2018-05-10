# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

allow_experimental_apis = true
sources = files('bpf.c',
		'bpf_exec.c',
		'bpf_load.c',
		'bpf_validate.c')

install_headers = files('bpf_def.h',
			'rte_bpf.h')

deps += ['mbuf', 'net']
