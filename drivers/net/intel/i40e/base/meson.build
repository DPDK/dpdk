# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2020 Intel Corporation

sources = [
        'i40e_adminq.c',
        'i40e_common.c',
        'i40e_dcb.c',
        'i40e_diag.c',
        'i40e_hmc.c',
        'i40e_lan_hmc.c',
        'i40e_nvm.c',
]

base_lib = static_library('i40e_base', sources,
    dependencies: static_rte_eal,
    c_args: cflags)
base_objs = base_lib.extract_all_objects(recursive: true)
