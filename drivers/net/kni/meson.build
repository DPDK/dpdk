# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

# this driver can be built if-and-only-if KNI library is buildable
build = dpdk_conf.has('RTE_LIB_KNI')
reason = 'missing dependency, DPDK KNI library'
sources = files('rte_eth_kni.c')
deps += 'kni'
