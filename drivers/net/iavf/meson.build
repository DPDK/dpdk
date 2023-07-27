# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

cflags += ['-Wno-strict-aliasing']

includes += include_directories('../../common/iavf')
deps += ['common_iavf', 'security', 'cryptodev']

sources = files(
        'iavf_ethdev.c',
        'iavf_rxtx.c',
        'iavf_vchnl.c',
        'iavf_generic_flow.c',
        'iavf_fdir.c',
        'iavf_hash.c',
        'iavf_tm.c',
        'iavf_ipsec_crypto.c',
        'iavf_fsub.c',
)

if arch_subdir == 'x86'
    sources += files('iavf_rxtx_vec_sse.c')

    if is_windows and cc.get_id() != 'clang'
        cflags += ['-fno-asynchronous-unwind-tables']
    endif

    iavf_avx2_lib = static_library('iavf_avx2_lib',
            'iavf_rxtx_vec_avx2.c',
            dependencies: [static_rte_ethdev, static_rte_kvargs, static_rte_hash],
            include_directories: includes,
            c_args: [cflags, '-mavx2'])
    objs += iavf_avx2_lib.extract_objects('iavf_rxtx_vec_avx2.c')

    iavf_avx512_cpu_support = (
        cc.get_define('__AVX512F__', args: machine_args) != '' and
        cc.get_define('__AVX512BW__', args: machine_args) != '')

    iavf_avx512_cc_support = (
        not machine_args.contains('-mno-avx512f') and
        cc.has_argument('-mavx512f') and
        cc.has_argument('-mavx512bw'))

    if iavf_avx512_cpu_support == true or iavf_avx512_cc_support == true
        cflags += ['-DCC_AVX512_SUPPORT']
        avx512_args = [cflags, '-mavx512f', '-mavx512bw']
        if cc.has_argument('-march=skylake-avx512')
            avx512_args += '-march=skylake-avx512'
        endif
        iavf_avx512_lib = static_library('iavf_avx512_lib',
                'iavf_rxtx_vec_avx512.c',
                dependencies: [static_rte_ethdev,
                    static_rte_kvargs, static_rte_hash],
                include_directories: includes,
                c_args: avx512_args)
        objs += iavf_avx512_lib.extract_objects('iavf_rxtx_vec_avx512.c')
    endif
elif arch_subdir == 'arm'
    sources += files('iavf_rxtx_vec_neon.c')
endif

headers = files('rte_pmd_iavf.h')
