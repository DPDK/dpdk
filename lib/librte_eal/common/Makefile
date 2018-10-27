# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

INC := rte_branch_prediction.h rte_common.h
INC += rte_debug.h rte_eal.h rte_eal_interrupts.h
INC += rte_errno.h rte_launch.h rte_lcore.h
INC += rte_log.h rte_memory.h rte_memzone.h
INC += rte_per_lcore.h rte_random.h
INC += rte_tailq.h rte_interrupts.h rte_alarm.h
INC += rte_string_fns.h rte_version.h
INC += rte_eal_memconfig.h rte_malloc_heap.h
INC += rte_hexdump.h rte_devargs.h rte_bus.h rte_dev.h rte_class.h
INC += rte_option.h
INC += rte_pci_dev_feature_defs.h rte_pci_dev_features.h
INC += rte_malloc.h rte_keepalive.h rte_time.h
INC += rte_service.h rte_service_component.h
INC += rte_bitmap.h rte_vfio.h rte_hypervisor.h rte_test.h
INC += rte_reciprocal.h rte_fbarray.h rte_uuid.h

GENERIC_INC := rte_atomic.h rte_byteorder.h rte_cycles.h rte_prefetch.h
GENERIC_INC += rte_spinlock.h rte_memcpy.h rte_cpuflags.h rte_rwlock.h
GENERIC_INC += rte_vect.h rte_pause.h rte_io.h

# defined in mk/arch/$(RTE_ARCH)/rte.vars.mk
ARCH_DIR ?= $(RTE_ARCH)
ARCH_INC := $(sort $(notdir $(wildcard $(RTE_SDK)/lib/librte_eal/common/include/arch/$(ARCH_DIR)/*.h)))

SYMLINK-$(CONFIG_RTE_LIBRTE_EAL)-include := $(addprefix include/,$(INC))
SYMLINK-$(CONFIG_RTE_LIBRTE_EAL)-include += \
	$(addprefix include/arch/$(ARCH_DIR)/,$(ARCH_INC))
SYMLINK-$(CONFIG_RTE_LIBRTE_EAL)-include/generic := \
	$(addprefix include/generic/,$(GENERIC_INC))

include $(RTE_SDK)/mk/rte.install.mk
