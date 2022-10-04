# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Loongson Technology Corporation Limited

if not dpdk_conf.get('RTE_ARCH_64')
    error('Only 64-bit compiles are supported for this platform type')
endif
dpdk_conf.set('RTE_ARCH', 'loongarch')
dpdk_conf.set('RTE_ARCH_LOONGARCH', 1)
dpdk_conf.set('RTE_FORCE_INTRINSICS', 1)

machine_args_generic = [
    ['default', ['-march=loongarch64']],
]

flags_generic = [
    ['RTE_MACHINE', '"loongarch64"'],
    ['RTE_MAX_LCORE', 64],
    ['RTE_MAX_NUMA_NODES', 16],
    ['RTE_CACHE_LINE_SIZE', 64]]

impl_generic = ['Generic loongarch', flags_generic, machine_args_generic]

machine = []
machine_args = []

machine = impl_generic
impl_pn = 'default'

message('Implementer : ' + machine[0])
foreach flag: machine[1]
    if flag.length() > 0
        dpdk_conf.set(flag[0], flag[1])
    endif
endforeach

foreach marg: machine[2]
    if marg[0] == impl_pn
        foreach f: marg[1]
           machine_args += f
        endforeach
    endif
endforeach
message(machine_args)
