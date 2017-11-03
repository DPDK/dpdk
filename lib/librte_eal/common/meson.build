#   BSD LICENSE
#
#   Copyright(c) 2017 Intel Corporation.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

eal_inc += include_directories('.', 'include',
		join_paths('include/arch', arch_subdir))

common_objs = []
common_sources = files(
	'eal_common_bus.c',
	'eal_common_cpuflags.c',
	'eal_common_devargs.c',
	'eal_common_dev.c',
	'eal_common_errno.c',
	'eal_common_hexdump.c',
	'eal_common_launch.c',
	'eal_common_lcore.c',
	'eal_common_log.c',
	'eal_common_memory.c',
	'eal_common_memzone.c',
	'eal_common_options.c',
	'eal_common_proc.c',
	'eal_common_string_fns.c',
	'eal_common_tailqs.c',
	'eal_common_thread.c',
	'eal_common_timer.c',
	'eal_filesystem.h',
	'eal_hugepages.h',
	'eal_internal_cfg.h',
	'eal_options.h',
	'eal_private.h',
	'eal_thread.h',
	'malloc_elem.c',
	'malloc_elem.h',
	'malloc_heap.c',
	'malloc_heap.h',
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
