# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

cflags += '-std=gnu99'
cflags += '-D_DEFAULT_SOURCE'
cflags += '-D_XOPEN_SOURCE=700'
cflags += '-pedantic'
if is_linux
	cflags += '-DLINUX'
else
	cflags += '-DBSD'
endif

sources = files('failsafe_args.c',
	'failsafe.c',
	'failsafe_eal.c',
	'failsafe_ether.c',
	'failsafe_flow.c',
	'failsafe_intr.c',
	'failsafe_ops.c',
	'failsafe_rxtx.c')
