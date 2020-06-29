# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2018 Intel Corporation

if is_windows
	build = false
	reason = 'not supported on Windows'
	subdir_done()
endif

deps += ['pci', 'kvargs', 'rawdev']
install_headers('rte_bus_ifpga.h')
sources = files('ifpga_common.c', 'ifpga_bus.c')
