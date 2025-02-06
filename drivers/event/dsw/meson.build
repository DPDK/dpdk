# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Ericsson AB

deps += ['bus_vdev']
cflags += no_wvla_cflag
if cc.has_argument('-Wno-format-nonliteral')
    cflags += '-Wno-format-nonliteral'
endif
sources = files('dsw_evdev.c', 'dsw_event.c', 'dsw_xstats.c')
require_iova_in_mbuf = false
