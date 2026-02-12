# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2024 Marvell.

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

deps += ['common_nitrox', 'bus_pci', 'compressdev']

cflags += no_shadow_cflag

sources += files(
        'nitrox_comp.c',
        'nitrox_comp_reqmgr.c',
)
