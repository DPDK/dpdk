# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Luca Boccassi <bluca@debian.org>

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

sources = files(
        'rte_eth_tap.c',
        'tap_intr.c',
        'tap_netlink.c',
)

deps = ['bus_vdev', 'gso', 'hash']

max_queues = '-DTAP_MAX_QUEUES=64'
cflags += max_queues
cflags += no_wvla_cflag

require_iova_in_mbuf = false

if cc.has_header_symbol('linux/pkt_cls.h', 'TCA_FLOWER_ACT')
    cflags += '-DHAVE_TCA_FLOWER'
    sources += files(
            'tap_flow.c',
            'tap_tcmsgs.c',
    )

    enable_tap_rss = false

    subdir('bpf')
    if enable_tap_rss
        cflags += '-DHAVE_BPF_RSS'
        ext_deps += libbpf
        sources += tap_rss_skel_h
    endif
endif
