# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation
# Copyright(c) 2020 Broadcom

install_headers('rte_pmd_bnxt.h')

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

	'tf_core/tf_core.c',
	'tf_core/bitalloc.c',
	'tf_core/tf_msg.c',
	'tf_core/rand.c',
	'tf_core/stack.c',
	'tf_core/tf_em.c',
	'tf_core/tf_rm.c',
	'tf_core/tf_tbl.c',
	'tf_core/tfp.c',

	'tf_ulp/bnxt_ulp.c',
	'tf_ulp/ulp_mark_mgr.c',
	'tf_ulp/ulp_flow_db.c',
	'tf_ulp/ulp_template_db.c',
	'tf_ulp/ulp_utils.c',
	'tf_ulp/ulp_mapper.c',
	'tf_ulp/ulp_matcher.c',
	'tf_ulp/ulp_rte_parser.c',
	'tf_ulp/bnxt_ulp_flow.c',
	'tf_ulp/ulp_port_db.c',

	'rte_pmd_bnxt.c')

if arch_subdir == 'x86'
	sources += files('bnxt_rxtx_vec_sse.c')
endif
