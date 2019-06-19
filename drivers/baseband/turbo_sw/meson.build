# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

path = get_option('flexran_sdk')

if dpdk_conf.has('RTE_BBDEV_SDK_AVX2')
	lib = cc.find_library('libturbo', dirs: [path + '/lib_turbo'], required: false)
	if not lib.found()
		build = false
	else
		ext_deps += cc.find_library('libturbo', dirs: [path + '/lib_turbo'], required: true)
		ext_deps += cc.find_library('libcrc', dirs: [path + '/lib_crc'], required: true)
		ext_deps += cc.find_library('librate_matching', dirs: [path + '/lib_rate_matching'], required: true)
		ext_deps += cc.find_library('libcommon', dirs: [path + '/lib_common'], required: true)
		ext_deps += cc.find_library('libstdc++', required: true)
		ext_deps += cc.find_library('libirc', required: true)
		ext_deps += cc.find_library('libimf', required: true)
		ext_deps += cc.find_library('libipps', required: true)
		ext_deps += cc.find_library('libsvml', required: true)
		includes += include_directories(path + '/lib_turbo')
		includes += include_directories(path + '/lib_crc')
		includes += include_directories(path + '/lib_rate_matching')
		includes += include_directories(path + '/lib_common')
	endif
endif

deps += ['bbdev', 'bus_vdev', 'ring']
name = 'bbdev_turbo_sw'
allow_experimental_apis = true
sources = files('bbdev_turbo_software.c')
