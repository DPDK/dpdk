#   BSD LICENSE
#
#   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
LIB = librte_pmd_e1000.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_e1000_version.map

LIBABIVER := 1

ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
#
# CFLAGS for icc
#
CFLAGS_BASE_DRIVER = -wd177 -wd181 -wd188 -wd869 -wd2259
else
#
# CFLAGS for gcc/clang
#
CFLAGS_BASE_DRIVER = -Wno-uninitialized -Wno-unused-parameter
CFLAGS_BASE_DRIVER += -Wno-unused-variable
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
ifeq ($(shell test $(GCC_VERSION) -ge 60 && echo 1), 1)
CFLAGS_BASE_DRIVER += -Wno-misleading-indentation
endif
endif
endif

#
# Add extra flags for base driver files (also known as shared code)
# to disable warnings in them
#
BASE_DRIVER_OBJS=$(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c)))
$(foreach obj, $(BASE_DRIVER_OBJS), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

VPATH += $(SRCDIR)/base

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_80003es2lan.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82540.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82541.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82542.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82543.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82571.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82575.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_i210.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_api.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_ich8lan.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_mac.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_manage.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_mbx.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_nvm.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_osdep.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_phy.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_IGB_PMD) += igb_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_IGB_PMD) += igb_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_IGB_PMD) += igb_pf.c
SRCS-$(CONFIG_RTE_LIBRTE_EM_PMD) += em_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_EM_PMD) += em_rxtx.c

# this lib depends upon:
DEPDIRS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += lib/librte_eal lib/librte_ether
DEPDIRS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += lib/librte_mempool lib/librte_mbuf
DEPDIRS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += lib/librte_net

include $(RTE_SDK)/mk/rte.lib.mk
