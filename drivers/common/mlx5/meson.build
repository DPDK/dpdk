# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Mellanox Technologies, Ltd

if not (is_linux or is_windows)
	build = false
	reason = 'only supported on Linux and Windows'
	subdir_done()
endif

allow_experimental_apis = true

deps += ['hash', 'pci', 'net', 'eal', 'kvargs']
sources += files(
	'mlx5_devx_cmds.c',
	'mlx5_common.c',
	'mlx5_common_mp.c',
	'mlx5_common_mr.c',
)

cflags_options = [
	'-std=c11',
	'-Wno-strict-prototypes',
	'-D_BSD_SOURCE',
	'-D_DEFAULT_SOURCE',
	'-D_XOPEN_SOURCE=600'
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

subdir(exec_env)
