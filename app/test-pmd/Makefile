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
APP = testpmd

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

ifeq ($(CONFIG_RTE_LIBRTE_PMD_PCAP),y)
LDFLAGS += -lpcap
endif

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_TEST_PMD) := testpmd.c
SRCS-$(CONFIG_RTE_TEST_PMD) += parameters.c
ifeq ($(CONFIG_RTE_LIBRTE_CMDLINE),y)
SRCS-$(CONFIG_RTE_TEST_PMD) += cmdline.c
endif
SRCS-$(CONFIG_RTE_TEST_PMD) += config.c
SRCS-$(CONFIG_RTE_TEST_PMD) += iofwd.c
SRCS-$(CONFIG_RTE_TEST_PMD) += macfwd.c
SRCS-$(CONFIG_RTE_TEST_PMD) += macfwd-retry.c
SRCS-$(CONFIG_RTE_TEST_PMD) += macswap.c
SRCS-$(CONFIG_RTE_TEST_PMD) += flowgen.c
SRCS-$(CONFIG_RTE_TEST_PMD) += rxonly.c
SRCS-$(CONFIG_RTE_TEST_PMD) += txonly.c
SRCS-$(CONFIG_RTE_TEST_PMD) += csumonly.c
SRCS-$(CONFIG_RTE_TEST_PMD) += icmpecho.c
ifeq ($(CONFIG_RTE_LIBRTE_IEEE1588),y)
SRCS-$(CONFIG_RTE_TEST_PMD) += ieee1588fwd.c
endif
SRCS-$(CONFIG_RTE_TEST_PMD) += mempool_anon.c

ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP),y)
CFLAGS_mempool_anon.o := -D_GNU_SOURCE
endif
CFLAGS_cmdline.o := -D_GNU_SOURCE

# this application needs libraries first
DEPDIRS-$(CONFIG_RTE_TEST_PMD) += lib

include $(RTE_SDK)/mk/rte.app.mk
