# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

cflags += ['-Wno-strict-aliasing']

includes += include_directories('../../common/iavf')
deps += ['common_iavf']

sources = files(
	'iavf_ethdev.c',
	'iavf_rxtx.c',
	'iavf_vchnl.c',
	'iavf_generic_flow.c',
	'iavf_fdir.c',
	'iavf_hash.c',
)

if arch_subdir == 'x86'
	sources += files('iavf_rxtx_vec_sse.c')

	# compile AVX2 version if either:
	# a. we have AVX supported in minimum instruction set baseline
	# b. it's not minimum instruction set, but supported by compiler
	if dpdk_conf.has('RTE_MACHINE_CPUFLAG_AVX2')
		cflags += ['-DCC_AVX2_SUPPORT']
		sources += files('iavf_rxtx_vec_avx2.c')
	elif cc.has_argument('-mavx2')
		cflags += ['-DCC_AVX2_SUPPORT']
		iavf_avx2_lib = static_library('iavf_avx2_lib',
				'iavf_rxtx_vec_avx2.c',
				dependencies: [static_rte_ethdev,
					static_rte_kvargs, static_rte_hash],
				include_directories: includes,
				c_args: [cflags, '-mavx2'])
		objs += iavf_avx2_lib.extract_objects('iavf_rxtx_vec_avx2.c')
	endif
endif
