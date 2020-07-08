# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

subdir('base')
objs = [base_objs]

sources = files(
	'qede_ethdev.c',
	'qede_filter.c',
	'qede_main.c',
	'qede_rxtx.c',
	'qede_debug.c',
	'qede_regs.c',
)

if cc.has_argument('-Wno-format-nonliteral')
	cflags += '-Wno-format-nonliteral'
endif
