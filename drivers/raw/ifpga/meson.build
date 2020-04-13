# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

dep = dependency('libfdt', required: false)
if not dep.found()
	dep = cc.find_library('libfdt', required: false)
endif
if not dep.found()
	build = false
	reason = 'missing dependency, "libfdt"'
	subdir_done()
endif

subdir('base')
objs = [base_objs]

deps += ['ethdev', 'rawdev', 'pci', 'bus_pci', 'kvargs',
	'bus_vdev', 'bus_ifpga', 'net', 'pmd_i40e', 'pmd_ipn3ke']
ext_deps += dep

sources = files('ifpga_rawdev.c')

includes += include_directories('base')
includes += include_directories('../../net/ipn3ke')
includes += include_directories('../../net/i40e')
