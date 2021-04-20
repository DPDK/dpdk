# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

allow_experimental_apis = true
librt = cc.find_library('rt', required: false)
if not librt.found()
    build = false
    subdir_done()
endif

ext_deps += librt
deps += 'timer'
sources = files(
        'main.c',
        'shm.c',
)
