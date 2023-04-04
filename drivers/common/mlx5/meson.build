# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Mellanox Technologies, Ltd

if not (is_linux or is_windows)
    build = false
    reason = 'only supported on Linux and Windows'
    subdir_done()
endif
if is_windows and not is_ms_linker and not meson.is_cross_build()
    build = false
    reason = 'MinGW is supported only for cross-compilation test'
    subdir_done()
endif

deps += ['hash', 'pci', 'bus_pci', 'bus_auxiliary', 'net', 'eal', 'kvargs']
sources += files(
        'mlx5_devx_cmds.c',
        'mlx5_common.c',
        'mlx5_common_mp.c',
        'mlx5_common_mr.c',
        'mlx5_malloc.c',
        'mlx5_common_pci.c',
        'mlx5_common_devx.c',
        'mlx5_common_utils.c',
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
if get_option('buildtype').contains('debug')
    cflags += [ '-pedantic', '-DPEDANTIC' ]
else
    cflags += [ '-UPEDANTIC' ]
endif

mlx5_config = configuration_data()
subdir(exec_env)
configure_file(output: 'mlx5_autoconf.h', configuration: mlx5_config)

annotate_locks = false
