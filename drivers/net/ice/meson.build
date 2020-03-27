# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

subdir('base')
objs = [base_objs]

sources = files(
	'ice_ethdev.c',
	'ice_rxtx.c',
	'ice_switch_filter.c',
	'ice_generic_flow.c',
	'ice_fdir_filter.c',
	'ice_hash.c'
	)

deps += ['hash', 'net', 'common_iavf']
includes += include_directories('base', '../../common/iavf')

if arch_subdir == 'x86'
	sources += files('ice_rxtx_vec_sse.c')

	# compile AVX2 version if either:
	# a. we have AVX supported in minimum instruction set baseline
	# b. it's not minimum instruction set, but supported by compiler
	if dpdk_conf.has('RTE_MACHINE_CPUFLAG_AVX2')
		sources += files('ice_rxtx_vec_avx2.c')
	elif cc.has_argument('-mavx2')
		ice_avx2_lib = static_library('ice_avx2_lib',
				'ice_rxtx_vec_avx2.c',
				dependencies: [static_rte_ethdev,
					static_rte_kvargs, static_rte_hash],
				include_directories: includes,
				c_args: [cflags, '-mavx2'])
		objs += ice_avx2_lib.extract_objects('ice_rxtx_vec_avx2.c')
	endif
endif

sources += files('ice_dcf.c',
		 'ice_dcf_ethdev.c',
		 'ice_dcf_parent.c')

install_headers('rte_pmd_ice.h')
