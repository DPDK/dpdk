# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Microsoft Corporation

build = dpdk_conf.has('RTE_LIBRTE_VMBUS_BUS')
reason = 'missing dependency, DPDK VMBus driver'
sources = files('hn_ethdev.c', 'hn_rxtx.c', 'hn_rndis.c', 'hn_nvs.c', 'hn_vf.c')

deps += ['bus_vmbus' ]
