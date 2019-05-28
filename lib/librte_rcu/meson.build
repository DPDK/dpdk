# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Arm Limited

allow_experimental_apis = true

sources = files('rte_rcu_qsbr.c')
headers = files('rte_rcu_qsbr.h')

# for clang 32-bit compiles we need libatomic for 64-bit atomic ops
if cc.get_id() == 'clang' and dpdk_conf.get('RTE_ARCH_64') == false
	ext_deps += cc.find_library('atomic')
endif
