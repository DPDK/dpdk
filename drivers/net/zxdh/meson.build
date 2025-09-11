# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 ZTE Corporation

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources = files(
        'zxdh_common.c',
        'zxdh_ethdev.c',
        'zxdh_msg.c',
        'zxdh_pci.c',
        'zxdh_queue.c',
        'zxdh_np.c',
        'zxdh_tables.c',
        'zxdh_rxtx.c',
        'zxdh_ethdev_ops.c',
        'zxdh_mtr.c',
        'zxdh_flow.c',
)
