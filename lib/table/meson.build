# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

cflags += no_wvla_cflag

sources = files(
        'rte_swx_keycmp.c',
        'rte_swx_table_em.c',
        'rte_swx_table_learner.c',
        'rte_swx_table_selector.c',
        'rte_swx_table_wm.c',
)

headers = files(
        'rte_swx_hash_func.h',
        'rte_swx_table.h',
        'rte_swx_table_em.h',
        'rte_swx_table_learner.h',
        'rte_swx_table_selector.h',
        'rte_swx_table_wm.h',
)
deps += ['hash', 'acl']
