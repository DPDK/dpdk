# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 6WIND S.A.
# Copyright 2018 Mellanox Technologies, Ltd

if not (is_linux or is_windows)
    build = false
    reason = 'only supported on Linux and Windows'
    subdir_done()
endif

deps += ['hash', 'common_mlx5']
if not dpdk_conf.has('RTE_COMMON_MLX5')
    # avoid referencing undefined variables from common/mlx5
    subdir_done()
endif

headers = files('rte_pmd_mlx5.h')
sources = files(
        'mlx5.c',
        'mlx5_ethdev.c',
        'mlx5_flow.c',
        'mlx5_flow_meter.c',
        'mlx5_flow_dv.c',
        'mlx5_flow_aso.c',
        'mlx5_flow_flex.c',
        'mlx5_mac.c',
        'mlx5_rss.c',
        'mlx5_rx.c',
        'mlx5_rxmode.c',
        'mlx5_rxq.c',
        'mlx5_rxtx.c',
        'mlx5_stats.c',
        'mlx5_trigger.c',
        'mlx5_tx.c',
        'mlx5_tx_empw.c',
        'mlx5_tx_mpw.c',
        'mlx5_tx_nompw.c',
        'mlx5_tx_txpp.c',
        'mlx5_txq.c',
        'mlx5_txpp.c',
        'mlx5_vlan.c',
        'mlx5_utils.c',
        'mlx5_devx.c',
)

if is_linux
    sources += files(
            'mlx5_flow_hw.c',
            'mlx5_hws_cnt.c',
            'mlx5_flow_verbs.c',
    )
endif

if is_linux and (dpdk_conf.has('RTE_ARCH_X86_64')
              or dpdk_conf.has('RTE_ARCH_ARM64')
              or dpdk_conf.has('RTE_ARCH_PPC_64'))
    sources += files('mlx5_rxtx_vec.c')
else
    sources += files('mlx5_rxtx_vec_null.c')
endif

cflags_options = [
        '-std=c11',
        '-Wno-strict-prototypes',
        '-D_BSD_SOURCE',
        '-D_DEFAULT_SOURCE',
        '-D_XOPEN_SOURCE=600',
]
foreach option:cflags_options
    if cc.has_argument(option)
        cflags += option
    endif
endforeach
if get_option('buildtype').contains('debug')
    cflags += [ '-pedantic', '-DPEDANTIC' ]
else
    cflags += [ '-UPEDANTIC' ]
endif

testpmd_sources += files('mlx5_testpmd.c')

subdir(exec_env)

subdir('hws')

annotate_locks = false
