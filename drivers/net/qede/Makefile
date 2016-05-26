#    Copyright (c) 2016 QLogic Corporation.
#    All rights reserved.
#    www.qlogic.com
#
#    See LICENSE.qede_pmd for copyright and licensing details.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_qede.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lz

EXPORT_MAP := rte_pmd_qede_version.map

LIBABIVER := 1

#
# OS
#
OS_TYPE := $(shell uname -s)

#
# CFLAGS
#
CFLAGS_BASE_DRIVER = -Wno-unused-parameter
CFLAGS_BASE_DRIVER += -Wno-sign-compare
CFLAGS_BASE_DRIVER += -Wno-missing-prototypes
CFLAGS_BASE_DRIVER += -Wno-cast-qual
CFLAGS_BASE_DRIVER += -Wno-unused-function
CFLAGS_BASE_DRIVER += -Wno-unused-variable
CFLAGS_BASE_DRIVER += -Wno-strict-aliasing
CFLAGS_BASE_DRIVER += -Wno-missing-prototypes

ifneq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS_BASE_DRIVER += -Wno-unused-value
CFLAGS_BASE_DRIVER += -Wno-format-nonliteral
ifeq ($(OS_TYPE),Linux)
ifeq ($(shell clang -Wno-shift-negative-value -Werror -E - < /dev/null > /dev/null 2>&1; echo $$?),0)
CFLAGS_BASE_DRIVER += -Wno-shift-negative-value
endif
endif
endif

ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_BASE_DRIVER += -Wno-unused-but-set-variable
CFLAGS_BASE_DRIVER += -Wno-missing-declarations
CFLAGS_BASE_DRIVER += -Wno-maybe-uninitialized
CFLAGS_BASE_DRIVER += -Wno-strict-prototypes
ifeq ($(shell test $(GCC_VERSION) -ge 60 && echo 1), 1)
CFLAGS_BASE_DRIVER += -Wno-shift-negative-value
endif
else ifeq ($(CONFIG_RTE_TOOLCHAIN_CLANG),y)
CFLAGS_BASE_DRIVER += -Wno-format-extra-args
CFLAGS_BASE_DRIVER += -Wno-visibility
CFLAGS_BASE_DRIVER += -Wno-empty-body
CFLAGS_BASE_DRIVER += -Wno-invalid-source-encoding
CFLAGS_BASE_DRIVER += -Wno-sometimes-uninitialized
ifeq ($(shell clang -Wno-pointer-bool-conversion -Werror -E - < /dev/null > /dev/null 2>&1; echo $$?),0)
CFLAGS_BASE_DRIVER += -Wno-pointer-bool-conversion
endif
else
CFLAGS_BASE_DRIVER += -wd188 #188: enumerated type mixed with another type
endif

#
# Add extra flags for base ecore driver files
# to disable warnings in them
#
#
BASE_DRIVER_OBJS=$(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c)))
$(foreach obj, $(BASE_DRIVER_OBJS), $(eval CFLAGS+=$(CFLAGS_BASE_DRIVER)))

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_hw.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_cxt.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_l2.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_sp_commands.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_init_fw_funcs.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_spq.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_init_ops.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_mcp.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_int.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_dcbx.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/bcm_osal.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_sriov.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += base/ecore_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += qede_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += qede_eth_if.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += qede_main.c
SRCS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += qede_rxtx.c

# dependent libs:
DEPDIRS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += lib/librte_eal lib/librte_ether
DEPDIRS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += lib/librte_mempool lib/librte_mbuf
DEPDIRS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += lib/librte_net

include $(RTE_SDK)/mk/rte.lib.mk
