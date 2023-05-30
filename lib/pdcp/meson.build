# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2023 Marvell.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'pdcp_cnt.c',
        'pdcp_crypto.c',
        'pdcp_ctrl_pdu.c',
        'pdcp_process.c',
        'pdcp_reorder.c',
        'rte_pdcp.c',
        )
headers = files('rte_pdcp.h')
indirect_headers += files('rte_pdcp_group.h')

deps += ['mbuf', 'net', 'cryptodev', 'security', 'reorder']
