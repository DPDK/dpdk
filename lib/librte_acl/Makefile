#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_acl.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)

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
CFLAGS_acl_run_neon.o += -flax-vector-conversions -Wno-maybe-uninitialized
else ifeq ($(CONFIG_RTE_ARCH_PPC_64),y)
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += acl_run_altivec.c
else
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += acl_run_sse.c
#check if flag for SSE4.1 is already on, if not set it up manually
	ifeq ($(findstring RTE_MACHINE_CPUFLAG_SSE4_1,$(CFLAGS)),)
		CFLAGS_acl_run_sse.o += -msse4.1
	endif
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

# this lib needs eal
DEPDIRS-$(CONFIG_RTE_LIBRTE_ACL) += lib/librte_eal

include $(RTE_SDK)/mk/rte.lib.mk
