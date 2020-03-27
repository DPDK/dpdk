# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

includes += global_inc
subdir('include')

subdir('common')

dpdk_conf.set('RTE_EXEC_ENV_' + exec_env.to_upper(), 1)
subdir(exec_env)

subdir(arch_subdir)

allow_experimental_apis = true
deps += 'kvargs'
if dpdk_conf.has('RTE_USE_LIBBSD')
	ext_deps += libbsd
endif
if cc.has_function('getentropy', prefix : '#include <unistd.h>')
	cflags += '-DRTE_LIBEAL_USE_GETENTROPY'
endif
if cc.has_header('getopt.h')
	cflags += ['-DHAVE_GETOPT_H', '-DHAVE_GETOPT', '-DHAVE_GETOPT_LONG']
endif
