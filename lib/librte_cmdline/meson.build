# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = files('cmdline.c',
	'cmdline_cirbuf.c',
	'cmdline_parse.c',
	'cmdline_parse_etheraddr.c',
	'cmdline_parse_ipaddr.c',
	'cmdline_parse_num.c',
	'cmdline_parse_portlist.c',
	'cmdline_parse_string.c',
	'cmdline_rdline.c',
	'cmdline_socket.c',
	'cmdline_vt100.c')

headers = files('cmdline.h',
	'cmdline_parse.h',
	'cmdline_parse_num.h',
	'cmdline_parse_ipaddr.h',
	'cmdline_parse_etheraddr.h',
	'cmdline_parse_string.h',
	'cmdline_rdline.h',
	'cmdline_vt100.h',
	'cmdline_socket.h',
	'cmdline_cirbuf.h',
	'cmdline_parse_portlist.h')

if is_windows
	sources += files('cmdline_os_windows.c')
else
	sources += files('cmdline_os_unix.c')
endif

deps += ['net']
