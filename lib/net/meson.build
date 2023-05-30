# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2020 Intel Corporation

headers = files(
        'rte_ip.h',
        'rte_tcp.h',
        'rte_udp.h',
        'rte_esp.h',
        'rte_sctp.h',
        'rte_icmp.h',
        'rte_arp.h',
        'rte_ether.h',
        'rte_macsec.h',
        'rte_vxlan.h',
        'rte_gre.h',
        'rte_gtp.h',
        'rte_net.h',
        'rte_net_crc.h',
        'rte_mpls.h',
        'rte_higig.h',
        'rte_ecpri.h',
        'rte_pdcp_hdr.h',
        'rte_geneve.h',
        'rte_l2tpv2.h',
        'rte_ppp.h',
        'rte_ib.h',
)

sources = files(
        'rte_arp.c',
        'rte_ether.c',
        'rte_net.c',
        'rte_net_crc.c',
)
deps += ['mbuf']

if dpdk_conf.has('RTE_ARCH_X86_64')
    net_crc_sse42_cpu_support = (cc.get_define('__PCLMUL__', args: machine_args) != '')
    net_crc_avx512_cpu_support = (
            cc.get_define('__AVX512F__', args: machine_args) != '' and
            cc.get_define('__AVX512BW__', args: machine_args) != '' and
            cc.get_define('__AVX512DQ__', args: machine_args) != '' and
            cc.get_define('__AVX512VL__', args: machine_args) != '' and
            cc.get_define('__VPCLMULQDQ__', args: machine_args) != ''
    )

    net_crc_sse42_cc_support = (cc.has_argument('-mpclmul') and cc.has_argument('-maes'))
    net_crc_avx512_cc_support = (
            not machine_args.contains('-mno-avx512f') and
            cc.has_argument('-mavx512f') and
            cc.has_argument('-mavx512bw') and
            cc.has_argument('-mavx512dq') and
            cc.has_argument('-mavx512vl') and
            cc.has_argument('-mvpclmulqdq') and
            cc.has_argument('-mavx2') and
            cc.has_argument('-mavx')
    )

    build_static_net_crc_sse42_lib = 0
    build_static_net_crc_avx512_lib = 0

    if net_crc_sse42_cpu_support == true
        sources += files('net_crc_sse.c')
        cflags += ['-DCC_X86_64_SSE42_PCLMULQDQ_SUPPORT']
        if net_crc_avx512_cpu_support == true
            sources += files('net_crc_avx512.c')
            cflags += ['-DCC_X86_64_AVX512_VPCLMULQDQ_SUPPORT']
        elif net_crc_avx512_cc_support == true
            build_static_net_crc_avx512_lib = 1
            net_crc_avx512_lib_cflags = [
                    '-mavx512f',
                    '-mavx512bw',
                    '-mavx512dq',
                    '-mavx512vl',
                    '-mvpclmulqdq',
                    '-mavx2',
                    '-mavx',
            ]
            cflags += ['-DCC_X86_64_AVX512_VPCLMULQDQ_SUPPORT']
        endif
    elif net_crc_sse42_cc_support == true
        build_static_net_crc_sse42_lib = 1
        net_crc_sse42_lib_cflags = ['-mpclmul', '-maes']
        cflags += ['-DCC_X86_64_SSE42_PCLMULQDQ_SUPPORT']
        if net_crc_avx512_cc_support == true
            build_static_net_crc_avx512_lib = 1
            net_crc_avx512_lib_cflags = [
                    '-mpclmul',
                    '-maes',
                    '-mavx512f',
                    '-mavx512bw',
                    '-mavx512dq',
                    '-mavx512vl',
                    '-mvpclmulqdq',
                    '-mavx2',
                    '-mavx',
            ]
            cflags += ['-DCC_X86_64_AVX512_VPCLMULQDQ_SUPPORT']
        endif
    endif

    if build_static_net_crc_sse42_lib == 1
        net_crc_sse42_lib = static_library(
                'net_crc_sse42_lib',
                'net_crc_sse.c',
                dependencies: static_rte_eal,
                c_args: [cflags,
                    net_crc_sse42_lib_cflags])
        objs += net_crc_sse42_lib.extract_objects('net_crc_sse.c')
    endif

    if build_static_net_crc_avx512_lib == 1
        net_crc_avx512_lib = static_library(
                'net_crc_avx512_lib',
                'net_crc_avx512.c',
                dependencies: static_rte_eal,
                c_args: [cflags,
                    net_crc_avx512_lib_cflags])
        objs += net_crc_avx512_lib.extract_objects('net_crc_avx512.c')
    endif

elif (dpdk_conf.has('RTE_ARCH_ARM64') and
        cc.get_define('__ARM_FEATURE_CRYPTO', args: machine_args) != '')
    sources += files('net_crc_neon.c')
    cflags += ['-DCC_ARM64_NEON_PMULL_SUPPORT']
endif
