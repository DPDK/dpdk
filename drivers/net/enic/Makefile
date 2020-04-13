# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
# Copyright 2007 Nuova Systems, Inc.  All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_enic.a

EXPORT_MAP := rte_pmd_enic_version.map

CFLAGS += -I$(SRCDIR)/base/
CFLAGS += -I$(SRCDIR)
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -Wno-strict-aliasing
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_hash
LDLIBS += -lrte_bus_pci

VPATH += $(SRCDIR)/src

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_main.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_clsf.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_res.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_fm_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_cq.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_wq.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_intr.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_rq.c

# The current implementation assumes 64-bit pointers
CC_AVX2_SUPPORT=0
ifeq ($(CONFIG_RTE_ARCH_X86_64),y)
# Figure out if the compiler supports avx2. The extra check using
# -march=core-avx2 is necessary to support users who build for the
# 'default' machine (corei7 which has no avx2) and run the binary on
# newer CPUs that have avx2.
# This part is verbatim from i40e makefile.
ifeq ($(findstring RTE_MACHINE_CPUFLAG_AVX2,$(CFLAGS)),RTE_MACHINE_CPUFLAG_AVX2)
	CC_AVX2_SUPPORT=1
else
	CC_AVX2_SUPPORT=\
	$(shell $(CC) -march=core-avx2 -dM -E - </dev/null 2>&1 | \
	grep -q AVX2 && echo 1)
	ifeq ($(CC_AVX2_SUPPORT), 1)
		ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
			CFLAGS_enic_rxtx_vec_avx2.o += -march=core-avx2
		else
			CFLAGS_enic_rxtx_vec_avx2.o += -mavx2
		endif
	endif
endif
endif

ifeq ($(CC_AVX2_SUPPORT), 1)
	SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_rxtx_vec_avx2.c
endif

include $(RTE_SDK)/mk/rte.lib.mk
