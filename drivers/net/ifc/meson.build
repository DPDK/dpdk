# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

build = dpdk_conf.has('RTE_LIBRTE_VHOST')
allow_experimental_apis = true
sources = files('ifcvf_vdpa.c', 'base/ifcvf.c')
includes += include_directories('base')
deps += 'vhost'
