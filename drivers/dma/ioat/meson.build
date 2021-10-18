# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021 Intel Corporation

build = dpdk_conf.has('RTE_ARCH_X86')
reason = 'only supported on x86'
sources = files('ioat_dmadev.c')
deps += ['bus_pci', 'dmadev']
