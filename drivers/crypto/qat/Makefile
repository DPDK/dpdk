# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_qat.a

# library version
LIBABIVER := 1

# build flags
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -O3

# external library include paths
CFLAGS += -I$(SRCDIR)/qat_adf

# library common source files
SRCS-y += qat_device.c
SRCS-y += qat_common.c
SRCS-y += qat_logs.c
SRCS-y += qat_qp.c

# library symmetric crypto source files
LDLIBS += -lrte_cryptodev
LDLIBS += -lcrypto
SRCS-y += qat_sym.c
SRCS-y += qat_sym_session.c
SRCS-y += qat_sym_pmd.c

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool
LDLIBS += -lrte_pci -lrte_bus_pci

# export include files
SYMLINK-y-include +=

# versioning export map
EXPORT_MAP := rte_pmd_qat_version.map

include $(RTE_SDK)/mk/rte.lib.mk
