# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_avf.a

CFLAGS += -O3
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_hash
LDLIBS += -lrte_bus_pci

# used to dump HW descriptor for debugging
# CFLAGS += -DDEBUG_DUMP_DESC

EXPORT_MAP := rte_pmd_avf_version.map

LIBABIVER := 1

#
# Add extra flags for base driver files (also known as shared code)
# to disable warnings
#
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS_BASE_DRIVER =
else ifeq ($(CONFIG_RTE_TOOLCHAIN_CLANG),y)
CFLAGS_BASE_DRIVER = -Wno-pointer-to-int-cast
else
CFLAGS_BASE_DRIVER = -Wno-pointer-to-int-cast

endif
OBJS_BASE_DRIVER=$(sort $(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c))))
$(foreach obj, $(OBJS_BASE_DRIVER), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))


VPATH += $(SRCDIR)/base

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_AVF_PMD) += avf_adminq.c
SRCS-$(CONFIG_RTE_LIBRTE_AVF_PMD) += avf_common.c

SRCS-$(CONFIG_RTE_LIBRTE_AVF_PMD) += avf_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_AVF_PMD) += avf_vchnl.c
SRCS-$(CONFIG_RTE_LIBRTE_AVF_PMD) += avf_rxtx.c
ifeq ($(CONFIG_RTE_ARCH_X86), y)
SRCS-$(CONFIG_RTE_LIBRTE_AVF_INC_VECTOR) += avf_rxtx_vec_sse.c
endif

include $(RTE_SDK)/mk/rte.lib.mk
