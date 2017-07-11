#   BSD LICENSE
#
#   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
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

ifeq ($(CONFIG_RTE_APP_TEST),y)

# default rule
all:

# Define an externally linked resource. A linked resource is an arbitrary
# file that is linked into the test binary. The application refers to this
# resource by name. The linked generates identifiers beg_<name> and end_<name>
# for referencing by the C code.
#
# Parameters: <unique name>, <file to be linked>
define linked_resource
SRCS-y += $(1).res.o
$(1).res.o: $(2)
	@  echo '  MKRES $$@'
	$Q [ "$$(<D)" = . ] || ln -fs $$<
	$Q $(OBJCOPY) -I binary -B $(RTE_OBJCOPY_ARCH) -O $(RTE_OBJCOPY_TARGET) \
		--rename-section                                         \
			.data=.rodata,alloc,load,data,contents,readonly  \
		--redefine-sym _binary_$$(subst .,_,$$(<F))_start=beg_$(1) \
		--redefine-sym _binary_$$(subst .,_,$$(<F))_end=end_$(1)   \
		--redefine-sym _binary_$$(subst .,_,$$(<F))_size=siz_$(1)  \
		$$(<F) $$@
endef

ifeq ($(CONFIG_RTE_APP_TEST_RESOURCE_TAR),y)
define linked_tar_resource
$(1).tar: $(2)
	@  echo '  TAR $$@'
	$Q tar -C $$(dir $$<) -cf $$@ $$(notdir $$<)
$(call linked_resource,$(1),$(1).tar)
endef
else # ! CONFIG_RTE_APP_TEST_RESOURCE_TAR
linked_tar_resource =
endif # CONFIG_RTE_APP_TEST_RESOURCE_TAR

#
# library name
#
APP = test

#
# all sources are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) := commands.c
SRCS-y += test.c
SRCS-y += resource.c
SRCS-y += test_resource.c
test_resource.res: test_resource.c
	@ cp $< $@
$(eval $(call linked_resource,test_resource_c,test_resource.res))
$(eval $(call linked_tar_resource,test_resource_tar,test_resource.c))
SRCS-$(CONFIG_RTE_APP_TEST_RESOURCE_TAR) += test_cfgfile.c
$(eval $(call linked_tar_resource,test_cfgfiles,test_cfgfiles))
SRCS-y += test_prefetch.c
SRCS-y += test_byteorder.c
SRCS-y += test_per_lcore.c
SRCS-y += test_atomic.c
SRCS-y += test_malloc.c
SRCS-y += test_cycles.c
SRCS-y += test_spinlock.c
SRCS-y += test_memory.c
SRCS-y += test_memzone.c

SRCS-y += test_ring.c
SRCS-y += test_ring_perf.c
SRCS-y += test_pmd_perf.c

ifeq ($(CONFIG_RTE_LIBRTE_TABLE),y)
SRCS-y += test_table.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += test_table_pipeline.c
SRCS-y += test_table_tables.c
SRCS-y += test_table_ports.c
SRCS-y += test_table_combined.c
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += test_table_acl.c
endif

SRCS-y += test_rwlock.c

SRCS-$(CONFIG_RTE_LIBRTE_TIMER) += test_timer.c
SRCS-$(CONFIG_RTE_LIBRTE_TIMER) += test_timer_perf.c
SRCS-$(CONFIG_RTE_LIBRTE_TIMER) += test_timer_racecond.c

SRCS-y += test_mempool.c
SRCS-y += test_mempool_perf.c

SRCS-y += test_mbuf.c
SRCS-y += test_logs.c

SRCS-y += test_memcpy.c
SRCS-y += test_memcpy_perf.c

SRCS-$(CONFIG_RTE_LIBRTE_EFD) += test_efd.c
SRCS-$(CONFIG_RTE_LIBRTE_EFD) += test_efd_perf.c

SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_thash.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash_perf.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash_functions.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash_scaling.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash_multiwriter.c

SRCS-$(CONFIG_RTE_LIBRTE_LPM) += test_lpm.c
SRCS-$(CONFIG_RTE_LIBRTE_LPM) += test_lpm_perf.c
SRCS-$(CONFIG_RTE_LIBRTE_LPM) += test_lpm6.c
SRCS-$(CONFIG_RTE_LIBRTE_LPM) += test_lpm6_perf.c

SRCS-y += test_debug.c
SRCS-y += test_errno.c
SRCS-y += test_tailq.c
SRCS-y += test_string_fns.c
SRCS-y += test_cpuflags.c
SRCS-y += test_mp_secondary.c
SRCS-y += test_eal_flags.c
SRCS-y += test_eal_fs.c
SRCS-y += test_alarm.c
SRCS-y += test_interrupts.c
SRCS-y += test_version.c
SRCS-y += test_func_reentrancy.c

SRCS-y += test_service_cores.c

SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_num.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_etheraddr.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_portlist.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_ipaddr.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_cirbuf.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_string.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test_cmdline_lib.c

SRCS-$(CONFIG_RTE_LIBRTE_NET) += test_crc.c

ifeq ($(CONFIG_RTE_LIBRTE_SCHED),y)
SRCS-y += test_red.c
SRCS-y += test_sched.c
endif

SRCS-$(CONFIG_RTE_LIBRTE_METER) += test_meter.c
SRCS-$(CONFIG_RTE_LIBRTE_KNI) += test_kni.c
SRCS-$(CONFIG_RTE_LIBRTE_POWER) += test_power.c test_power_acpi_cpufreq.c
SRCS-$(CONFIG_RTE_LIBRTE_POWER) += test_power_kvm_vm.c
SRCS-y += test_common.c

SRCS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR) += test_distributor.c
SRCS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR) += test_distributor_perf.c

SRCS-$(CONFIG_RTE_LIBRTE_REORDER) += test_reorder.c

SRCS-y += test_devargs.c
SRCS-y += virtual_pmd.c
SRCS-y += packet_burst_generator.c
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += test_acl.c

ifeq ($(CONFIG_RTE_LIBRTE_PMD_RING),y)
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += test_link_bonding.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += test_link_bonding_mode4.c
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_NULL),y)
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += test_link_bonding_rssconf.c
endif

SRCS-$(CONFIG_RTE_LIBRTE_PMD_RING) += test_pmd_ring.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_RING) += test_pmd_ring_perf.c

SRCS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += test_cryptodev_blockcipher.c
SRCS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += test_cryptodev_perf.c
SRCS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += test_cryptodev.c

ifeq ($(CONFIG_RTE_LIBRTE_EVENTDEV),y)
SRCS-y += test_eventdev.c
SRCS-y += test_event_ring.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += test_eventdev_sw.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += test_eventdev_octeontx.c
endif

SRCS-$(CONFIG_RTE_LIBRTE_KVARGS) += test_kvargs.c

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

CFLAGS += -D_GNU_SOURCE

LDLIBS += -lm

# Disable VTA for memcpy test
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
ifeq ($(shell test $(GCC_VERSION) -ge 44 && echo 1), 1)
CFLAGS_test_memcpy.o += -fno-var-tracking-assignments
CFLAGS_test_memcpy_perf.o += -fno-var-tracking-assignments
# for older GCC versions, allow us to initialize an event using
# designated initializers.
ifeq ($(shell test $(GCC_VERSION) -le 50 && echo 1), 1)
CFLAGS_test_eventdev_sw.o += -Wno-missing-field-initializers
endif
endif
endif

# Link against shared libraries when needed
ifeq ($(CONFIG_RTE_LIBRTE_PMD_BOND),y)
ifneq ($(CONFIG_RTE_LIBRTE_PMD_RING),y)
$(error Link bonding tests require CONFIG_RTE_LIBRTE_PMD_RING=y)
endif
endif

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)

ifeq ($(CONFIG_RTE_LIBRTE_PMD_BOND),y)
LDLIBS += -lrte_pmd_bond
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_NULL),y)
LDLIBS += -lrte_pmd_null
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_RING),y)
LDLIBS += -lrte_pmd_ring
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER),y)
LDLIBS += -lrte_pmd_crypto_scheduler
endif

endif

ifeq ($(CONFIG_RTE_APP_TEST_RESOURCE_TAR),y)
LDLIBS += -larchive
endif

include $(RTE_SDK)/mk/rte.app.mk

endif
