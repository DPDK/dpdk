# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

if not dpdk_conf.has('RTE_HAS_JANSSON')
    build = false
    reason = 'missing dependency, "jansson"'
    subdir_done()
endif

deps += ['cryptodev']
ext_deps += jansson_dep
sources = files('main.c')
