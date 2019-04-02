# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_ipsec.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
CFLAGS += -DALLOW_EXPERIMENTAL_API
LDLIBS += -lrte_eal -lrte_mbuf -lrte_net -lrte_cryptodev -lrte_security

EXPORT_MAP := rte_ipsec_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_IPSEC) += esp_inb.c
SRCS-$(CONFIG_RTE_LIBRTE_IPSEC) += esp_outb.c
SRCS-$(CONFIG_RTE_LIBRTE_IPSEC) += sa.c
SRCS-$(CONFIG_RTE_LIBRTE_IPSEC) += ses.c

# install header files
SYMLINK-$(CONFIG_RTE_LIBRTE_IPSEC)-include += rte_ipsec.h
SYMLINK-$(CONFIG_RTE_LIBRTE_IPSEC)-include += rte_ipsec_group.h
SYMLINK-$(CONFIG_RTE_LIBRTE_IPSEC)-include += rte_ipsec_sa.h

include $(RTE_SDK)/mk/rte.lib.mk
