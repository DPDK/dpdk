# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

#
# Add the experimental APIs called from this PMD
#  rte_eth_switch_domain_alloc()
#  rte_eth_dev_create()
#  rte_eth_dev_destroy()
#  rte_eth_switch_domain_free()
#

if not has_libfdt
    build = false
    reason = 'missing dependency, "libfdt"'
    subdir_done()
endif

includes += include_directories('../../raw/ifpga')

sources += files(
        'ipn3ke_ethdev.c',
        'ipn3ke_flow.c',
        'ipn3ke_representor.c',
        'ipn3ke_tm.c',
)
deps += ['bus_ifpga', 'ethdev', 'sched']
