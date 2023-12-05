# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

headers = files(
        'rte_fbk_hash.h',
        'rte_hash_crc.h',
        'rte_hash.h',
        'rte_jhash.h',
        'rte_thash.h',
        'rte_thash_gfni.h',
)
indirect_headers += files(
        'rte_crc_arm64.h',
        'rte_crc_generic.h',
        'rte_crc_sw.h',
        'rte_crc_x86.h',
        'rte_thash_x86_gfni.h',
)

sources = files(
        'rte_cuckoo_hash.c',
        'rte_hash_crc.c',
        'rte_fbk_hash.c',
        'rte_thash.c',
        'rte_thash_gfni.c',
)

deps += ['net']
deps += ['ring']
deps += ['rcu']
