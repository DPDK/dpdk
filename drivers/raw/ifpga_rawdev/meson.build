# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

version = 1

subdir('base')
objs = [base_objs]

deps += ['rawdev', 'pci', 'bus_pci', 'kvargs',
	'bus_vdev', 'bus_ifpga']
sources = files('ifpga_rawdev.c')

includes += include_directories('base')

allow_experimental_apis = true
