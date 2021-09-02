# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2021 Marvell.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources = files(
        'cn9k_eventdev.c',
        'cn9k_worker.c',
        'cn9k_worker_deq.c',
        'cn9k_worker_deq_burst.c',
        'cn9k_worker_deq_ca.c',
        'cn9k_worker_deq_tmo.c',
        'cn9k_worker_dual_deq.c',
        'cn9k_worker_dual_deq_burst.c',
        'cn9k_worker_dual_deq_ca.c',
        'cn9k_worker_dual_deq_tmo.c',
        'cn9k_worker_tx_enq.c',
        'cn9k_worker_tx_enq_seg.c',
        'cn9k_worker_dual_tx_enq.c',
        'cn9k_worker_dual_tx_enq_seg.c',
        'cn10k_eventdev.c',
        'cn10k_worker.c',
        'cn10k_worker_deq.c',
        'cn10k_worker_deq_burst.c',
        'cn10k_worker_deq_ca.c',
        'cn10k_worker_deq_tmo.c',
        'cn10k_worker_tx_enq.c',
        'cn10k_worker_tx_enq_seg.c',
        'cnxk_eventdev.c',
        'cnxk_eventdev_adptr.c',
        'cnxk_eventdev_selftest.c',
        'cnxk_eventdev_stats.c',
        'cnxk_tim_evdev.c',
        'cnxk_tim_worker.c',
)

extra_flags = ['-flax-vector-conversions', '-Wno-strict-aliasing']
foreach flag: extra_flags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach

deps += ['bus_pci', 'common_cnxk', 'net_cnxk', 'crypto_cnxk']
