# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = files(
        'ethdev_driver.c',
        'ethdev_private.c',
        'ethdev_profile.c',
        'ethdev_trace_points.c',
        'rte_class_eth.c',
        'rte_ethdev.c',
        'rte_ethdev_cman.c',
        'rte_flow.c',
        'rte_mtr.c',
        'rte_tm.c',
        'sff_telemetry.c',
        'sff_common.c',
        'sff_8079.c',
        'sff_8472.c',
        'sff_8636.c',
)

headers = files(
        'rte_cman.h',
        'rte_ethdev.h',
        'rte_ethdev_trace_fp.h',
        'rte_dev_info.h',
        'rte_flow.h',
        'rte_flow_driver.h',
        'rte_mtr.h',
        'rte_mtr_driver.h',
        'rte_tm.h',
        'rte_tm_driver.h',
)

indirect_headers += files(
        'rte_ethdev_core.h',
        'rte_eth_ctrl.h',
)

driver_sdk_headers += files(
        'ethdev_driver.h',
        'ethdev_pci.h',
        'ethdev_vdev.h',
)

deps += ['net', 'kvargs', 'meter', 'telemetry']

if is_freebsd
    annotate_locks = false
endif
