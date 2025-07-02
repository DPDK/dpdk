# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Huawei Technologies Co., Ltd

sources = files(
        'hinic3_cmdq.c',
        'hinic3_eqs.c',
        'hinic3_hw_cfg.c',
        'hinic3_hw_comm.c',
        'hinic3_hwdev.c',
        'hinic3_hwif.c',
        'hinic3_mbox.c',
        'hinic3_mgmt.c',
        'hinic3_nic_cfg.c',
        'hinic3_nic_event.c',
        'hinic3_wq.c',
)

extra_flags = []

# The driver runs only on arch64 machine, remove 32bit warnings
if not dpdk_conf.get('RTE_ARCH_64')
    extra_flags += [
            '-Wno-int-to-pointer-cast',
            '-Wno-pointer-to-int-cast',
    ]
endif

foreach flag: extra_flags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach

deps += ['hash']
c_args = cflags
includes += include_directories('../')

base_lib = static_library(
    'spnic_base',
    sources,
    dependencies: [
        static_rte_eal,
        static_rte_ethdev,
        static_rte_bus_pci,
        static_rte_hash,
    ],
    include_directories: includes,
    c_args: c_args,
)
base_objs = base_lib.extract_all_objects()
