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

# FIXME: this library was enabled for mingw target (a Windows target).
# Relying on no_wvla_cflag would trigger a build error until the VLA in rte_rcu_qsbr.c is removed.
# Disable the warning here for now.
if cc.has_argument('-Wvla')
    cflags += '-Wno-vla'
endif
