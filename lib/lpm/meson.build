# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = files('rte_lpm.c', 'rte_lpm6.c')
headers = files('rte_lpm.h', 'rte_lpm6.h')
# since header files have different names, we can install all vector headers
# without worrying about which architecture we actually need
indirect_headers += files(
        'rte_lpm_altivec.h',
        'rte_lpm_neon.h',
        'rte_lpm_scalar.h',
        'rte_lpm_sse.h',
        'rte_lpm_sve.h',
)
deps += ['hash']
deps += ['rcu']
