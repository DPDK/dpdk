# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation
# Copyright(c) 2023 Broadcom

#Include the folder for headers
includes += include_directories('.')
cflags += '-DBNXT_TF_APP_ID=0'

#Add the source files
sources += files(
        'bnxt_tf_pmd_shim.c',
        'bnxt_ulp.c',
        'bnxt_ulp_flow.c',
        'bnxt_ulp_meter.c',
        'ulp_def_rules.c',
        'ulp_fc_mgr.c',
        'ulp_flow_db.c',
        'ulp_gen_hash.c',
        'ulp_gen_tbl.c',
        'ulp_ha_mgr.c',
        'ulp_mapper.c',
        'ulp_mark_mgr.c',
        'ulp_matcher.c',
        'ulp_port_db.c',
        'ulp_rte_handler_tbl.c',
        'ulp_rte_parser.c',
        'ulp_tun.c',
        'ulp_utils.c')

subdir('generic_templates')
