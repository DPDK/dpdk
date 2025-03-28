# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2023 Mucse IC Design Ltd.

sources = [
        'rnp_mbx.c',
]

c_args = cflags
base_lib = static_library('rnp_base', sources,
    dependencies: [static_rte_eal, static_rte_net, static_rte_ethdev],
    c_args: c_args)
base_objs = base_lib.extract_all_objects(recursive: true)
