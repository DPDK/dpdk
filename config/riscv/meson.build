# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation.
# Copyright(c) 2017 Cavium, Inc
# Copyright(c) 2021 PANTHEON.tech s.r.o.
# Copyright(c) 2022 StarFive
# Copyright(c) 2022 SiFive
# Copyright(c) 2022 Semihalf

if not is_linux
    error('Only Linux is supported at this point in time.')
endif

if not dpdk_conf.get('RTE_ARCH_64')
    error('Only 64-bit compiles are supported for this platform type')
endif

dpdk_conf.set('RTE_ARCH', 'riscv')
dpdk_conf.set('RTE_ARCH_RISCV', 1)
dpdk_conf.set('RTE_FORCE_INTRINSICS', 1)

# common flags to all riscv builds, with lowest priority
flags_common = [
    ['RTE_ARCH_RISCV', true],
    ['RTE_CACHE_LINE_SIZE', 64],
    # Manually set wall time clock frequency for the target. If 0, then it is
    # read from /proc/device-tree/cpus/timebase-frequency. This property is
    # guaranteed on Linux, as riscv time_init() requires it.
    ['RTE_RISCV_TIME_FREQ', 0],
]

## SoC-specific options.
# The priority is like this: arch > vendor > common.
#
# Note that currently there's no way of getting vendor/microarchitecture id
# values in userspace which is why the logic of choosing the right flag
# combination is strictly based on the values passed from a cross-file.
vendor_generic = {
    'description': 'Generic RISC-V',
    'flags': [
        ['RTE_MACHINE', '"riscv"'],
        ['RTE_USE_C11_MEM_MODEL', true],
        ['RTE_MAX_LCORE', 128],
        ['RTE_MAX_NUMA_NODES', 2]
    ],
    'arch_config': {
        'generic': {'machine_args': ['-march=rv64gc']}
    }
}

arch_config_riscv = {
    '0x8000000000000007': {
        'machine_args':  ['-march=rv64gc', '-mtune=sifive-7-series'],
        'flags': []
    },
}

vendor_sifive = {
    'description': 'SiFive',
    'flags': [
        ['RTE_MACHINE', '"riscv"'],
        ['RTE_USE_C11_MEM_MODEL', true],
        ['RTE_MAX_LCORE', 4],
        ['RTE_MAX_NUMA_NODES', 1],
    ],
    'arch_config': arch_config_riscv
}

vendors = {
    'generic': vendor_generic,
    '0x489': vendor_sifive
}

# Native/cross vendor/arch detection
if not meson.is_cross_build()
    if machine == 'default'
        # default build
        vendor_id = 'generic'
        arch_id = 'generic'
        message('generic RISC-V')
    else
        vendor_id = 'generic'
        arch_id = 'generic'
        warning('RISC-V arch discovery not available, using generic!')
    endif
else
    # cross build
    vendor_id = meson.get_external_property('vendor_id')
    arch_id = meson.get_external_property('arch_id')
endif

if not vendors.has_key(vendor_id)
    error('Unsupported RISC-V vendor: @0@. '.format(vendor_id) +
          'Please add support for it or use the generic ' +
          '(-Dmachine=generic) build.')
endif
vendor_config = vendors[vendor_id]

message('RISC-V vendor: ' + vendor_config['description'])
message('RISC-V architecture id: ' + arch_id)

arch_config = vendor_config['arch_config']
if not arch_config.has_key(arch_id)
    # unknown micro-architecture id
    error('Unsupported architecture @0@ of vendor @1@. '
          .format(arch_id, vendor_id) +
          'Please add support for it or use the generic ' +
          '(-Dmachine=generic) build.')
endif
arch_config = arch_config[arch_id]

# Concatenate flags respecting priorities.
dpdk_flags = flags_common + vendor_config['flags'] + arch_config.get('flags', [])

if (cpu_instruction_set == 'rv64gc')
    # apply supported machine args
    machine_args = [] # Clear previous machine args
    foreach flag: arch_config['machine_args']
        if cc.has_argument(flag)
            machine_args += flag
        endif
    endforeach
endif

# check if we can do buildtime detection of extensions supported by the target
riscv_extension_macros = false
if (cc.get_define('__riscv_arch_test', args: machine_args) == '1')
    message('Detected architecture extension test macros')
    riscv_extension_macros = true
else
    warning('RISC-V architecture extension test macros not available. Build-time detection of extensions not possible')
endif

# detect extensions
# Requires intrinsics available in GCC 14.1.0+ and Clang 18.1.0+
if (riscv_extension_macros and
        (cc.get_define('__riscv_vector', args: machine_args) != ''))
    if ((cc.get_id() == 'gcc' and cc.version().version_compare('>=14.1.0'))
            or (cc.get_id() == 'clang' and cc.version().version_compare('>=18.1.0')))
        if (cc.compiles('''#include <riscv_vector.h>
                int main(void) { size_t vl = __riscv_vsetvl_e32m1(1); }''', args: machine_args))
            message('Compiling with the V extension')
            machine_args += ['-DRTE_RISCV_FEATURE_V']
        endif
    else
        warning('Detected V extension but cannot use because intrinsics are not available (present in GCC 14.1.0+ and Clang 18.1.0+)')
    endif
endif

# apply flags
foreach flag: dpdk_flags
    if flag.length() > 0
        dpdk_conf.set(flag[0], flag[1])
    endif
endforeach
message('Using machine args: @0@'.format(machine_args))
