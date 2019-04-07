# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Cesnet
# Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
# All rights reserved.

dep = cc.find_library('nfb', required: false)

build = dep.found() and cc.has_header('nfb/nfb.h', dependencies: dep)

nc = dependency('netcope-common', required: false)

ext_deps += dep
ext_deps += nc

sources = files('nfb_rx.c', 'nfb_tx.c', 'nfb_stats.c', 'nfb_ethdev.c', 'nfb_rxmode.c')
