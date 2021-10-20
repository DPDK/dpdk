# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Microsoft Corporation

if not dpdk_conf.has('RTE_PORT_PCAP')
    build = false
    reason = 'missing dependency, "libpcap"'
endif

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('main.c')
deps += ['ethdev', 'pdump', 'pcapng', 'bpf']
