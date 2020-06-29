# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
	subdir_done()
endif

drivers = ['dpaa', 'dpaa2', 'octeontx2', 'opdl', 'skeleton', 'sw', 'dsw']
if not (toolchain == 'gcc' and cc.version().version_compare('<4.8.6') and
	dpdk_conf.has('RTE_ARCH_ARM64'))
	drivers += 'octeontx'
endif
std_deps = ['eventdev', 'kvargs']
config_flag_fmt = 'RTE_LIBRTE_@0@_EVENTDEV_PMD'
driver_name_fmt = 'rte_pmd_@0@_event'
