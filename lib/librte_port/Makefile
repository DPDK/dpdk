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
LIB = librte_port.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_port_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_ring.c
ifeq ($(CONFIG_RTE_LIBRTE_IP_FRAG),y)
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_frag.c
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_ras.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_sched.c
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_source_sink.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port.h
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_ethdev.h
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_ring.h
ifeq ($(CONFIG_RTE_LIBRTE_IP_FRAG),y)
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_frag.h
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_ras.h
endif
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_sched.h
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_source_sink.h

# this lib depends upon:
DEPDIRS-$(CONFIG_RTE_LIBRTE_PORT) := lib/librte_eal
DEPDIRS-$(CONFIG_RTE_LIBRTE_PORT) += lib/librte_mbuf
DEPDIRS-$(CONFIG_RTE_LIBRTE_PORT) += lib/librte_mempool
DEPDIRS-$(CONFIG_RTE_LIBRTE_PORT) += lib/librte_malloc
DEPDIRS-$(CONFIG_RTE_LIBRTE_PORT) += lib/librte_ether
DEPDIRS-$(CONFIG_RTE_LIBRTE_PORT) += lib/librte_ip_frag

include $(RTE_SDK)/mk/rte.lib.mk
