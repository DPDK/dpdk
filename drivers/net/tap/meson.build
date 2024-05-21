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

if cc.has_header_symbol('linux/pkt_cls.h', 'TCA_FLOWER_ACT')
    cflags += '-DHAVE_TCA_FLOWER'
    sources += files(
            'tap_bpf_api.c',
            'tap_flow.c',
            'tap_tcmsgs.c',
    )
endif

deps = ['bus_vdev', 'gso', 'hash']

cflags += '-DTAP_MAX_QUEUES=16'

# input array for meson symbol search:
# [ "MACRO to define if found", "header for the search",
#   "enum/define", "symbol to search" ]
#
args = [
        [ 'HAVE_TC_ACT_BPF', 'linux/tc_act/tc_bpf.h', 'TCA_ACT_BPF_UNSPEC' ],
]
config = configuration_data()
foreach arg:args
    config.set(arg[0], cc.has_header_symbol(arg[1], arg[2]))
endforeach
configure_file(output : 'tap_autoconf.h', configuration : config)

require_iova_in_mbuf = false
