# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Huawei Technologies Co., Ltd

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

if (arch_subdir != 'x86' and arch_subdir != 'arm'
        or not dpdk_conf.get('RTE_ARCH_64'))
    build = false
    reason = 'only supported on x86_64 and aarch64'
    subdir_done()
endif

cflags += ['-DHW_CONVERT_ENDIAN']

subdir('base')

sources = files(
        'hinic3_ethdev.c',
        'hinic3_nic_io.c',
        'hinic3_rx.c',
        'hinic3_tx.c',
)

includes += include_directories('base')