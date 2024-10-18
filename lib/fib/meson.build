# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
# Copyright(c) 2019 Intel Corporation

if is_ms_compiler
    build = false
    reason = 'not supported building with Visual Studio Toolset'
    subdir_done()
endif

sources = files('rte_fib.c', 'rte_fib6.c', 'dir24_8.c', 'trie.c')
headers = files('rte_fib.h', 'rte_fib6.h')
deps += ['rib']
deps += ['rcu']
deps += ['net']

if dpdk_conf.has('RTE_ARCH_X86_64')
    if target_has_avx512
        cflags += ['-DCC_DIR24_8_AVX512_SUPPORT', '-DCC_TRIE_AVX512_SUPPORT']
        sources += files('dir24_8_avx512.c', 'trie_avx512.c')

    elif cc_has_avx512
        cflags += ['-DCC_DIR24_8_AVX512_SUPPORT', '-DCC_TRIE_AVX512_SUPPORT']
        dir24_8_avx512_tmp = static_library('dir24_8_avx512_tmp',
                'dir24_8_avx512.c',
                dependencies: [static_rte_eal, static_rte_rcu],
                c_args: cflags + cc_avx512_flags)
        objs += dir24_8_avx512_tmp.extract_objects('dir24_8_avx512.c')
        trie_avx512_tmp = static_library('trie_avx512_tmp',
                'trie_avx512.c',
                dependencies: [static_rte_eal, static_rte_rcu, static_rte_net],
                c_args: cflags + cc_avx512_flags)
        objs += trie_avx512_tmp.extract_objects('trie_avx512.c')
    endif
endif
