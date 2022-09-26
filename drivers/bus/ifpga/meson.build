# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

deps += ['pci', 'kvargs', 'rawdev']
driver_sdk_headers += files('bus_ifpga_driver.h')
sources = files('ifpga_bus.c')
