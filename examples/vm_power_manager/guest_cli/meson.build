# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

if not dpdk_conf.has('RTE_LIB_POWER')
        build = false
        subdir_done()
endif

deps += ['power']

sources = files(
	'main.c', 'parse.c', 'vm_power_cli_guest.c'
)

allow_experimental_apis = true

opt_dep = cc.find_library('virt', required : false)
build = opt_dep.found()
ext_deps += opt_dep
