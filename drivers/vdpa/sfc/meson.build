# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020-2021 Xilinx, Inc.

if ((arch_subdir != 'x86' or not dpdk_conf.get('RTE_ARCH_64')) and
        (arch_subdir != 'arm' or not
            host_machine.cpu_family().startswith('aarch64')))
    build = false
    reason = 'only supported on x86_64 and aarch64'
endif

extra_flags = []

foreach flag: extra_flags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach

deps += ['common_sfc_efx', 'bus_pci']
sources = files(
        'sfc_vdpa.c',
        'sfc_vdpa_hw.c',
        'sfc_vdpa_mcdi.c',
        'sfc_vdpa_ops.c',
        'sfc_vdpa_filter.c',
)
