# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, 2024 NXP

if not is_linux
    build = false
    reason = 'only supported on linux'
endif

build = dpdk_conf.has('RTE_MEMPOOL_DPAA2')
reason = 'missing dependency, DPDK DPAA2 mempool driver'
deps += ['dmadev', 'bus_fslmc', 'mempool_dpaa2', 'ring', 'kvargs']
sources = files('dpaa2_qdma.c')

if cc.has_argument('-Wno-pointer-arith')
    cflags += '-Wno-pointer-arith'
endif

cflags += no_shadow_cflag
