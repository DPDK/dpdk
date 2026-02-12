# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

# check for FlexRAN SDK libraries
dep_turbo = dependency('flexran_sdk_turbo', required: false)
dep_dec5g = dependency('flexran_sdk_ldpc_decoder_5gnr', required: false)

if dep_turbo.found()
    stdcpp_dep = cc.find_library('stdc++', required: true)
    if not cc.links(min_c_code, dependencies: stdcpp_dep)
        error('broken dependency, "libstdc++"')
    endif
    ext_deps += stdcpp_dep
    irc_dep = cc.find_library('irc', required: true)
    if not cc.links(min_c_code, dependencies: irc_dep)
        error('broken dependency, "libirc"')
    endif
    ext_deps += irc_dep
    imf_dep = cc.find_library('imf', required: true)
    if not cc.links(min_c_code, dependencies: imf_dep)
        error('broken dependency, "libimf"')
    endif
    ext_deps += imf_dep
    ipps_dep = cc.find_library('ipps', required: true)
    if not cc.links(min_c_code, dependencies: ipps_dep)
        error('broken dependency, "libipps"')
    endif
    ext_deps += ipps_dep
    svml_dep = cc.find_library('svml', required: true)
    if not cc.links(min_c_code, dependencies: svml_dep)
        error('broken dependency, "libsvml"')
    endif
    ext_deps += svml_dep
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

deps += ['bus_vdev', 'ring']
sources = files('bbdev_turbo_software.c')
