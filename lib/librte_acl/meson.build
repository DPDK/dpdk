# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = files('acl_bld.c', 'acl_gen.c', 'acl_run_scalar.c',
		'rte_acl.c', 'tb_mem.c')
headers = files('rte_acl.h', 'rte_acl_osdep.h')

if dpdk_conf.has('RTE_ARCH_X86')
	sources += files('acl_run_sse.c')

	# compile AVX2 version if either:
	# a. we have AVX supported in minimum instruction set baseline
	# b. it's not minimum instruction set, but supported by compiler
	#
	# in former case, just add avx2 C file to files list
	# in latter case, compile c file to static lib, using correct compiler
	# flags, and then have the .o file from static lib linked into main lib.
	if cc.get_define('__AVX2__', args: machine_args) != ''
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

	# compile AVX512 version if:
	# we are building 64-bit binary AND binutils can generate proper code

	if dpdk_conf.has('RTE_ARCH_X86_64') and binutils_ok

		# compile AVX512 version if either:
		# a. we have AVX512 supported in minimum instruction set
		#    baseline
		# b. it's not minimum instruction set, but supported by
		#    compiler
		#
		# in former case, just add avx512 C file to files list
		# in latter case, compile c file to static lib, using correct
		# compiler flags, and then have the .o file from static lib
		# linked into main lib.

		# check if all required flags already enabled (variant a).
		acl_avx512_flags = ['__AVX512F__', '__AVX512VL__',
			'__AVX512CD__', '__AVX512BW__']

		acl_avx512_on = true
		foreach f:acl_avx512_flags

			if cc.get_define(f, args: machine_args) == ''
				acl_avx512_on = false
			endif
		endforeach

		if acl_avx512_on == true

			sources += files('acl_run_avx512.c')
			cflags += '-DCC_AVX512_SUPPORT'

		elif cc.has_multi_arguments('-mavx512f', '-mavx512vl',
					'-mavx512cd', '-mavx512bw')

			avx512_tmplib = static_library('avx512_tmp',
				'acl_run_avx512.c',
				dependencies: static_rte_eal,
				c_args: cflags +
					['-mavx512f', '-mavx512vl',
					 '-mavx512cd', '-mavx512bw'])
			objs += avx512_tmplib.extract_objects(
					'acl_run_avx512.c')
			cflags += '-DCC_AVX512_SUPPORT'
		endif
	endif

elif dpdk_conf.has('RTE_ARCH_ARM')
	cflags += '-flax-vector-conversions'
	sources += files('acl_run_neon.c')
elif dpdk_conf.has('RTE_ARCH_PPC_64')
	sources += files('acl_run_altivec.c')
endif
