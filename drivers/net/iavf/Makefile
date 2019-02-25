# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_iavf.a

CFLAGS += -O3 $(WERROR_FLAGS) -Wno-strict-aliasing -DALLOW_EXPERIMENTAL_API
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_hash
LDLIBS += -lrte_bus_pci

# used to dump HW descriptor for debugging
# CFLAGS += -DDEBUG_DUMP_DESC

EXPORT_MAP := rte_pmd_iavf_version.map

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
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_adminq.c
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_common.c

SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_vchnl.c
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_rxtx.c
ifeq ($(CONFIG_RTE_ARCH_X86), y)
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_INC_VECTOR) += iavf_rxtx_vec_sse.c
endif

include $(RTE_SDK)/mk/rte.lib.mk
