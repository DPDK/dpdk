# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_ipsec.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf -lrte_net
LDLIBS += -lrte_cryptodev -lrte_security -lrte_hash

EXPORT_MAP := rte_ipsec_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_IPSEC) += esp_inb.c
SRCS-$(CONFIG_RTE_LIBRTE_IPSEC) += esp_outb.c
SRCS-$(CONFIG_RTE_LIBRTE_IPSEC) += sa.c
SRCS-$(CONFIG_RTE_LIBRTE_IPSEC) += ses.c
SRCS-$(CONFIG_RTE_LIBRTE_IPSEC) += ipsec_sad.c

# install header files
SYMLINK-$(CONFIG_RTE_LIBRTE_IPSEC)-include += rte_ipsec.h
SYMLINK-$(CONFIG_RTE_LIBRTE_IPSEC)-include += rte_ipsec_group.h
SYMLINK-$(CONFIG_RTE_LIBRTE_IPSEC)-include += rte_ipsec_sa.h
SYMLINK-$(CONFIG_RTE_LIBRTE_IPSEC)-include += rte_ipsec_sad.h

include $(RTE_SDK)/mk/rte.lib.mk
