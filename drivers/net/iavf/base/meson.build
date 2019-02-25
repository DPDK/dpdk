# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

sources = [
	'iavf_adminq.c',
	'iavf_common.c',
]

error_cflags = ['-Wno-pointer-to-int-cast']
c_args = cflags
if allow_experimental_apis
	c_args += '-DALLOW_EXPERIMENTAL_API'
endif
foreach flag: error_cflags
	if cc.has_argument(flag)
		c_args += flag
	endif
endforeach

base_lib = static_library('iavf_base', sources,
	dependencies: static_rte_eal,
	c_args: c_args)
base_objs = base_lib.extract_all_objects()
