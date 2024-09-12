# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

if not is_linux
    build = false
    subdir_done()
endif

deps += 'vhost'
deps += 'dmadev'
allow_experimental_apis = true
sources = files(
        'main.c',
        'virtio_net.c',
)
