# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = files('rte_distributor.c', 'rte_distributor_v20.c')
if arch_subdir == 'x86'
	sources += files('rte_distributor_match_sse.c')
else
	sources += files('rte_distributor_match_generic.c')
endif
headers = files('rte_distributor.h')
deps += ['mbuf']
