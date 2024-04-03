# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Arm Limited

if is_ms_compiler
    build = false
    reason = 'not supported building with Visual Studio Toolset'
    subdir_done()
endif

sources = files('rte_rcu_qsbr.c')
headers = files('rte_rcu_qsbr.h')

deps += ['ring']
