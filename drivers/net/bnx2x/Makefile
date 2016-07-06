include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_bnx2x.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -DZLIB_CONST
LDLIBS += -lz

EXPORT_MAP := rte_pmd_bnx2x_version.map

LIBABIVER := 1

ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -wd188 #188: enumerated type mixed with another type
endif

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x_stats.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += ecore_sp.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += elink.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x_vfpf.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_DEBUG_PERIODIC) += debug.c

# this lib depends upon:
DEPDIRS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += lib/librte_eal lib/librte_ether
DEPDIRS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += lib/librte_mempool lib/librte_mbuf

include $(RTE_SDK)/mk/rte.lib.mk
