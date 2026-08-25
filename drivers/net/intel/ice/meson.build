# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sources = files(
        'ice_acl_filter.c',
        'ice_diagnose.c',
        'ice_ethdev.c',
        'ice_fdir_filter.c',
        'ice_generic_flow.c',
        'ice_hash.c',
        'ice_rxtx.c',
        'ice_switch_filter.c',
        'ice_tm.c',
)

testpmd_sources = files('ice_testpmd.c')

deps += ['hash', 'net']

if dpdk_conf.has('RTE_NET_IAVF')
    deps += 'net_iavf'
else
    includes += include_directories(
            '../iavf',
            '../iavf/base'
    )
    sources += files(
            '../iavf/base/iavf_adminq.c',
            '../iavf/base/iavf_common.c',
            '../iavf/iavf_osdep.c',
    )
endif

if arch_subdir == 'x86'
    sources_avx2 += files('ice_rxtx_vec_avx2.c')
    sources_avx512 += files('ice_rxtx_vec_avx512.c')
elif arch_subdir == 'arm' and dpdk_conf.get('RTE_ARCH_64')
    sources += files('ice_rxtx_vec_neon.c')
endif

sources += files(
        'ice_dcf.c',
        'ice_dcf_vf_representor.c',
        'ice_dcf_ethdev.c',
        'ice_dcf_parent.c',
        'ice_dcf_sched.c',
)

require_iova_in_mbuf = false

base_sources = files(
        'base/ice_controlq.c',
        'base/ice_common.c',
        'base/ice_sched.c',
        'base/ice_switch.c',
        'base/ice_nvm.c',
        'base/ice_flex_pipe.c',
        'base/ice_flow.c',
        'base/ice_dcb.c',
        'base/ice_fdir.c',
        'base/ice_acl.c',
        'base/ice_acl_ctrl.c',
        'base/ice_vlan_mode.c',
        'base/ice_ptp_hw.c',
        'base/ice_parser.c',
        'base/ice_imem.c',
        'base/ice_metainit.c',
        'base/ice_pg_cam.c',
        'base/ice_bst_tcam.c',
        'base/ice_ptype_mk.c',
        'base/ice_mk_grp.c',
        'base/ice_proto_grp.c',
        'base/ice_flg_rd.c',
        'base/ice_xlt_kb.c',
        'base/ice_parser_rt.c',
        'base/ice_ddp.c',
        'base/ice_fwlog.c',
        'base/ice_vf_mbx.c',
)

if is_ms_compiler
    base_error_cflags = [
            '/wd4101', # unreferenced local variable
            '/wd4334', # result of 32-bit shift implicitly converted to 64 bits
            '/wd4319', # zero extending 'unsigned long' to 'u64' of greater size
    ]
else
    base_error_cflags = [
            '-Wno-unused-but-set-variable',
            '-Wno-unused-parameter',
    ]
endif

foreach flag: base_error_cflags
    if cc.has_argument(flag)
        base_cflags += flag
    endif
endforeach
