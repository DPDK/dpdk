# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation
# Copyright(c) 2020 Broadcom

headers = files('rte_pmd_bnxt.h')

includes += include_directories('tf_ulp')
includes += include_directories('tf_core')

sources = files('bnxt_cpr.c',
	'bnxt_ethdev.c',
	'bnxt_filter.c',
	'bnxt_flow.c',
	'bnxt_hwrm.c',
	'bnxt_irq.c',
	'bnxt_ring.c',
	'bnxt_rxq.c',
	'bnxt_rxr.c',
	'bnxt_stats.c',
	'bnxt_txq.c',
	'bnxt_txr.c',
	'bnxt_util.c',
	'bnxt_vnic.c',
	'bnxt_reps.c',

	'tf_core/tf_core.c',
	'tf_core/bitalloc.c',
	'tf_core/tf_msg.c',
	'tf_core/rand.c',
	'tf_core/stack.c',
        'tf_core/tf_em_common.c',
        'tf_core/tf_em_internal.c',
	'tf_core/tf_rm.c',
	'tf_core/tf_tbl.c',
	'tf_core/tfp.c',
	'tf_core/tf_session.c',
	'tf_core/tf_device.c',
	'tf_core/tf_device_p4.c',
	'tf_core/tf_identifier.c',
	'tf_core/tf_shadow_tbl.c',
	'tf_core/tf_shadow_tcam.c',
	'tf_core/tf_tcam.c',
	'tf_core/tf_util.c',
	'tf_core/tf_if_tbl.c',
	'tf_core/ll.c',
	'tf_core/tf_global_cfg.c',
	'tf_core/tf_em_host.c',
	'tf_core/tf_shadow_identifier.c',
	'tf_core/tf_hash.c',

	'hcapi/hcapi_cfa_p4.c',

	'tf_ulp/bnxt_ulp.c',
	'tf_ulp/ulp_mark_mgr.c',
	'tf_ulp/ulp_flow_db.c',
	'tf_ulp/ulp_template_db_tbl.c',
	'tf_ulp/ulp_template_db_class.c',
	'tf_ulp/ulp_template_db_act.c',
	'tf_ulp/ulp_utils.c',
	'tf_ulp/ulp_mapper.c',
	'tf_ulp/ulp_matcher.c',
	'tf_ulp/ulp_rte_parser.c',
	'tf_ulp/bnxt_ulp_flow.c',
	'tf_ulp/ulp_port_db.c',
	'tf_ulp/ulp_def_rules.c',
	'tf_ulp/ulp_fc_mgr.c',
	'tf_ulp/ulp_tun.c',
	'tf_ulp/ulp_template_db_wh_plus_act.c',
	'tf_ulp/ulp_template_db_wh_plus_class.c',
	'tf_ulp/ulp_template_db_stingray_act.c',
	'tf_ulp/ulp_template_db_stingray_class.c',

	'rte_pmd_bnxt.c')

if arch_subdir == 'x86'
	sources += files('bnxt_rxtx_vec_sse.c')
elif arch_subdir == 'arm' and dpdk_conf.get('RTE_ARCH_64')
	sources += files('bnxt_rxtx_vec_neon.c')
endif
