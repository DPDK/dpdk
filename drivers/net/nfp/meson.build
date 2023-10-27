# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Corigine, Inc.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
endif

sources = files(
        'flower/nfp_conntrack.c',
        'flower/nfp_flower.c',
        'flower/nfp_flower_cmsg.c',
        'flower/nfp_flower_ctrl.c',
        'flower/nfp_flower_representor.c',
        'nfd3/nfp_nfd3_dp.c',
        'nfdk/nfp_nfdk_dp.c',
        'nfpcore/nfp_cppcore.c',
        'nfpcore/nfp_crc.c',
        'nfpcore/nfp_hwinfo.c',
        'nfpcore/nfp_mip.c',
        'nfpcore/nfp_mutex.c',
        'nfpcore/nfp_nffw.c',
        'nfpcore/nfp_nsp.c',
        'nfpcore/nfp_nsp_cmds.c',
        'nfpcore/nfp_nsp_eth.c',
        'nfpcore/nfp_resource.c',
        'nfpcore/nfp_rtsym.c',
        'nfpcore/nfp_target.c',
        'nfpcore/nfp6000_pcie.c',
        'nfp_cpp_bridge.c',
        'nfp_ethdev.c',
        'nfp_ethdev_vf.c',
        'nfp_flow.c',
        'nfp_ipsec.c',
        'nfp_logs.c',
        'nfp_mtr.c',
        'nfp_net_common.c',
        'nfp_net_ctrl.c',
        'nfp_rxtx.c',
)

deps += ['hash', 'security', 'common_nfp']
