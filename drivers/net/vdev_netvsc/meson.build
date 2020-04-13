# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

if not is_linux
	build = false
	reason = 'only supported on linux'
endif
sources = files('vdev_netvsc.c')

cflags_options = [
        '-D_BSD_SOURCE',
        '-D_DEFAULT_SOURCE',
        '-D_XOPEN_SOURCE=600'
]
foreach option:cflags_options
        if cc.has_argument(option)
                cflags += option
        endif
endforeach
