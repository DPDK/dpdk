# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

# Custom EAL processing. EAL is complicated enough that it can't just
# have a straight list of headers and source files.
# Initially pull in common settings
eal_inc = [global_inc]
subdir('include')

subdir('common')

# Now do OS/exec-env specific settings, including building kernel modules
# The <exec-env>/eal/meson.build file should define env_sources, etc.
dpdk_conf.set('RTE_EXEC_ENV_' + exec_env.to_upper(), 1)
subdir(exec_env + '/eal')

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
sources += env_sources
objs = env_objs
headers += env_headers
includes += eal_inc
