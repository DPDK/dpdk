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

# set the machine type and cflags for it
machine = get_option('machine')
dpdk_conf.set('RTE_MACHINE', machine)
add_project_arguments('-march=@0@'.format(machine), language: 'c')
# some libs depend on maths lib
add_project_link_arguments('-lm', language: 'c')
dpdk_extra_ldflags += '-lm'

# add -include rte_config to cflags
add_project_arguments('-include', 'rte_config.h', language: 'c')

# enable extra warnings and disable any unwanted warnings
warning_flags = [
	'-Wsign-compare',
	'-Wcast-qual',
	'-Wno-address-of-packed-member',
	'-Wno-format-truncation'
]
foreach arg: warning_flags
	if cc.has_argument(arg)
		add_project_arguments(arg, language: 'c')
	endif
endforeach

compile_time_cpuflags = []
if host_machine.cpu_family().startswith('x86')
	arch_subdir = 'x86'
	subdir(arch_subdir)
endif
dpdk_conf.set('RTE_COMPILE_TIME_CPUFLAGS', ','.join(compile_time_cpuflags))

# set the install path for the drivers
dpdk_conf.set_quoted('RTE_EAL_PMD_PATH', eal_pmd_path)

# set other values pulled from the build options
dpdk_conf.set('RTE_MAX_LCORE', get_option('max_lcores'))
dpdk_conf.set('RTE_MAX_NUMA_NODES', get_option('max_numa_nodes'))
dpdk_conf.set('RTE_LIBEAL_USE_HPET', get_option('use_hpet'))
dpdk_conf.set('RTE_EAL_ALLOW_INV_SOCKET_ID', get_option('allow_invalid_socket_id'))
# values which have defaults which may be overridden
dpdk_conf.set('RTE_MAX_VFIO_GROUPS', 64)

install_headers('rte_config.h', subdir: get_option('include_subdir_arch'))
