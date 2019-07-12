# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

if not dpdk_conf.has('RTE_LIBRTE_POWER')
	build = false
	subdir_done()
endif

deps += ['power']

if dpdk_conf.has('RTE_LIBRTE_BNXT_PMD')
	deps += ['pmd_bnxt']
endif

if dpdk_conf.has('RTE_LIBRTE_I40E_PMD')
	deps += ['pmd_i40e']
endif

if dpdk_conf.has('RTE_LIBRTE_IXGBE_PMD')
	deps += ['pmd_ixgbe']
endif

sources = files(
	'channel_manager.c', 'channel_monitor.c', 'main.c', 'parse.c', 'power_manager.c', 'vm_power_cli.c'
)

# If we're on X86, pull in the x86 code for the branch monitor algo.
if dpdk_conf.has('RTE_ARCH_X86_64')
	sources += files('oob_monitor_x86.c')
else
	sources += files('oob_monitor_nop.c')
endif

opt_dep = cc.find_library('virt', required : false)
build = opt_dep.found()
ext_deps += opt_dep

opt_dep = dependency('jansson', required : false)
if opt_dep.found()
	ext_deps += opt_dep
	cflags += '-DUSE_JANSSON'
endif
