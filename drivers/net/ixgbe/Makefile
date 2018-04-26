# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_ixgbe.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_ixgbe_version.map

LIBABIVER := 2

ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
#
# CFLAGS for icc
#
CFLAGS_BASE_DRIVER  = -diag-disable 174 -diag-disable 593 -diag-disable 869
CFLAGS_BASE_DRIVER += -diag-disable 981 -diag-disable 2259

CFLAGS_ixgbe_rxtx.o += -diag-disable 3656

else ifeq ($(CONFIG_RTE_TOOLCHAIN_CLANG),y)
#
# CFLAGS for clang
#
CFLAGS_BASE_DRIVER = -Wno-unused-parameter -Wno-unused-value
CFLAGS_BASE_DRIVER += -Wno-strict-aliasing -Wno-format-extra-args

else
#
# CFLAGS for gcc
#
ifeq ($(shell test $(GCC_VERSION) -ge 44 && echo 1), 1)
CFLAGS     += -Wno-deprecated
CFLAGS_ixgbe_common.o += -Wno-unused-but-set-variable
CFLAGS_ixgbe_x550.o += -Wno-unused-but-set-variable
endif
CFLAGS_BASE_DRIVER = -Wno-unused-parameter -Wno-unused-value
CFLAGS_BASE_DRIVER += -Wno-strict-aliasing -Wno-format-extra-args

ifeq ($(shell test $(GCC_VERSION) -ge 46 && echo 1), 1)
CFLAGS_ixgbe_x550.o += -Wno-maybe-uninitialized
endif

ifeq ($(shell test $(GCC_VERSION) -ge 50 && echo 1), 1)
CFLAGS_ixgbe_common.o += -Wno-logical-not-parentheses
ifeq ($(shell test $(GCC_VERSION) -ge 70 && echo 1), 1)
CFLAGS_BASE_DRIVER += -Wno-implicit-fallthrough
endif
endif

endif
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_hash
LDLIBS += -lrte_bus_pci

#
# Add extra flags for base driver files (also known as shared code)
# to disable warnings in them
#
BASE_DRIVER_OBJS=$(sort $(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c))))
$(foreach obj, $(BASE_DRIVER_OBJS), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

VPATH += $(SRCDIR)/base

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_common.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_82598.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_82599.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_x540.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_x550.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_phy.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_api.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_hv_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_dcb.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_dcb_82599.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_dcb_82598.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_mbx.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_fdir.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_pf.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_flow.c
ifeq ($(CONFIG_RTE_ARCH_ARM64),y)
SRCS-$(CONFIG_RTE_IXGBE_INC_VECTOR) += ixgbe_rxtx_vec_neon.c
else
SRCS-$(CONFIG_RTE_IXGBE_INC_VECTOR) += ixgbe_rxtx_vec_sse.c
endif
ifeq ($(CONFIG_RTE_LIBRTE_IXGBE_BYPASS),y)
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_bypass.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_82599_bypass.c
endif
ifeq ($(CONFIG_RTE_LIBRTE_SECURITY),y)
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_ipsec.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += rte_pmd_ixgbe.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_tm.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_vf_representor.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_IXGBE_PMD)-include := rte_pmd_ixgbe.h

include $(RTE_SDK)/mk/rte.lib.mk
