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

eal_inc += include_directories('.')

common_headers = [
	'rte_alarm.h',
	'rte_branch_prediction.h',
	'rte_bus.h',
	'rte_bitmap.h',
	'rte_common.h',
	'rte_debug.h',
	'rte_devargs.h',
	'rte_dev.h',
	'rte_eal.h',
	'rte_eal_interrupts.h',
	'rte_eal_memconfig.h',
	'rte_errno.h',
	'rte_hexdump.h',
	'rte_interrupts.h',
	'rte_keepalive.h',
	'rte_launch.h',
	'rte_lcore.h',
	'rte_log.h',
	'rte_malloc.h',
	'rte_malloc_heap.h',
	'rte_memory.h',
	'rte_memzone.h',
	'rte_pci_dev_feature_defs.h',
	'rte_pci_dev_features.h',
	'rte_per_lcore.h',
	'rte_random.h',
	'rte_reciprocal.h',
	'rte_service.h',
	'rte_service_component.h',
	'rte_string_fns.h',
	'rte_tailq.h',
	'rte_time.h',
	'rte_version.h']

install_headers(common_headers)
install_subdir('generic', install_dir : get_option('includedir'))

subdir('arch')
