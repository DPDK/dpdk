# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

cflags += ['-DPF_DRIVER',
	'-DVF_DRIVER',
	'-DINTEGRATED_VF',
	'-DX722_A0_SUPPORT']

subdir('base')
objs = [base_objs]

sources = files(
	'i40e_ethdev.c',
	'i40e_rxtx.c',
	'i40e_ethdev_vf.c',
	'i40e_pf.c',
	'i40e_fdir.c',
	'i40e_flow.c',
	'i40e_tm.c',
	'i40e_vf_representor.c',
	'rte_pmd_i40e.c'
	)

deps += ['hash']
includes += include_directories('base')

if arch_subdir == 'x86'
	dpdk_conf.set('RTE_LIBRTE_I40E_INC_VECTOR', 1)
	sources += files('i40e_rxtx_vec_sse.c')

	# compile AVX2 version if either:
	# a. we have AVX supported in minimum instruction set baseline
	# b. it's not minimum instruction set, but supported by compiler
	if cc.get_define('__AVX2__', args: machine_args) != ''
		cflags += ['-DCC_AVX2_SUPPORT']
		sources += files('i40e_rxtx_vec_avx2.c')
	elif cc.has_argument('-mavx2')
		cflags += ['-DCC_AVX2_SUPPORT']
		i40e_avx2_lib = static_library('i40e_avx2_lib',
				'i40e_rxtx_vec_avx2.c',
				dependencies: [static_rte_ethdev,
					static_rte_kvargs, static_rte_hash],
				include_directories: includes,
				c_args: [cflags, '-mavx2'])
		objs += i40e_avx2_lib.extract_objects('i40e_rxtx_vec_avx2.c')
	endif
elif arch_subdir == 'ppc'
       dpdk_conf.set('RTE_LIBRTE_I40E_INC_VECTOR', 1)
       sources += files('i40e_rxtx_vec_altivec.c')
elif arch_subdir == 'arm'
       dpdk_conf.set('RTE_LIBRTE_I40E_INC_VECTOR', 1)
       sources += files('i40e_rxtx_vec_neon.c')
endif

headers = files('rte_pmd_i40e.h')
