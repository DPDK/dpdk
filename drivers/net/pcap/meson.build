# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

pcap_dep = cc.find_library('pcap', required: false)
if pcap_dep.found() and cc.has_header('pcap.h', dependencies: pcap_dep)
	build = true
else
	build = false
endif
sources = files('rte_eth_pcap.c')
ext_deps += pcap_dep
pkgconfig_extra_libs += '-lpcap'
