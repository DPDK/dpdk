# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

pcap_dep = dependency('pcap', required: false)
if pcap_dep.found() == true
	ext_deps += pcap_dep
elif find_program('pcap-config', required: false).found() == true
	ext_deps += cc.find_library('pcap')
else
	build = false
endif
sources = files('rte_eth_pcap.c')
pkgconfig_extra_libs += '-lpcap'
