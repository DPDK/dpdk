# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

subdir('include')

sources += files(
	'eal.c',
	'eal_alarm.c',
	'eal_debug.c',
	'eal_file.c',
	'eal_hugepages.c',
	'eal_interrupts.c',
	'eal_lcore.c',
	'eal_log.c',
	'eal_memalloc.c',
	'eal_memory.c',
	'eal_mp.c',
	'eal_thread.c',
	'eal_timer.c',
	'fnmatch.c',
	'getopt.c',
)

dpdk_conf.set10('RTE_EAL_NUMA_AWARE_HUGEPAGES', true)
