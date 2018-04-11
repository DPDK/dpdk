# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

eal_inc += include_directories('.', 'include',
		join_paths('include/arch', arch_subdir))

common_objs = []
common_sources = files(
	'eal_common_bus.c',
	'eal_common_cpuflags.c',
	'eal_common_devargs.c',
	'eal_common_dev.c',
	'eal_common_errno.c',
	'eal_common_fbarray.c',
	'eal_common_hexdump.c',
	'eal_common_launch.c',
	'eal_common_lcore.c',
	'eal_common_log.c',
	'eal_common_memalloc.c',
	'eal_common_memory.c',
	'eal_common_memzone.c',
	'eal_common_options.c',
	'eal_common_proc.c',
	'eal_common_string_fns.c',
	'eal_common_tailqs.c',
	'eal_common_thread.c',
	'eal_common_timer.c',
	'malloc_elem.c',
	'malloc_heap.c',
	'malloc_mp.c',
	'rte_keepalive.c',
	'rte_malloc.c',
	'rte_reciprocal.c',
	'rte_service.c'
)

# get architecture specific sources and objs
eal_common_arch_sources = []
eal_common_arch_objs = []
subdir(join_paths('arch', arch_subdir))
common_sources += eal_common_arch_sources
common_objs += eal_common_arch_objs

common_headers = files(
	'include/rte_alarm.h',
	'include/rte_branch_prediction.h',
	'include/rte_bus.h',
	'include/rte_bitmap.h',
	'include/rte_common.h',
	'include/rte_debug.h',
	'include/rte_devargs.h',
	'include/rte_dev.h',
	'include/rte_eal.h',
	'include/rte_eal_memconfig.h',
	'include/rte_eal_interrupts.h',
	'include/rte_errno.h',
	'include/rte_fbarray.h',
	'include/rte_hexdump.h',
	'include/rte_interrupts.h',
	'include/rte_keepalive.h',
	'include/rte_launch.h',
	'include/rte_lcore.h',
	'include/rte_log.h',
	'include/rte_malloc.h',
	'include/rte_malloc_heap.h',
	'include/rte_memory.h',
	'include/rte_memzone.h',
	'include/rte_pci_dev_feature_defs.h',
	'include/rte_pci_dev_features.h',
	'include/rte_per_lcore.h',
	'include/rte_random.h',
	'include/rte_reciprocal.h',
	'include/rte_service.h',
	'include/rte_service_component.h',
	'include/rte_string_fns.h',
	'include/rte_tailq.h',
	'include/rte_time.h',
	'include/rte_version.h')

# special case install the generic headers, since they go in a subdir
generic_headers = files(
	'include/generic/rte_atomic.h',
	'include/generic/rte_byteorder.h',
	'include/generic/rte_cpuflags.h',
	'include/generic/rte_cycles.h',
	'include/generic/rte_io.h',
	'include/generic/rte_memcpy.h',
	'include/generic/rte_pause.h',
	'include/generic/rte_prefetch.h',
	'include/generic/rte_rwlock.h',
	'include/generic/rte_spinlock.h',
	'include/generic/rte_vect.h')
install_headers(generic_headers, subdir: 'generic')

# get and install the architecture specific headers
subdir(join_paths('include/arch', arch_subdir))
