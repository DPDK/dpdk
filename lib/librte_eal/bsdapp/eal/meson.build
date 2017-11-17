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

sources = ['eal_alarm.c',
		'eal_debug.c',
		'eal_hugepage_info.c',
		'eal_interrupts.c',
		'eal_lcore.c',
		'eal_thread.c',
		'eal_timer.c',
		'eal.c',
		'eal_memory.c',
]

eal_extra_link_arg = '-lexecinfo'

eal_inc += include_directories('../../../librte_compat')
eal_lib = library('rte_eal', sources, eal_common_sources, eal_common_arch_sources,
			dependencies: dependency('threads'),
			include_directories : eal_inc,
			version: '@0@.1'.format(version),
			c_args: ['-D_GNU_SOURCE', '-DALLOW_EXPERIMENTAL_API'],
			link_depends: version_map,
			link_args: [eal_extra_link_arg,
				'-Wl,--version-script=' + version_map],
			install: true
)

dpdk_libraries += eal_lib
dpdk_extra_ldflags += ['-pthread', eal_extra_link_arg]

rte_eal = declare_dependency(link_with: eal_lib,
		include_directories: eal_inc,
		dependencies: dependency('threads'))
set_variable('dep_rte_eal', rte_eal)
