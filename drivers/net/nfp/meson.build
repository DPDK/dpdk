# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Corigine, Inc.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources = files(
        'flower/nfp_conntrack.c',
        'flower/nfp_flower.c',
        'flower/nfp_flower_cmsg.c',
        'flower/nfp_flower_ctrl.c',
        'flower/nfp_flower_flow.c',
        'flower/nfp_flower_representor.c',
        'flower/nfp_flower_service.c',
        'nfd3/nfp_nfd3_dp.c',
        'nfdk/nfp_nfdk_dp.c',
        'nfdk/nfp_nfdk_vec_stub.c',
        'nfpcore/nfp_cppcore.c',
        'nfpcore/nfp_crc.c',
        'nfpcore/nfp_elf.c',
        'nfpcore/nfp_hwinfo.c',
        'nfpcore/nfp_mip.c',
        'nfpcore/nfp_mutex.c',
        'nfpcore/nfp_nffw.c',
        'nfpcore/nfp_nsp.c',
        'nfpcore/nfp_nsp_cmds.c',
        'nfpcore/nfp_nsp_eth.c',
        'nfpcore/nfp_resource.c',
        'nfpcore/nfp_rtsym.c',
        'nfpcore/nfp_sync.c',
        'nfpcore/nfp_target.c',
        'nfpcore/nfp6000_pcie.c',
        'nfp_cpp_bridge.c',
        'nfp_ethdev.c',
        'nfp_ethdev_vf.c',
        'nfp_ipsec.c',
        'nfp_logs.c',
        'nfp_mtr.c',
        'nfp_net_cmsg.c',
        'nfp_net_common.c',
        'nfp_net_ctrl.c',
        'nfp_net_flow.c',
        'nfp_net_meta.c',
        'nfp_rxtx.c',
        'nfp_rxtx_vec_stub.c',
        'nfp_service.c',
)

if arch_subdir == 'x86'
        includes += include_directories('../../common/nfp')

        avx2_sources = files(
                'nfdk/nfp_nfdk_vec_avx2_dp.c',
                'nfp_rxtx_vec_avx2.c',
        )

        nfp_avx2_lib = static_library('nfp_avx2_lib',
                avx2_sources,
                dependencies: [static_rte_ethdev, static_rte_bus_pci],
                include_directories: includes,
                c_args: [cflags, '-mavx2']
        )

        objs += nfp_avx2_lib.extract_all_objects(recursive: true)
endif

deps += ['hash', 'security', 'common_nfp']
