# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021 NVIDIA Corporation & Affiliates

if not (is_linux or is_windows)
    build = false
    reason = 'only supported on Linux and Windows'
    subdir_done()
endif

fmt_name = 'mlx5_crypto'
deps += ['common_mlx5', 'eal', 'cryptodev']
sources = files(
        'mlx5_crypto.c',
        'mlx5_crypto_dek.c',
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
