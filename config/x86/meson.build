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

# for checking defines we need to use the correct compiler flags
march_opt = '-march=@0@'.format(machine)

# we require SSE4.2 for DPDK
sse_errormsg = '''SSE4.2 instruction set is required for DPDK.
Please set the machine type to "nehalem" or "corei7" or higher value'''

if cc.get_define('__SSE4_2__', args: march_opt) == ''
	error(sse_errormsg)
endif

dpdk_conf.set('RTE_ARCH_X86', 1)
if (host_machine.cpu_family() == 'x86_64')
	dpdk_conf.set('RTE_ARCH_X86_64', 1)
	dpdk_conf.set('RTE_ARCH', 'x86_64')
	dpdk_conf.set('RTE_ARCH_64', 1)
else
	dpdk_conf.set('RTE_ARCH_I686', 1)
	dpdk_conf.set('RTE_ARCH', 'i686')
endif

if cc.get_define('__AES__', args: march_opt) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_AES', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_AES']
endif
if cc.get_define('__PCLMUL__', args: march_opt) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_PCLMULQDQ', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_PCLMULQDQ']
endif
if cc.get_define('__AVX__', args: march_opt) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_AVX', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_AVX']
endif
if cc.get_define('__AVX2__', args: march_opt) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_AVX2', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_AVX2']
endif

dpdk_conf.set('RTE_CACHE_LINE_SIZE', 64)
