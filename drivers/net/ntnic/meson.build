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
        include_directories('ntlog'),
        include_directories('ntutil'),
]

# all sources
sources = files(
        'ntlog/ntlog.c',
        'ntutil/nt_util.c',
        'ntnic_vfio.c',
        'ntnic_ethdev.c',
)
