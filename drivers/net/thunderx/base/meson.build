# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

sources = [
        'nicvf_hw.c',
        'nicvf_mbox.c',
        'nicvf_bsvf.c',
]

c_args = cflags
base_lib = static_library('nicvf_base', sources,
    c_args: c_args,
    dependencies: static_rte_ethdev
)

base_objs = base_lib.extract_all_objects(recursive: true)
