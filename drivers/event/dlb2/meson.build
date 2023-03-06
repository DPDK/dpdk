# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019-2020 Intel Corporation

if not is_linux or not dpdk_conf.has('RTE_ARCH_X86_64')
        build = false
        reason = 'only supported on x86_64 Linux'
        subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

sources = files(
        'dlb2.c',
        'dlb2_iface.c',
        'dlb2_xstats.c',
        'pf/dlb2_main.c',
        'pf/dlb2_pf.c',
        'pf/base/dlb2_resource.c',
        'rte_pmd_dlb2.c',
        'dlb2_selftest.c',
)

# compile AVX512 version if:
# we are building 64-bit binary (checked above) AND binutils
# can generate proper code

if binutils_ok

    # compile AVX512 version if either:
    # a. we have AVX512VL supported in minimum instruction set
    #    baseline
    # b. it's not minimum instruction set, but supported by
    #    compiler
    #
    # in former case, just add avx512 C file to files list
    # in latter case, compile c file to static lib, using correct
    # compiler flags, and then have the .o file from static lib
    # linked into main lib.

    # check if all required flags already enabled (variant a).
    dlb2_avx512_on = false
    if cc.get_define('__AVX512VL__', args: machine_args) != ''
        dlb2_avx512_on = true
    endif

    if dlb2_avx512_on == true

        sources += files('dlb2_avx512.c')
        cflags += '-DCC_AVX512_SUPPORT'

    elif cc.has_multi_arguments('-mavx512vl')

        cflags += '-DCC_AVX512_SUPPORT'
        avx512_tmplib = static_library('avx512_tmp',
                               'dlb2_avx512.c',
                               dependencies: [static_rte_eal, static_rte_eventdev],
                               c_args: cflags + ['-mavx512vl'])
        objs += avx512_tmplib.extract_objects('dlb2_avx512.c')
    else
        sources += files('dlb2_sse.c')
    endif
else
        sources += files('dlb2_sse.c')
endif

headers = files('rte_pmd_dlb2.h')

deps += ['mbuf', 'mempool', 'ring', 'pci', 'bus_pci']
