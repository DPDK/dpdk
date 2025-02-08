# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 ZTE Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

deps += ['bus_pci', 'mbuf', 'compressdev']
sources += files(
        'zsda_device.c',
        'zsda_logs.c',
        'zsda_qp.c',
        'zsda_qp_common.c',
)

zsda_compress = true
zsda_compress_path = 'compress/zsda'
zsda_compress_relpath = '../../' + zsda_compress_path
includes += include_directories(zsda_compress_relpath)
if zsda_compress
    foreach f: ['zsda_comp_pmd.c', 'zsda_comp.c']
        sources += files(join_paths(zsda_compress_relpath, f))
    endforeach
endif
