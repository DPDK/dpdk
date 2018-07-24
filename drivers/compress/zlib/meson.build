# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium Networks

dep = dependency('zlib', required: false)
if not dep.found()
	build = false
endif

deps += 'bus_vdev'
sources = files('zlib_pmd.c', 'zlib_pmd_ops.c')
ext_deps += dep
pkgconfig_extra_libs += '-lz'

allow_experimental_apis = true
