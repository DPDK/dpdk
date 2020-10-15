# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if has_libfdt == 0
	build = false
	reason = 'missing dependency, "libfdt"'
	subdir_done()
endif

subdir('base')
objs = [base_objs]

deps += ['ethdev', 'rawdev', 'pci', 'bus_pci', 'kvargs',
	'bus_vdev', 'bus_ifpga', 'net', 'net_i40e', 'net_ipn3ke']

sources = files('ifpga_rawdev.c')

includes += include_directories('base')
includes += include_directories('../../net/ipn3ke')
includes += include_directories('../../net/i40e')
