# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

if not dpdk_conf.has('RTE_PORT_PCAP')
    build = false
    reason = 'missing dependency, "libpcap"'
endif
sources = files(
        'pcap_ethdev.c',
        'pcap_osdep_@0@.c'.format(exec_env),
)
ext_deps += pcap_dep
