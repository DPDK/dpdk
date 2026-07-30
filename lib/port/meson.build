# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'rte_swx_port_ethdev.c',
        'rte_swx_port_fd.c',
        'rte_swx_port_ring.c',
        'rte_swx_port_source_sink.c',
)
headers = files(
        'rte_swx_port.h',
        'rte_swx_port_ethdev.h',
        'rte_swx_port_fd.h',
        'rte_swx_port_ring.h',
        'rte_swx_port_source_sink.h',
)
deps += ['ethdev']

if dpdk_conf.has('RTE_HAS_LIBPCAP')
    dpdk_conf.set('RTE_PORT_PCAP', 1)
    ext_deps += pcap_dep # dependency provided in config/meson.build
endif
