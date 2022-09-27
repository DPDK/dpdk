# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

deps += ['cryptodev']
allow_experimental_apis = true
sources = files(
        'fips_validation_aes.c',
        'fips_validation.c',
        'fips_validation_hmac.c',
        'fips_validation_tdes.c',
        'fips_validation_gcm.c',
        'fips_validation_cmac.c',
        'fips_validation_ccm.c',
        'fips_validation_sha.c',
        'fips_validation_xts.c',
        'fips_validation_rsa.c',
        'fips_validation_ecdsa.c',
        'fips_dev_self_test.c',
        'main.c',
)

if dpdk_conf.has('RTE_HAS_JANSSON')
    ext_deps += jansson_dep
    cflags += '-DUSE_JANSSON'
endif

if dpdk_conf.has('RTE_HAS_OPENSSL')
    ext_deps += openssl_dep
    cflags += '-DUSE_OPENSSL'
endif
