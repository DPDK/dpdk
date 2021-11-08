# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if not dpdk_conf.has('RTE_HAS_LIBPCAP')
    build = false
    reason = 'missing dependency, "libpcap"'
endif

sources = files(
        'pcap_ethdev.c',
        'pcap_osdep_@0@.c'.format(exec_env),
)

ext_deps += pcap_dep
if is_windows
    ext_deps += cc.find_library('iphlpapi', required: true)
endif
