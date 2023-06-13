# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

subdir('include')

sources += files(
        'eal.c',
        'eal_alarm.c',
        'eal_cpuflags.c',
        'eal_dev.c',
        'eal_hugepage_info.c',
        'eal_interrupts.c',
        'eal_lcore.c',
        'eal_log.c',
        'eal_memalloc.c',
        'eal_memory.c',
        'eal_thread.c',
        'eal_timer.c',
        'eal_vfio.c',
        'eal_vfio_mp_sync.c',
)

deps += ['kvargs', 'telemetry']
if has_libnuma
    dpdk_conf.set10('RTE_EAL_NUMA_AWARE_HUGEPAGES', true)
endif
