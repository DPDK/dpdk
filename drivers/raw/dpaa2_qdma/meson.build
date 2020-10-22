# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

build = dpdk_conf.has('RTE_MEMPOOL_DPAA2')
reason = 'missing dependency, DPDK DPAA2 mempool driver'
deps += ['rawdev', 'mempool_dpaa2', 'ring', 'kvargs']
sources = files('dpaa2_qdma.c')

headers = files('rte_pmd_dpaa2_qdma.h')
