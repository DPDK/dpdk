# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
# Copyright(c) 2019 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_fib.c', 'rte_fib6.c', 'dir24_8.c', 'trie.c')
headers = files('rte_fib.h', 'rte_fib6.h')
deps += ['rib']

# compile AVX512 version if:
# we are building 64-bit binary AND binutils can generate proper code
if dpdk_conf.has('RTE_ARCH_X86_64') and binutils_ok
    # compile AVX512 version if either:
    # a. we have AVX512F supported in minimum instruction set baseline
    # b. it's not minimum instruction set, but supported by compiler
    #
    # in former case, just add avx512 C file to files list
    # in latter case, compile c file to static lib, using correct
    # compiler flags, and then have the .o file from static lib
    # linked into main lib.

    # check if all required flags already enabled (variant a).
    acl_avx512_flags = ['__AVX512F__','__AVX512DQ__']
    acl_avx512_on = true
    foreach f:acl_avx512_flags
        if cc.get_define(f, args: machine_args) == ''
            acl_avx512_on = false
        endif
    endforeach

    if acl_avx512_on == true
        cflags += ['-DCC_DIR24_8_AVX512_SUPPORT']
        sources += files('dir24_8_avx512.c')
        # TRIE AVX512 implementation uses avx512bw intrinsics along with
        # avx512f and avx512dq
        if cc.get_define('__AVX512BW__', args: machine_args) != ''
            cflags += ['-DCC_TRIE_AVX512_SUPPORT']
            sources += files('trie_avx512.c')
        endif
    elif cc.has_multi_arguments('-mavx512f', '-mavx512dq')
        dir24_8_avx512_tmp = static_library('dir24_8_avx512_tmp',
                'dir24_8_avx512.c',
                dependencies: static_rte_eal,
                c_args: cflags + ['-mavx512f', '-mavx512dq'])
        objs += dir24_8_avx512_tmp.extract_objects('dir24_8_avx512.c')
        cflags += ['-DCC_DIR24_8_AVX512_SUPPORT']
        # TRIE AVX512 implementation uses avx512bw intrinsics along with
        # avx512f and avx512dq
        if cc.has_argument('-mavx512bw')
            trie_avx512_tmp = static_library('trie_avx512_tmp',
                'trie_avx512.c',
                dependencies: static_rte_eal,
                c_args: cflags + ['-mavx512f', \
                    '-mavx512dq', '-mavx512bw'])
            objs += trie_avx512_tmp.extract_objects('trie_avx512.c')
            cflags += ['-DCC_TRIE_AVX512_SUPPORT']
        endif
    endif
endif
