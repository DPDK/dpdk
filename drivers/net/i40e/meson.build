# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if arch_subdir == 'loongarch'
    build = false
    reason = 'not supported on LoongArch'
    subdir_done()
endif

if arch_subdir == 'riscv'
    build = false
    reason = 'not supported on RISC-V'
    subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

cflags += ['-DPF_DRIVER',
    '-DVF_DRIVER',
    '-DINTEGRATED_VF',
    '-DX722_A0_SUPPORT']

subdir('base')
objs = [base_objs]

sources = files(
        'i40e_ethdev.c',
        'i40e_rxtx.c',
        'i40e_pf.c',
        'i40e_fdir.c',
        'i40e_flow.c',
        'i40e_tm.c',
        'i40e_hash.c',
        'i40e_vf_representor.c',
        'rte_pmd_i40e.c',
)

testpmd_sources = files('i40e_testpmd.c')

deps += ['hash']
includes += include_directories('base')

if arch_subdir == 'x86'
    sources += files('i40e_rxtx_vec_sse.c')

    if is_windows and cc.get_id() != 'clang'
        cflags += ['-fno-asynchronous-unwind-tables']
    endif

    # compile AVX2 version if either:
    # a. we have AVX supported in minimum instruction set baseline
    # b. it's not minimum instruction set, but supported by compiler
    if cc.get_define('__AVX2__', args: machine_args) != ''
        cflags += ['-DCC_AVX2_SUPPORT']
        sources += files('i40e_rxtx_vec_avx2.c')
    elif cc.has_argument('-mavx2')
        cflags += ['-DCC_AVX2_SUPPORT']
        i40e_avx2_lib = static_library('i40e_avx2_lib',
                'i40e_rxtx_vec_avx2.c',
                dependencies: [static_rte_ethdev,
                    static_rte_kvargs, static_rte_hash],
                include_directories: includes,
                c_args: [cflags, '-mavx2'])
        objs += i40e_avx2_lib.extract_objects('i40e_rxtx_vec_avx2.c')
    endif

    i40e_avx512_cpu_support = (
        cc.get_define('__AVX512F__', args: machine_args) != '' and
        cc.get_define('__AVX512BW__', args: machine_args) != '')

    i40e_avx512_cc_support = (
        not machine_args.contains('-mno-avx512f') and
        cc.has_argument('-mavx512f') and
        cc.has_argument('-mavx512bw'))

    if i40e_avx512_cpu_support == true or i40e_avx512_cc_support == true
        cflags += ['-DCC_AVX512_SUPPORT']
        avx512_args = [cflags, '-mavx512f', '-mavx512bw']
        if cc.has_argument('-march=skylake-avx512')
            avx512_args += '-march=skylake-avx512'
        endif
        i40e_avx512_lib = static_library('i40e_avx512_lib',
                'i40e_rxtx_vec_avx512.c',
                dependencies: [static_rte_ethdev,
                    static_rte_kvargs, static_rte_hash],
                include_directories: includes,
                c_args: avx512_args)
        objs += i40e_avx512_lib.extract_objects('i40e_rxtx_vec_avx512.c')
    endif
elif arch_subdir == 'ppc'
       sources += files('i40e_rxtx_vec_altivec.c')
elif arch_subdir == 'arm'
       sources += files('i40e_rxtx_vec_neon.c')
endif

headers = files('rte_pmd_i40e.h')
