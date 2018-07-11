# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

eal_inc += include_directories('include', '../../../librte_compat')
install_subdir('include/exec-env', install_dir: get_option('includedir'))

env_objs = []
env_headers = []
env_sources = files('eal_alarm.c',
		'eal_cpuflags.c',
		'eal_debug.c',
		'eal_hugepage_info.c',
		'eal_interrupts.c',
		'eal_memalloc.c',
		'eal_lcore.c',
		'eal_log.c',
		'eal_thread.c',
		'eal_timer.c',
		'eal_vfio.c',
		'eal_vfio_mp_sync.c',
		'eal.c',
		'eal_memory.c',
		'eal_dev.c',
)

deps += ['kvargs']
if has_libnuma == 1
	dpdk_conf.set10('RTE_EAL_NUMA_AWARE_HUGEPAGES', true)
endif
