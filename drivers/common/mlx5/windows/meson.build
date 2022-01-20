# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Mellanox Technologies, Ltd

includes += include_directories('.')

sources += files(
        'mlx5_glue.c',
        'mlx5_common_os.c',
)

res_lib = run_command(python3, '-c', 'import os; print(os.environ["DEVX_LIB_PATH"])', check: false)
res_inc = run_command(python3, '-c', 'import os; print(os.environ["DEVX_INC_PATH"])', check: false)

if (res_lib.returncode() != 0 or res_inc.returncode() != 0)
    build = false
    reason = 'DevX environment variables are not set, DEVX_LIB_PATH and DEVX_INC_PATH vars must be exported'
    subdir_done()
endif

devx_lib_dir = res_lib.stdout().strip()
devx_inc_dir = res_inc.stdout().strip()

ext_deps += cc.find_library('mlx5devx', dirs: devx_lib_dir, required: true)
includes += include_directories(devx_inc_dir)
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

# Generate an empty mlx5_autoconf.h file for compatibility with Linux
config = configuration_data()
configure_file(output : 'mlx5_autoconf.h', configuration : config)
