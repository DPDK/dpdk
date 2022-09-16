# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

headers = files('rte_member.h')

sources = files(
        'rte_member.c',
        'rte_member_ht.c',
        'rte_member_sketch.c',
        'rte_member_vbf.c',
)

deps += ['hash', 'ring']

# compile AVX512 version if:
if dpdk_conf.has('RTE_ARCH_X86_64') and binutils_ok
    # compile AVX512 version if either:
    # a. we have AVX512 supported in minimum instruction set
    #    baseline
    # b. it's not minimum instruction set, but supported by
    #    compiler
    #
    # in former case, just add avx512 C file to files list
    # in latter case, compile c file to static lib, using correct
    # compiler flags, and then have the .o file from static lib
    # linked into main lib.

    # check if all required flags already enabled
    sketch_avx512_flags = ['__AVX512F__', '__AVX512DQ__', '__AVX512IFMA__']

    sketch_avx512_on = true
    foreach f:sketch_avx512_flags
        if cc.get_define(f, args: machine_args) == ''
            sketch_avx512_on = false
        endif
    endforeach

    if sketch_avx512_on == true
        cflags += ['-DCC_AVX512_SUPPORT']
        sources += files('rte_member_sketch_avx512.c')
    elif cc.has_multi_arguments('-mavx512f', '-mavx512dq', '-mavx512ifma')
        sketch_avx512_tmp = static_library('sketch_avx512_tmp',
            'rte_member_sketch_avx512.c',
            include_directories: includes,
            dependencies: [static_rte_eal, static_rte_hash],
            c_args: cflags +
                ['-mavx512f', '-mavx512dq', '-mavx512ifma'])
        objs += sketch_avx512_tmp.extract_objects('rte_member_sketch_avx512.c')
        cflags += ['-DCC_AVX512_SUPPORT']
    endif
endif
