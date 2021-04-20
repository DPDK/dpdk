# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

includes += global_inc
subdir('include')

subdir('common')

if not is_windows
    subdir('unix')
endif

dpdk_conf.set('RTE_EXEC_ENV_' + exec_env.to_upper(), 1)
subdir(exec_env)

subdir(arch_subdir)

deps += ['kvargs']
if not is_windows
    deps += ['telemetry']
endif
if dpdk_conf.has('RTE_USE_LIBBSD')
    ext_deps += libbsd
endif
if cc.has_function('getentropy', prefix : '#include <unistd.h>')
    cflags += '-DRTE_LIBEAL_USE_GETENTROPY'
endif
