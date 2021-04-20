# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

deps += ['pci']
headers = files('rte_bus_pci.h')
sources = files('pci_common.c',
    'pci_params.c')
if is_linux
    sources += files(
            'pci_common_uio.c',
            'linux/pci.c',
            'linux/pci_uio.c',
            'linux/pci_vfio.c',
    )
    includes += include_directories('linux')
endif
if is_freebsd
    sources += files(
            'pci_common_uio.c',
            'bsd/pci.c',
    )
    includes += include_directories('bsd')
endif
if is_windows
    sources += files(
            'windows/pci.c',
            'windows/pci_netuio.c',
    )
    includes += include_directories('windows')
endif

deps += ['kvargs']
