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

sources += files(
        'sxe2_ethdev.c',
        'sxe2_cmd_chnl.c',
        'sxe2_vsi.c',
        'sxe2_queue.c',
        'sxe2_tx.c',
        'sxe2_rx.c',
        'sxe2_txrx_poll.c',
        'sxe2_txrx.c',
)
