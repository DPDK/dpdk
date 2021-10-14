# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources = files(
        'ssovf_worker.c',
        'ssovf_evdev.c',
        'ssovf_evdev_selftest.c',
        'ssovf_probe.c',
        'timvf_worker.c',
        'timvf_evdev.c',
        'timvf_probe.c',
)

deps += ['common_octeontx', 'mempool_octeontx', 'bus_vdev', 'net_octeontx']
deps += ['crypto_octeontx']
