# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_cmdline.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3

EXPORT_MAP := rte_cmdline_version.map

LIBABIVER := 2

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) := cmdline.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_cirbuf.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse_etheraddr.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse_ipaddr.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse_num.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse_string.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_rdline.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_vt100.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_socket.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse_portlist.c

CFLAGS += -D_GNU_SOURCE
LDLIBS += -lrte_eal

# install includes
INCS := cmdline.h cmdline_parse.h cmdline_parse_num.h cmdline_parse_ipaddr.h
INCS += cmdline_parse_etheraddr.h cmdline_parse_string.h cmdline_rdline.h
INCS += cmdline_vt100.h cmdline_socket.h cmdline_cirbuf.h cmdline_parse_portlist.h
SYMLINK-$(CONFIG_RTE_LIBRTE_CMDLINE)-include := $(INCS)

include $(RTE_SDK)/mk/rte.lib.mk
