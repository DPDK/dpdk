# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 HiSilicon Limited

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

if (arch_subdir != 'x86' and arch_subdir != 'arm') or (not dpdk_conf.get('RTE_ARCH_64'))
    build = false
    reason = 'only supported on x86_64 and aarch64'
    subdir_done()
endif

deps += ['bus_pci', 'dmadev']
sources = files(
        'hisi_dmadev.c',
)
