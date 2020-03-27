# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_ice.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/iavf

LDLIBS += -lrte_eal -lrte_mbuf -lrte_ethdev -lrte_kvargs
LDLIBS += -lrte_bus_pci -lrte_mempool -lrte_hash
LDLIBS += -lrte_net -lrte_common_iavf
LDLIBS += -lpthread

EXPORT_MAP := rte_pmd_ice_version.map

#
# Add extra flags for base driver files (also known as shared code)
# to disable warnings
#
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS_BASE_DRIVER +=
else ifeq ($(CONFIG_RTE_TOOLCHAIN_CLANG),y)
CFLAGS_BASE_DRIVER += -Wno-unused-parameter
CFLAGS_BASE_DRIVER += -Wno-unused-variable
else
CFLAGS_BASE_DRIVER += -Wno-unused-parameter
CFLAGS_BASE_DRIVER += -Wno-unused-variable

ifeq ($(shell test $(GCC_VERSION) -ge 44 && echo 1), 1)
CFLAGS_BASE_DRIVER += -Wno-unused-but-set-variable
endif

endif
OBJS_BASE_DRIVER=$(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c)))
$(foreach obj, $(OBJS_BASE_DRIVER), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

VPATH += $(SRCDIR)/base

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_controlq.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_common.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_sched.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_switch.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_nvm.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_flex_pipe.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_dcb.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_fdir.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_acl.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_acl_ctrl.c

SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_rxtx.c
ifeq ($(CONFIG_RTE_ARCH_X86), y)
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_rxtx_vec_sse.c
endif

SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_switch_filter.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_fdir_filter.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_hash.c
ifeq ($(findstring RTE_MACHINE_CPUFLAG_AVX2,$(CFLAGS)),RTE_MACHINE_CPUFLAG_AVX2)
	CC_AVX2_SUPPORT=1
else
	CC_AVX2_SUPPORT=\
	$(shell $(CC) -march=core-avx2 -dM -E - </dev/null 2>&1 | \
	grep -q AVX2 && echo 1)
	ifeq ($(CC_AVX2_SUPPORT), 1)
		ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
			CFLAGS_ice_rxtx_vec_avx2.o += -march=core-avx2
		else
			CFLAGS_ice_rxtx_vec_avx2.o += -mavx2
		endif
	endif
endif

ifeq ($(CC_AVX2_SUPPORT), 1)
	SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_rxtx_vec_avx2.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_generic_flow.c

SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_dcf.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_dcf_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_ICE_PMD) += ice_dcf_parent.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_ICE_PMD)-include := rte_pmd_ice.h

include $(RTE_SDK)/mk/rte.lib.mk
