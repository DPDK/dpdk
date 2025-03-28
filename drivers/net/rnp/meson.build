# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2023 Mucse IC Design Ltd.
#
if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

if arch_subdir == 'riscv'
    build = false
    reason = 'not supported on RISC-V'
    subdir_done()
endif

subdir('base')
objs = [base_objs]

includes += include_directories('base')

sources = files(
        'rnp_ethdev.c',
)
