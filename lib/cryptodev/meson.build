# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

sources = files(
        'cryptodev_pmd.c',
        'cryptodev_trace_points.c',
        'rte_cryptodev.c',
)
headers = files(
        'rte_cryptodev.h',
        'rte_cryptodev_trace_fp.h',
        'rte_crypto.h',
        'rte_crypto_sym.h',
        'rte_crypto_asym.h',
)
indirect_headers += files(
        'rte_cryptodev_core.h',
)
driver_sdk_headers += files(
        'cryptodev_pmd.h',
)

deps += ['kvargs', 'mbuf', 'rcu', 'telemetry']
