# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.

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

deps += ['bus_uacce', 'dmadev']
sources = files(
        'hisi_acc_dmadev.c',
)

require_iova_in_mbuf = false
