# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 HiSilicon Limited.

if is_ms_compiler
    build = false
    reason = 'not supported building with Visual Studio Toolset'
    subdir_done()
endif

sources = files('rte_argparse.c')
headers = files('rte_argparse.h')

deps += ['log']
