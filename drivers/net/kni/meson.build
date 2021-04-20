# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

# this driver can be built if-and-only-if KNI library is buildable
build = dpdk_conf.has('RTE_LIB_KNI')
reason = 'missing dependency, DPDK KNI library'
sources = files('rte_eth_kni.c')
deps += 'kni'
