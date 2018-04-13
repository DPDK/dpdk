# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2013-2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_fm10k.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_fm10k_version.map

LIBABIVER := 1

ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
#
# CFLAGS for icc
#
CFLAGS_BASE_DRIVER  = -diag-disable 174 -diag-disable 593 -diag-disable 869
CFLAGS_BASE_DRIVER += -diag-disable 981 -diag-disable 2259

else ifeq ($(CONFIG_RTE_TOOLCHAIN_CLANG),y)

#
## CFLAGS for clang
#
CFLAGS_BASE_DRIVER = -Wno-unused-parameter -Wno-unused-value
CFLAGS_BASE_DRIVER += -Wno-strict-aliasing -Wno-format-extra-args
CFLAGS_BASE_DRIVER += -Wno-unused-variable
CFLAGS_BASE_DRIVER += -Wno-missing-field-initializers

else
#
# CFLAGS for gcc
#
CFLAGS_BASE_DRIVER = -Wno-unused-parameter -Wno-unused-value
CFLAGS_BASE_DRIVER += -Wno-strict-aliasing -Wno-format-extra-args
CFLAGS_BASE_DRIVER += -Wno-unused-variable
CFLAGS_BASE_DRIVER += -Wno-missing-field-initializers

ifeq ($(shell test $(GCC_VERSION) -ge 44 && echo 1), 1)
CFLAGS     += -Wno-deprecated
CFLAGS_BASE_DRIVER += -Wno-unused-but-set-variable
ifeq ($(shell test $(GCC_VERSION) -ge 70 && echo 1), 1)
CFLAGS_BASE_DRIVER += -Wno-implicit-fallthrough
endif
endif
endif
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_hash
LDLIBS += -lrte_bus_pci

#
# Add extra flags for base driver source files to disable warnings in them
#
BASE_DRIVER_OBJS=$(sort $(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c))))
$(foreach obj, $(BASE_DRIVER_OBJS), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

VPATH += $(SRCDIR)/base

#
# all source are stored in SRCS-y
# base driver is based on the package of cid-fm10k.2017.01.24.tar.gz
#
SRCS-$(CONFIG_RTE_LIBRTE_FM10K_PMD) += fm10k_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_FM10K_PMD) += fm10k_rxtx.c

SRCS-$(CONFIG_RTE_LIBRTE_FM10K_PMD) += fm10k_pf.c
SRCS-$(CONFIG_RTE_LIBRTE_FM10K_PMD) += fm10k_tlv.c
SRCS-$(CONFIG_RTE_LIBRTE_FM10K_PMD) += fm10k_common.c
SRCS-$(CONFIG_RTE_LIBRTE_FM10K_PMD) += fm10k_mbx.c
SRCS-$(CONFIG_RTE_LIBRTE_FM10K_PMD) += fm10k_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_FM10K_PMD) += fm10k_api.c
SRCS-$(CONFIG_RTE_LIBRTE_FM10K_INC_VECTOR) += fm10k_rxtx_vec.c

include $(RTE_SDK)/mk/rte.lib.mk
