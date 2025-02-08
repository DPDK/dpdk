# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 ZTE Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

deps += ['bus_pci', 'mbuf']
sources += files(
        'zsda_device.c',
)
