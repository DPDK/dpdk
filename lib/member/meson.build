# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_member.c', 'rte_member_ht.c', 'rte_member_vbf.c')
headers = files('rte_member.h')
deps += ['hash']
