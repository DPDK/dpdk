# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

cflags += ['-Wno-strict-aliasing']

allow_experimental_apis = true

subdir('base')
objs = [base_objs]

sources = files(
	'avf_ethdev.c',
	'avf_rxtx.c',
	'avf_vchnl.c',
)

if arch_subdir == 'x86'
	dpdk_conf.set('RTE_LIBRTE_AVF_INC_VECTOR', 1)
	sources += files('avf_rxtx_vec_sse.c')
endif
