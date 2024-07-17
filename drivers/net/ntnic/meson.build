# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020-2023 Napatech A/S

if not is_linux or not dpdk_conf.has('RTE_ARCH_X86_64')
        build = false
        reason = 'only supported on x86_64 Linux'
        subdir_done()
endif

# includes
includes = [
        include_directories('.'),
        include_directories('include'),
        include_directories('ntlog'),
        include_directories('ntutil'),
        include_directories('nthw/core/include'),
        include_directories('nthw'),
        include_directories('nthw/supported'),
]

# all sources
sources = files(
        'adapter/nt4ga_adapter.c',
        'nthw/supported/nthw_fpga_9563_055_039_0000.c',
        'nthw/supported/nthw_fpga_instances.c',
        'nthw/supported/nthw_fpga_mod_str_map.c',
        'nthw/nthw_platform.c',
        'ntlog/ntlog.c',
        'ntutil/nt_util.c',
        'ntnic_mod_reg.c',
        'ntnic_vfio.c',
        'ntnic_ethdev.c',
)
