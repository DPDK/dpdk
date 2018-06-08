# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

dep = cc.find_library('z', required: false)
build = dep.found()
ext_deps += dep
cflags += '-DZLIB_CONST'
sources = files('bnx2x.c',
	'bnx2x_ethdev.c',
	'bnx2x_rxtx.c',
	'bnx2x_stats.c',
	'bnx2x_vfpf.c',
	'ecore_sp.c',
	'elink.c')
