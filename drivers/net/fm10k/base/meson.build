# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = [
        'fm10k_api.c',
        'fm10k_common.c',
        'fm10k_mbx.c',
        'fm10k_pf.c',
        'fm10k_tlv.c',
        'fm10k_vf.c',
]

base_lib = static_library('fm10k_base', sources,
    dependencies: static_rte_eal,
    c_args: cflags)
base_objs = base_lib.extract_all_objects(recursive: true)
