# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

deps += ['security', 'lpm', 'acl', 'hash']
allow_experimental_apis = true
sources = files(
	'esp.c', 'ipsec.c', 'ipsec-secgw.c', 'parser.c',
	'rt.c', 'sa.c', 'sp4.c', 'sp6.c'
)
