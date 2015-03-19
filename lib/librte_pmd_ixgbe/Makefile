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

#
# library name
#
LIB = librte_pmd_ixgbe.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_ixgbe_version.map

LIBABIVER := 1

ifeq ($(CC), icc)
#
# CFLAGS for icc
#
CFLAGS_BASE_DRIVER = -wd174 -wd593 -wd869 -wd981 -wd2259

else ifeq ($(CC), clang)
#
# CFLAGS for clang
#
CFLAGS_BASE_DRIVER = -Wno-unused-parameter -Wno-unused-value
CFLAGS_BASE_DRIVER += -Wno-strict-aliasing -Wno-format-extra-args

else
#
# CFLAGS for gcc
#
ifeq ($(shell test $(GCC_VERSION) -ge 44 && echo 1), 1)
CFLAGS     += -Wno-deprecated
CFLAGS_ixgbe_common.o += -Wno-unused-but-set-variable
CFLAGS_ixgbe_x550.o += -Wno-unused-but-set-variable
endif
CFLAGS_BASE_DRIVER = -Wno-unused-parameter -Wno-unused-value
CFLAGS_BASE_DRIVER += -Wno-strict-aliasing -Wno-format-extra-args

ifeq ($(shell test $(GCC_VERSION) -ge 46 && echo 1), 1)
CFLAGS_ixgbe_x550.o += -Wno-maybe-uninitialized
endif

ifeq ($(shell test $(GCC_VERSION) -ge 50 && echo 1), 1)
CFLAGS_ixgbe_common.o += -Wno-logical-not-parentheses
endif

ifeq ($(shell test $(GCC_VERSION) -le 46 && echo 1), 1)
CFLAGS_ixgbe_x550.o += -Wno-uninitialized
CFLAGS_ixgbe_phy.o += -Wno-uninitialized
endif
endif

#
# Add extra flags for base driver files (also known as shared code)
# to disable warnings in them
#
BASE_DRIVER_OBJS=$(patsubst %.c,%.o,$(notdir $(wildcard $(RTE_SDK)/lib/librte_pmd_ixgbe/ixgbe/*.c)))
$(foreach obj, $(BASE_DRIVER_OBJS), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

VPATH += $(RTE_SDK)/lib/librte_pmd_ixgbe/ixgbe

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_common.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_82598.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_82599.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_x540.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_x550.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_phy.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_api.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_dcb.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_dcb_82599.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_dcb_82598.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_mbx.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_fdir.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_pf.c
SRCS-$(CONFIG_RTE_IXGBE_INC_VECTOR) += ixgbe_rxtx_vec.c

ifeq ($(CONFIG_RTE_NIC_BYPASS),y)
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_bypass.c
SRCS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe_82599_bypass.c
endif


# this lib depends upon:
DEPDIRS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += lib/librte_eal lib/librte_ether
DEPDIRS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += lib/librte_mempool lib/librte_mbuf
DEPDIRS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += lib/librte_net lib/librte_malloc

ifeq ($(CONFIG_RTE_IXGBE_INC_VECTOR)$(CONFIG_RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC),yn)
$(error The ixgbe vpmd depends on Rx bulk alloc)
endif

include $(RTE_SDK)/mk/rte.lib.mk
