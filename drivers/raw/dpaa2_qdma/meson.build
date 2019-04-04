# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

version = 2

build = dpdk_conf.has('RTE_LIBRTE_DPAA2_MEMPOOL')
deps += ['rawdev', 'mempool_dpaa2', 'ring', 'kvargs']
sources = files('dpaa2_qdma.c')

allow_experimental_apis = true

install_headers('rte_pmd_dpaa2_qdma.h')
