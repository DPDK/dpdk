# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

# Custom EAL processing. EAL is complicated enough that it can't just
# have a straight list of headers and source files.
# Initially pull in common settings
eal_inc = [global_inc]
subdir('common') # defines common_sources, common_objs, etc.

# Now do OS/exec-env specific settings, including building kernel modules
# The <exec-env>/eal/meson.build file should define env_sources, etc.
dpdk_conf.set('RTE_EXEC_ENV_' + exec_env.to_upper(), 1)
subdir(exec_env + '/eal')

version = 10  # the version of the EAL API
allow_experimental_apis = true
deps += 'kvargs'
if dpdk_conf.has('RTE_USE_LIBBSD')
	ext_deps += libbsd
endif
sources = common_sources + env_sources
objs = common_objs + env_objs
headers = common_headers + env_headers
includes = eal_inc
