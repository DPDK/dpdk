# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_i40e.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -DPF_DRIVER -DVF_DRIVER -DINTEGRATED_VF
CFLAGS += -DX722_A0_SUPPORT
CFLAGS += -DALLOW_EXPERIMENTAL_API

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_hash
LDLIBS += -lrte_bus_pci

EXPORT_MAP := rte_pmd_i40e_version.map

LIBABIVER := 2

#
# Add extra flags for base driver files (also known as shared code)
# to disable warnings
#
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS_BASE_DRIVER = -diag-disable 593
else ifeq ($(CONFIG_RTE_TOOLCHAIN_CLANG),y)
CFLAGS_BASE_DRIVER += -Wno-sign-compare
CFLAGS_BASE_DRIVER += -Wno-unused-value
CFLAGS_BASE_DRIVER += -Wno-unused-parameter
CFLAGS_BASE_DRIVER += -Wno-strict-aliasing
CFLAGS_BASE_DRIVER += -Wno-format
CFLAGS_BASE_DRIVER += -Wno-missing-field-initializers
CFLAGS_BASE_DRIVER += -Wno-pointer-to-int-cast
CFLAGS_BASE_DRIVER += -Wno-format-nonliteral
CFLAGS_BASE_DRIVER += -Wno-unused-variable
else
CFLAGS_BASE_DRIVER  = -Wno-sign-compare
CFLAGS_BASE_DRIVER += -Wno-unused-value
CFLAGS_BASE_DRIVER += -Wno-unused-parameter
CFLAGS_BASE_DRIVER += -Wno-strict-aliasing
CFLAGS_BASE_DRIVER += -Wno-format
CFLAGS_BASE_DRIVER += -Wno-missing-field-initializers
CFLAGS_BASE_DRIVER += -Wno-pointer-to-int-cast
CFLAGS_BASE_DRIVER += -Wno-format-nonliteral
CFLAGS_BASE_DRIVER += -Wno-format-security
CFLAGS_BASE_DRIVER += -Wno-unused-variable

ifeq ($(shell test $(GCC_VERSION) -ge 44 && echo 1), 1)
CFLAGS_BASE_DRIVER += -Wno-unused-but-set-variable
endif

CFLAGS_i40e_lan_hmc.o += -Wno-error
endif
OBJS_BASE_DRIVER=$(sort $(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c))))
$(foreach obj, $(OBJS_BASE_DRIVER), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

VPATH += $(SRCDIR)/base

#
# all source are stored in SRCS-y
# base driver is based on the package of dpdk-i40e.2016.04.18.12.tar.gz.
#
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_adminq.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_common.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_diag.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_hmc.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_lan_hmc.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_nvm.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_dcb.c

SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_rxtx.c
ifeq ($(CONFIG_RTE_ARCH_ARM64),y)
SRCS-$(CONFIG_RTE_LIBRTE_I40E_INC_VECTOR) += i40e_rxtx_vec_neon.c
else ifeq ($(CONFIG_RTE_ARCH_PPC_64),y)
SRCS-$(CONFIG_RTE_LIBRTE_I40E_INC_VECTOR) += i40e_rxtx_vec_altivec.c
else
SRCS-$(CONFIG_RTE_LIBRTE_I40E_INC_VECTOR) += i40e_rxtx_vec_sse.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_ethdev_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_pf.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_fdir.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += rte_pmd_i40e.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_tm.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_vf_representor.c

ifeq ($(findstring RTE_MACHINE_CPUFLAG_AVX2,$(CFLAGS)),RTE_MACHINE_CPUFLAG_AVX2)
	CC_AVX2_SUPPORT=1
else
	CC_AVX2_SUPPORT=\
	$(shell $(CC) -march=core-avx2 -dM -E - </dev/null 2>&1 | \
	grep -q AVX2 && echo 1)
	ifeq ($(CC_AVX2_SUPPORT), 1)
		ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
			CFLAGS_i40e_rxtx_vec_avx2.o += -march=core-avx2
		else
			CFLAGS_i40e_rxtx_vec_avx2.o += -mavx2
		endif
	endif
endif

ifeq ($(CC_AVX2_SUPPORT), 1)
	SRCS-$(CONFIG_RTE_LIBRTE_I40E_INC_VECTOR) += i40e_rxtx_vec_avx2.c
endif

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_I40E_PMD)-include := rte_pmd_i40e.h

include $(RTE_SDK)/mk/rte.lib.mk
