# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Mellanox Technologies, Ltd

if not cc.has_header('mlx5devx.h')
    build = false
    reason = 'missing dependency, "mlx5devx.h"'
    subdir_done()
endif

devxlib = cc.find_library('mlx5devx', required: true)
if not devxlib.found()
    build = false
    reason = 'missing dependency, "mlx5devx"'
    subdir_done()
endif
ext_deps += devxlib

includes += include_directories('.')

sources += files(
        'mlx5_glue.c',
        'mlx5_common_os.c',
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
