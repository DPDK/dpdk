# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019 Cisco Systems, Inc.  All rights reserved.

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

sources = files(
        'memif_socket.c',
        'rte_eth_memif.c',
)

deps += ['hash']
