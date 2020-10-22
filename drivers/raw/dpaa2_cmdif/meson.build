# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

build = dpdk_conf.has('RTE_MEMPOOL_DPAA2')
reason = 'missing dependency, DPDK DPAA2 mempool driver'
deps += ['rawdev', 'mempool_dpaa2', 'bus_vdev']
sources = files('dpaa2_cmdif.c')

headers = files('rte_pmd_dpaa2_cmdif.h')
