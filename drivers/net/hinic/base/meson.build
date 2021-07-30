# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Huawei Technologies Co., Ltd

sources = [
        'hinic_pmd_api_cmd.c',
        'hinic_pmd_cfg.c',
        'hinic_pmd_cmdq.c',
        'hinic_pmd_eqs.c',
        'hinic_pmd_hwdev.c',
        'hinic_pmd_hwif.c',
        'hinic_pmd_mgmt.c',
        'hinic_pmd_niccfg.c',
        'hinic_pmd_nicio.c',
        'hinic_pmd_wq.c',
        'hinic_pmd_mbox.c',
]

extra_flags = []
# The driver runs only on arch64 machine, remove 32bit warnings
if not dpdk_conf.get('RTE_ARCH_64')
        extra_flags += ['-Wno-int-to-pointer-cast', '-Wno-pointer-to-int-cast']
endif

foreach flag: extra_flags
        if cc.has_argument(flag)
                cflags += flag
        endif
endforeach

deps += ['hash']

c_args = cflags

base_lib = static_library('hinic_base', sources,
    dependencies: [static_rte_eal, static_rte_ethdev, static_rte_bus_pci, static_rte_hash],
    c_args: c_args)
base_objs = base_lib.extract_all_objects(recursive: true)
