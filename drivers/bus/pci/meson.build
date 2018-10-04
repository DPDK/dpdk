# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

version = 2

deps += ['pci']
install_headers('rte_bus_pci.h')
sources = files('pci_common.c',
	'pci_common_uio.c',
	'pci_params.c')
if host_machine.system() == 'linux'
	sources += files('linux/pci.c',
			'linux/pci_uio.c',
			'linux/pci_vfio.c')
	includes += include_directories('linux')
else
	sources += files('bsd/pci.c')
	includes += include_directories('bsd')
endif

# memseg walk is not part of stable API yet
allow_experimental_apis = true

deps += ['kvargs']
