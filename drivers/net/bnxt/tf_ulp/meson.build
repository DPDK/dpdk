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
        'ulp_mapper_tf.c',
        'ulp_mapper_tfc.c',
        'bnxt_ulp_tf.c',
        'bnxt_ulp_tfc.c',
        'ulp_fc_mgr_tfc.c',
        'ulp_fc_mgr_tf.c',
        'ulp_alloc_tbl.c',
        'ulp_sc_mgr.c',
        'ulp_sc_mgr_tfc.c',
        'ulp_tfc_ha_mgr.c')

#include the templates for compile

if get_option('bnxt_tf_template').contains('app19')
includes += include_directories('app_19_templates')
subdir('app_19_templates')
cflags += '-DBNXT_TF_APP_ID=19'

elif get_option('bnxt_tf_template').contains('app17')
includes += include_directories('app_16_17_templates')
subdir('app_16_17_templates')
cflags += '-DBNXT_TF_APP_ID=17'

elif get_option('bnxt_tf_template').contains('app16')
includes += include_directories('app_16_17_templates')
subdir('app_16_17_templates')
cflags += '-DBNXT_TF_APP_ID=16'

elif get_option('bnxt_tf_template').contains('app14')
includes += include_directories('app_14_templates')
subdir('app_14_templates')
cflags += '-DBNXT_TF_APP_ID=14'

elif get_option('bnxt_tf_template').contains('app13')
includes += include_directories('app_13_templates')
subdir('app_13_templates')
cflags += '-DBNXT_TF_APP_ID=13'

elif get_option('bnxt_tf_template').contains('app10')
includes += include_directories('app_10_templates')
subdir('app_10_templates')
cflags += '-DBNXT_TF_APP_ID=10'

elif get_option('bnxt_tf_template').contains('app11')
includes += include_directories('app_11_templates')
subdir('app_11_templates')
cflags += '-DBNXT_TF_APP_ID=11'

elif get_option('bnxt_tf_template').contains('app12')
includes += include_directories('app_12_templates')
subdir('app_12_templates')
cflags += '-DBNXT_TF_APP_ID=12'

elif get_option('bnxt_tf_template').contains('app15')
includes += include_directories('app_15_templates')
subdir('app_15_templates')
cflags += '-DBNXT_TF_APP_ID=15'

elif get_option('bnxt_tf_template').contains('app1')
includes += include_directories('app_1_2_templates')
subdir('app_1_2_templates')
cflags += '-DBNXT_TF_APP_ID=1'
endif

subdir('generic_templates')
