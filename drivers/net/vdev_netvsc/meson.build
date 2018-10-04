# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

if host_machine.system() != 'linux'
        build = false
endif
sources = files('vdev_netvsc.c')

allow_experimental_apis = true

cflags_options = [
        '-Wall',
        '-Wextra',
        '-D_BSD_SOURCE',
        '-D_DEFAULT_SOURCE',
        '-D_XOPEN_SOURCE=600'
]
foreach option:cflags_options
        if cc.has_argument(option)
                cflags += option
        endif
endforeach
