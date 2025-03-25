# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2020 Intel Corporation

headers = files(
        'rte_cksum.h',
        'rte_ip.h',
        'rte_ip4.h',
        'rte_ip6.h',
        'rte_tcp.h',
        'rte_udp.h',
        'rte_tls.h',
        'rte_dtls.h',
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

use_function_versioning = true

if dpdk_conf.has('RTE_ARCH_X86_64')
    sources += files('net_crc_sse.c')
    cflags += ['-mpclmul', '-maes']
    if cc.has_argument('-mvpclmulqdq') and cc_has_avx512
        cflags += ['-DCC_X86_64_AVX512_VPCLMULQDQ_SUPPORT']
        net_crc_avx512_lib = static_library(
                'net_crc_avx512_lib',
                'net_crc_avx512.c',
                dependencies: static_rte_eal,
                c_args: [cflags, cc_avx512_flags, '-mvpclmulqdq'])
        objs += net_crc_avx512_lib.extract_objects('net_crc_avx512.c')
    endif

elif (dpdk_conf.has('RTE_ARCH_ARM64') and
        cc.get_define('__ARM_FEATURE_CRYPTO', args: machine_args) != '')
    sources += files('net_crc_neon.c')
    cflags += ['-DCC_ARM64_NEON_PMULL_SUPPORT']
endif
