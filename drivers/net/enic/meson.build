# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cisco Systems, Inc.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

sources = files(
        'base/vnic_cq.c',
        'base/vnic_dev.c',
        'base/vnic_intr.c',
        'base/vnic_rq.c',
        'base/vnic_wq.c',
        'enic_ethdev.c',
        'enic_flow.c',
        'enic_fm_flow.c',
        'enic_main.c',
        'enic_res.c',
        'enic_rxtx.c',
        'enic_vf_representor.c',
)
deps += ['hash']
includes += include_directories('base')

# The current implementation assumes 64-bit pointers
if cc.get_define('__AVX2__', args: machine_args) != '' and dpdk_conf.get('RTE_ARCH_64')
    sources += files('enic_rxtx_vec_avx2.c')
# Build the avx2 handler if the compiler supports it, even though 'machine'
# does not. This is to support users who build for the min supported machine
# and need to run the binary on newer CPUs too.
# This part is from i40e meson.build
elif cc.has_argument('-mavx2') and dpdk_conf.get('RTE_ARCH_64')
    enic_avx2_lib = static_library('enic_avx2_lib',
            'enic_rxtx_vec_avx2.c',
            dependencies: [static_rte_ethdev, static_rte_bus_pci],
            include_directories: includes,
            c_args: [cflags, '-mavx2'])
    objs += enic_avx2_lib.extract_objects('enic_rxtx_vec_avx2.c')
endif

annotate_locks = false
