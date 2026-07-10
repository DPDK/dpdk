# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 Advanced Micro Devices, Inc. All rights reserved.

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

build = dpdk_conf.has('RTE_ARCH_X86')
reason = 'only supported on x86'
sources = files('ae4dma_dmadev.c')
deps += ['bus_pci', 'dmadev']
