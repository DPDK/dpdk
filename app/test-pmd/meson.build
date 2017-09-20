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

sources = files('cmdline.c',
	'cmdline_flow.c',
	'cmdline_mtr.c',
	'cmdline_tm.c',
	'config.c',
	'csumonly.c',
	'flowgen.c',
	'icmpecho.c',
	'ieee1588fwd.c',
	'iofwd.c',
	'macfwd.c',
	'macswap.c',
	'parameters.c',
	'rxonly.c',
	'testpmd.c',
	'txonly.c')

deps = ['ethdev', 'gro', 'gso', 'cmdline', 'metrics', 'meter']
if dpdk_conf.has('RTE_LIBRTE_PDUMP')
	deps += 'pdump'
endif
if dpdk_conf.has('RTE_LIBRTE_I40E_PMD')
	deps += 'pmd_i40e'
endif
if dpdk_conf.has('RTE_LIBRTE_IXGBE_PMD')
	deps += 'pmd_ixgbe'
endif
if dpdk_conf.has('RTE_LIBRTE_SOFTNIC_PMD')
	sources += files('tm.c')
	deps += 'pmd_softnic'
endif

dep_objs = []
foreach d:deps
	dep_objs += get_variable('dep_rte_' + d)
endforeach

link_libs = []
if get_option('default_library') == 'static'
	link_libs = dpdk_drivers
endif

executable('dpdk-testpmd',
	sources,
	c_args: '-DALLOW_EXPERIMENTAL_API',
	link_whole: link_libs,
	dependencies: dep_objs,
	link_args: eal_extra_link_arg, # add -ldl for linux, -lexecinfo for BSD
	install_rpath: join_paths(get_option('prefix'), driver_install_path),
	install: true)
