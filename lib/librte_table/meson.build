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
sources = files('rte_table_lpm.c', 'rte_table_lpm_ipv6.c',
		'rte_table_hash_cuckoo.c', 'rte_table_hash_key8.c',
		'rte_table_hash_key16.c', 'rte_table_hash_key32.c',
		'rte_table_hash_ext.c', 'rte_table_hash_lru.c',
		'rte_table_array.c', 'rte_table_stub.c')
headers = files('rte_table.h', 'rte_table_lpm.h',
		'rte_table_lpm_ipv6.h', 'rte_table_hash.h',
		'rte_lru.h', 'rte_table_array.h',
		'rte_table_stub.h')
deps += ['mbuf', 'port', 'lpm', 'hash']

if arch_subdir == 'x86'
	headers += files('rte_lru_x86.h')
endif

if dpdk_conf.has('RTE_LIBRTE_ACL')
	sources += files('rte_table_acl.c')
	headers += files('rte_table_acl.h')
	deps += ['acl']
endif
