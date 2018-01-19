# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

subdir('base')
objs = [base_objs]

sources = files('nicvf_rxtx.c',
		'nicvf_ethdev.c',
		'nicvf_svf.c'
)

if cc.has_argument('-fno-prefetch-loop-arrays')
	cflags += '-fno-prefetch-loop-arrays'
endif

if cc.has_argument('-Wno-maybe-uninitialized')
	cflags += '-Wno-maybe-uninitialized'
endif

includes += include_directories('base')
