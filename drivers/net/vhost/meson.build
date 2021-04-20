# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

build = dpdk_conf.has('RTE_LIB_VHOST')
reason = 'missing dependency, DPDK vhost library'
sources = files('rte_eth_vhost.c')
headers = files('rte_eth_vhost.h')
deps += 'vhost'
