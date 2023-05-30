# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2023 Marvell.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'pdcp_crypto.c',
        'pdcp_process.c',
        'rte_pdcp.c',
        )
headers = files('rte_pdcp.h')
indirect_headers += files('rte_pdcp_group.h')

deps += ['mbuf', 'net', 'cryptodev', 'security']
