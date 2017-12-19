# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_distributor.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_ethdev

EXPORT_MAP := rte_distributor_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR) := rte_distributor_v20.c
SRCS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR) += rte_distributor.c
ifeq ($(CONFIG_RTE_ARCH_X86),y)
SRCS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR) += rte_distributor_match_sse.c
else
SRCS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR) += rte_distributor_match_generic.c
endif


# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR)-include := rte_distributor.h

include $(RTE_SDK)/mk/rte.lib.mk
