# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

deps += ['security', 'lpm', 'acl', 'hash', 'ip_frag', 'ipsec', 'eventdev', 'telemetry']
allow_experimental_apis = true
sources = files(
        'esp.c',
        'event_helper.c',
        'flow.c',
        'ipsec.c',
        'ipsec_process.c',
        'ipsec-secgw.c',
        'ipsec_worker.c',
        'parser.c',
        'rt.c',
        'sa.c',
        'sad.c',
        'sp4.c',
        'sp6.c',
)
