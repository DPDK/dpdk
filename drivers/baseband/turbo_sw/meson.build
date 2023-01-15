# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

# check for FlexRAN SDK libraries
dep_turbo = dependency('flexran_sdk_turbo', required: false)
dep_dec5g = dependency('flexran_sdk_ldpc_decoder_5gnr', required: false)

if dep_turbo.found()
    ext_deps += cc.find_library('stdc++', required: true)
    ext_deps += cc.find_library('irc', required: true)
    ext_deps += cc.find_library('imf', required: true)
    ext_deps += cc.find_library('ipps', required: true)
    ext_deps += cc.find_library('svml', required: true)
    ext_deps += dep_turbo
    ext_deps += dependency('flexran_sdk_crc', required: true)
    ext_deps += dependency('flexran_sdk_rate_matching', required: true)
    ext_deps += dependency('flexran_sdk_common', required: true)
    cflags += ['-DRTE_BBDEV_SDK_AVX2']
endif

if dep_dec5g.found()
    ext_deps += dep_dec5g
    ext_deps += dependency('flexran_sdk_ldpc_encoder_5gnr', required: true)
    ext_deps += dependency('flexran_sdk_LDPC_ratematch_5gnr', required: true)
    ext_deps += dependency('flexran_sdk_rate_dematching_5gnr', required: true)
    cflags += ['-DRTE_BBDEV_SDK_AVX512']
endif

deps += ['bbdev', 'bus_vdev', 'ring']
sources = files('bbdev_turbo_software.c')
