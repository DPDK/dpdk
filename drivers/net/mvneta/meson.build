# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Marvell International Ltd.
# Copyright(c) 2018 Semihalf.
# All rights reserved.

path = get_option('lib_musdk_dir')
lib_dir = path + '/lib'
inc_dir = path + '/include'

lib = cc.find_library('libmusdk', dirs : [lib_dir], required: false)
if not lib.found()
	build = false
	reason = 'missing dependency, "libmusdk"'
else
	ext_deps += lib
	includes += include_directories(inc_dir)
	cflags += [
	  '-DMVCONF_TYPES_PUBLIC',
	  '-DMVCONF_DMA_PHYS_ADDR_T_PUBLIC',
	  '-DMVCONF_DMA_PHYS_ADDR_T_SIZE=64'
	]
endif

sources = files(
	'mvneta_ethdev.c',
	'mvneta_rxtx.c'
)

deps += ['cfgfile', 'common_mvep']
