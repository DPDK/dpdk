# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Luca Boccassi <bluca@debian.org>

if host_machine.system() != 'linux'
        build = false
endif
sources = files(
	'rte_eth_tap.c',
	'tap_bpf_api.c',
	'tap_flow.c',
	'tap_intr.c',
	'tap_netlink.c',
	'tap_tcmsgs.c',
)

deps = ['bus_vdev', 'gso', 'hash']

cflags += '-DTAP_MAX_QUEUES=16'

# To maintain the compatibility with the make build system
# tap_autoconf.h file is still generated.
# input array for meson symbol search:
# [ "MACRO to define if found", "header for the search",
#   "enum/define", "symbol to search" ]
#
args = [
	[ 'HAVE_TC_FLOWER', 'linux/pkt_cls.h',
	  'TCA_FLOWER_UNSPEC' ],
	[ 'HAVE_TC_VLAN_ID', 'linux/pkt_cls.h',
	  'TCA_FLOWER_KEY_VLAN_PRIO' ],
	[ 'HAVE_TC_BPF', 'linux/pkt_cls.h',
	  'TCA_BPF_UNSPEC' ],
	[ 'HAVE_TC_BPF_FD', 'linux/pkt_cls.h',
	  'TCA_BPF_FD' ],
	[ 'HAVE_TC_ACT_BPF', 'linux/tc_act/tc_bpf.h',
	  'TCA_ACT_BPF_UNSPEC' ],
	[ 'HAVE_TC_ACT_BPF_FD', 'linux/tc_act/tc_bpf.h',
	  'TCA_ACT_BPF_FD' ],
]
config = configuration_data()
allow_experimental_apis = true
foreach arg:args
	config.set(arg[0], cc.has_header_symbol(arg[1], arg[2]))
endforeach
configure_file(output : 'tap_autoconf.h', configuration : config)
