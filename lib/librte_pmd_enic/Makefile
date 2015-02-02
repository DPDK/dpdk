#
# Copyright 2008-2014 Cisco Systems, Inc.  All rights reserved.
# Copyright 2007 Nuova Systems, Inc.  All rights reserved.
#
# Copyright (c) 2014, Cisco Systems, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in
# the documentation and/or other materials provided with the
# distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_enic.a

EXPORT_MAP := rte_pmd_enic_version.map

LIBABIVER := 1

CFLAGS += -I$(RTE_SDK)/lib/librte_pmd_enic/vnic/
CFLAGS += -I$(RTE_SDK)/lib/librte_pmd_enic/
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -Wno-strict-aliasing

VPATH += $(RTE_SDK)/lib/librte_pmd_enic/src

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_main.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_clsf.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_res.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += vnic/vnic_cq.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += vnic/vnic_wq.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += vnic/vnic_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += vnic/vnic_intr.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += vnic/vnic_rq.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += vnic/vnic_rss.c

# this lib depends upon:
DEPDIRS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += lib/librte_eal lib/librte_ether
DEPDIRS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += lib/librte_mempool lib/librte_mbuf
DEPDIRS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += lib/librte_net lib/librte_malloc
DEPDIRS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += lib/librte_hash

include $(RTE_SDK)/mk/rte.lib.mk
