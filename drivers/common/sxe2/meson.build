# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 Corigine, Inc.

if is_windows
        build = false
        reason = 'only supported on Linux'
        subdir_done()
endif

deps += ['bus_pci', 'net', 'eal', 'ethdev']

sources = files(
        'sxe2_common.c',
        'sxe2_ioctl_chnl.c',
)
