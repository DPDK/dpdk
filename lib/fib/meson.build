# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
# Copyright(c) 2019 Intel Corporation

sources = files('rte_fib.c', 'rte_fib6.c', 'dir24_8.c', 'trie.c')
headers = files('rte_fib.h', 'rte_fib6.h')
deps += ['rib']
deps += ['rcu']
deps += ['net']

if dpdk_conf.has('RTE_ARCH_X86_64')
    sources_avx512 += files('dir24_8_avx512.c', 'trie_avx512.c')
elif dpdk_conf.has('RTE_ARCH_RISCV')
    sources += files('dir24_8_rvv.c')
endif
