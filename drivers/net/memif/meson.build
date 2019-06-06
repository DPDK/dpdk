# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019 Cisco Systems, Inc.  All rights reserved.

if host_machine.system() != 'linux'
        build = false
endif

sources = files('rte_eth_memif.c',
		'memif_socket.c')

allow_experimental_apis = true
# Experimantal APIs:
# - rte_intr_callback_unregister_pending

deps += ['hash']
