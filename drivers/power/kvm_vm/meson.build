# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Advanced Micro Devices, Inc.

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif
sources = files(
        'guest_channel.c',
        'kvm_vm.c',
)
headers = files('rte_power_guest_channel.h')

deps += ['power']
