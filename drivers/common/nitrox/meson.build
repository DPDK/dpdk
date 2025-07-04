# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2024 Marvell.

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

deps += ['bus_pci']

sources += files(
        'nitrox_device.c',
        'nitrox_hal.c',
        'nitrox_logs.c',
        'nitrox_qp.c',
)
