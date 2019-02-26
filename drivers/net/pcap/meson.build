# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

pcap_dep = dependency('pcap', required: false)
if pcap_dep.found()
	build = true
else
	# pcap got a pkg-config file only in 1.9.0 and before that meson uses
	# an internal pcap-config finder, which is not compatible with
	# cross-compilation, so try to fallback to find_library
	pcap_dep = cc.find_library('pcap', required: false)
	if pcap_dep.found() and cc.has_header('pcap.h', dependencies: pcap_dep)
		build = true
		pkgconfig_extra_libs += '-lpcap'
	else
		build = false
	endif
endif
sources = files('rte_eth_pcap.c')
ext_deps += pcap_dep
