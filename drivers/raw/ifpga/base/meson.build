# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sources = [
        'ifpga_api.c',
        'ifpga_enumerate.c',
        'ifpga_feature_dev.c',
        'ifpga_fme.c',
        'ifpga_fme_iperf.c',
        'ifpga_fme_dperf.c',
        'ifpga_fme_error.c',
        'ifpga_port.c',
        'ifpga_port_error.c',
        'ifpga_fme_pr.c',
        'ifpga_fme_rsu.c',
        'ifpga_sec_mgr.c',
        'opae_hw_api.c',
        'opae_ifpga_hw_api.c',
        'opae_debug.c',
        'opae_spi.c',
        'opae_spi_transaction.c',
        'opae_intel_max10.c',
        'opae_i2c.c',
        'opae_at24_eeprom.c',
        'opae_eth_group.c',
]

rtdep = dependency('librt', required: false)
if not rtdep.found()
    rtdep = cc.find_library('rt', required: false)
endif
if not rtdep.found()
    build = false
    reason = 'missing dependency, "librt"'
    subdir_done()
endif

ext_deps += rtdep

base_lib = static_library('ifpga_rawdev_base', sources,
    dependencies: static_rte_eal,
    c_args: cflags)
base_objs = base_lib.extract_all_objects(recursive: true)
