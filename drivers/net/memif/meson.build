# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019 Cisco Systems, Inc.  All rights reserved.

if not is_linux
	build = false
	reason = 'only supported on Linux'
endif

sources = files('rte_eth_memif.c',
		'memif_socket.c')

allow_experimental_apis = true
# Experimantal APIs:
# - rte_intr_callback_unregister_pending
# - rte_mp_action_register
# - rte_mp_reply
# - rte_mp_request_sync
# - rte_mcfg_get_single_file_segments

deps += ['hash']
