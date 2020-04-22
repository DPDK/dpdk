# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_iavf.a

CFLAGS += -I$(RTE_SDK)/drivers/common/iavf
CFLAGS += -O3 $(WERROR_FLAGS) -Wno-strict-aliasing
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_hash
LDLIBS += -lrte_bus_pci
LDLIBS += -lrte_common_iavf

EXPORT_MAP := rte_pmd_iavf_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_vchnl.c
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_generic_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_fdir.c
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_hash.c
ifeq ($(CONFIG_RTE_ARCH_X86), y)
SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_rxtx_vec_sse.c
endif

ifeq ($(CONFIG_RTE_LIBRTE_IAVF_PMD), y)
	ifeq ($(findstring RTE_MACHINE_CPUFLAG_AVX2,$(CFLAGS)),RTE_MACHINE_CPUFLAG_AVX2)
		CC_AVX2_SUPPORT=1
	else
		CC_AVX2_SUPPORT=\
		$(shell $(CC) -march=core-avx2 -dM -E - </dev/null 2>&1 | \
		grep -q AVX2 && echo 1)
		ifeq ($(CC_AVX2_SUPPORT), 1)
			ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
				CFLAGS_iavf_rxtx_vec_avx2.o += -march=core-avx2
			else
				CFLAGS_iavf_rxtx_vec_avx2.o += -mavx2
			endif
		endif
	endif
endif

ifeq ($(CC_AVX2_SUPPORT), 1)
	SRCS-$(CONFIG_RTE_LIBRTE_IAVF_PMD) += iavf_rxtx_vec_avx2.c
endif

include $(RTE_SDK)/mk/rte.lib.mk
