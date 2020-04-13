# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

path = get_option('flexran_sdk')

if dpdk_conf.has('RTE_BBDEV_SDK_AVX2')
	lib = cc.find_library('libturbo', dirs: [path + '/lib_turbo'], required: false)
	if not lib.found()
		build = false
		reason = 'missing dependency, "libturbo"'
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
if dpdk_conf.has('RTE_BBDEV_SDK_AVX512')
	ext_deps += cc.find_library('libldpc_encoder_5gnr', dirs: [path + '/lib_ldpc_encoder_5gnr'], required: true)
	ext_deps += cc.find_library('libldpc_decoder_5gnr', dirs: [path + '/lib_ldpc_decoder_5gnr'], required: true)
	ext_deps += cc.find_library('libLDPC_ratematch_5gnr', dirs: [path + '/lib_LDPC_ratematch_5gnr'], required: true)
	ext_deps += cc.find_library('librate_dematching_5gnr', dirs: [path + '/lib_rate_dematching_5gnr'], required: true)
	includes += include_directories(path + '/lib_ldpc_encoder_5gnr')
	includes += include_directories(path + '/lib_ldpc_decoder_5gnr')
	includes += include_directories(path + '/lib_LDPC_ratematch_5gnr')
	includes += include_directories(path + '/lib_rate_dematching_5gnr')
endif

deps += ['bbdev', 'bus_vdev', 'ring']
sources = files('bbdev_turbo_software.c')
