#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
APP = test

#
# all sources are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) := commands.c
SRCS-$(CONFIG_RTE_APP_TEST) += test.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_pci.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_prefetch.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_byteorder.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_per_lcore.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_atomic.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_malloc.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_cycles.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_spinlock.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_memory.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_memzone.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_ring.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_table.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_table_pipeline.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_table_tables.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_table_ports.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_table_combined.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_table_acl.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_ring_perf.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_rwlock.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_timer.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_mempool.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_mempool_perf.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_mbuf.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_logs.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_memcpy.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_memcpy_perf.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_hash.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_hash_perf.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_lpm.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_lpm6.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_debug.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_errno.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_tailq.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_string_fns.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_mp_secondary.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_cpuflags.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_eal_flags.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_alarm.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_interrupts.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_version.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_eal_fs.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_func_reentrancy.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_cmdline.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_num.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_etheraddr.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_portlist.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_ipaddr.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_cirbuf.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_string.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_lib.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_red.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_sched.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_meter.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_kni.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_power.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_common.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_timer_perf.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_ivshmem.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_distributor.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_distributor_perf.c
SRCS-$(CONFIG_RTE_APP_TEST) += test_devargs.c
SRCS-$(CONFIG_RTE_APP_TEST) += virtual_pmd.c
SRCS-$(CONFIG_RTE_APP_TEST) += packet_burst_generator.c
ifeq ($(CONFIG_RTE_APP_TEST),y)
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += test_acl.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += test_link_bonding.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_RING) += test_pmd_ring.c
SRCS-$(CONFIG_RTE_LIBRTE_KVARGS) += test_kvargs.c
endif

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# Allow use of deprecated rte_snprintf in test_string_fns.c
ifeq ($(CC), icc)
CFLAGS_test_string_fns.o += -Wd1478
else
CFLAGS_test_string_fns.o += -Wno-deprecated-declarations
endif

# Disable warnings of deprecated-declarations in test_kni.c
ifeq ($(CC), icc)
CFLAGS_test_kni.o += -wd1478
else
CFLAGS_test_kni.o += -Wno-deprecated-declarations
endif
CFLAGS += -D_GNU_SOURCE

# this application needs libraries first
DEPDIRS-$(CONFIG_RTE_APP_TEST) += lib

include $(RTE_SDK)/mk/rte.app.mk
