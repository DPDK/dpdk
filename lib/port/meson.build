# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'rte_port_ethdev.c',
        'rte_port_fd.c',
        'rte_port_frag.c',
        'rte_port_ras.c',
        'rte_port_ring.c',
        'rte_port_sched.c',
        'rte_port_source_sink.c',
        'rte_port_sym_crypto.c',
        'rte_port_eventdev.c',
        'rte_swx_port_ethdev.c',
        'rte_swx_port_fd.c',
        'rte_swx_port_ring.c',
        'rte_swx_port_source_sink.c',
)
headers = files(
        'rte_port_ethdev.h',
        'rte_port_fd.h',
        'rte_port_frag.h',
        'rte_port_ras.h',
        'rte_port.h',
        'rte_port_ring.h',
        'rte_port_sched.h',
        'rte_port_source_sink.h',
        'rte_port_sym_crypto.h',
        'rte_port_eventdev.h',
        'rte_swx_port.h',
        'rte_swx_port_ethdev.h',
        'rte_swx_port_fd.h',
        'rte_swx_port_ring.h',
        'rte_swx_port_source_sink.h',
)
deps += ['ethdev', 'sched', 'ip_frag', 'cryptodev', 'eventdev']

if dpdk_conf.has('RTE_HAS_LIBPCAP')
    dpdk_conf.set('RTE_PORT_PCAP', 1)
    ext_deps += pcap_dep # dependency provided in config/meson.build
endif

if dpdk_conf.has('RTE_LIB_KNI')
    sources += files('rte_port_kni.c')
    headers += files('rte_port_kni.h')
    deps += 'kni'
endif
