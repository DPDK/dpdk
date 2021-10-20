# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_distributor.c', 'rte_distributor_single.c')
if arch_subdir == 'x86'
    sources += files('rte_distributor_match_sse.c')
else
    sources += files('rte_distributor_match_generic.c')
endif
headers = files('rte_distributor.h')
deps += ['mbuf']
