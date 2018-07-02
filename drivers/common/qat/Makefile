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

# build directories
QAT_CRYPTO_DIR := $(RTE_SDK)/drivers/crypto/qat
VPATH=$(QAT_CRYPTO_DIR)

# external library include paths
CFLAGS += -I$(SRCDIR)/qat_adf
CFLAGS += -I$(SRCDIR)
CFLAGS += -I$(QAT_CRYPTO_DIR)

# library common source files
SRCS-y += qat_device.c
SRCS-y += qat_common.c
SRCS-y += qat_logs.c
SRCS-y += qat_qp.c

# library symmetric crypto source files
ifeq ($(CONFIG_RTE_LIBRTE_CRYPTODEV),y)
	LDLIBS += -lrte_cryptodev
	LDLIBS += -lcrypto
	CFLAGS += -DBUILD_QAT_SYM
	SRCS-y += qat_sym.c
	SRCS-y += qat_sym_session.c
	SRCS-y += qat_sym_pmd.c
endif

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool
LDLIBS += -lrte_pci -lrte_bus_pci

# export include files
SYMLINK-y-include +=

# versioning export map
EXPORT_MAP := ../../crypto/qat/rte_pmd_qat_version.map

include $(RTE_SDK)/mk/rte.lib.mk
