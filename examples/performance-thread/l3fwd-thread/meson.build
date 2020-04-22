# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

build = dpdk_conf.has('RTE_ARCH_X86_64')
deps += ['timer', 'lpm']
allow_experimental_apis = true

# get the performance thread (pt) architecture subdir
if dpdk_conf.has('RTE_ARCH_ARM64')
	pt_arch_dir = '../common/arch/arm64'
else
	pt_arch_dir = '../common/arch/x86'
endif
sources += files('main.c',
	'../common/lthread.c',
	'../common/lthread_cond.c',
	'../common/lthread_diag.c',
	'../common/lthread_mutex.c',
	'../common/lthread_sched.c',
	'../common/lthread_tls.c',
	pt_arch_dir + '/ctx.c')

includes += include_directories('../common', pt_arch_dir)
