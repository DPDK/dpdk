#   BSD LICENSE
#
#   Copyright(c) 2017 Intel Corporation.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
				c_args: '-mavx2')
		objs += avx2_tmplib.extract_objects('acl_run_avx2.c')
		cflags += '-DCC_AVX2_SUPPORT'
	endif

endif
