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
LIB = librte_cmdline.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3

EXPORT_MAP := rte_cmdline_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) := cmdline.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_cirbuf.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse_etheraddr.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse_ipaddr.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse_num.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse_string.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_rdline.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_vt100.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_socket.c
SRCS-$(CONFIG_RTE_LIBRTE_CMDLINE) += cmdline_parse_portlist.c

CFLAGS += -D_GNU_SOURCE

# install includes
INCS := cmdline.h cmdline_parse.h cmdline_parse_num.h cmdline_parse_ipaddr.h
INCS += cmdline_parse_etheraddr.h cmdline_parse_string.h cmdline_rdline.h
INCS += cmdline_vt100.h cmdline_socket.h cmdline_cirbuf.h cmdline_parse_portlist.h
SYMLINK-$(CONFIG_RTE_LIBRTE_CMDLINE)-include := $(INCS)

# this lib needs eal
DEPDIRS-$(CONFIG_RTE_LIBRTE_CMDLINE) += lib/librte_eal

include $(RTE_SDK)/mk/rte.lib.mk
