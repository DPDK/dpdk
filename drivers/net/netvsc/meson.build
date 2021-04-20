# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Microsoft Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

build = dpdk_conf.has('RTE_BUS_VMBUS')
reason = 'missing dependency, DPDK VMBus driver'
sources = files(
        'hn_ethdev.c',
        'hn_nvs.c',
        'hn_rndis.c',
        'hn_rxtx.c',
        'hn_vf.c',
)

deps += ['bus_vmbus' ]
