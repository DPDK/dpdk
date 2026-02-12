# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2021 Intel Corporation

base_sources = files(
        'ice_controlq.c',
        'ice_common.c',
        'ice_sched.c',
        'ice_switch.c',
        'ice_nvm.c',
        'ice_flex_pipe.c',
        'ice_flow.c',
        'ice_dcb.c',
        'ice_fdir.c',
        'ice_acl.c',
        'ice_acl_ctrl.c',
        'ice_vlan_mode.c',
        'ice_ptp_hw.c',
        'ice_parser.c',
        'ice_imem.c',
        'ice_metainit.c',
        'ice_pg_cam.c',
        'ice_bst_tcam.c',
        'ice_ptype_mk.c',
        'ice_mk_grp.c',
        'ice_proto_grp.c',
        'ice_flg_rd.c',
        'ice_xlt_kb.c',
        'ice_parser_rt.c',
        'ice_ddp.c',
        'ice_fwlog.c',
        'ice_vf_mbx.c',
)

if is_ms_compiler
    error_cflags = [
            '/wd4101', # unreferenced local variable
            '/wd4334', # result of 32-bit shift implicitly converted to 64 bits
            '/wd4319', # zero extending 'unsigned long' to 'u64' of greater size
    ]
else
    error_cflags = [
            '-Wno-unused-but-set-variable',
            '-Wno-unused-parameter',
    ]
endif

# Bugzilla ID: 678
if (toolchain == 'gcc' and cc.version().version_compare('>=11.0.0'))
    error_cflags += ['-Wno-array-bounds']
endif

foreach flag: error_cflags
    if cc.has_argument(flag)
        base_cflags += flag
    endif
endforeach
