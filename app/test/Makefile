# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2017 Intel Corporation

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
SRCS-y += test_barrier.c
SRCS-y += test_malloc.c
SRCS-y += test_cycles.c
SRCS-y += test_spinlock.c
SRCS-y += test_ticketlock.c
SRCS-y += test_memory.c
SRCS-y += test_memzone.c
SRCS-y += test_bitmap.c
SRCS-y += test_reciprocal_division.c
SRCS-y += test_reciprocal_division_perf.c
SRCS-y += test_fbarray.c
SRCS-y += test_external_mem.c

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
SRCS-$(CONFIG_RTE_LIBRTE_FLOW_CLASSIFY) += test_flow_classify.c
endif

SRCS-y += test_rwlock.c

SRCS-$(CONFIG_RTE_LIBRTE_STACK) += test_stack.c
SRCS-$(CONFIG_RTE_LIBRTE_STACK) += test_stack_perf.c

SRCS-$(CONFIG_RTE_LIBRTE_TIMER) += test_timer.c
SRCS-$(CONFIG_RTE_LIBRTE_TIMER) += test_timer_perf.c
SRCS-$(CONFIG_RTE_LIBRTE_TIMER) += test_timer_racecond.c
SRCS-$(CONFIG_RTE_LIBRTE_TIMER) += test_timer_secondary.c

SRCS-y += test_mempool.c
SRCS-y += test_mempool_perf.c

SRCS-y += test_mbuf.c
SRCS-y += test_logs.c

SRCS-y += test_memcpy.c
SRCS-y += test_memcpy_perf.c


SRCS-$(CONFIG_RTE_LIBRTE_MEMBER) += test_member.c
SRCS-$(CONFIG_RTE_LIBRTE_MEMBER) += test_member_perf.c

SRCS-$(CONFIG_RTE_LIBRTE_EFD) += test_efd.c
SRCS-$(CONFIG_RTE_LIBRTE_EFD) += test_efd_perf.c

SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_thash.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash_perf.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash_functions.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash_multiwriter.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash_readwrite.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += test_hash_readwrite_lf.c

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

SRCS-$(CONFIG_RTE_LIBRTE_BITRATE) += test_bitratestats.c
SRCS-$(CONFIG_RTE_LIBRTE_LATENCY_STATS) += test_latencystats.c

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

SRCS-$(CONFIG_RTE_LIBRTE_PDUMP) += test_pdump.c

SRCS-y += virtual_pmd.c
SRCS-y += packet_burst_generator.c
SRCS-y += sample_packet_forward.c
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
SRCS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += test_cryptodev.c
SRCS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += test_cryptodev_asym.c

SRCS-$(CONFIG_RTE_LIBRTE_METRICS) += test_metrics.c

ifeq ($(CONFIG_RTE_COMPRESSDEV_TEST),y)
SRCS-$(CONFIG_RTE_LIBRTE_COMPRESSDEV) += test_compressdev.c
endif

ifeq ($(CONFIG_RTE_LIBRTE_EVENTDEV),y)
SRCS-y += test_eventdev.c
SRCS-y += test_event_ring.c
SRCS-y += test_event_eth_rx_adapter.c
SRCS-y += test_event_eth_tx_adapter.c
SRCS-y += test_event_timer_adapter.c
SRCS-y += test_event_crypto_adapter.c
endif

ifeq ($(CONFIG_RTE_LIBRTE_RAWDEV),y)
SRCS-y += test_rawdev.c
endif

SRCS-$(CONFIG_RTE_LIBRTE_KVARGS) += test_kvargs.c

SRCS-$(CONFIG_RTE_LIBRTE_BPF) += test_bpf.c

SRCS-$(CONFIG_RTE_LIBRTE_RCU) += test_rcu_qsbr.c test_rcu_qsbr_perf.c

SRCS-$(CONFIG_RTE_LIBRTE_IPSEC) += test_ipsec.c
ifeq ($(CONFIG_RTE_LIBRTE_IPSEC),y)
LDLIBS += -lrte_ipsec
endif

CFLAGS += -DALLOW_EXPERIMENTAL_API

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lm

ifeq ($(CONFIG_RTE_LIBRTE_PDUMP),y)
LDLIBS += -lpthread
endif

ifeq ($(CONFIG_RTE_COMPRESSDEV_TEST),y)
ifeq ($(CONFIG_RTE_LIBRTE_COMPRESSDEV),y)
LDLIBS += -lz
endif
endif

# Disable VTA for memcpy test
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
ifeq ($(shell test $(GCC_VERSION) -ge 44 && echo 1), 1)
CFLAGS_test_memcpy.o += -fno-var-tracking-assignments
CFLAGS_test_memcpy_perf.o += -fno-var-tracking-assignments
# for older GCC versions, allow us to initialize an event using
# designated initializers.
ifeq ($(shell test $(GCC_VERSION) -le 50 && echo 1), 1)
CFLAGS_test_eventdev_sw.o += -Wno-missing-field-initializers
CFLAGS_test_event_timer_adapter.o += -Wno-missing-field-initializers
CFLAGS_test_event_crypto_adapter.o += -Wno-missing-field-initializers
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
