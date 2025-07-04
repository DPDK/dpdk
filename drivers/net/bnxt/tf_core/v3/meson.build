# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation
# Copyright(c) 2022 Broadcom

#Include the folder for headers
includes += include_directories('.')

foreach option:cflags_options
        if cc.has_argument(option)
                cflags += option
        endif
endforeach

#Add the source files
sources += files(
        'tfc_act.c',
        'tfc_cpm.c',
        'tfc_em.c',
        'tfc_global_id.c',
        'tfc_ident.c',
        'tfc_idx_tbl.c',
        'tfc_if_tbl.c',
        'tfc_init.c',
        'tfc_mpc_table.c',
        'tfc_msg.c',
        'tfc_priv.c',
        'tfc_resources.c',
        'tfc_session.c',
        'tfc_tbl_scope.c',
        'tfc_tcam.c',
        'tfc_util.c',
        'tfo.c',
        'tfc_vf2pf_msg.c',
)
