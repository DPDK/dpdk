# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Huawei Technologies Co., Ltd

sources = files(
    'hinic3_dbg.c',
    'hinic3_mml_cmd.c',
    'hinic3_mml_ioctl.c',
    'hinic3_mml_lib.c',
    'hinic3_mml_main.c',
    'hinic3_mml_queue.c',
)

extra_flags = [
    '-Wno-cast-qual',
    '-Wno-format',
    '-Wno-format-nonliteral',
    '-Wno-format-security',
    '-Wno-missing-braces',
    '-Wno-missing-field-initializers',
    '-Wno-missing-prototypes',
    '-Wno-pointer-sign',
    '-Wno-pointer-to-int-cast',
    '-Wno-sign-compare',
    '-Wno-strict-aliasing',
    '-Wno-unused-parameter',
    '-Wno-unused-value',
    '-Wno-unused-variable',
]

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
includes += include_directories('../base/')

mml_lib = static_library(
    'hinic3_mml',
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
mml_objs = mml_lib.extract_all_objects()
