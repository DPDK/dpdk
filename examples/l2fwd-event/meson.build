# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

allow_experimental_apis = true
deps += 'eventdev'
sources = files(
        'main.c',
        'l2fwd_common.c',
        'l2fwd_event.c',
        'l2fwd_event_generic.c',
        'l2fwd_event_internal_port.c',
        'l2fwd_poll.c',
)
