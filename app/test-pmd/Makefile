# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

ifeq ($(CONFIG_RTE_TEST_PMD),y)

#
# library name
#
APP = testpmd

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

#
# all source are stored in SRCS-y
#
SRCS-y := testpmd.c
SRCS-y += parameters.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_mtr.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_tm.c
SRCS-y += config.c
SRCS-y += iofwd.c
SRCS-y += macfwd.c
SRCS-y += macswap.c
SRCS-y += flowgen.c
SRCS-y += rxonly.c
SRCS-y += txonly.c
SRCS-y += csumonly.c
SRCS-y += icmpecho.c
SRCS-$(CONFIG_RTE_LIBRTE_IEEE1588) += ieee1588fwd.c
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf_cmd.c

ifeq ($(CONFIG_RTE_LIBRTE_PMD_SOFTNIC)$(CONFIG_RTE_LIBRTE_SCHED),yy)
SRCS-y += tm.c
endif

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)

ifeq ($(CONFIG_RTE_LIBRTE_PMD_BOND),y)
LDLIBS += -lrte_pmd_bond
endif

ifeq ($(CONFIG_RTE_LIBRTE_DPAA_BUS)$(CONFIG_RTE_LIBRTE_DPAA_PMD),yy)
LDLIBS += -lrte_pmd_dpaa
LDLIBS += -lrte_bus_dpaa
LDLIBS += -lrte_mempool_dpaa
endif

ifeq ($(CONFIG_RTE_LIBRTE_IXGBE_PMD),y)
LDLIBS += -lrte_pmd_ixgbe
endif

ifeq ($(CONFIG_RTE_LIBRTE_I40E_PMD),y)
LDLIBS += -lrte_pmd_i40e
endif

ifeq ($(CONFIG_RTE_LIBRTE_BNXT_PMD),y)
LDLIBS += -lrte_pmd_bnxt
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_SOFTNIC),y)
LDLIBS += -lrte_pmd_softnic
endif

endif

CFLAGS_cmdline.o := -D_GNU_SOURCE

include $(RTE_SDK)/mk/rte.app.mk

endif
