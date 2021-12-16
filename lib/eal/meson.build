# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

includes += global_inc
subdir('include')

subdir('common')

if not is_windows
    subdir('unix')
endif

exec_envs = {'freebsd': 0, 'linux': 1, 'windows': 2}
foreach env, id:exec_envs
    dpdk_conf.set('RTE_ENV_' + env.to_upper(), id)
    dpdk_conf.set10('RTE_EXEC_ENV_IS_' + env.to_upper(), (exec_env == env))
endforeach
dpdk_conf.set('RTE_EXEC_ENV', exec_envs[exec_env])

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
