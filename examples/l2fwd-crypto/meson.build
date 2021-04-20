# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

deps += 'cryptodev'
if dpdk_conf.has('RTE_CRYPTO_SCHEDULER')
    deps += 'crypto_scheduler'
endif
allow_experimental_apis = true
sources = files(
        'main.c',
)
