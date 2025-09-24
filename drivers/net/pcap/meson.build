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
    iphlpapi_dep = cc.find_library('iphlpapi', required: true)
    if not cc.links(min_c_code, dependencies: iphlpapi_dep)
        error('broken dependency, "iphlpapi"')
    endif
    ext_deps += iphlpapi_dep
endif

require_iova_in_mbuf = false
