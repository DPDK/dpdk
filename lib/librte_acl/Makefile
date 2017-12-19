# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_acl.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal

EXPORT_MAP := rte_acl_version.map

LIBABIVER := 2

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += tb_mem.c

SRCS-$(CONFIG_RTE_LIBRTE_ACL) += rte_acl.c
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += acl_bld.c
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += acl_gen.c
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += acl_run_scalar.c

ifneq ($(filter y,$(CONFIG_RTE_ARCH_ARM) $(CONFIG_RTE_ARCH_ARM64)),)
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += acl_run_neon.c
CFLAGS_acl_run_neon.o += -flax-vector-conversions
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_acl_run_neon.o += -Wno-maybe-uninitialized
endif
else ifeq ($(CONFIG_RTE_ARCH_PPC_64),y)
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += acl_run_altivec.c
else
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += acl_run_sse.c
endif

#
# If the compiler supports AVX2 instructions,
# then add support for AVX2 classify method.
#

#check if flag for AVX2 is already on, if not set it up manually
ifeq ($(findstring RTE_MACHINE_CPUFLAG_AVX2,$(CFLAGS)),RTE_MACHINE_CPUFLAG_AVX2)
	CC_AVX2_SUPPORT=1
else
	CC_AVX2_SUPPORT=\
	$(shell $(CC) -march=core-avx2 -dM -E - </dev/null 2>&1 | \
	grep -q AVX2 && echo 1)
	ifeq ($(CC_AVX2_SUPPORT), 1)
		ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
		CFLAGS_acl_run_avx2.o += -march=core-avx2
		else
		CFLAGS_acl_run_avx2.o += -mavx2
		endif
	endif
endif

ifeq ($(CC_AVX2_SUPPORT), 1)
	SRCS-$(CONFIG_RTE_LIBRTE_ACL) += acl_run_avx2.c
	CFLAGS_rte_acl.o += -DCC_AVX2_SUPPORT
endif

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_ACL)-include := rte_acl_osdep.h
SYMLINK-$(CONFIG_RTE_LIBRTE_ACL)-include += rte_acl.h

include $(RTE_SDK)/mk/rte.lib.mk
