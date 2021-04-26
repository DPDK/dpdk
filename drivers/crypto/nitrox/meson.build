# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

deps += ['bus_pci']
sources = files(
        'nitrox_device.c',
        'nitrox_hal.c',
        'nitrox_logs.c',
        'nitrox_sym.c',
        'nitrox_sym_capabilities.c',
        'nitrox_sym_reqmgr.c',
        'nitrox_qp.c',
)
