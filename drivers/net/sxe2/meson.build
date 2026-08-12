# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.

if is_windows
        build = false
        reason = 'only supported on Linux'
        subdir_done()
endif

cflags += ['-g']

deps += ['common_sxe2', 'hash','cryptodev','security']

includes += include_directories('../../common/sxe2')

if arch_subdir == 'x86'
        sources += files('sxe2_txrx_vec_sse.c')
        sources_avx2 += files('sxe2_txrx_vec_avx2.c')
        sources_avx512 += files('sxe2_txrx_vec_avx512.c')
elif arch_subdir == 'arm'
        sources += files('sxe2_txrx_vec_neon.c')
endif

sources += files(
        'sxe2_ethdev.c',
        'sxe2_cmd_chnl.c',
        'sxe2_vsi.c',
        'sxe2_queue.c',
        'sxe2_tx.c',
        'sxe2_rx.c',
        'sxe2_txrx_poll.c',
        'sxe2_txrx.c',
        'sxe2_txrx_vec.c',
        'sxe2_mac.c',
        'sxe2_filter.c',
        'sxe2_rss.c',
        'sxe2_tm.c',
        'sxe2_ipsec.c',
        'sxe2_security.c',
        'sxe2_mp.c',
        'sxe2_stats.c',
        'sxe2_irq.c',
        'sxe2_switchdev.c',
        'sxe2_ethdev_repr.c',
        'sxe2_flow.c',
        'sxe2_flow_parse_action.c',
        'sxe2_flow_parse_pattern.c',
        'sxe2_flow_parse_engine.c',
        'sxe2_dump.c',
)
