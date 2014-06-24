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
LIB = librte_pmd_i40e.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

#
# Add extra flags for base driver files (also known as shared code)
# to disable warnings
#
ifeq ($(CC), icc)
CFLAGS_BASE_DRIVER = -wd593
else
CFLAGS_BASE_DRIVER  = -Wno-unused-but-set-variable
CFLAGS_BASE_DRIVER += -Wno-sign-compare
CFLAGS_BASE_DRIVER += -Wno-unused-value
CFLAGS_BASE_DRIVER += -Wno-unused-parameter
CFLAGS_BASE_DRIVER += -Wno-strict-aliasing
CFLAGS_BASE_DRIVER += -Wno-format
CFLAGS_BASE_DRIVER += -Wno-missing-field-initializers
CFLAGS_BASE_DRIVER += -Wno-pointer-to-int-cast
CFLAGS_BASE_DRIVER += -Wno-format-nonliteral
CFLAGS_BASE_DRIVER += -Wno-format-security
CFLAGS_i40e_lan_hmc.o += -Wno-error
endif
OBJS_BASE_DRIVER=$(patsubst %.c,%.o,$(notdir $(wildcard $(RTE_SDK)/lib/librte_pmd_i40e/i40e/*.c)))
$(foreach obj, $(OBJS_BASE_DRIVER), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

VPATH += $(RTE_SDK)/lib/librte_pmd_i40e/i40e

#
# all source are stored in SRCS-y
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
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_ethdev_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e_pf.c
# this lib depends upon:
DEPDIRS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += lib/librte_eal lib/librte_ether
DEPDIRS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += lib/librte_mempool lib/librte_mbuf
DEPDIRS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += lib/librte_net lib/librte_malloc

include $(RTE_SDK)/mk/rte.lib.mk
