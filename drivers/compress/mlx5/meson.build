# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021 Mellanox Technologies, Ltd

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

fmt_name = 'mlx5_compress'
deps += ['common_mlx5', 'eal', 'compressdev']
sources = files(
        'mlx5_compress.c',
)
cflags_options = [
        '-std=c11',
        '-Wno-strict-prototypes',
        '-D_BSD_SOURCE',
        '-D_DEFAULT_SOURCE',
        '-D_XOPEN_SOURCE=600',
]
foreach option:cflags_options
    if cc.has_argument(option)
        cflags += option
    endif
endforeach
