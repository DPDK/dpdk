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

cflags = ['-DPF_DRIVER',
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
	'rte_pmd_i40e.c'
	)

deps += ['hash']

if arch_subdir == 'x86'
	dpdk_conf.set('RTE_LIBRTE_I40E_INC_VECTOR', 1)
	sources += files('i40e_rxtx_vec_sse.c')

	# compile AVX2 version if either:
	# a. we have AVX supported in minimum instruction set baseline
	# b. it's not minimum instruction set, but supported by compiler
	if dpdk_conf.has('RTE_MACHINE_CPUFLAG_AVX2')
		sources += files('i40e_rxtx_vec_avx2.c')
	elif cc.has_argument('-mavx2')
		i40e_avx2_lib = static_library('i40e_avx2_lib',
				'i40e_rxtx_vec_avx2.c',
				dependencies: static_rte_eal,
				c_args: '-mavx2')
		objs += i40e_avx2_lib.extract_objects('i40e_rxtx_vec_avx2.c')
	endif
endif

includes += include_directories('base')

install_headers('rte_pmd_i40e.h')
