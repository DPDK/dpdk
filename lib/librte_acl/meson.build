# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

version = 2
sources = files('acl_bld.c', 'acl_gen.c', 'acl_run_scalar.c',
		'rte_acl.c', 'tb_mem.c')
headers = files('rte_acl.h', 'rte_acl_osdep.h')

if arch_subdir == 'x86'
	sources += files('acl_run_sse.c')

	# compile AVX2 version if either:
	# a. we have AVX supported in minimum instruction set baseline
	# b. it's not minimum instruction set, but supported by compiler
	#
	# in former case, just add avx2 C file to files list
	# in latter case, compile c file to static lib, using correct compiler
	# flags, and then have the .o file from static lib linked into main lib.
	if dpdk_conf.has('RTE_MACHINE_CPUFLAG_AVX2')
		sources += files('acl_run_avx2.c')
		cflags += '-DCC_AVX2_SUPPORT'
	elif cc.has_argument('-mavx2')
		avx2_tmplib = static_library('avx2_tmp',
				'acl_run_avx2.c',
				dependencies: static_rte_eal,
				c_args: cflags + ['-mavx2'])
		objs += avx2_tmplib.extract_objects('acl_run_avx2.c')
		cflags += '-DCC_AVX2_SUPPORT'
	endif

endif
